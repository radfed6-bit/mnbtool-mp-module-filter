# M&BTool 4.3 — Multiplayer Module Filter Mod & Technical Bug Report

> **[Русская версия (README.ru.md)](README.ru.md)** | **[Upstream Bug Report (BUG_REPORT.md)](BUG_REPORT.md)** | **[Original Authors & Credits (CREDITS.md)](CREDITS.md)**

Clean-room native mod (official ModFramework SDK, ARMv7 + ARM64) restoring the missing **"Compatible with module"** filter checkbox in **Mount & Blade: Warband** multiplayer under **M&BTool 4.3** (`com.taleworlds.mbwarband.m1p`) and fixing the "0 servers" problem for PC 1.174 multiplayer (`v1.1.2`), accompanied by an independent technical bug report.

> **Clean-Room & Non-Redistribution Notice**: This repository contains **only original C++17 source code, build scripts, CI workflows, and technical documentation** (MIT licensed). It contains **no** smali diffs, **no** binary patchers, **no** M&BTool APKs or `.so` libraries, and **no** Mount & Blade: Warband assets. All credit for M&BTool 4.3 belongs to its original author and contributors — see below and [CREDITS.md](CREDITS.md).

---

## Original Author & Credits

- **Original M&BTool 4.3 Author**: **remixer-dec**
  - Official website: https://remixer-dec.github.io
  - Telegram releases channel: https://t.me/rdreleases
  - GitHub: https://github.com/remixer-dec/MNBTool
  - Live library manifest: https://remixer-dec.github.io/MNBTool/library.json
- **Contributors credited in M&BTool 4.3**: **leakman**, **Oreo922**, **M&BTool contributors**
- **Supporters (from `library.json`)**: **Oreo922**, **Corukae**, **LenHa**
- **Original Game**: **TaleWorlds Entertainment** (Mount & Blade: Warband)
- Complete third-party library, font (SIL OFL 1.1), and voice-list attribution is in **[CREDITS.md](CREDITS.md)**.

---

## Why Setting `https://warbandmain.taleworlds.com/` + `1.174` Alone Still Shows 0 Servers

Setting **Server address** = `https://warbandmain.taleworlds.com/`, **Host version** = `1.174 (PC)` and **Start in host version** = `true` in `builtin.custom-server` is **not enough** on the unmodified official APK (confirmed on a real Android device):

1. **FIFO `MF_HOOK_CHAIN` blackhole in `builtin.multiplayer-master-server-fix`** — `libmodlauncher.so` (`0x1898368..0x1898390`) executes `MF_HOOK_CHAIN` hooks in strict FIFO order. `builtin.multiplayer-master-server-fix` installs **first** on `Request::SendHTTP` (`0x001d2718`), matches `warbandmain.taleworlds.com/handlerservers.ashx?type=` on **both** `http://` and `https://`, and returns `""` (empty response, HTTP success, zero DNS/HTTP traffic) without calling `previous` whenever `chkserialandroid` is absent — so `builtin.custom-server`'s `https://` setting never even runs for stock `?type=list`.
2. **Startup load-order bug** — `builtin.custom-server::mod_start()` runs ~42 ms **before** `builtin.resource-overrides::mod_start()`:
   ```text
   00:35:33.246 I/CustomServer:       master servers: 1, hosting on http://warbandmain.taleworlds.com/; version 1157
   00:35:33.288 I/ResourceOverrides:  layer files=122 base module=Native stacked=0 archive files=0
   ```
   `CustomServer` only switches to protocol `1170` when `ResourceOverrides::g_archive_ready` is set, which is always `0` that early. **`Start in host version` therefore never takes effect and the game always boots in `1157`.**
3. **Stock filter hides every `1170` server** — while the engine runs `1157`, `apply_filter` (`cmp w20, #0x485`) drops all PC 1.174 rows. For non-Native modules (e.g. *Napoleonic Wars*) `CustomServer` never spoofs the version at all.
4. **Query string filtering** — `Internet (Android Only)` sends `gametype=androidonly`, which TaleWorlds answers with 0 bytes; plain *Internet* sends `gametype=android` (~156–185 servers) instead of the full PC list (`gametype=warband`, ~228–280 servers).

### Fix: `mp-module-filter.zip` `v1.1.2` (No APK Patching, No Re-Signing)

Built with the official `remixer-dec/MNBTool/modframework` SDK (`ANDROID_ABI=both` for `arm64` and `armv7`), `v1.1.2` fixes all of the above in one external mod:

- **FIFO-head master-list hook (`Network_manager::send_http_request`, `0x001487ec`, `CHAIN`)**: runs before `builtin.multiplayer-master-server-fix` on `0x001d2718` and rewrites `warbandmain.taleworlds.com/handlerservers.ashx?type=list` requests to `https://warbandmain.taleworlds.com/handlerservers.ashx?gametype=warband&type=list`. The swapped query order misses `master-server-fix`'s `?type=` substring check cleanly and routes straight into M&BTool's own `ModDevice.httpsGet()` — working even with only default-on *LAN Multiplayer Fix*. Guarded by `api->is_mod_active()` so raw Tegra `libcurl` never receives `https://`; `chkserialandroid` and custom non-TaleWorlds hosts are left untouched.
- **PC `1170..1174` filter normalization (`0x0013ea98`, `CHAIN`)**: temporarily normalizes PC `1170..1174` server rows (`1157` / `1174` retry, immediately restoring the real version) around stock `apply_filter` so PC 1.174 servers are no longer hidden on Native or 3rd-party modules.
- **Restores "Compatible with module" checkbox (`0x0012f06c`, `0x0012f040`, `0x00122c38`, `CHAIN`)**: places `is_version_compatible_checkbox` (`ui_is_version_compatible`) in PC Warband's exact `game_variables.txt` slot (`x=0.03, y=0.046`), unchecked = all modules, checked = active module only (case-insensitive, respecting the master *"Server filter"* toggle).

**Install**: M&BTool -> **Engine mods** -> **Add** -> **From device (.zip)** -> pick `mp-module-filter.zip`, enable it (keep **LAN Multiplayer Fix** enabled), and restart the game.

> **Real-Device Status (`v1.1.2`) & External-Mod Join Limitation**:
> - **Verified working on Android ARM64**: loads cleanly, fetches the full PC 1.174 server list (~228–280 servers), renders the **"Compatible with module"** checkbox at `y=0.046`, and filters rows by active module.
> - **Known upstream M&BTool 4.3 ARM64 bug when clicking Join ([BUG_REPORT.md #1d](BUG_REPORT.md#1d-critical-arm64-sigsegv-crash-in-libmodlauncherso-when-joining-any-server-with-any-external-mod-enabled))**: Whenever *any* external (non-bundled/unreviewed) mod is enabled, M&BTool sets `multiplayer.untrusted-mods=true`. On ARM64, `libmodlauncher.so`'s `Network_manager::start_client` guard (`0x188df78 -> 0x18970a4`) passes raw 32-bit guest pointer `x2` without adding guest window base `x28` / calling `decode_pointer`, causing an immediate `SIGSEGV (SEGV_MAPERR)` when clicking **Join** on **any** server (even `"MOD..."` servers, before M&BTool's intended `"MOD"` prefix check or `trustedOnly=true` restart can run).
> - Because this crash is inside `libmodlauncher.so` itself (affecting *every* external mod on ARM64), `v1.1.2` enables browsing and module filtering on the unmodified APK, while joining servers requires either the upstream fix to `libmodlauncher.so+0x188df78` or bundling/trusting the mod in an official M&BTool release.

---

## How M&BTool 4.3 Works (Summary)

- **Dual-Process Launcher (`com.taleworlds.mbwarband.m1p`, `minSdk=24`, `targetSdk=29`, `compileSdk=35`)**:
  - **`MainActivity` (main process)**: Full-screen `WebView` serving an encrypted/verified UI bundle (`assets/cache-index`) via `libmodlauncher.so` (`SessionNative`), exposing JS bridges (`GameLauncher`, `GameManager`, `SavedataManager`, `NativeMods`, `Library`, `AiMod`, `MbImages`).
  - **`GameLaunchActivity` -> `GameActivity` (`:game` process, `NativeActivity`)**: Mounts PC Warband files (`main/` + `patch/` from `/sdcard/mnb`, app storage, or SAF custom folder), applies PC compatibility overlays (`assets/resources/pc-base/`), loads built-in mods + bundled/external `assets/mods/*` packages via `libmodlauncher.so` (`ByteHook` + `ShadowHook`), and runs `libMBExpMobile.so` (a statically recompiled ARM64 translation of the 32-bit NVIDIA Shield Tegra Warband 1.158 engine).
- **Certificate-Bound Attestation (`SessionNative`)**:
  - `libmodlauncher.so` cryptographically binds `MainActivity`'s WebView UI (`assets/cache-index`) and `:game` `SessionHost` attestation to the original APK signing certificate (`SHA-256: f5d6bdaa9a19f3c87be2ba5cdbc4e89ec74e9a9dc1b4daf166fd46baa140c026`). A re-signed APK intentionally refuses to load the M&BTool UI (*"no fallback by design"*).
  - **This repository never bypasses attestation**: all launcher & engine bugs are documented cleanly in **[BUG_REPORT.md](BUG_REPORT.md)** for upstream integration by `remixer-dec`, and the module-filter mod is built cleanly against M&BTool's official `ModFramework` SDK (`modAbi: 1`).

---

## Root Causes Summary (Full Details in [BUG_REPORT.md](BUG_REPORT.md))

1. **Why 0 Servers Appear** (stacked causes):
   - `builtin.multiplayer-master-server-fix` (`0x019950a4` in `libmodlauncher.so`) is FIFO-first in `MF_HOOK_CHAIN` on `Request::SendHTTP` (`0x001d2718`), matches `warbandmain.taleworlds.com/handlerservers.ashx?type=` on both `http://` and `https://`, and returns `""` without calling `previous` (blocking `builtin.custom-server` too).
   - **Startup load order**: `builtin.custom-server` starts ~42 ms before `builtin.resource-overrides`, so `Start in host version = true` never takes effect (game boots in `1157`).
   - **Stock `apply_filter`** hides every PC `1170` server while the engine runs `1157`.
   - `C0031g.c()` silently replaces the default master server with `http://oreo922.cn` from `library.json` (only 2 servers).
   - `ModDevice.httpsGet()` returns HTTP 4xx/5xx HTML error pages as valid `byte[]` and does not follow `http<->https` redirects.
   - The Tegra engine appends `&gametype=android` (~156–185 servers) or `&gametype=androidonly` (0 bytes) instead of the full PC list (`gametype=warband`, ~228–280 servers).
2. **Why Joining Crashes With Any External Mod on ARM64**:
   - In `libmodlauncher.so`, `multiplayer.untrusted-mods` hooks `Network_manager::start_client` (`0x00143770` -> `0x188df78`) and passes raw 32-bit guest pointer `x2` to `0x1897084` without `decode_pointer` / `+ x28`, immediately faulting at `0x18970a4` (`SIGSEGV`).
3. **Why "Filter by Module" Is Missing**:
   - The server browser is native C++ (`Multiplayer_client_window` in `libMBExpMobile.so`). TaleWorlds compiled out PC Warband's 3rd filter checkbox (`is_version_compatible_checkbox`, `ui_is_version_compatible`) in the Tegra port even though its exact layout (`x=0.03, y=0.046`) remains in `game_variables.txt` lines 1301–1304, leaving a blank gap between *Has players* and *No password*.
4. **Additional Launcher Bugs Documented for Upstream (`BUG_REPORT.md`)**:
   - Total UI failure on Android 7.0–8.1 (API 24–27) due to unguarded API-28 `GET_SIGNING_CERTIFICATES` in `e/g.e()`.
   - `builtin.ai-mod` `"read-options"` and dialog `"end"` no-ops.
   - Destructive SAF rename in `GameFiles.k()` deleting `dst` before validating `src` and parent URIs.
   - Mixed `MODE_PRIVATE` vs `Z2.a0()` on `"game_manager"` `SharedPreferences` and `mod_start_failure` `.apply()` racing `Process.killProcess()` in `GameActivity.onDestroy()`.
   - Swallowed savedata migration exceptions in `AbstractC0111w0.k()`, overbroad rollback in `AbstractC0111w0.e()`, missing `smallestScreenSize` on `GameActivity`, and duplicate `enable_blood = 1` in `rgl_config.txt`.

---

## Native Mod: `native/mp-module-filter/` (`v1.1.2`)

Clean-room C++17 mod written against the official `remixer-dec/MNBTool/modframework` SDK (`mod_api.h`, `build-mod.sh`, automatic ARM64 bindings via `generate-bindings.py`).

- `native/mp-module-filter/mod.cpp` — `MF_MOD_START` with 5 `MF_HOOK_CHAIN` hooks (`0x12f06c`, `0x12f040`, `0x122c38`, `0x13ea98`, `0x1487ec`), `is_mod_active` HTTPS bridge guard, `1157`/`1174` filter normalization, case-insensitive module comparison, and real `mf_result_v1` error returns.
- `native/mp-module-filter/manifest.json` — `schemaVersion: 2`, `minLauncherVersion: "4.3"`, `version: "1.1.2"`, `modAbi: 1`, `gameBuilds: ["default"]`, `compatibility.loadAfter: ["builtin.custom-server", "builtin.multiplayer-master-server-fix"]`, EN/RU localizations, `default_enabled` toggle, and all 5 hook declarations.
- `native/mp-module-filter/Makefile` — thin wrapper around the official `build-mod.sh`.
- **Upstream Trust Note**: Placing the mod folder under `assets/mods/` in an official M&BTool build automatically marks it **trusted for multiplayer** (`AssetManager.list("mods")` -> `InstalledMod.bundled = true`) with zero Java trust-list changes, avoiding both `multiplayer.untrusted-mods` and upstream Bug #1d.

---

## GitHub Actions Artifacts & Building Locally

[`.github/workflows/build.yml`](.github/workflows/build.yml) fetches the official SDK (`remixer-dec/MNBTool`, pinned to Release 4.3 commit `2797afa175ae1763635f265fa348798770cb7e1e`), builds **both** `mod.armv7.o` and `mod.arm64.o` with `ANDROID_ABI=both ./modframework/build-mod.sh` (loader-rule validation and SafeStack flags included), validates `manifest.json` (5 hooks) and publishes **`mp-module-filter`** (plus GitHub Release assets on `v*` tags):

- `mp-module-filter.zip` — M&BTool mod package (`manifest.json` at the archive root + `mod.arm64.o` + `mod.armv7.o` + `source/mod.cpp`)
- `mod.arm64.o`, `mod.armv7.o` — relocatable objects
- `SHA256SUMS.txt` — SHA-256 checksums

Build locally with Android NDK:
```bash
git clone --depth 1 https://github.com/remixer-dec/MNBTool.git
export ANDROID_NDK_ROOT=/path/to/android-ndk
make -C native/mp-module-filter MNBTOOL_SDK=$PWD/MNBTool/modframework package
```
