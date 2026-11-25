# M&BTool 4.3 — Multiplayer Module Filter Mod & Technical Bug Report

> **[Русская версия (README.ru.md)](README.ru.md)** | **[Upstream Bug Report (BUG_REPORT.md)](BUG_REPORT.md)** | **[Original Authors & Credits (CREDITS.md)](CREDITS.md)**

Clean-room native mod (official ModFramework SDK, ARMv7 + ARM64) restoring the missing **"Compatible with module"** filter checkbox in **Mount & Blade: Warband** multiplayer under **M&BTool 4.3** (`com.taleworlds.mbwarband.m1p`), and fixing the "0 servers" problem for PC 1.174 multiplayer (v1.1.0), accompanied by an independent technical bug report.

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

Setting **Server address** = `https://warbandmain.taleworlds.com/`, **Host version** = `1.174 (PC)` and **Start in host version** = `true` in `builtin.custom-server` is **not enough** on the unmodified official APK (confirmed on a real device, logcat `Nov 25 00:35`):

1. **Startup load-order bug** — `builtin.custom-server::mod_start()` runs ~42 ms **before** `builtin.resource-overrides::mod_start()`:
   ```
   00:35:33.246 I/CustomServer:       master servers: 1, hosting on http://warbandmain.taleworlds.com/; version 1157
   00:35:33.288 I/ResourceOverrides:  layer files=122 base module=Native stacked=0 archive files=0
   ```
   `CustomServer` only switches to protocol `1170` when `ResourceOverrides::g_archive_ready` is set, which is always `0` that early. **`Start in host version` therefore never takes effect and the game always boots in `1157`.**
2. **Stock filter hides every `1170` server** — while the engine runs `1157`, `apply_filter` (`cmp w20, #0x485`) drops all PC 1.174 rows. For non-Native modules (e.g. *Napoleonic Wars*) `CustomServer` never spoofs the version at all.
3. **Default `http://` URL is black-holed** — with the built-in default `http://warbandmain.taleworlds.com/`, `builtin.multiplayer-master-server-fix` answers `type=list` with an empty string (HTTP success, 0 servers). An explicit `https://` URL avoids this, but only fixes this one layer.
4. `Internet (Android Only)` sends `gametype=androidonly`, which TaleWorlds answers with 0 bytes; plain *Internet* sends `gametype=android` (~156 servers) instead of the full PC list (`gametype=warband`, ~228 servers).

### Fix: `mp-module-filter.zip` v1.1.0 (no APK patching, no re-signing)

The v1.1.0 mod (built with the official `remixer-dec/MNBTool/modframework` SDK for both `arm64` and `armv7`) fixes all of this in one external mod:

- Rewrites `warbandmain.taleworlds.com/handlerservers.ashx?type=list` requests to `https://...&gametype=warband` (hook on `Request::SendHTTP`, `0x001d2718`, `CHAIN`) so they go through M&BTool's own `ModDevice.httpsGet()`; `chkserialandroid` and non-TaleWorlds hosts are left untouched.
- Normalises PC `1170` servers around the stock `apply_filter` (hook `0x0013ea98`) so PC 1.174 servers are no longer silently hidden.
- Restores the missing **"Compatible with module"** checkbox (unchecked = show all modules, checked = only the current module).

Install: M&BTool -> **Engine mods** -> **Add** -> **From device (.zip)** -> pick `mp-module-filter.zip`, enable it, restart the game. Because it is an unreviewed external mod, M&BTool treats multiplayer as *untrusted-mods* until the author bundles it (see below).

> **Status note**: v1.0.0 failed on real devices with `mbtool.mp-module-filter: mod_start returned -4 (not found)` (overlapping `original`/`previous_core` slots rejected by `libmodlauncher.so`, error masked as `-4`). v1.1.0 is rewritten against the official SDK; please report real-device results.

---

## How M&BTool 4.3 Works (Summary)

- **Dual-Process Launcher (`com.taleworlds.mbwarband.m1p`, `minSdk=24`, `targetSdk=29`, `compileSdk=35`)**:
  - **`MainActivity` (main process)**: Full-screen `WebView` serving an encrypted/verified UI bundle (`assets/cache-index`) via `libmodlauncher.so` (`SessionNative`), exposing JS bridges (`GameLauncher`, `GameManager`, `SavedataManager`, `NativeMods`, `Library`, `AiMod`, `MbImages`).
  - **`GameLaunchActivity` -> `GameActivity` (`:game` process, `NativeActivity`)**: Mounts PC Warband files (`main/` + `patch/` from `/sdcard/mnb`, app storage, or SAF custom folder), applies PC compatibility overlays (`assets/resources/pc-base/`), loads 46 built-in mods + bundled/external `assets/mods/*` packages via `libmodlauncher.so` (`ByteHook` + `ShadowHook`), and runs `libMBExpMobile.so` (a statically recompiled ARM64 translation of the 32-bit NVIDIA Shield Tegra Warband 1.158 engine).
- **Certificate-Bound Attestation (`SessionNative`)**:
  - `libmodlauncher.so` cryptographically binds `MainActivity`'s WebView UI (`assets/cache-index`) and `:game` `SessionHost` attestation to the original APK signing certificate (`SHA-256: f5d6bdaa9a19f3c87be2ba5cdbc4e89ec74e9a9dc1b4daf166fd46baa140c026`). A re-signed APK intentionally refuses to load the M&BTool UI (*"no fallback by design"*).
  - **This repository never bypasses attestation**: all launcher & engine bugs are documented cleanly in **[BUG_REPORT.md](BUG_REPORT.md)** for upstream integration by `remixer-dec`, and the module-filter mod is built cleanly against M&BTool's official `NativeMods` API (`modAbi: 1`).

---

## Root Causes Summary (Full Details in [BUG_REPORT.md](BUG_REPORT.md))

1. **Why 0 Servers Appear** (several stacked causes):
   - `builtin.multiplayer-master-server-fix` (`0x01995118` in `libmodlauncher.so`) drops non-`chkserialandroid` HTTP requests to `warbandmain.taleworlds.com/handlerservers.ashx?type=` and returns `""`.
   - **Startup load order**: `builtin.custom-server` starts ~42 ms before `builtin.resource-overrides`, so `Start in host version = true` never takes effect (game boots in `1157`).
   - **Stock `apply_filter`** hides every PC `1170` server while the engine runs `1157`.
   - `C0031g.c()` silently replaces the default master server with `http://oreo922.cn` from `library.json` (only 2 servers).
   - `ModDevice.httpsGet()` returns HTTP 4xx/5xx HTML error pages as valid `byte[]` and does not follow `http<->https` redirects.
   - The Tegra engine appends `&gametype=android` (~156 servers) or `&gametype=androidonly` (0 bytes) instead of the full PC list.
2. **Why "Filter by Module" Is Missing**:
   - The server browser is native C++ (`Multiplayer_client_window` in `libMBExpMobile.so`). TaleWorlds compiled out PC Warband's 3rd filter checkbox (`is_version_compatible_checkbox`, `ui_is_version_compatible`) in the Tegra port even though its exact layout (`x=0.03, y=0.046`) remains in `game_variables.txt` lines 1301–1304, leaving a blank gap between *Has players* and *No password*.
3. **Additional Launcher Bugs Documented for Upstream (`BUG_REPORT.md`)**:
   - Total UI failure on Android 7.0–8.1 (API 24–27) due to unguarded API-28 `GET_SIGNING_CERTIFICATES` in `e/g.e()`.
   - `builtin.ai-mod` `"read-options"` and dialog `"end"` no-ops.
   - Destructive SAF rename in `GameFiles.k()` deleting `dst` before validating `src` and parent URIs.
   - Mixed `MODE_PRIVATE` vs `Z2.a0()` on `"game_manager"` `SharedPreferences` and `mod_start_failure` `.apply()` racing `Process.killProcess()` in `GameActivity.onDestroy()`.
   - Swallowed savedata migration exceptions in `AbstractC0111w0.k()`, overbroad rollback in `AbstractC0111w0.e()`, missing `smallestScreenSize` on `GameActivity`, and duplicate `enable_blood = 1` in `rgl_config.txt`.

---

## Native Mod: `native/mp-module-filter/` (v1.1.0)

Clean-room C++17 mod written against the official `remixer-dec/MNBTool/modframework` SDK (`mod_api.h`, `build-mod.sh`, automatic ARM64 bindings via `generate-bindings.py`). It restores `is_version_compatible_checkbox` (`ui_is_version_compatible`) at PC Warband's exact coordinates (`x=0.03, y=0.046`), filters server rows case-insensitively against the active module (respecting the master *"Server filter"* checkbox and chaining cleanly with `builtin.custom-server`), and carries the multiplayer server-list fixes described above.

- `native/mp-module-filter/mod.cpp` — mod source (`MF_MOD_START`, `MF_HOOK_CHAIN` hooks, returns real `mf_result_v1` codes).
- `native/mp-module-filter/manifest.json` — `schemaVersion: 2`, `minLauncherVersion: "4.3"`, `modAbi: 1`, `gameBuilds: ["default"]`, EN/RU localizations, `default_enabled` toggle.
- `native/mp-module-filter/Makefile` — thin wrapper around the official `build-mod.sh`.
- **Upstream Trust Note**: Placing the mod folder under `assets/mods/` in an official M&BTool build automatically marks it **trusted for multiplayer** (`AssetManager.list("mods")` -> `InstalledMod.bundled = true`) with zero Java trust-list changes. Unreviewed external mods installed manually on the official APK trigger `multiplayer.untrusted-mods` until reviewed/bundled upstream.

---

## GitHub Actions Artifacts & Building Locally

[`.github/workflows/build.yml`](.github/workflows/build.yml) fetches the official SDK (`remixer-dec/MNBTool`, pinned to the Release 4.3 commit), builds **both** `mod.armv7.o` and `mod.arm64.o` with `ANDROID_ABI=both ./modframework/build-mod.sh` (loader-rule validation and SafeStack flags included), validates `manifest.json` and publishes **`mp-module-filter`** (plus GitHub Release assets on `v*` tags):

- `mp-module-filter.zip` — M&BTool mod package (`manifest.json` at the archive root + `mod.arm64.o` + `mod.armv7.o` + `source/`)
- `mod.arm64.o`, `mod.armv7.o` — relocatable objects
- `SHA256SUMS.txt` — SHA-256 checksums

Build locally with Android NDK:
```bash
git clone --depth 1 https://github.com/remixer-dec/MNBTool.git
export ANDROID_NDK_ROOT=/path/to/android-ndk
make -C native/mp-module-filter MNBTOOL_SDK=$PWD/MNBTool/modframework package
```
