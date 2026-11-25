/*
 * M&BTool / Mount & Blade: Warband (Android arm64-v8a) Native Mod & Guest ABI
 *
 * All struct layouts, field offsets, guest function addresses, and symbol
 * contracts below were verified by disassembly of libmodlauncher.so (v4.3,
 * SHA-256 0cf1d64b677b712047a424d30b5c0aaa8f21938ae9e6a10cfa21434363428b7e)
 * and libMBExpMobile.so (embedded ARMv7 ELF @ 0x538a0..0x42cfd8).
 */

#ifndef MNB_MF_ABI_H
#define MNB_MF_ABI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * 1. M&BTool NativeMods ET_REL ABI (modAbi == 1)
 * ========================================================================= */

#define MF_API_VERSION               1u
#define MF_API_V1_SIZE               0xf0u
#define MF_FUNCTION_HOOK_V1_SIZE     0x38u
#define MF_HOOK_ADAPTER_V1_SIZE      0x28u
#define MF_COMPAT_MEMORY_V1_SIZE     0x50u

#define MF_LOG_INFO                  4
#define MF_LOG_WARN                  5
#define MF_LOG_ERROR                 6

#define MF_HOOK_KIND_OFFSET          0u
#define MF_HOOK_KIND_SYMBOL          1u

typedef struct mf_mod_context_v1 mf_mod_context_v1;

typedef struct mf_function_hook_v1 {
    uint32_t    size;          /* +0x00: sizeof(mf_function_hook_v1) == 0x38 */
    uint32_t    kind;          /* +0x04: 0 = offset hook, 1 = symbol hook    */
    const char *name;          /* +0x08: matches hooks[].id in manifest.json */
    const char *library;       /* +0x10: e.g. "libMBExpMobile.so"            */
    const char *symbol;        /* +0x18: NULL for offset hooks               */
    uint64_t    offset;        /* +0x20: guest offset (e.g. 0x0012f06c)      */
    void       *replacement;   /* +0x28: C replacement function              */
    void      **original_out;  /* +0x30: receives host-callable orig ptr     */
} mf_function_hook_v1;

typedef uint64_t (*mf_compat_entry_fn)(uint32_t r0, uint32_t r1,
                                       uint32_t r2, uint32_t r3);

typedef struct mf_hook_adapter_v1 {
    uint32_t            size;           /* +0x00: 0x28                       */
    uint32_t            version;        /* +0x04: 1                          */
    void               *replacement;    /* +0x08: C replacement function     */
    mf_compat_entry_fn  entry;          /* +0x10: guest-register entry point */
    void              **previous_slot;  /* +0x18: host stores raw prev ptr   */
    void               *previous_thunk; /* +0x20: C thunk calling *prev_slot */
} mf_hook_adapter_v1;

typedef struct mf_compat_memory_v1 {
    uint32_t size;             /* +0x00: 0x50                                */
    uint32_t version;          /* +0x04: 1                                   */
    int    (*stack_read_u32)(const mf_mod_context_v1 *ctx, uint32_t idx, uint32_t *out); /* +0x08 */
    int    (*stack_write_u32)(const mf_mod_context_v1 *ctx, uint32_t idx, uint32_t val); /* +0x10 */
    void  *(*decode_ptr)(const mf_mod_context_v1 *ctx, uint64_t guest_addr, size_t len, int writable); /* +0x18 */
    int    (*encode_ptr)(const mf_mod_context_v1 *ctx, const void *host_ptr, uint32_t *guest_out);     /* +0x20 */
    void  *call_helper;        /* +0x28: host compat helper (0x1896854)      */
    uint64_t guest_base;       /* +0x30: 4 GiB guest window base (x28)       */
    int    (*stack_read_bulk)(const mf_mod_context_v1 *ctx, uint32_t start, uint32_t *buf, uint32_t n);       /* +0x38 */
    int    (*stack_write_bulk)(const mf_mod_context_v1 *ctx, uint32_t start, const uint32_t *buf, uint32_t n); /* +0x40 */
    uint32_t direct_call_ok;   /* +0x48: 1 if hook runs on native stack      */
    uint32_t _pad;             /* +0x4c                                      */
} mf_compat_memory_v1;

typedef struct mf_api_v1 {
    uint32_t size;             /* +0x00: 0xf0 (240)                          */
    uint32_t version;          /* +0x04: 1                                   */
    void    *_slot08;          /* +0x08                                      */
    void    *_slot10;          /* +0x10                                      */
    void   (*log)(const mf_mod_context_v1 *ctx, int level, const char *msg); /* +0x18 */
    void    *_slot20;          /* +0x20                                      */
    int    (*install_hook_raw)(const mf_mod_context_v1 *ctx,
                               const mf_function_hook_v1 *hook,
                               const mf_hook_adapter_v1 *adapter);           /* +0x28 */
    void    *_slot30;          /* +0x30                                      */
    void  *(*resolve_function)(const mf_mod_context_v1 *ctx,
                               const char *library,
                               const char *symbol,
                               uint64_t offset);                             /* +0x38 */
    void  *(*resolve_data)(const mf_mod_context_v1 *ctx,
                           const char *library,
                           uint64_t offset,
                           size_t size,
                           int writable);                                    /* +0x40 */
    void    *_io_slots[10];    /* +0x48..+0x90                               */
    int    (*get_bool)(const mf_mod_context_v1 *ctx, const char *key, int def_val);        /* +0x98 */
    int    (*get_int)(const mf_mod_context_v1 *ctx, const char *key, int def_val);         /* +0xa0 */
    float  (*get_float)(const mf_mod_context_v1 *ctx, const char *key, float def_val);     /* +0xa8 */
    const char *(*get_string)(const mf_mod_context_v1 *ctx, const char *key, const char *def_val); /* +0xb0 */
    void    *_text_slots[2];   /* +0xb8, +0xc0                               */
    int    (*install_hook)(const mf_mod_context_v1 *ctx,
                           const mf_function_hook_v1 *hook,
                           const mf_hook_adapter_v1 *adapter);               /* +0xc8 */
    int    (*run_on_compat_stack)(const mf_mod_context_v1 *ctx,
                                  void (*fn)(void *), void *arg);            /* +0xd0 */
    const mf_compat_memory_v1 *(*get_compat_memory)(const mf_mod_context_v1 *ctx);         /* +0xd8 */
    int    (*vibrate)(const mf_mod_context_v1 *ctx, int ms, int amplitude);  /* +0xe0 */
    void    *_slotE8;          /* +0xe8                                      */
} mf_api_v1;

/*
 * Exported by ET_REL mods that call arbitrary guest functions via
 * api->resolve_function(). Must be terminated by an all-zero entry.
 *
 * Signature string grammar (verified @ libmodlauncher.so 0x1872a10..0x1872c80):
 *   char[0]  = return type ('v'=void, 'p'=guest<->host ptr, 'i'=int32, 'I'=uint32, 'f'=float, ...)
 *   char[1+] = parameter types in order (up to 4 register words + stack words via sr_rt_call_guest)
 */
typedef struct mf_game_function_v1 {
    const char *library;       /* +0x00: "libMBExpMobile.so"                 */
    const char *symbol;        /* +0x08: mangled ARMv7 guest sym, or NULL    */
    uint64_t    offset;        /* +0x10: guest offset when symbol == NULL    */
    const char *signature;     /* +0x18: e.g. "ppppppp", "vpp", "pp", "ipi"  */
} mf_game_function_v1;

/* =========================================================================
 * 2. libmodlauncher.so Runtime Exports (for standalone NDK .so variant)
 * ========================================================================= */

typedef struct sr_rt_guest_call_t {
    uint32_t        guest_pc;         /* +0x00: 0x10000000 + guest_offset    */
    uint32_t        r[4];             /* +0x04: ARM32 EABI r0..r3            */
    uint32_t        stack_arg_words;  /* +0x14: number of 32-bit stack args  */
    const uint32_t *stack_args;       /* +0x18: pointer to stack arg words   */
    uint64_t        ret;              /* +0x20: ARM32 r0 (low32) / r1 (hi32) */
} sr_rt_guest_call_t;

/* Compile-time ABI layout verification */
_Static_assert(sizeof(mf_api_v1) == MF_API_V1_SIZE, "mf_api_v1 size mismatch");
_Static_assert(offsetof(mf_api_v1, log) == 0x18, "mf_api_v1.log offset");
_Static_assert(offsetof(mf_api_v1, resolve_function) == 0x38, "mf_api_v1.resolve_function offset");
_Static_assert(offsetof(mf_api_v1, resolve_data) == 0x40, "mf_api_v1.resolve_data offset");
_Static_assert(offsetof(mf_api_v1, get_bool) == 0x98, "mf_api_v1.get_bool offset");
_Static_assert(offsetof(mf_api_v1, get_string) == 0xb0, "mf_api_v1.get_string offset");
_Static_assert(offsetof(mf_api_v1, install_hook) == 0xc8, "mf_api_v1.install_hook offset");
_Static_assert(offsetof(mf_api_v1, run_on_compat_stack) == 0xd0, "mf_api_v1.run_on_compat_stack offset");
_Static_assert(offsetof(mf_api_v1, get_compat_memory) == 0xd8, "mf_api_v1.get_compat_memory offset");
_Static_assert(sizeof(mf_function_hook_v1) == MF_FUNCTION_HOOK_V1_SIZE, "mf_function_hook_v1 size");
_Static_assert(sizeof(mf_hook_adapter_v1) == MF_HOOK_ADAPTER_V1_SIZE, "mf_hook_adapter_v1 size");
_Static_assert(sizeof(mf_compat_memory_v1) == MF_COMPAT_MEMORY_V1_SIZE, "mf_compat_memory_v1 size");
_Static_assert(offsetof(mf_compat_memory_v1, guest_base) == 0x30, "mf_compat_memory_v1.guest_base offset");
_Static_assert(offsetof(mf_compat_memory_v1, direct_call_ok) == 0x48, "mf_compat_memory_v1.direct_call_ok offset");
_Static_assert(sizeof(mf_game_function_v1) == 0x20, "mf_game_function_v1 size");
_Static_assert(sizeof(sr_rt_guest_call_t) == 0x28, "sr_rt_guest_call_t size");
_Static_assert(offsetof(sr_rt_guest_call_t, ret) == 0x20, "sr_rt_guest_call_t.ret offset");

/* =========================================================================
 * 3. Mount & Blade: Warband Guest Addresses & Field Offsets
 * ========================================================================= */

#define WB_LIB_NAME                         "libMBExpMobile.so"
#define WB_GUEST_LOAD_BASE                  0x10000000u

/* Hooked Multiplayer_client_window methods (guest offsets) */
#define WB_OFF_MP_WIN_CHECK                 0x00122c38ULL /* Multiplayer_client_window::check(float) */
#define WB_OFF_MP_WIN_DEACTIVATE            0x0012f040ULL /* Multiplayer_client_window::deactivate() */
#define WB_OFF_MP_WIN_ACTIVATE              0x0012f06cULL /* Multiplayer_client_window::activate() */
#define WB_OFF_MP_WIN_APPLY_FILTER_ROW      0x0013ea98ULL /* Multiplayer_client_window::apply_filter(rglTable_row*) */

/* Invoked guest functions (guest offsets & mangled symbols) */
#define WB_OFF_MP_WIN_APPLY_FILTER_ALL      0x0012ac18ULL
#define WB_SYM_MP_WIN_APPLY_FILTER_ALL      "_ZN25Multiplayer_client_window12apply_filterEv"

#define WB_OFF_WIDGET_CREATE_CHECK_BOX      0x001331f8ULL
#define WB_SYM_WIDGET_CREATE_CHECK_BOX      "_Z23widget_create_check_boxP19rglContainer_widgetRK9rglStringS3_RKfS5_S5_"

#define WB_OFF_CHECKBOX_SET_VALUE_I         0x00138370ULL
#define WB_SYM_CHECKBOX_SET_VALUE_I         "_ZN19rglCheck_box_widget11set_value_iEi"

#define WB_OFF_WIDGET_GET_VALUE_I           0x00133db8ULL
#define WB_SYM_WIDGET_GET_VALUE_I           "_ZN14rglWidget_base11get_value_iEv"

#define WB_OFF_WIDGET_GET_EVENT             0x00133e10ULL
#define WB_SYM_WIDGET_GET_EVENT             "_ZN14rglWidget_base9get_eventE15rglWindow_event"

#define WB_OFF_RGL_STRING_CTOR              0x0020ddf0ULL
#define WB_SYM_RGL_STRING_CTOR              "_ZN9rglStringC1EPKc"

#define WB_OFF_RGL_STRING_DTOR              0x0020df1cULL
#define WB_SYM_RGL_STRING_DTOR              "_ZN9rglStringD1Ev"

#define WB_OFF_RGL_STRING_CHAR_P            0x0020dff4ULL
#define WB_SYM_RGL_STRING_CHAR_P            "_ZNK9rglString6char_pEv"

#define WB_OFF_TABLE_UPDATE_TABLE           0x002185dcULL
#define WB_SYM_TABLE_UPDATE_TABLE           "_ZN15rglTable_widget12update_tableEv"

/* Guest data global: rglString selected_module_name (size 0x90) */
#define WB_OFF_SELECTED_MODULE_NAME         0x02785884ULL
#define WB_RGL_STRING_SIZE                  0x90u

/* Multiplayer_client_window member offsets (32-bit guest pointers) */
#define WB_MP_WIN_SERVERS_LIST_OFF          0x1e8u /* rglTable_widget*     */
#define WB_MP_WIN_FILTER_CHECKBOX_OFF       0x338u /* rglCheck_box_widget* */
#define WB_MP_WIN_FILTER_PANEL_OFF          0x33cu /* rglContainer_widget* */
#define WB_MP_WIN_HAS_PLAYERS_CB_OFF        0x340u /* rglCheck_box_widget* */
#define WB_MP_WIN_NO_PASSWORD_CB_OFF        0x344u /* rglCheck_box_widget* */

/* rglTable_row -> cells[8] (Module column) -> rglString text */
#define WB_TABLE_ROW_CELLS_DATA_OFF         0x35cu /* rglTable_cell** (guest u32) */
#define WB_TABLE_ROW_MODULE_CELL_INDEX      8u     /* byte offset +0x20           */
#define WB_TABLE_CELL_STRING_OFF            0x2ccu /* rglString inside cell       */

/* Event mask passed by Multiplayer_client_window::check() to checkbox get_event() */
#define WB_WIDGET_EVENT_CHANGED             0x10000

/*
 * Existing game_variables.txt widget name inside server_filter_panel:
 *   server_filter_panel_is_version_compatible_checkbox_position_x = 0.03
 *   server_filter_panel_is_version_compatible_checkbox_position_y = 0.046
 *   server_filter_panel_is_version_compatible_checkbox_text_size_x = 0.02
 *   server_filter_panel_is_version_compatible_checkbox_text_size_y = 0.02
 * Sits directly between has_players_checkbox (y=0.071) and
 * is_password_free_checkbox (y=0.021) in every Warband module.
 *
 * Label key "ui_is_version_compatible" matches Warband's native ui.csv key
 * (pairs with patch 08-en-ui-module-filter-label.diff for EN localization).
 */
#define WB_FILTER_CB_WIDGET_NAME            "is_version_compatible_checkbox"
#define WB_FILTER_CB_DEFAULT_LABEL          "ui_is_version_compatible"

#ifdef __cplusplus
}
#endif

#endif /* MNB_MF_ABI_H */
