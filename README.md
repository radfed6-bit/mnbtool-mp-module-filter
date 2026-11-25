# M&BTool 4.3 — Multiplayer Module Filter Mod & Technical Bug Report

> **[Русская версия (README.ru.md)](README.ru.md)** | **[Upstream Bug Report (BUG_REPORT.md)](BUG_REPORT.md)** | **[Original Authors & Credits (CREDITS.md)](CREDITS.md)**

Clean-room AArch64 native mod restoring the missing **"Compatible with module"** filter checkbox in **Mount & Blade: Warband** multiplayer under **M&BTool 4.3** (`com.taleworlds.mbwarband.m1p`), accompanied by an independent technical bug report and immediate workarounds for PC 1.174 multiplayer.

> **Clean-Room & Non-Redistribution Notice**: This repository contains **only original C11 source code, build scripts, CI workflows, and technical documentation** (MIT licensed). It contains **no** smali diffs, **no** binary patchers, **no** M&BTool APKs or `.so` libraries, and **no** Mount & Blade: Warband assets. All credit for M&BTool 4.3 belongs to its original author and contributors — see below and [CREDITS.md](CREDITS.md).

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

## Immediate Workaround for 0 Servers (Unmodified Official M&BTool 4.3 APK)

To restore the PC 1.174 multiplayer server list **right now on the unmodified official APK**:

1. Open **M&BTool 4.3** -> **Native Mods** -> **Custom Server** (`builtin.custom-server`).
2. Set **Server address** explicitly to:
   ```
   https://warbandmain.taleworlds.com/
   ```
   - **Why `https://` works**:
     - An explicit user setting overrides `C0031g.c()`'s silent replacement of the default master server with `http://oreo922.cn` (from `library.json`, which returns only 2 servers).
     - Using **`https://`** causes `builtin.custom-server` to dispatch `type=list` directly via Java `ModDevice.httpsGet()`, bypassing `builtin.multiplayer-master-server-fix`'s HTTP bug (`0x01995118`) that otherwise drops `http://warbandmain.taleworlds.com/handlerservers.ashx?type=list*` requests with an empty response (`""`).
   - **Verified behavior**: Live HTTPS probes confirm `https://warbandmain.taleworlds.com/handlerservers.ashx?type=list&gametype=android` returns HTTP 200 directly (~185 servers; full PC list without `&gametype=android` is ~278–288 servers). Note that with `https://`, `builtin.custom-server` also forwards `chkserialandroid` directly to TaleWorlds over HTTPS rather than falling through to `master_fix`'s local stub.
3. Set **Host version** = **`1.174 (PC)`** and **Start in host version** = **`true`** (or add `compatible_multiplayer_version_no = 1170` to `/sdcard/mnb/patch/Modules/Native/module.ini`) so the client uses PC 1.174 protocol (`1170`).

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

1. **Why 0 Servers Appear**:
   - `builtin.multiplayer-master-server-fix` (`0x01995118` in `libmodlauncher.so`) drops non-`chkserialandroid` HTTP requests to `warbandmain.taleworlds.com/handlerservers.ashx?type=` and returns `""`.
   - `C0031g.c()` silently replaces the default master server with `http://oreo922.cn` from `library.json` (returning only 2 entries).
   - `ModDevice.httpsGet()` returns HTTP 4xx/5xx HTML error pages as valid `byte[]` and does not follow `http↔https` redirects.
   - Tegra engine appends `&gametype=android` (~185 servers, missing ~100 PC servers) or `&gametype=androidonly` (0 bytes), and `builtin.custom-server` defaults to protocol `1157` (`1.158`) instead of `1170` (`1.174`).
2. **Why "Filter by Module" Is Missing**:
   - The server browser is native C++ (`Multiplayer_client_window` in `libMBExpMobile.so`). TaleWorlds compiled out PC Warband's 3rd filter checkbox (`is_version_compatible_checkbox`, `ui_is_version_compatible`) in the Tegra port even though its exact layout (`x=0.03, y=0.046`) remains in `game_variables.txt` lines 1301–1304, leaving a blank gap between *Has players* and *No password*.
3. **Additional Launcher Bugs Documented for Upstream (`BUG_REPORT.md`)**:
   - Total UI failure on Android 7.0–8.1 (API 24–27) due to unguarded API-28 `GET_SIGNING_CERTIFICATES` in `e/g.e()`.
   - `builtin.ai-mod` `"read-options"` and dialog `"end"` no-ops.
   - Destructive SAF rename in `GameFiles.k()` deleting `dst` before validating `src` and parent URIs.
   - Mixed `MODE_PRIVATE` vs `Z2.a0()` on `"game_manager"` `SharedPreferences` and `mod_start_failure` `.apply()` racing `Process.killProcess()` in `GameActivity.onDestroy()`.
   - Swallowed savedata migration exceptions in `AbstractC0111w0.k()`, overbroad rollback in `AbstractC0111w0.e()`, missing `smallestScreenSize` on `GameActivity`, and duplicate `enable_blood = 1` in `rgl_config.txt`.

---

## Native Mod: `native/mp-module-filter/`

Clean-room C11 implementation restoring `is_version_compatible_checkbox` (`ui_is_version_compatible`) at PC Warband's exact coordinates (`x=0.03, y=0.046`) and filtering server rows case-insensitively against the active module (respecting the master *"Server filter"* checkbox and chaining cleanly with `builtin.custom-server`):

- `native/mp-module-filter/mf_abi.h` — `_Static_assert`-verified M&BTool `modAbi: 1` & Warband guest offsets/symbols.
- `native/mp-module-filter/mod_etrel.c` — AArch64 `ET_REL` (`mod.arm64.o`) strictly adhering to `libmodlauncher.so`'s allowed symbol whitelist.
- `native/mp-module-filter/mod_so.c` — Standalone ShadowHook shared-library variant (`libmp_module_filter.so`).
- `native/mp-module-filter/manifest.json` — Official M&BTool mod manifest (`id: "mbtool.mp-module-filter"`, EN/RU localizations, `default_enabled` toggle).
- **Upstream Trust Note**: Placing `mp-module-filter/{manifest.json,mod.arm64.o}` under `assets/mods/` in an official M&BTool build automatically marks it **trusted for multiplayer** (`AssetManager.list("mods")` -> `InstalledMod.bundled = true`) with zero Java trust-list changes. Unreviewed external mods installed manually on the official APK trigger `multiplayer.untrusted-mods` until reviewed/bundled upstream.

---

## GitHub Actions Artifacts & Building Locally

[`.github/workflows/build.yml`](.github/workflows/build.yml) compiles `native/mp-module-filter` with Android NDK (`aarch64-linux-android24-clang`), enforces `make check-whitelist`, validates `manifest.json`, and publishes **`mp-module-filter-arm64`** (plus GitHub Release assets on `v*` tags):

- `mp-module-filter.zip` — M&BTool mod package (`mp-module-filter/manifest.json` + `mp-module-filter/mod.arm64.o`)
- `mod.arm64.o` — AArch64 `ET_REL` object for `assets/mods/mp-module-filter/`
- `libmp_module_filter.so` — ShadowHook shared-library variant
- `SHA256SUMS.txt` — SHA-256 checksums

Build locally with Android NDK:
```bash
make -C native/mp-module-filter \
  CC="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang" \
  all check-whitelist
```
