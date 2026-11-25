/*
 * M&BTool Native Mod (ET_REL mod.arm64.o) — Multiplayer "Filter by Module" Checkbox
 *
 * Restores the missing "Compatible with module" checkbox in Mount & Blade:
 * Warband's multiplayer server browser (Multiplayer_client_window), placing it
 * in the pre-existing game_variables.txt slot (y = 0.046) between "Has players"
 * (y = 0.071) and "No password" (y = 0.021).
 *
 * Strictly adheres to libmodlauncher.so's ET_REL symbol whitelist:
 *   External imports allowed: mf_game_pointer_v1, malloc, free, memcpy, strlen.
 *   No TLS, no COMMON symbols, no SafeStack, no C++ runtime, no libc extras.
 *
 * Build (Android NDK clang):
 *   clang --target=aarch64-linux-android24 -O2 -Wall -Wextra -Werror \
 *         -fno-PIC -fno-common -fno-sanitize=safe-stack \
 *         -fno-stack-protector -fno-asynchronous-unwind-tables \
 *         -c mod_etrel.c -o mod.arm64.o
 */

#include "mf_abi.h"

#include <stddef.h>
#include <stdint.h>

/* Permitted external symbols resolved by libmodlauncher.so ET_REL loader */
extern void  *mf_game_pointer_v1(uint32_t guest_va);
extern void  *malloc(size_t size);
extern void   free(void *ptr);
extern void  *memcpy(void *dst, const void *src, size_t n);
extern size_t strlen(const char *s);

/* =========================================================================
 * 1. Declared Guest Functions (marshaled by libmodlauncher.so)
 * ========================================================================= */

const mf_game_function_v1 mf_game_functions_v1[] = {
    { WB_LIB_NAME, WB_SYM_WIDGET_CREATE_CHECK_BOX, 0, "ppppppp" },
    { WB_LIB_NAME, WB_SYM_RGL_STRING_CTOR,         0, "vpp"     },
    { WB_LIB_NAME, WB_SYM_RGL_STRING_DTOR,         0, "vp"      },
    { WB_LIB_NAME, WB_SYM_RGL_STRING_CHAR_P,       0, "pp"      },
    { WB_LIB_NAME, WB_SYM_CHECKBOX_SET_VALUE_I,    0, "vpi"     },
    { WB_LIB_NAME, WB_SYM_WIDGET_GET_VALUE_I,      0, "ip"      },
    { WB_LIB_NAME, WB_SYM_WIDGET_GET_EVENT,        0, "ipi"     },
    { WB_LIB_NAME, WB_SYM_MP_WIN_APPLY_FILTER_ALL, 0, "vp"      },
    { WB_LIB_NAME, WB_SYM_TABLE_UPDATE_TABLE,      0, "vp"      },
    { NULL, NULL, 0, NULL }
};

typedef void       *(*fn_create_checkbox_t)(void *parent, const void *name,
                                            const void *text, const float *x,
                                            const float *y, const float *z);
typedef void        (*fn_rglstr_ctor_t)(void *str_obj, const char *cstr);
typedef void        (*fn_rglstr_dtor_t)(void *str_obj);
typedef const char *(*fn_rglstr_charp_t)(const void *str_obj);
typedef void        (*fn_cb_set_val_t)(void *cb, int32_t val);
typedef int32_t     (*fn_w_get_val_t)(void *w);
typedef int32_t     (*fn_w_get_event_t)(void *w, int32_t mask);
typedef void        (*fn_mp_apply_all_t)(void *win);
typedef void        (*fn_table_update_t)(void *table);

/* =========================================================================
 * 2. Mod Runtime State
 * ========================================================================= */

static const mf_api_v1           *g_api = NULL;
static const mf_mod_context_v1   *g_ctx = NULL;
static const mf_compat_memory_v1 *g_mem = NULL;

static fn_create_checkbox_t g_fn_create_checkbox = NULL;
static fn_rglstr_ctor_t     g_fn_rglstr_ctor     = NULL;
static fn_rglstr_dtor_t     g_fn_rglstr_dtor     = NULL;
static fn_rglstr_charp_t    g_fn_rglstr_charp    = NULL;
static fn_cb_set_val_t      g_fn_cb_set_val      = NULL;
static fn_w_get_val_t       g_fn_w_get_val       = NULL;
static fn_w_get_event_t     g_fn_w_get_event     = NULL;
static fn_mp_apply_all_t    g_fn_mp_apply_all    = NULL;
static fn_table_update_t    g_fn_table_update    = NULL;

static const void *g_selected_module_rglstr = NULL;

/* Active Multiplayer_client_window & our injected checkbox (host pointers) */
static void    *g_active_win      = NULL;
static void    *g_module_checkbox = NULL;
static int32_t  g_filter_enabled  = 1;

/* Raw previous-hook slots filled by libmodlauncher.so */
static void *g_prev_activate     = NULL;
static void *g_prev_deactivate   = NULL;
static void *g_prev_check        = NULL;
static void *g_prev_apply_filter = NULL;

/* =========================================================================
 * 3. Helpers (whitelist-safe, guest-memory-safe)
 * ========================================================================= */

static inline void *guest_u32_to_host(uint32_t guest_va) {
    if (guest_va == 0u) {
        return NULL;
    }
    if (g_mem && g_mem->guest_base != 0u) {
        return (void *)(uintptr_t)(g_mem->guest_base + (uint64_t)guest_va);
    }
    return mf_game_pointer_v1(guest_va);
}

static inline uint32_t host_to_guest_u32(const void *host_ptr) {
    if (!host_ptr) {
        return 0u;
    }
    if (g_mem && g_mem->guest_base != 0u) {
        uintptr_t addr = (uintptr_t)host_ptr;
        if (addr >= (uintptr_t)g_mem->guest_base) {
            uint64_t diff = (uint64_t)(addr - (uintptr_t)g_mem->guest_base);
            if ((diff >> 32) == 0u) {
                return (uint32_t)diff;
            }
        }
    }
    if (g_mem && g_mem->encode_ptr) {
        uint32_t out = 0u;
        if (g_mem->encode_ptr(g_ctx, host_ptr, &out) == 0) {
            return out;
        }
    }
    return 0u;
}

static inline uint32_t read_u32(const void *base, size_t off) {
    uint32_t v = 0u;
    memcpy(&v, (const uint8_t *)base + off, sizeof(v));
    return v;
}

static int ascii_tolower(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

static int ascii_strcasecmp(const char *a, const char *b) {
    if (!a || !b) {
        return (a == b) ? 0 : (a ? 1 : -1);
    }
    while (*a && *b) {
        int ca = ascii_tolower((unsigned char)*a++);
        int cb = ascii_tolower((unsigned char)*b++);
        if (ca != cb) {
            return ca - cb;
        }
    }
    return ascii_tolower((unsigned char)*a) - ascii_tolower((unsigned char)*b);
}

static const char *get_current_module_name(void) {
    if (g_selected_module_rglstr && g_fn_rglstr_charp) {
        const char *s = g_fn_rglstr_charp(g_selected_module_rglstr);
        if (s && s[0] != '\0') {
            return s;
        }
    }
    if (g_api && g_api->get_string) {
        const char *s = g_api->get_string(g_ctx, "game.module.directory", "");
        if (s && s[0] != '\0') {
            return s;
        }
    }
    return NULL;
}

/*
 * Allocates a NUL-terminated copy of src in the guest heap (ET_REL malloc
 * allocates inside the 4 GiB guest window so guest functions can read it).
 */
static char *guest_strdup(const char *src) {
    size_t len = strlen(src);
    char *buf = (char *)malloc(len + 1u);
    if (!buf) {
        return NULL;
    }
    memcpy(buf, src, len + 1u);
    return buf;
}

static void create_module_filter_checkbox(void *win) {
    if (!win || !g_fn_create_checkbox || !g_fn_rglstr_ctor || !g_fn_rglstr_dtor) {
        return;
    }

    uint32_t panel_va = read_u32(win, WB_MP_WIN_FILTER_PANEL_OFF);
    void *panel = guest_u32_to_host(panel_va);
    if (!panel) {
        return;
    }

    /* All temporaries passed by pointer to guest code must live in guest RAM. */
    void  *name_str = malloc(WB_RGL_STRING_SIZE);
    void  *text_str = malloc(WB_RGL_STRING_SIZE);
    char  *name_c   = guest_strdup(WB_FILTER_CB_WIDGET_NAME);
    char  *text_c   = guest_strdup(WB_FILTER_CB_DEFAULT_LABEL);
    float *coords   = (float *)malloc(sizeof(float) * 3u);

    if (name_str && text_str && name_c && text_c && coords) {
        coords[0] = -1.0f; /* X: read server_filter_panel_is_version_compatible_checkbox_position_x */
        coords[1] = -1.0f; /* Y: read server_filter_panel_is_version_compatible_checkbox_position_y */
        coords[2] =  0.0f;

        g_fn_rglstr_ctor(name_str, name_c);
        g_fn_rglstr_ctor(text_str, text_c);

        void *cb = g_fn_create_checkbox(panel, name_str, text_str,
                                        &coords[0], &coords[1], &coords[2]);

        g_fn_rglstr_dtor(text_str);
        g_fn_rglstr_dtor(name_str);

        if (cb) {
            g_active_win      = win;
            g_module_checkbox = cb;
            if (g_fn_cb_set_val) {
                g_fn_cb_set_val(cb, g_filter_enabled ? 1 : 0);
            }
        }
    }

    if (coords)   free(coords);
    if (text_c)   free(text_c);
    if (name_c)   free(name_c);
    if (text_str) free(text_str);
    if (name_str) free(name_str);
}

/* =========================================================================
 * 4. Hook Implementations & Compat Bridges
 * ========================================================================= */

/* ---- Hook 1: Multiplayer_client_window::activate() (0x0012f06c) ---- */

static void prev_activate_thunk(void *win) {
    mf_compat_entry_fn fn = (mf_compat_entry_fn)g_prev_activate;
    if (fn) {
        (void)fn(host_to_guest_u32(win), 0u, 0u, 0u);
    }
}

static void hook_activate(void *win) {
    g_active_win      = NULL;
    g_module_checkbox = NULL;
    prev_activate_thunk(win);
    create_module_filter_checkbox(win);
}

typedef struct { uint32_t r0, r1; uint64_t ret; } bridge_args_t;

static void run_activate(void *p) {
    bridge_args_t *a = (bridge_args_t *)p;
    hook_activate(guest_u32_to_host(a->r0));
}

static uint64_t entry_activate(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3) {
    (void)r1; (void)r2; (void)r3;
    if (g_mem && g_mem->direct_call_ok) {
        hook_activate(guest_u32_to_host(r0));
    } else if (g_api && g_api->run_on_compat_stack) {
        bridge_args_t a = { r0, 0u, 0u };
        g_api->run_on_compat_stack(g_ctx, run_activate, &a);
    }
    return 0u;
}

static const mf_hook_adapter_v1 k_adapter_activate = {
    MF_HOOK_ADAPTER_V1_SIZE, 1u,
    (void *)hook_activate, entry_activate,
    &g_prev_activate, (void *)prev_activate_thunk
};

/* ---- Hook 2: Multiplayer_client_window::deactivate() (0x0012f040) ---- */

static void prev_deactivate_thunk(void *win) {
    mf_compat_entry_fn fn = (mf_compat_entry_fn)g_prev_deactivate;
    if (fn) {
        (void)fn(host_to_guest_u32(win), 0u, 0u, 0u);
    }
}

static void hook_deactivate(void *win) {
    if (win == g_active_win) {
        g_active_win      = NULL;
        g_module_checkbox = NULL;
    }
    prev_deactivate_thunk(win);
}

static void run_deactivate(void *p) {
    bridge_args_t *a = (bridge_args_t *)p;
    hook_deactivate(guest_u32_to_host(a->r0));
}

static uint64_t entry_deactivate(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3) {
    (void)r1; (void)r2; (void)r3;
    if (g_mem && g_mem->direct_call_ok) {
        hook_deactivate(guest_u32_to_host(r0));
    } else if (g_api && g_api->run_on_compat_stack) {
        bridge_args_t a = { r0, 0u, 0u };
        g_api->run_on_compat_stack(g_ctx, run_deactivate, &a);
    }
    return 0u;
}

static const mf_hook_adapter_v1 k_adapter_deactivate = {
    MF_HOOK_ADAPTER_V1_SIZE, 1u,
    (void *)hook_deactivate, entry_deactivate,
    &g_prev_deactivate, (void *)prev_deactivate_thunk
};

/* ---- Hook 3: Multiplayer_client_window::check(float) (0x00122c38) ---- */

static void prev_check_thunk(void *win, uint32_t dt_bits) {
    mf_compat_entry_fn fn = (mf_compat_entry_fn)g_prev_check;
    if (fn) {
        (void)fn(host_to_guest_u32(win), dt_bits, 0u, 0u);
    }
}

static void hook_check(void *win, uint32_t dt_bits) {
    prev_check_thunk(win, dt_bits);

    if (!win || win != g_active_win || !g_module_checkbox) {
        return;
    }

    int32_t toggled = 0;
    if (g_fn_w_get_event &&
        g_fn_w_get_event(g_module_checkbox, WB_WIDGET_EVENT_CHANGED) != 0) {
        toggled = 1;
    }

    int32_t cur_val = g_fn_w_get_val ? (g_fn_w_get_val(g_module_checkbox) != 0 ? 1 : 0)
                                     : g_filter_enabled;
    if (cur_val != g_filter_enabled) {
        g_filter_enabled = cur_val;
        toggled = 1;
    }

    if (toggled) {
        if (g_fn_mp_apply_all) {
            g_fn_mp_apply_all(win);
        }
        uint32_t table_va = read_u32(win, WB_MP_WIN_SERVERS_LIST_OFF);
        void *table = guest_u32_to_host(table_va);
        if (table && g_fn_table_update) {
            g_fn_table_update(table);
        }
    }
}

static void run_check(void *p) {
    bridge_args_t *a = (bridge_args_t *)p;
    hook_check(guest_u32_to_host(a->r0), a->r1);
}

static uint64_t entry_check(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3) {
    (void)r2; (void)r3;
    if (g_mem && g_mem->direct_call_ok) {
        hook_check(guest_u32_to_host(r0), r1);
    } else if (g_api && g_api->run_on_compat_stack) {
        bridge_args_t a = { r0, r1, 0u };
        g_api->run_on_compat_stack(g_ctx, run_check, &a);
    }
    return 0u;
}

static const mf_hook_adapter_v1 k_adapter_check = {
    MF_HOOK_ADAPTER_V1_SIZE, 1u,
    (void *)hook_check, entry_check,
    &g_prev_check, (void *)prev_check_thunk
};

/* ---- Hook 4: Multiplayer_client_window::apply_filter(rglTable_row*) (0x0013ea98) ----
 * Return convention (verified in sr_f_1013ea98):
 *   1 = HIDE (row is filtered out)
 *   0 = SHOW (row passes filter)
 */

static uint32_t prev_apply_filter_thunk(void *win, void *row) {
    mf_compat_entry_fn fn = (mf_compat_entry_fn)g_prev_apply_filter;
    if (!fn) {
        return 0u;
    }
    return (uint32_t)fn(host_to_guest_u32(win), host_to_guest_u32(row), 0u, 0u);
}

static uint32_t hook_apply_filter(void *win, void *row) {
    /* Delegate first so built-in custom_server and Warband's stock filters run. */
    uint32_t filtered_out = prev_apply_filter_thunk(win, row);
    if (filtered_out != 0u || !win || !row) {
        return filtered_out;
    }

    /* Respect master "Server filter" toggle (this + 0x338), matching sr_f_1013ea98. */
    uint32_t master_cb_va = read_u32(win, WB_MP_WIN_FILTER_CHECKBOX_OFF);
    void *master_cb = guest_u32_to_host(master_cb_va);
    if (master_cb && g_fn_w_get_val && g_fn_w_get_val(master_cb) == 0) {
        return 0u;
    }

    int32_t enabled = g_filter_enabled;
    if (win == g_active_win && g_module_checkbox && g_fn_w_get_val) {
        enabled = (g_fn_w_get_val(g_module_checkbox) != 0) ? 1 : 0;
        g_filter_enabled = enabled;
    }
    if (!enabled) {
        return 0u;
    }

    const char *cur_mod = get_current_module_name();
    if (!cur_mod || !g_fn_rglstr_charp) {
        return 0u;
    }

    uint32_t cells_va = read_u32(row, WB_TABLE_ROW_CELLS_DATA_OFF);
    void *cells = guest_u32_to_host(cells_va);
    if (!cells) {
        return 0u;
    }

    uint32_t mod_cell_va = read_u32(cells, WB_TABLE_ROW_MODULE_CELL_INDEX * 4u);
    void *mod_cell = guest_u32_to_host(mod_cell_va);
    if (!mod_cell) {
        return 0u;
    }

    const void *row_mod_rglstr = (const uint8_t *)mod_cell + WB_TABLE_CELL_STRING_OFF;
    const char *row_mod = g_fn_rglstr_charp(row_mod_rglstr);
    if (!row_mod || row_mod[0] == '\0') {
        return 1u; /* hide row with missing module name */
    }

    return (ascii_strcasecmp(row_mod, cur_mod) == 0) ? 0u : 1u;
}

static void run_apply_filter(void *p) {
    bridge_args_t *a = (bridge_args_t *)p;
    a->ret = (uint64_t)hook_apply_filter(guest_u32_to_host(a->r0),
                                         guest_u32_to_host(a->r1));
}

static uint64_t entry_apply_filter(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3) {
    (void)r2; (void)r3;
    if (g_mem && g_mem->direct_call_ok) {
        return (uint64_t)hook_apply_filter(guest_u32_to_host(r0),
                                           guest_u32_to_host(r1));
    }
    if (g_api && g_api->run_on_compat_stack) {
        bridge_args_t a = { r0, r1, 0u };
        g_api->run_on_compat_stack(g_ctx, run_apply_filter, &a);
        return a.ret;
    }
    return 0u;
}

static const mf_hook_adapter_v1 k_adapter_apply_filter = {
    MF_HOOK_ADAPTER_V1_SIZE, 1u,
    (void *)hook_apply_filter, entry_apply_filter,
    &g_prev_apply_filter, (void *)prev_apply_filter_thunk
};

/* =========================================================================
 * 5. Mod Entry Points
 * ========================================================================= */

static int install_one_hook(const char *id, uint64_t offset,
                            void *replacement, void **orig_out,
                            const mf_hook_adapter_v1 *adapter) {
    mf_function_hook_v1 desc;
    desc.size         = MF_FUNCTION_HOOK_V1_SIZE;
    desc.kind         = MF_HOOK_KIND_OFFSET;
    desc.name         = id;
    desc.library      = WB_LIB_NAME;
    desc.symbol       = NULL;
    desc.offset       = offset;
    desc.replacement  = replacement;
    desc.original_out = orig_out;
    return g_api->install_hook(g_ctx, &desc, adapter);
}

int mod_start(const mf_api_v1 *api, const mf_mod_context_v1 *ctx) {
    if (!api || !ctx) {
        return -1;
    }
    if (api->version != MF_API_VERSION || api->size < 0xe0u) {
        return -1;
    }
    if (!api->install_hook || !api->get_compat_memory || !api->resolve_function) {
        return -2;
    }

    g_api = api;
    g_ctx = ctx;

    const mf_compat_memory_v1 *mem = api->get_compat_memory(ctx);
    if (!mem || mem->size < MF_COMPAT_MEMORY_V1_SIZE || mem->version != 1u) {
        if (api->log) {
            api->log(ctx, MF_LOG_ERROR, "inactive: compat memory services unavailable");
        }
        return -2;
    }
    g_mem = mem;

    if (api->get_bool) {
        g_filter_enabled = api->get_bool(ctx, "default_enabled", 1) ? 1 : 0;
    }

    /* Resolve guest data global: selected_module_name */
    if (api->resolve_data) {
        g_selected_module_rglstr = api->resolve_data(ctx, WB_LIB_NAME,
                                                     WB_OFF_SELECTED_MODULE_NAME,
                                                     WB_RGL_STRING_SIZE, 0);
    }

    /* Resolve declared guest functions via mf_game_functions_v1 adapters */
    g_fn_create_checkbox = (fn_create_checkbox_t)api->resolve_function(ctx, WB_LIB_NAME, WB_SYM_WIDGET_CREATE_CHECK_BOX, 0);
    g_fn_rglstr_ctor     = (fn_rglstr_ctor_t)    api->resolve_function(ctx, WB_LIB_NAME, WB_SYM_RGL_STRING_CTOR,         0);
    g_fn_rglstr_dtor     = (fn_rglstr_dtor_t)    api->resolve_function(ctx, WB_LIB_NAME, WB_SYM_RGL_STRING_DTOR,         0);
    g_fn_rglstr_charp    = (fn_rglstr_charp_t)   api->resolve_function(ctx, WB_LIB_NAME, WB_SYM_RGL_STRING_CHAR_P,       0);
    g_fn_cb_set_val      = (fn_cb_set_val_t)     api->resolve_function(ctx, WB_LIB_NAME, WB_SYM_CHECKBOX_SET_VALUE_I,    0);
    g_fn_w_get_val       = (fn_w_get_val_t)      api->resolve_function(ctx, WB_LIB_NAME, WB_SYM_WIDGET_GET_VALUE_I,      0);
    g_fn_w_get_event     = (fn_w_get_event_t)    api->resolve_function(ctx, WB_LIB_NAME, WB_SYM_WIDGET_GET_EVENT,        0);
    g_fn_mp_apply_all    = (fn_mp_apply_all_t)   api->resolve_function(ctx, WB_LIB_NAME, WB_SYM_MP_WIN_APPLY_FILTER_ALL, 0);
    g_fn_table_update    = (fn_table_update_t)   api->resolve_function(ctx, WB_LIB_NAME, WB_SYM_TABLE_UPDATE_TABLE,      0);

    if (!g_fn_create_checkbox || !g_fn_rglstr_ctor || !g_fn_rglstr_dtor ||
        !g_fn_rglstr_charp || !g_fn_cb_set_val || !g_fn_w_get_val ||
        !g_fn_w_get_event || !g_fn_mp_apply_all || !g_fn_table_update) {
        if (api->log) {
            api->log(ctx, MF_LOG_ERROR, "inactive: failed to resolve required Warband UI functions");
        }
        return -3;
    }

    /* Install hooks matching manifest.json declarations */
    if (install_one_hook("mp-win-activate", WB_OFF_MP_WIN_ACTIVATE,
                         (void *)hook_activate, &g_prev_activate, &k_adapter_activate) != 0 ||
        install_one_hook("mp-win-deactivate", WB_OFF_MP_WIN_DEACTIVATE,
                         (void *)hook_deactivate, &g_prev_deactivate, &k_adapter_deactivate) != 0 ||
        install_one_hook("mp-win-check", WB_OFF_MP_WIN_CHECK,
                         (void *)hook_check, &g_prev_check, &k_adapter_check) != 0 ||
        install_one_hook("mp-win-apply-filter", WB_OFF_MP_WIN_APPLY_FILTER_ROW,
                         (void *)hook_apply_filter, &g_prev_apply_filter, &k_adapter_apply_filter) != 0) {
        if (api->log) {
            api->log(ctx, MF_LOG_ERROR, "inactive: failed to install multiplayer UI hooks");
        }
        return -4;
    }

    if (api->log) {
        api->log(ctx, MF_LOG_INFO, "active: Filter by Module checkbox enabled");
    }
    return 0;
}

void mod_stop(const mf_api_v1 *api, const mf_mod_context_v1 *ctx) {
    (void)api;
    (void)ctx;
    g_active_win      = NULL;
    g_module_checkbox = NULL;
}
