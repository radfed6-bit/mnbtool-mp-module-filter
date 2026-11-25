/*
 * Standalone Android NDK Shared Library (.so) — Multiplayer "Filter by Module" Checkbox
 *
 * Designed for upstream / standalone launcher integration where a normal
 * arm64-v8a shared library (libmp_module_filter.so) is loaded after
 * libmodlauncher.so and libMBExpMobile.so are mapped.
 *
 * Uses:
 *   - libmodlauncher.so runtime exports:
 *       sr_rt_call_guest, sr_rt_window_base, sr_rt_heap_malloc, sr_rt_heap_free
 *   - libshadowhook.so (already initialized in UNIQUE mode by the host):
 *       shadowhook_hook_sym_name
 *   - libMBExpMobile.so translated guest function exports:
 *       sr_f_1012f06c, sr_f_1012f040, sr_f_10122c38, sr_f_1013ea98
 *
 * Note on translated-guest register ABI:
 *   libMBExpMobile.so's sr_f_10xxxxxx functions follow the static recompiler
 *   register contract: w0..w3 = ARM32 r0..r3, w10 = guest return PC, w27 =
 *   guest SP, x28 = 4 GiB guest window base. Standard AAPCS64 preserves x27/x28,
 *   but treats w10 as scratch; therefore each hook uses a minimal naked AArch64
 *   stub that preserves w10 and delegates cleanly to C + original sr_f_*.
 */

#include "mf_abi.h"

#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#ifdef __ANDROID__
#include <android/log.h>
#include <jni.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "MpModuleFilter", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "MpModuleFilter", __VA_ARGS__)
#else
#define LOGI(...) ((void)0)
#define LOGE(...) ((void)0)
#endif

/* Runtime function pointer types from libmodlauncher.so & libshadowhook.so */
typedef int       (*fn_sr_rt_call_guest_t)(sr_rt_guest_call_t *req);
typedef uintptr_t (*fn_sr_rt_window_base_t)(void);
typedef void     *(*fn_sr_rt_heap_malloc_t)(size_t size);
typedef void      (*fn_sr_rt_heap_free_t)(void *ptr);
typedef void     *(*fn_shadowhook_hook_sym_name_t)(const char *lib_name,
                                                   const char *sym_name,
                                                   void *new_addr,
                                                   void **orig_addr);

static fn_sr_rt_call_guest_t          g_call_guest  = NULL;
static fn_sr_rt_window_base_t         g_window_base = NULL;
static fn_sr_rt_heap_malloc_t         g_guest_alloc = NULL;
static fn_sr_rt_heap_free_t           g_guest_free  = NULL;
static fn_shadowhook_hook_sym_name_t  g_sh_hook     = NULL;

/* Original sr_f_10xxxxxx pointers populated by ShadowHook (hidden for PC-rel adrp) */
__attribute__((visibility("hidden"))) void *g_orig_sr_activate     = NULL;
__attribute__((visibility("hidden"))) void *g_orig_sr_deactivate   = NULL;
__attribute__((visibility("hidden"))) void *g_orig_sr_check        = NULL;
__attribute__((visibility("hidden"))) void *g_orig_sr_apply_filter = NULL;

/* UI state (stored as 32-bit guest virtual addresses) */
static uint32_t g_active_win_va      = 0u;
static uint32_t g_module_cb_va       = 0u;
static int32_t  g_filter_enabled     = 1;
static int      g_initialized        = 0;

/* =========================================================================
 * 1. Guest Memory & Call Wrappers
 * ========================================================================= */

static inline void *guest_to_host(uint32_t guest_va) {
    if (guest_va == 0u || !g_window_base) {
        return NULL;
    }
    uintptr_t base = g_window_base();
    if (base == 0u) {
        return NULL;
    }
    return (void *)(base + (uintptr_t)guest_va);
}

static inline uint32_t host_to_guest(const void *host_ptr) {
    if (!host_ptr || !g_window_base) {
        return 0u;
    }
    uintptr_t base = g_window_base();
    uintptr_t addr = (uintptr_t)host_ptr;
    if (base == 0u || addr < base) {
        return 0u;
    }
    uint64_t off = (uint64_t)(addr - base);
    return ((off >> 32) == 0u) ? (uint32_t)off : 0u;
}

static inline uint32_t read_guest_u32(uint32_t guest_va) {
    void *p = guest_to_host(guest_va);
    if (!p) {
        return 0u;
    }
    uint32_t v = 0u;
    memcpy(&v, p, sizeof(v));
    return v;
}

static uint32_t call_guest_4(uint32_t guest_off,
                             uint32_t r0, uint32_t r1,
                             uint32_t r2, uint32_t r3) {
    sr_rt_guest_call_t req;
    memset(&req, 0, sizeof(req));
    req.guest_pc = WB_GUEST_LOAD_BASE + guest_off;
    req.r[0] = r0;
    req.r[1] = r1;
    req.r[2] = r2;
    req.r[3] = r3;
    if (g_call_guest(&req) != 0) {
        return 0u;
    }
    return (uint32_t)req.ret;
}

static uint32_t call_guest_6(uint32_t guest_off,
                             uint32_t r0, uint32_t r1,
                             uint32_t r2, uint32_t r3,
                             uint32_t s0, uint32_t s1) {
    uint32_t stack_args[2] = { s0, s1 };
    sr_rt_guest_call_t req;
    memset(&req, 0, sizeof(req));
    req.guest_pc        = WB_GUEST_LOAD_BASE + guest_off;
    req.r[0]            = r0;
    req.r[1]            = r1;
    req.r[2]            = r2;
    req.r[3]            = r3;
    req.stack_arg_words = 2u;
    req.stack_args      = stack_args;
    if (g_call_guest(&req) != 0) {
        return 0u;
    }
    return (uint32_t)req.ret;
}

static const char *guest_rglstr_cstr(uint32_t rglstr_va) {
    if (rglstr_va == 0u) {
        return NULL;
    }
    uint32_t cstr_va = call_guest_4((uint32_t)WB_OFF_RGL_STRING_CHAR_P,
                                    rglstr_va, 0u, 0u, 0u);
    return (const char *)guest_to_host(cstr_va);
}

/* =========================================================================
 * 2. Checkbox Lifecycle & Row Filter Logic (called from AArch64 stubs)
 * ========================================================================= */

__attribute__((visibility("hidden")))
void mp_on_activate_post(uint32_t win_va) {
    g_active_win_va = 0u;
    g_module_cb_va  = 0u;

    if (win_va == 0u || !g_guest_alloc || !g_guest_free) {
        return;
    }
    uint32_t panel_va = read_guest_u32(win_va + WB_MP_WIN_FILTER_PANEL_OFF);
    if (panel_va == 0u) {
        return;
    }

    size_t name_len = strlen(WB_FILTER_CB_WIDGET_NAME) + 1u;
    size_t text_len = strlen(WB_FILTER_CB_DEFAULT_LABEL) + 1u;

    void  *name_str = g_guest_alloc(WB_RGL_STRING_SIZE);
    void  *text_str = g_guest_alloc(WB_RGL_STRING_SIZE);
    char  *name_c   = (char *)g_guest_alloc(name_len);
    char  *text_c   = (char *)g_guest_alloc(text_len);
    float *coords   = (float *)g_guest_alloc(sizeof(float) * 3u);

    if (name_str && text_str && name_c && text_c && coords) {
        memcpy(name_c, WB_FILTER_CB_WIDGET_NAME, name_len);
        memcpy(text_c, WB_FILTER_CB_DEFAULT_LABEL, text_len);
        coords[0] = -1.0f;
        coords[1] = -1.0f;
        coords[2] =  0.0f;

        uint32_t name_str_va = host_to_guest(name_str);
        uint32_t text_str_va = host_to_guest(text_str);
        uint32_t name_c_va   = host_to_guest(name_c);
        uint32_t text_c_va   = host_to_guest(text_c);
        uint32_t coords_va   = host_to_guest(coords);

        call_guest_4((uint32_t)WB_OFF_RGL_STRING_CTOR, name_str_va, name_c_va, 0u, 0u);
        call_guest_4((uint32_t)WB_OFF_RGL_STRING_CTOR, text_str_va, text_c_va, 0u, 0u);

        uint32_t cb_va = call_guest_6((uint32_t)WB_OFF_WIDGET_CREATE_CHECK_BOX,
                                      panel_va, name_str_va, text_str_va,
                                      coords_va, coords_va + 4u, coords_va + 8u);

        call_guest_4((uint32_t)WB_OFF_RGL_STRING_DTOR, text_str_va, 0u, 0u, 0u);
        call_guest_4((uint32_t)WB_OFF_RGL_STRING_DTOR, name_str_va, 0u, 0u, 0u);

        if (cb_va != 0u) {
            g_active_win_va = win_va;
            g_module_cb_va  = cb_va;
            call_guest_4((uint32_t)WB_OFF_CHECKBOX_SET_VALUE_I,
                         cb_va, (uint32_t)(g_filter_enabled ? 1 : 0), 0u, 0u);
        }
    }

    if (coords)   g_guest_free(coords);
    if (text_c)   g_guest_free(text_c);
    if (name_c)   g_guest_free(name_c);
    if (text_str) g_guest_free(text_str);
    if (name_str) g_guest_free(name_str);
}

__attribute__((visibility("hidden")))
void mp_on_deactivate_pre(uint32_t win_va) {
    if (win_va == g_active_win_va) {
        g_active_win_va = 0u;
        g_module_cb_va  = 0u;
    }
}

__attribute__((visibility("hidden")))
void mp_on_check_post(uint32_t win_va) {
    if (win_va == 0u || win_va != g_active_win_va || g_module_cb_va == 0u) {
        return;
    }

    uint32_t ev = call_guest_4((uint32_t)WB_OFF_WIDGET_GET_EVENT,
                               g_module_cb_va, (uint32_t)WB_WIDGET_EVENT_CHANGED,
                               0u, 0u);
    int32_t val = call_guest_4((uint32_t)WB_OFF_WIDGET_GET_VALUE_I,
                               g_module_cb_va, 0u, 0u, 0u) != 0u ? 1 : 0;

    int32_t toggled = (ev != 0u) ? 1 : 0;
    if (val != g_filter_enabled) {
        g_filter_enabled = val;
        toggled = 1;
    }

    if (toggled) {
        call_guest_4((uint32_t)WB_OFF_MP_WIN_APPLY_FILTER_ALL, win_va, 0u, 0u, 0u);
        uint32_t table_va = read_guest_u32(win_va + WB_MP_WIN_SERVERS_LIST_OFF);
        if (table_va != 0u) {
            call_guest_4((uint32_t)WB_OFF_TABLE_UPDATE_TABLE, table_va, 0u, 0u, 0u);
        }
    }
}

/*
 * Return convention (verified in sr_f_1013ea98):
 *   1 = HIDE (row is filtered out)
 *   0 = SHOW (row passes filter)
 */
__attribute__((visibility("hidden")))
uint32_t mp_on_apply_filter_post(uint32_t win_va, uint32_t row_va, uint32_t orig_ret) {
    if (orig_ret != 0u || win_va == 0u || row_va == 0u) {
        return orig_ret;
    }

    /* Respect master "Server filter" toggle (this + 0x338), matching sr_f_1013ea98. */
    uint32_t master_cb_va = read_guest_u32(win_va + WB_MP_WIN_FILTER_CHECKBOX_OFF);
    if (master_cb_va != 0u &&
        call_guest_4((uint32_t)WB_OFF_WIDGET_GET_VALUE_I, master_cb_va, 0u, 0u, 0u) == 0u) {
        return 0u;
    }

    int32_t enabled = g_filter_enabled;
    if (win_va == g_active_win_va && g_module_cb_va != 0u) {
        enabled = (call_guest_4((uint32_t)WB_OFF_WIDGET_GET_VALUE_I,
                                g_module_cb_va, 0u, 0u, 0u) != 0u) ? 1 : 0;
        g_filter_enabled = enabled;
    }
    if (!enabled) {
        return 0u;
    }

    const char *cur_mod = guest_rglstr_cstr(WB_GUEST_LOAD_BASE +
                                            (uint32_t)WB_OFF_SELECTED_MODULE_NAME);
    if (!cur_mod || cur_mod[0] == '\0') {
        return 0u;
    }

    uint32_t cells_va = read_guest_u32(row_va + WB_TABLE_ROW_CELLS_DATA_OFF);
    if (cells_va == 0u) {
        return 0u;
    }
    uint32_t mod_cell_va = read_guest_u32(cells_va + WB_TABLE_ROW_MODULE_CELL_INDEX * 4u);
    if (mod_cell_va == 0u) {
        return 0u;
    }
    const char *row_mod = guest_rglstr_cstr(mod_cell_va + WB_TABLE_CELL_STRING_OFF);
    if (!row_mod || row_mod[0] == '\0') {
        return 1u; /* hide row with missing module name */
    }

    return (strcasecmp(row_mod, cur_mod) == 0) ? 0u : 1u;
}

/* =========================================================================
 * 3. AArch64 Register-Preserving Hook Stubs (preserve w10, w27, x28)
 *    Defined as top-level asm for compatibility with both Clang and GCC.
 * ========================================================================= */

#if defined(__aarch64__)

extern void stub_activate(void);
extern void stub_deactivate(void);
extern void stub_check(void);
extern void stub_apply_filter(void);

__asm__(
    ".text\n"
    ".p2align 2\n"
    ".hidden stub_activate\n"
    ".type stub_activate, %function\n"
    "stub_activate:\n"
    "    stp  x29, x30, [sp, #-32]!\n"
    "    stp  x19, x20, [sp, #16]\n"
    "    mov  x29, sp\n"
    "    mov  w19, w0\n"
    "    mov  w20, w10\n"
    "    adrp x16, g_orig_sr_activate\n"
    "    ldr  x16, [x16, :lo12:g_orig_sr_activate]\n"
    "    blr  x16\n"
    "    mov  w0, w19\n"
    "    bl   mp_on_activate_post\n"
    "    mov  w10, w20\n"
    "    ldp  x19, x20, [sp, #16]\n"
    "    ldp  x29, x30, [sp], #32\n"
    "    ret\n"
    ".size stub_activate, .-stub_activate\n"

    ".p2align 2\n"
    ".hidden stub_deactivate\n"
    ".type stub_deactivate, %function\n"
    "stub_deactivate:\n"
    "    stp  x29, x30, [sp, #-32]!\n"
    "    stp  x19, x20, [sp, #16]\n"
    "    mov  x29, sp\n"
    "    mov  w19, w0\n"
    "    mov  w20, w10\n"
    "    bl   mp_on_deactivate_pre\n"
    "    mov  w0, w19\n"
    "    mov  w10, w20\n"
    "    adrp x16, g_orig_sr_deactivate\n"
    "    ldr  x16, [x16, :lo12:g_orig_sr_deactivate]\n"
    "    blr  x16\n"
    "    mov  w10, w20\n"
    "    ldp  x19, x20, [sp, #16]\n"
    "    ldp  x29, x30, [sp], #32\n"
    "    ret\n"
    ".size stub_deactivate, .-stub_deactivate\n"

    ".p2align 2\n"
    ".hidden stub_check\n"
    ".type stub_check, %function\n"
    "stub_check:\n"
    "    stp  x29, x30, [sp, #-32]!\n"
    "    stp  x19, x20, [sp, #16]\n"
    "    mov  x29, sp\n"
    "    mov  w19, w0\n"
    "    mov  w20, w10\n"
    "    adrp x16, g_orig_sr_check\n"
    "    ldr  x16, [x16, :lo12:g_orig_sr_check]\n"
    "    blr  x16\n"
    "    mov  w0, w19\n"
    "    bl   mp_on_check_post\n"
    "    mov  w10, w20\n"
    "    ldp  x19, x20, [sp, #16]\n"
    "    ldp  x29, x30, [sp], #32\n"
    "    ret\n"
    ".size stub_check, .-stub_check\n"

    ".p2align 2\n"
    ".hidden stub_apply_filter\n"
    ".type stub_apply_filter, %function\n"
    "stub_apply_filter:\n"
    "    stp  x29, x30, [sp, #-48]!\n"
    "    stp  x19, x20, [sp, #16]\n"
    "    str  x21, [sp, #32]\n"
    "    mov  x29, sp\n"
    "    mov  w19, w0\n"
    "    mov  w20, w1\n"
    "    mov  w21, w10\n"
    "    adrp x16, g_orig_sr_apply_filter\n"
    "    ldr  x16, [x16, :lo12:g_orig_sr_apply_filter]\n"
    "    blr  x16\n"
    "    mov  w2, w0\n"
    "    mov  w0, w19\n"
    "    mov  w1, w20\n"
    "    bl   mp_on_apply_filter_post\n"
    "    mov  w10, w21\n"
    "    ldr  x21, [sp, #32]\n"
    "    ldp  x19, x20, [sp, #16]\n"
    "    ldp  x29, x30, [sp], #48\n"
    "    ret\n"
    ".size stub_apply_filter, .-stub_apply_filter\n"
);

#endif /* __aarch64__ */

/* =========================================================================
 * 4. Public Initialization Entry Points
 * ========================================================================= */

__attribute__((visibility("default")))
int mp_module_filter_init(void) {
    if (g_initialized) {
        return 0;
    }

    void *ml = dlopen("libmodlauncher.so", RTLD_NOW | RTLD_NOLOAD);
    if (!ml) {
        LOGE("libmodlauncher.so is not loaded");
        return -1;
    }

    g_call_guest  = (fn_sr_rt_call_guest_t) dlsym(ml, "sr_rt_call_guest");
    g_window_base = (fn_sr_rt_window_base_t)dlsym(ml, "sr_rt_window_base");
    g_guest_alloc = (fn_sr_rt_heap_malloc_t)dlsym(ml, "sr_rt_heap_malloc");
    g_guest_free  = (fn_sr_rt_heap_free_t)  dlsym(ml, "sr_rt_heap_free");

    void *sh = dlopen("libshadowhook.so", RTLD_NOW | RTLD_NOLOAD);
    if (!sh) {
        sh = RTLD_DEFAULT;
    }
    g_sh_hook = (fn_shadowhook_hook_sym_name_t)dlsym(sh, "shadowhook_hook_sym_name");

    if (!g_call_guest || !g_window_base || !g_guest_alloc || !g_guest_free || !g_sh_hook) {
        LOGE("missing required runtime symbols (sr_rt_* / shadowhook_hook_sym_name)");
        return -2;
    }

#if defined(__aarch64__)
    if (!g_sh_hook(WB_LIB_NAME, "sr_f_1012f06c", (void *)stub_activate,     &g_orig_sr_activate) ||
        !g_sh_hook(WB_LIB_NAME, "sr_f_1012f040", (void *)stub_deactivate,   &g_orig_sr_deactivate) ||
        !g_sh_hook(WB_LIB_NAME, "sr_f_10122c38", (void *)stub_check,        &g_orig_sr_check) ||
        !g_sh_hook(WB_LIB_NAME, "sr_f_1013ea98", (void *)stub_apply_filter, &g_orig_sr_apply_filter)) {
        LOGE("failed to install ShadowHook hooks on libMBExpMobile.so");
        return -3;
    }
#endif

    g_initialized = 1;
    LOGI("Multiplayer Filter by Module hooks installed");
    return 0;
}

#ifdef __ANDROID__
__attribute__((visibility("default")))
jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    (void)vm;
    (void)reserved;
    (void)mp_module_filter_init();
    return JNI_VERSION_1_6;
}
#endif
