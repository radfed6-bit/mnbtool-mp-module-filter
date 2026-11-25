// Restores multiplayer server discovery on PC Warband v1.174 and the missing
// "Compatible with module" checkbox in Mount & Blade: Warband's multiplayer
// server browser (Multiplayer_client_window):
//   1. Hooks Network_manager::send_http_request (0x1487ec) — which runs BEFORE
//      Request::SendHTTP (0x1d2718) and therefore ahead of
//      builtin.multiplayer-master-server-fix's FIFO chain head — to rewrite
//      TaleWorlds master-server list queries to HTTPS + ?gametype=warband&type=list
//      so builtin.multiplayer-master-server-fix (0x1995150) / builtin.custom-server
//      fetches the full PC server list via ModDevice.httpsGet instead of
//      blackholing it.
//   2. Temporarily normalizes PC 1.170-1.174 server rows to 1157 across
//      apply_filter() when CustomServer booted in 1157 (due to startup load
//      order with ResourceOverrides or non-Native modules), restoring the real
//      version immediately afterward.
//   3. Restores the "Compatible with module" checkbox in the pre-existing
//      game_variables.txt slot (y = 0.046) between "Has players" (y = 0.071)
//      and "No password" (y = 0.021).
//
// Built with the official M&BTool ModFramework SDK (mod_api.h + build-mod.sh).
// Game records keep their ARM32 layout on translated ARM64; the SDK's build
// pipeline generates ARM64 hook adapters, game-call wrappers, and pointer
// conversions automatically from the ARMv7 source contract.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "mod_api.h"

extern "C" void* malloc(size_t);
extern "C" void free(void*);

namespace {
constexpr const char* kGame = "libMBExpMobile.so";

// Hooked game methods (libMBExpMobile.so offsets)
constexpr uintptr_t kMpWinCheck          = 0x122c38; // Multiplayer_client_window::check(float)
constexpr uintptr_t kMpWinDeactivate     = 0x12f040; // Multiplayer_client_window::deactivate()
constexpr uintptr_t kMpWinActivate       = 0x12f06c; // Multiplayer_client_window::activate()
constexpr uintptr_t kMpWinApplyFilterRow = 0x13ea98; // Multiplayer_client_window::apply_filter(rglTable_row*)
constexpr uintptr_t kSendHttpRequest     = 0x1487ec; // Network_manager::send_http_request(const rglString&) (sret r0, this r1, url r2)

// Game data global: rglString selected_module_name (size 0x90)
constexpr uintptr_t kSelectedModuleName = 0x2785884;
constexpr size_t    kRglStringSize      = 0x90;

// Invoked game functions (mangled ARMv7 symbols in libMBExpMobile.so)
constexpr const char* kSymCreateCheckBox = "_Z23widget_create_check_boxP19rglContainer_widgetRK9rglStringS3_RKfS5_S5_";
constexpr const char* kSymRglStringCtor  = "_ZN9rglStringC1EPKc";
constexpr const char* kSymRglStringDtor  = "_ZN9rglStringD1Ev";
constexpr const char* kSymRglStringCharP = "_ZNK9rglString6char_pEv";
constexpr const char* kSymCheckBoxSetVal = "_ZN19rglCheck_box_widget11set_value_iEi";
constexpr const char* kSymWidgetGetVal   = "_ZN14rglWidget_base11get_value_iEv";
constexpr const char* kSymWidgetGetEvent = "_ZN14rglWidget_base9get_eventE15rglWindow_event";
constexpr const char* kSymApplyFilterAll = "_ZN25Multiplayer_client_window12apply_filterEv";
constexpr const char* kSymTableUpdate    = "_ZN15rglTable_widget12update_tableEv";

// Multiplayer_client_window member offsets (32-bit game pointers)
constexpr size_t kMpWinServersList    = 0x1e8; // rglTable_widget*
constexpr size_t kMpWinFilterCheckbox = 0x338; // rglCheck_box_widget* (master "Server filter")
constexpr size_t kMpWinFilterPanel    = 0x33c; // rglContainer_widget*

// rglTable_row -> cells[] -> cell payload at +0x2cc
constexpr size_t  kTableRowCellsData    = 0x35c; // rglTable_cell** (32-bit game pointer)
constexpr size_t  kTableRowVersionIndex = 2;     // byte offset +0x08 -> int32_t mb_version_no at +0x2cc
constexpr size_t  kTableRowModuleIndex  = 8;     // byte offset +0x20 -> rglString module_name at +0x2cc
constexpr size_t  kTableCellPayload     = 0x2cc; // payload inside rglTable_cell
constexpr int32_t kVersion1157          = 1157;  // 0x485 (Android/Tegra 1.158 protocol version)
constexpr int32_t kVersion1170Min       = 1170;  // 0x492 (PC Warband 1.170-1.174 protocol version)
constexpr int32_t kVersion1170Max       = 1174;

// Event mask passed by Multiplayer_client_window::check() to checkbox get_event()
constexpr int32_t kWidgetEventChanged = 0x10000;

// Master server list URL rewrite target (official TaleWorlds HTTPS endpoint).
// Query order ?gametype=warband&type=list is critical: TaleWorlds' ASP.NET
// handler accepts parameters in any order (returns full PC server list), while
// builtin.multiplayer-master-server-fix (0x19950f0) matches the literal
// substring "warbandmain.taleworlds.com/handlerservers.ashx?type=" to blackhole
// legacy queries and routes all other URLs directly through ModDevice.httpsGet.
constexpr const char* kMasterListHttpPrefix  = "http://warbandmain.taleworlds.com/handlerservers.ashx?";
constexpr const char* kMasterListHttpsPrefix = "https://warbandmain.taleworlds.com/handlerservers.ashx?";
constexpr const char* kMasterListHttpsUrl    = "https://warbandmain.taleworlds.com/handlerservers.ashx?gametype=warband&type=list";

// Existing game_variables.txt widget name inside server_filter_panel (y = 0.046)
// and native ui.csv label key.
constexpr const char* kWidgetName  = "is_version_compatible_checkbox";
constexpr const char* kWidgetLabel = "ui_is_version_compatible";

// Hooked function prototypes (ARMv7 base AAPCS; use built-in int/unsigned int
// so modframework/generate-bindings.py CALL_LETTERS resolves every signature).
using ActivateFn       = void (*)(void*);
using DeactivateFn     = void (*)(void*);
using CheckFn          = void (*)(void*, float);
using ApplyFilterRowFn  = int (*)(void*, void*);
using SendHttpRequestFn = void (*)(void*, unsigned int, const void*);

// Resolved game function prototypes
using CreateCheckBoxFn = void* (*)(void*, const void*, const void*, const float*, const float*, const float*);
using RglStringCtorFn  = void (*)(void*, const char*);
using RglStringDtorFn  = void (*)(void*);
using RglStringCharPFn = const char* (*)(const void*);
using CheckBoxSetValFn = void (*)(void*, int);
using WidgetGetValFn   = int (*)(void*);
using WidgetGetEventFn = int (*)(void*, int);
using ApplyFilterAllFn = void (*)(void*);
using TableUpdateFn    = void (*)(void*);

const mf_api_v1* api = nullptr;
const mf_mod_context_v1* context = nullptr;

ActivateFn       previous_activate     = nullptr;
DeactivateFn     previous_deactivate   = nullptr;
CheckFn          previous_check        = nullptr;
ApplyFilterRowFn  previous_apply_filter      = nullptr;
SendHttpRequestFn previous_send_http_request = nullptr;

CreateCheckBoxFn fn_create_checkbox = nullptr;
RglStringCtorFn  fn_rglstr_ctor     = nullptr;
RglStringDtorFn  fn_rglstr_dtor     = nullptr;
RglStringCharPFn fn_rglstr_charp    = nullptr;
CheckBoxSetValFn fn_cb_set_val      = nullptr;
WidgetGetValFn   fn_w_get_val       = nullptr;
WidgetGetEventFn fn_w_get_event     = nullptr;
ApplyFilterAllFn fn_mp_apply_all    = nullptr;
TableUpdateFn    fn_table_update    = nullptr;

const void* selected_module_rglstr    = nullptr;
void*       active_win                = nullptr;
void*       module_checkbox           = nullptr;
bool        filter_enabled            = true;
bool        https_transport_available = false;
bool        logged_first_filter_row   = false;

template<typename T>
const T& field(const void* base, size_t offset) {
    return *reinterpret_cast<const T*>(static_cast<const uint8_t*>(base) + offset);
}

template<typename T>
T& mut_field(void* base, size_t offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}

uint8_t* game_pointer(uint32_t address) {
    return reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(address));
}

int ascii_tolower(int c) {
    return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

int ascii_strcasecmp(const char* a, const char* b) {
    if (!a || !b) return (a == b) ? 0 : (a ? 1 : -1);
    while (*a && *b) {
        const int ca = ascii_tolower(static_cast<unsigned char>(*a++));
        const int cb = ascii_tolower(static_cast<unsigned char>(*b++));
        if (ca != cb) return ca - cb;
    }
    return ascii_tolower(static_cast<unsigned char>(*a)) -
           ascii_tolower(static_cast<unsigned char>(*b));
}

bool ascii_starts_with_ci(const char* str, const char* prefix) {
    if (!str || !prefix) return false;
    while (*prefix) {
        if (!*str) return false;
        if (ascii_tolower(static_cast<unsigned char>(*str++)) !=
            ascii_tolower(static_cast<unsigned char>(*prefix++))) {
            return false;
        }
    }
    return true;
}

bool ascii_contains_ci(const char* haystack, const char* needle) {
    if (!haystack || !needle || !*needle) return false;
    for (const char* p = haystack; *p; ++p) {
        if (ascii_starts_with_ci(p, needle)) return true;
    }
    return false;
}

bool should_rewrite_master_list_url(const char* url) {
    if (!url || !*url) return false;
    const char* query = nullptr;
    if (ascii_starts_with_ci(url, kMasterListHttpPrefix)) {
        query = url + strlen(kMasterListHttpPrefix);
    } else if (ascii_starts_with_ci(url, kMasterListHttpsPrefix)) {
        query = url + strlen(kMasterListHttpsPrefix);
    } else {
        return false; // Leave custom/non-TaleWorlds master servers untouched.
    }
    // Only rewrite server-list fetches; never touch auth/chkserial requests.
    if (!ascii_contains_ci(query, "type=list")) return false;
    if (ascii_contains_ci(query, "chkserial")) return false;
    return ascii_strcasecmp(url, kMasterListHttpsUrl) != 0;
}

int ascii_strcasecmp_len(const char* a, size_t a_len, const char* b, size_t b_len) {
    const size_t n = a_len < b_len ? a_len : b_len;
    for (size_t i = 0; i < n; ++i) {
        const int ca = ascii_tolower(static_cast<unsigned char>(a[i]));
        const int cb = ascii_tolower(static_cast<unsigned char>(b[i]));
        if (ca != cb) return ca - cb;
    }
    if (a_len == b_len) return 0;
    return a_len < b_len ? -1 : 1;
}

bool module_names_equal(const char* row_mod, const char* cur_mod) {
    if (!row_mod || !cur_mod) return false;
    // Strip optional trailing slashes and leading path components from cur_mod
    // (e.g. "Modules/Native/" -> "Native").
    size_t cur_len = strlen(cur_mod);
    while (cur_len > 0 && (cur_mod[cur_len - 1] == '/' || cur_mod[cur_len - 1] == '\\')) {
        --cur_len;
    }
    const char* cur_base = cur_mod;
    for (size_t i = 0; i < cur_len; ++i) {
        if (cur_mod[i] == '/' || cur_mod[i] == '\\') {
            cur_base = cur_mod + i + 1;
        }
    }
    const size_t base_len = static_cast<size_t>((cur_mod + cur_len) - cur_base);

    size_t row_len = strlen(row_mod);
    while (row_len > 0 && (row_mod[row_len - 1] == '/' || row_mod[row_len - 1] == '\\')) {
        --row_len;
    }
    if (base_len == 0 || row_len == 0) return false;
    return ascii_strcasecmp_len(row_mod, row_len, cur_base, base_len) == 0;
}

const char* current_module_name() {
    if (selected_module_rglstr && fn_rglstr_charp) {
        const char* name = fn_rglstr_charp(selected_module_rglstr);
        if (name && name[0] != '\0') return name;
    }
    if (api && MF_API_HAS(api, get_launcher_config) && api->get_launcher_config) {
        const char* name = api->get_launcher_config(context, MF_LAUNCHER_CONFIG_GAME_MODULE_DIRECTORY);
        if (name && name[0] != '\0') return name;
    }
    return nullptr;
}

char* guest_strdup(const char* src) {
    const size_t len = strlen(src);
    char* copy = static_cast<char*>(malloc(len + 1));
    if (copy) memcpy(copy, src, len + 1);
    return copy;
}

void create_module_filter_checkbox(void* win) {
    if (!win || !fn_create_checkbox || !fn_rglstr_ctor || !fn_rglstr_dtor) return;
    uint8_t* panel = game_pointer(field<uint32_t>(win, kMpWinFilterPanel));
    if (!panel) return;

    // Heap allocations live inside game-addressable memory on both ARMv7 and ARM64.
    void*  name_str = malloc(kRglStringSize);
    void*  text_str = malloc(kRglStringSize);
    char*  name_c   = guest_strdup(kWidgetName);
    char*  text_c   = guest_strdup(kWidgetLabel);
    float* coords   = static_cast<float*>(malloc(sizeof(float) * 3));

    if (name_str && text_str && name_c && text_c && coords) {
        coords[0] = -1.0f; // X: read server_filter_panel_is_version_compatible_checkbox_position_x
        coords[1] = -1.0f; // Y: read server_filter_panel_is_version_compatible_checkbox_position_y
        coords[2] =  0.0f;

        fn_rglstr_ctor(name_str, name_c);
        fn_rglstr_ctor(text_str, text_c);

        void* cb = fn_create_checkbox(panel, name_str, text_str, &coords[0], &coords[1], &coords[2]);

        fn_rglstr_dtor(text_str);
        fn_rglstr_dtor(name_str);

        if (cb) {
            active_win = win;
            module_checkbox = cb;
            if (fn_cb_set_val) fn_cb_set_val(cb, filter_enabled ? 1 : 0);
        }
    }

    if (coords)   free(coords);
    if (text_c)   free(text_c);
    if (name_c)   free(name_c);
    if (text_str) free(text_str);
    if (name_str) free(name_str);
}

void replacement_activate(void* win) {
    active_win = nullptr;
    module_checkbox = nullptr;
    previous_activate(win);
    create_module_filter_checkbox(win);
}

void replacement_deactivate(void* win) {
    if (win == active_win) {
        active_win = nullptr;
        module_checkbox = nullptr;
    }
    previous_deactivate(win);
}

void replacement_check(void* win, float dt) {
    previous_check(win, dt);
    if (!win || win != active_win || !module_checkbox) return;

    bool toggled = fn_w_get_event && fn_w_get_event(module_checkbox, kWidgetEventChanged) != 0;
    const bool current = fn_w_get_val ? (fn_w_get_val(module_checkbox) != 0) : filter_enabled;
    if (current != filter_enabled) {
        filter_enabled = current;
        toggled = true;
    }

    if (toggled) {
        if (fn_mp_apply_all) fn_mp_apply_all(win);
        uint8_t* table = game_pointer(field<uint32_t>(win, kMpWinServersList));
        if (table && fn_table_update) fn_table_update(table);
    }
}

// Return convention (verified in libMBExpMobile.so @ 0x13ea98):
//   1 = HIDE (row is filtered out)
//   0 = SHOW (row passes filter)
int replacement_apply_filter(void* win, void* row) {
    int filtered_out = previous_apply_filter(win, row);

    // Workaround for M&BTool 4.3 startup load-order bug (CustomServer starts before
    // ResourceOverrides -> boots in version 1157) and non-Native modules (e.g. NW):
    // stock apply_filter rejects row mb_version_no != 1157 at 0x6cc730 before any
    // checkbox check. If rejected and row is a PC 1.170-1.174 server, retry with
    // cells[2]+0x2cc temporarily normalized to 1157 and restore immediately after.
    if (filtered_out != 0 && win && row) {
        uint8_t* cells = game_pointer(field<uint32_t>(row, kTableRowCellsData));
        if (cells) {
            uint8_t* ver_cell = game_pointer(field<uint32_t>(cells, kTableRowVersionIndex * 4));
            if (ver_cell) {
                int32_t& row_ver = mut_field<int32_t>(ver_cell, kTableCellPayload);
                const int32_t orig_ver = row_ver;
                if (orig_ver >= kVersion1170Min && orig_ver <= kVersion1170Max) {
                    row_ver = kVersion1157;
                    filtered_out = previous_apply_filter(win, row);
                    if (filtered_out != 0 && orig_ver != kVersion1170Max) {
                        row_ver = kVersion1170Max;
                        filtered_out = previous_apply_filter(win, row);
                    }
                    row_ver = orig_ver;
                }
            }
        }
    }
    if (filtered_out != 0 || !win || !row) return filtered_out;

    // Respect the master "Server filter" toggle (this + 0x338).
    uint8_t* master_cb = game_pointer(field<uint32_t>(win, kMpWinFilterCheckbox));
    if (master_cb && fn_w_get_val && fn_w_get_val(master_cb) == 0) return 0;

    bool enabled = filter_enabled;
    if (win == active_win && module_checkbox && fn_w_get_val) {
        enabled = fn_w_get_val(module_checkbox) != 0;
        filter_enabled = enabled;
    }
    if (!enabled) return 0;

    const char* cur_mod = current_module_name();
    if (!cur_mod || !fn_rglstr_charp) return 0;

    const uint8_t* cells = game_pointer(field<uint32_t>(row, kTableRowCellsData));
    if (!cells) return 0;

    const uint8_t* mod_cell = game_pointer(field<uint32_t>(cells, kTableRowModuleIndex * 4));
    if (!mod_cell) return 0;

    const char* row_mod = fn_rglstr_charp(mod_cell + kTableCellPayload);
    if (!row_mod || row_mod[0] == '\0') return 1; // hide rows with missing module name

    const int hide = module_names_equal(row_mod, cur_mod) ? 0 : 1;
    if (!logged_first_filter_row && api && api->log) {
        logged_first_filter_row = true;
        api->log(context, MF_LOG_INFO,
                 hide == 0 ? "apply-filter: first PC server row matched current module (shown)"
                           : "apply-filter: first PC server row differed from current module (filtered)");
    }
    return hide;
}

void replacement_send_http_request(void* out_response, unsigned int net_mgr, const void* url) {
    if (!https_transport_available || !url || !fn_rglstr_charp || !fn_rglstr_ctor || !fn_rglstr_dtor) {
        previous_send_http_request(out_response, net_mgr, url);
        return;
    }

    const char* raw_url = fn_rglstr_charp(url);
    if (!should_rewrite_master_list_url(raw_url)) {
        previous_send_http_request(out_response, net_mgr, url);
        return;
    }

    // Allocate the rewritten rglString and C-string in game-addressable heap memory.
    void* rewritten_url = malloc(kRglStringSize);
    char* rewritten_c   = guest_strdup(kMasterListHttpsUrl);
    if (!rewritten_url || !rewritten_c) {
        if (rewritten_c) free(rewritten_c);
        if (rewritten_url) free(rewritten_url);
        previous_send_http_request(out_response, net_mgr, url);
        return;
    }

    fn_rglstr_ctor(rewritten_url, rewritten_c);
    previous_send_http_request(out_response, net_mgr, rewritten_url);
    if (api && api->log) {
        const char* body = out_response ? fn_rglstr_charp(out_response) : nullptr;
        const bool ok = body && body[0] != '\0' && ascii_strcasecmp(body, "ERROR") != 0;
        api->log(context, MF_LOG_INFO,
                 ok ? "master-list: fetched HTTPS server list (?gametype=warband&type=list)"
                    : "master-list: HTTPS server list request returned empty or ERROR");
    }
    fn_rglstr_dtor(rewritten_url);
    free(rewritten_c);
    free(rewritten_url);
}
} // namespace

MF_MOD_START(mp_module_filter)(const mf_api_v1* mod_api, const mf_mod_context_v1* mod_context) {
    if (!mod_api || !mod_context || mod_api->abi_version != MF_NATIVE_ABI_VERSION ||
        !MF_API_HAS(mod_api, resolve_game_address) ||
        !mod_api->resolve_game_symbol || !mod_api->resolve_game_address) {
        return MF_ERROR_INVALID_ARGUMENT;
    }
    api = mod_api;
    context = mod_context;

    if (MF_API_HAS(api, get_setting_bool) && api->get_setting_bool) {
        filter_enabled = api->get_setting_bool(context, "default_enabled", 1) != 0;
    }

    const bool has_master_fix =
        api->is_mod_active && api->is_mod_active(context, "builtin.multiplayer-master-server-fix") != 0;
    const bool has_custom_server =
        api->is_mod_active && api->is_mod_active(context, "builtin.custom-server") != 0;
    https_transport_available = has_master_fix || has_custom_server;
    if (!has_master_fix) {
        api->log(context, MF_LOG_WARNING,
                 "builtin.multiplayer-master-server-fix (LAN Multiplayer Fix) is inactive; "
                 "multiplayer profile screen will bounce without it");
    }
    if (!https_transport_available) {
        api->log(context, MF_LOG_WARNING,
                 "no built-in HTTPS bridge active; skipping master-server HTTPS rewrite");
    }

    // Optional: falls back to get_launcher_config("game.module.directory") if unavailable.
    selected_module_rglstr = api->resolve_game_address(
        context, kGame, kSelectedModuleName, kRglStringSize, MF_MEMORY_READ);

    fn_create_checkbox = reinterpret_cast<CreateCheckBoxFn>(api->resolve_game_symbol(context, kGame, kSymCreateCheckBox));
    fn_rglstr_ctor     = reinterpret_cast<RglStringCtorFn>(api->resolve_game_symbol(context, kGame, kSymRglStringCtor));
    fn_rglstr_dtor     = reinterpret_cast<RglStringDtorFn>(api->resolve_game_symbol(context, kGame, kSymRglStringDtor));
    fn_rglstr_charp    = reinterpret_cast<RglStringCharPFn>(api->resolve_game_symbol(context, kGame, kSymRglStringCharP));
    fn_cb_set_val      = reinterpret_cast<CheckBoxSetValFn>(api->resolve_game_symbol(context, kGame, kSymCheckBoxSetVal));
    fn_w_get_val       = reinterpret_cast<WidgetGetValFn>(api->resolve_game_symbol(context, kGame, kSymWidgetGetVal));
    fn_w_get_event     = reinterpret_cast<WidgetGetEventFn>(api->resolve_game_symbol(context, kGame, kSymWidgetGetEvent));
    fn_mp_apply_all    = reinterpret_cast<ApplyFilterAllFn>(api->resolve_game_symbol(context, kGame, kSymApplyFilterAll));
    fn_table_update    = reinterpret_cast<TableUpdateFn>(api->resolve_game_symbol(context, kGame, kSymTableUpdate));

    if (!fn_create_checkbox || !fn_rglstr_ctor || !fn_rglstr_dtor || !fn_rglstr_charp ||
        !fn_cb_set_val || !fn_w_get_val || !fn_w_get_event || !fn_mp_apply_all || !fn_table_update) {
        api->log(context, MF_LOG_ERROR, "inactive: failed to resolve required Warband UI symbols");
        return MF_ERROR_NOT_FOUND;
    }

    const mf_function_hook_v1 activate{
        sizeof(activate), MF_HOOK_CHAIN, "mp-win-activate", kGame, nullptr, kMpWinActivate,
        reinterpret_cast<void*>(&replacement_activate), reinterpret_cast<void**>(&previous_activate)};
    mf_result_v1 result = api->hook_function(context, &activate);
    if (result != MF_OK) {
        api->log(context, MF_LOG_ERROR, "inactive: hook mp-win-activate failed");
        return result;
    }

    const mf_function_hook_v1 deactivate{
        sizeof(deactivate), MF_HOOK_CHAIN, "mp-win-deactivate", kGame, nullptr, kMpWinDeactivate,
        reinterpret_cast<void*>(&replacement_deactivate), reinterpret_cast<void**>(&previous_deactivate)};
    result = api->hook_function(context, &deactivate);
    if (result != MF_OK) {
        api->log(context, MF_LOG_ERROR, "inactive: hook mp-win-deactivate failed");
        return result;
    }

    const mf_function_hook_v1 check{
        sizeof(check), MF_HOOK_CHAIN, "mp-win-check", kGame, nullptr, kMpWinCheck,
        reinterpret_cast<void*>(&replacement_check), reinterpret_cast<void**>(&previous_check)};
    result = api->hook_function(context, &check);
    if (result != MF_OK) {
        api->log(context, MF_LOG_ERROR, "inactive: hook mp-win-check failed");
        return result;
    }

    const mf_function_hook_v1 apply_filter{
        sizeof(apply_filter), MF_HOOK_CHAIN, "mp-win-apply-filter", kGame, nullptr, kMpWinApplyFilterRow,
        reinterpret_cast<void*>(&replacement_apply_filter), reinterpret_cast<void**>(&previous_apply_filter)};
    result = api->hook_function(context, &apply_filter);
    if (result != MF_OK) {
        api->log(context, MF_LOG_ERROR, "inactive: hook mp-win-apply-filter failed");
        return result;
    }

    const mf_function_hook_v1 send_http{
        sizeof(send_http), MF_HOOK_CHAIN, "mp-master-list-https", kGame, nullptr, kSendHttpRequest,
        reinterpret_cast<void*>(&replacement_send_http_request), reinterpret_cast<void**>(&previous_send_http_request)};
    result = api->hook_function(context, &send_http);
    if (result != MF_OK) {
        api->log(context, MF_LOG_ERROR, "inactive: hook mp-master-list-https failed");
        return result;
    }

    api->log(context, MF_LOG_INFO, "active: TaleWorlds HTTPS/1.174 server list fix and Filter by Module checkbox enabled");
    return MF_OK;
}
