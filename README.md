# M&BTool 4.3 — Multiplayer Module Filter Mod & Technical Bug Report

> **[Русская версия (README.ru.md)](README.ru.md)** | **[Upstream Bug Report (BUG_REPORT.md)](BUG_REPORT.md)** | **[Original Authors & Credits (CREDITS.md)](CREDITS.md)**

Clean-room native mod (official ModFramework SDK, ARMv7 + ARM64) restoring the missing **"Compatible with module"** filter checkbox in **Mount & Blade: Warband** multiplayer under **M&BTool 4.3** (`com.taleworlds.mbwarband.m1p`) and enabling TaleWorlds PC 1.174 server-list browsing (`v1.1.2`), accompanied by an independent technical bug report.

> **Clean-Room & Non-Redistribution Notice**: This repository contains **only original C++17 source code, build scripts, CI workflows, and technical documentation** (MIT licensed). It contains **no** smali diffs, **no** binary patchers, **no** M&BTool APKs or `.so` libraries, and **no** Mount & Blade: Warband assets. All credit for M&BTool 4.3 belongs to its original author and contributors — see below and [CREDITS.md](CREDITS.md).

> **Upstream Status (`2026-11-26`)**: `remixer-dec` reviewed [BUG_REPORT.md](BUG_REPORT.md) and confirmed fixes in the next M&BTool release for the **ARM64 external-mod join `SIGSEGV` (#1d)**, **HTTP↔HTTPS redirects (#3)**, **Android 7.0–8.1 signing API crash (#6)**, **SAF rename guard (#8)**, **`GameActivity` `smallestScreenSize` (#11)**, and **ARM64 LAN server search**, plus **built-in module filtering (#5)** as an option and an investigation into **multi-server `SendHTTP` chaining (#1)**.

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

## Why Setting `https://warbandmain.taleworlds.com/` Alone Still Shows 0 Servers

M&BTool 4.3 intentionally uses a community master server (`http://oreo922.cn` via `library.json`) rather than TaleWorlds' master server. Setting **Server address** = `https://warbandmain.taleworlds.com/` in `builtin.custom-server` alone still yields **0 servers** on the unmodified official APK:

1. **FIFO `MF_HOOK_CHAIN` interception in `builtin.multiplayer-master-server-fix`** — `libmodlauncher.so` (`0x1898368..0x1898390`) executes `MF_HOOK_CHAIN` hooks in strict FIFO order. `builtin.multiplayer-master-server-fix` (`0x19950a4`) installs **first** on `Request::SendHTTP` (`0x001d2718`), intentionally returns `""` (empty response, zero DNS/HTTP traffic) for `warbandmain.taleworlds.com/handlerservers.ashx?type=` on **both** `http://` and `https://`, and never calls `previous` — which also makes `CustomServer::SendHTTP` (`0x19168bc`) unreachable for multi-server (`>= 2` URLs) fanout.
2. **Stock filter hides `1170` rows in `1157` mode / non-Native modules** — when the engine runs in protocol `1157` (such as on non-Native modules like *Napoleonic Wars* whose `module.ini` lacks `compatible_multiplayer_version_no = 1170`), stock `apply_filter` (`cmp w20, #0x485`) hides every PC `1170` (1.174) server row.
3. **Query string filtering** — `Internet (Android Only)` sends `gametype=androidonly` (0 bytes from TaleWorlds); plain *Internet* sends `gametype=android` (~156–185 servers) instead of the full PC list (`gametype=warband`, ~228–280 servers).

### External Mod: `mp-module-filter.zip` `v1.1.2` (No APK Patching, No Re-Signing)

Built with the official `remixer-dec/MNBTool/modframework` SDK (`ANDROID_ABI=both` for `arm64` and `armv7`), `v1.1.2` provides:

- **FIFO-head master-list hook (`Network_manager::send_http_request`, `0x001487ec`, `CHAIN`)**: runs before `builtin.multiplayer-master-server-fix` on `0x001d2718` and rewrites `warbandmain.taleworlds.com/handlerservers.ashx?type=list` requests to `https://warbandmain.taleworlds.com/handlerservers.ashx?gametype=warband&type=list`. The swapped query order misses `master-server-fix`'s `?type=` substring check cleanly and routes straight into M&BTool's own `ModDevice.httpsGet()` — working even with only default-on *LAN Multiplayer Fix*. Guarded by `api->is_mod_active()` so raw Tegra `libcurl` never receives `https://`; `chkserialandroid` and custom non-TaleWorlds hosts are left untouched.
- **PC `1170..1174` filter normalization (`0x0013ea98`, `CHAIN`)**: temporarily normalizes PC `1170..1174` server rows (`1157` / `1174` retry, immediately restoring the real version) around stock `apply_filter` so PC 1.174 servers stay visible on Native and 3rd-party modules.
- **Restores "Compatible with module" checkbox (`0x0012f06c`, `0x0012f040`, `0x00122c38`, `CHAIN`)**: places `is_version_compatible_checkbox` (`ui_is_version_compatible`) in PC Warband's exact `game_variables.txt` slot (`x=0.03, y=0.046`), unchecked = all modules, checked = active module only (case-insensitive, respecting the master *"Server filter"* toggle).

**Install**: M&BTool -> **Engine mods** -> **Add** -> **From device (.zip)** -> pick `mp-module-filter.zip`, enable it (keep **LAN Multiplayer Fix** enabled), and restart the game.

> **Real-Device Status (`v1.1.2`) & M&BTool 4.3 ARM64 Join Limitation**:
> - **Verified working on Android ARM64**: loads cleanly, fetches the full PC 1.174 server list (~228–280 servers), renders the **"Compatible with module"** checkbox at `y=0.046`, and filters rows by active module on both Native and *Napoleonic Wars*.
> - **Known M&BTool 4.3 ARM64 bug when clicking Join ([BUG_REPORT.md #1d](BUG_REPORT.md#1d-critical--fixed-in-next-release-arm64-sigsegv-crash-in-libmodlauncherso-when-joining-any-server-with-any-external-mod-enabled) — fixed in next M&BTool release)**: Whenever *any* external (non-bundled/unreviewed) mod is enabled on M&BTool 4.3 ARM64, `libmodlauncher.so`'s `Network_manager::start_client` guard (`0x188df78 -> 0x18970a4`) passes raw 32-bit guest pointer `x2` without `decode_pointer` / `+ x28`, causing an immediate `SIGSEGV (SEGV_MAPERR)` when clicking **Join** on **any** server. `remixer-dec` has fixed this crash for the next M&BTool release and plans to add built-in module filtering as well.

---

## How M&BTool 4.3 Works (Summary)

- **Dual-Process Launcher (`com.taleworlds.mbwarband.m1p`, `minSdk=24`, `targetSdk=29`, `compileSdk=35`)**:
  - **`MainActivity` (main process)**: Full-screen `WebView` serving an encrypted/verified UI bundle (`assets/cache-index`) via `libmodlauncher.so` (`SessionNative`), exposing JS bridges (`GameLauncher`, `GameManager`, `SavedataManager`, `NativeMods`, `Library`, `AiMod`, `MbImages`).
  - **`GameLaunchActivity` -> `GameActivity` (`:game` process, `NativeActivity`)**: Mounts PC Warband files (`main/` + `patch/` from `/sdcard/mnb`, app storage, or SAF custom folder), applies PC compatibility overlays (`assets/resources/pc-base/`), loads built-in mods + bundled/external `assets/mods/*` packages via `libmodlauncher.so` (`ByteHook` + `ShadowHook`), and runs `libMBExpMobile.so` (a statically recompiled ARM64 translation of the 32-bit NVIDIA Shield Tegra Warband 1.158 engine).
- **Certificate-Bound Attestation (`SessionNative`)**:
  - `libmodlauncher.so` cryptographically binds `MainActivity`'s WebView UI (`assets/cache-index`) and `:game` `SessionHost` attestation to the original APK signing certificate (`SHA-256: f5d6bdaa9a19f3c87be2ba5cdbc4e89ec74e9a9dc1b4daf166fd46baa140c026`). A re-signed APK intentionally refuses to load the M&BTool UI (*"no fallback by design"*).
  - **This repository never bypasses attestation**: all launcher & engine findings are documented cleanly in **[BUG_REPORT.md](BUG_REPORT.md)** for upstream integration by `remixer-dec`, and the module-filter mod is built cleanly against M&BTool's official `ModFramework` SDK (`modAbi: 1`).

---

## Key Findings Summary (Full Status Table in [BUG_REPORT.md](BUG_REPORT.md))

1. **Multi-Server `SendHTTP` Chaining (`#1`, investigating upstream)**:
   - Single-server community masters (`http://oreo922.cn`) work via `CustomServer::mod_start`'s direct `main_server_url` overwrite, and omitting TaleWorlds is by design. However, because `builtin.multiplayer-master-server-fix` (`0x19950a4`) is FIFO head on `0x001d2718` and never calls `previous`, `CustomServer::SendHTTP` (`0x19168bc`) is unreachable — breaking multi-server (`>= 2` URLs) fanout and its error logging.
2. **ARM64 Join Crash With Any External Mod (`#1d`, fixed in next release)**:
   - In `libmodlauncher.so`, `multiplayer.untrusted-mods` hooks `Network_manager::start_client` (`0x00143770` -> `0x188df78`) and passed raw 32-bit guest pointer `x2` to `0x1897084` without `decode_pointer` / `+ x28`, immediately faulting at `0x18970a4` (`SIGSEGV`).
3. **Missing "Filter by Module" Checkbox (`#5`, planned as built-in option in next release)**:
   - TaleWorlds compiled out PC Warband's 3rd filter checkbox (`is_version_compatible_checkbox`, `ui_is_version_compatible`) in the Tegra port even though its exact layout (`x=0.03, y=0.046`) remains in `game_variables.txt` lines 1301–1304.
4. **Additional Launcher Fixes Confirmed for Next Release (`BUG_REPORT.md`)**:
   - **#6**: API 24–27 `GET_SIGNING_CERTIFICATES` crash in `e/g.e()`.
   - **#3**: `ModDevice.httpsGet()` `http↔https` redirect handling.
   - **#8**: Destructive SAF rename in `GameFiles.k()` deleting `dst` before validating `src` and parent URIs.
   - **#11**: Missing `smallestScreenSize` in `GameActivity`'s `android:configChanges`.
   - *(Items #9, #10, #12 acknowledged; #1b and #7 verified as not bugs and retracted.)*

---

## Native Mod: `native/mp-module-filter/` (`v1.1.2`)

Clean-room C++17 mod written against the official `remixer-dec/MNBTool/modframework` SDK (`mod_api.h`, `build-mod.sh`, automatic ARM64 bindings via `generate-bindings.py`).

- `native/mp-module-filter/mod.cpp` — `MF_MOD_START` with 5 `MF_HOOK_CHAIN` hooks (`0x12f06c`, `0x12f040`, `0x122c38`, `0x13ea98`, `0x1487ec`), `is_mod_active` HTTPS bridge guard, `1157`/`1174` filter normalization, case-insensitive module comparison, and real `mf_result_v1` error returns.
- `native/mp-module-filter/manifest.json` — `schemaVersion: 2`, `minLauncherVersion: "4.3"`, `version: "1.1.2"`, `modAbi: 1`, `gameBuilds: ["default"]`, `compatibility.loadAfter: ["builtin.custom-server", "builtin.multiplayer-master-server-fix"]`, EN/RU localizations, `default_enabled` toggle, and all 5 hook declarations.
- `native/mp-module-filter/Makefile` — thin wrapper around the official `build-mod.sh`.
- **Upstream Trust Note**: Placing the mod folder under `assets/mods/` in an official M&BTool build automatically marks it **trusted for multiplayer** (`AssetManager.list("mods")` -> `InstalledMod.bundled = true`) with zero Java trust-list changes.

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
