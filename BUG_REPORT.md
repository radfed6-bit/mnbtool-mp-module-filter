# Upstream Technical Bug Report for M&BTool 4.3 (`remixer-dec`)

**Target version**: M&BTool 4.3 (`com.taleworlds.mbwarband.m1p`, `versionCode=2`, `arm64-v8a`)
**Purpose**: Clean technical root-cause report, recommended source-level fixes, and tracked upstream status after feedback from the original author (**remixer-dec**). No proprietary binaries, smali excerpts, or binary patchers are included here.

> **Upstream Response Summary (`2026-11-26`)**: `remixer-dec` reviewed this report and confirmed fixes for **#1d** (ARM64 external-mod join `SIGSEGV`), **#3** (HTTP↔HTTPS redirects), **#6** (Android 7.0–8.1 signing API crash), **#8** (SAF rename guard), and **#11** (`GameActivity` `smallestScreenSize`) in the next M&BTool release, along with a separate fix for ARM64 LAN server search & tab refresh (search thread startup bug). **#5** (module filtering) will be added as a built-in option in the next release, and **#1** (multi-server `SendHTTP` hook shadowing) is being investigated. Independent disassembly/smali re-checks confirmed that **#1b** and **#7** were our misreads — both are retracted below.

---

## Summary Table

| # | Severity | Component | Summary | Upstream Status |
|---|---|---|---|---|
| **1** | High | `builtin.multiplayer-master-server-fix` vs `builtin.custom-server` (`libmodlauncher.so`) | FIFO-first `0x19950a4` on `0x1d2718` never calls `previous`, making `CustomServer::SendHTTP` (`0x19168bc`) unreachable (breaks multi-server `>= 2` URL fanout & `CustomServer` error logging); single-server Oreo works via `main_server_url` overwrite and omitting TaleWorlds is intentional | **Investigating / to be fixed** (single-server Oreo & omitting TaleWorlds: **By design**) |
| **1b** | — | `builtin.custom-server` vs `builtin.resource-overrides` | Initially reported as a startup load-order race; re-verified in `libmodlauncher.so` that `NativeMods_prepare` runs `ResourceOverrides::prepare` (`0x19b6950`) before any `mod_start` | **Retracted (Not a bug)** |
| **1c** | Info | `builtin.custom-server::apply_filter` + stock `apply_filter` (`cmp w20, #0x485`) | Stock filter hides `1170` rows when running in `1157` mode or non-Native modules without `compatible_multiplayer_version_no = 1170` | **By design** |
| **1d** | Critical | `libmodlauncher.so` (`multiplayer.untrusted-mods` guard on `Network_manager::start_client`, ARM64) | `0x188df78 -> 0x18970a4` passes untranslated 32-bit guest pointer `x2` (missing `+ x28` / `decode_pointer`), causing an immediate `SIGSEGV (SEGV_MAPERR)` when clicking **Join** on **any** server while **any** external mod is active | **Fixed in next release** (`MOD` prefix is a temporary placeholder pending future client/server mod negotiation) |
| **1e** | Feature / Compat | `builtin.multiplayer-master-server-fix` / `builtin.custom-server` (PC CD-key `chkserial` auth) | Tegra `libMBExpMobile.so` has only `chkserialandroid` (no PC CD-key code), which `master-server-fix` stubs locally as `"0\|<player_id>"`; public PC 1.174 servers with `use_secure_connection = 1` reject with *"Incorrect serial number"* | **Not planned** |
| **2** | Info | `builtin.custom-server` + `library.json` default override | Cached `library.json` `"servers"` sets the default master server (`http://oreo922.cn`) until the player changes the setting | **By design** |
| **3** | High | `ModDevice.httpsGet()` | Refused `http↔https` redirects; returns `getErrorStream()` bytes on HTTP `>= 400` | **Redirects fixed in next release** (error bodies kept intentionally to match stock game HTTP client) |
| **4** | Info | Engine query string (`&gametype=android*`) & protocol default (`1157` vs `1170`) | Tegra appends `&gametype=android*`; TaleWorlds PC list is intentionally unused | **By design** |
| **5** | Medium | Multiplayer server browser UI (`Multiplayer_client_window`) | Missing "Compatible with module" checkbox (`is_version_compatible_checkbox`); PC layout coordinates still exist in `game_variables.txt` (`y=0.046`) | **Planned as built-in option in next release** |
| **6** | Critical | Session/UI cert lookup on Android 7.0–8.1 (API 24–27) | `e/g.e(Context)` unconditionally uses API-28 `GET_SIGNING_CERTIFICATES` + `PackageInfo.signingInfo` despite `minSdkVersion=24`, breaking M&BTool UI on API 24–27 | **Fixed in next release** |
| **7** | — | `builtin.ai-mod` dialog event handler | Initially reported as no-ops on `read-options` / `end`; re-verified in smali that `choice` speaks chosen options and `end` intentionally lets the current line finish (discarded `c(...)` calls are R8-inlined empty phone-TTS callbacks) | **Retracted (Not a bug)** |
| **8** | High | `GameFiles` SAF custom storage rename (`GameFiles.k`) | Deletes existing destination file **before** validating that source exists and parent URIs are non-null | **Fixed in next release** |
| **9** | Medium | Cross-process `SharedPreferences` & `GameActivity.onDestroy()` | 7 input-saving sites in `GameManager` open `"game_manager"` with `MODE_PRIVATE` instead of `Z2.a0()`; `mod_start_failure.edit().clear().apply()` races `Process.killProcess()` in `onDestroy()` | **Acknowledged (unchanged for now)** |
| **10** | Medium | External savedata migration (`AbstractC0111w0`) | Storage-switch migration silently swallows exceptions (`catch (Throwable) {}`); symlink-failure rollback moves all pre-existing external saves back into internal storage | **Acknowledged (unchanged for now)** |
| **11** | Low/Med | `AndroidManifest.xml` (`GameActivity`) | Missing `smallestScreenSize` in `android:configChanges` (present on `MainActivity`), recreating `GameActivity` and killing `:game` on foldable/multi-window resize | **Fixed in next release** |
| **12** | Low | Assets (`rgl_config.txt`, `en/ui.csv`, `fonts/th/`) | Duplicate `enable_blood = 1` (L15 & L43); `en/ui.csv` L742 still says `"Android games only"`; `assets/resources/fonts/th/` bundles `OFL-Vazirmatn.txt` | **Acknowledged (unchanged for now)** |

---

## Detailed Findings & Upstream Status

### 1. [High — Investigating] `builtin.multiplayer-master-server-fix` Shadows `CustomServer::SendHTTP` (`0x19168bc`) in FIFO `MF_HOOK_CHAIN`
- **Upstream Status**: **Investigating / to be fixed** (single-server community master `http://oreo922.cn` works and omitting TaleWorlds is **By design**).
- **Clarification after re-verification (`libmodlauncher.so`)**:
  - Single-server custom masters (such as Oreo's `http://oreo922.cn/`) **do work** out of the box: `CustomServer::mod_start` (`0x19167bc..0x19167c8`) overwrites Warband's `main_server_url` global directly with `servers[0]`. When Warband sends `http://oreo922.cn/handlerservers.ashx?...` to `Request::SendHTTP` (`0x001d2718`), `builtin.multiplayer-master-server-fix` (`0x19950a4`) checks `strstr(url, "warbandmain.taleworlds.com/handlerservers.ashx?type=")` (`0x19950fc`) -> `NULL` -> falls through to `ModDevice.httpsGet()` (`0x1995130..0x1995150`).
  - `remixer-dec` confirmed that blocking the official TaleWorlds master server in `master-server-fix` is **intentional** (Android master is tied to Google Play; PC list is mostly spam servers).
- **Remaining Confirmed Bug (Multi-Server Fanout & Downstream `0x1d2718` Hooks Unreachable)**:
  - `builtin.custom-server` lists `builtin.multiplayer-master-server-fix` in `requires`, so `master-server-fix` installs `0x19950a4` on `0x001d2718` **first** and `custom-server` installs `0x19168bc` **second** (both `MF_HOOK_CHAIN`).
  - `libmodlauncher.so` (`0x18981b4`, `0x1898384`) chains `MF_HOOK_CHAIN` strictly **FIFO** (`caller -> hook1 -> *hook1_prev -> hook2 -> orig`).
  - Because `0x19950a4` **never calls its `previous` trampoline** (all paths return `0` at `0x19951f8..0x1995214`), **`CustomServer::SendHTTP` (`0x19168bc..0x1917920`) is 100% unreachable dead code** whenever `master-server-fix` is active.
  - **Consequence**: Configuring `>= 2` master server URLs in **Custom Server** never executes `CustomServer::SendHTTP`'s multi-server fanout/merging or `"master server failed: %s"` (`0x191729c`) error logging, and no downstream mod can chain `0x001d2718` via `MF_HOOK_CHAIN`.
- **Recommended Upstream Fix**: Chain to `previous` from `master-server-fix` (`0x19950a4`) when `builtin.custom-server` (or another `0x1d2718` hook) is installed, or consolidate `SendHTTP` handling so multi-server fanout in `0x19168bc` is reachable.

### 1b. [Retracted — Not a Bug] `CustomServer` vs `ResourceOverrides` Startup Order
- **Upstream Status**: **Retracted (Not a bug)** — `remixer-dec` was 100% right.
- **Disassembly Re-Verification**:
  - `NativeMods_prepare` (`0x187fb04 -> 0x188b148`) executes every built-in's `prepare` callback (`[+0x10]`) **before** `NativeMods_onGameLibrariesLoaded` (`0x1880168 -> 0x188cbb8`) executes any `mod_start` (`[+0x18]`).
  - `ResourceOverrides` (`0x19b68e4`) registers `prepare = 0x19b6950`, which sets `g_archive_ready = 1` (`[0x1b7c8b8]`) before `CustomServer::mod_start` runs.
  - With `start-host-version=true`, `host-version=1.174`, and Native, `CustomServer::mod_start` (`0x19165b8..0x1916634`) does switch to `1170` (`0x492`) and calls `0x19194ac(1170)`. The `version 1157` in `I/CustomServer` logs prints `[0x1b216a0]` (`g_module_version`), not `[0x1b216a8]` (`g_current_version`).

### 1c. [Info — By Design] Stock `apply_filter` Hides `1170` Servers When Running in `1157` Mode
- **Upstream Status**: **By design** (TaleWorlds PC master list is intentionally unused; non-Native PC 1.174 modules can declare `compatible_multiplayer_version_no = 1170` in `module.ini`).
- **Behavior Note**: `Multiplayer_client_window::apply_filter(rglTable_row*)` (`0x0013ea98`) compares `mb_version_no` (`cells[2]+0x2cc`) against the active engine version (`cmp w20, #0x485` in `1157` mode), hiding `1170` rows on non-Native modules unless `compatible_multiplayer_version_no = 1170` is set.

### 1d. [Critical — Fixed in Next Release] ARM64 `SIGSEGV` Crash in `libmodlauncher.so` When Joining Any Server With Any External Mod Enabled
- **Upstream Status**: **Fixed in next release** (`remixer-dec` confirmed the ARM64 crash is fixed; also clarified the `"MOD"` server-name prefix is a temporary placeholder pending future client/server mod negotiation).
- **Symptom**: On M&BTool 4.3 ARM64, whenever **any** external (non-bundled/unreviewed) mod is enabled (`multiplayer.untrusted-mods=true`), clicking **Join** on **any** server — even a `"MOD..."` server — immediately crashed `:game` with `SIGSEGV (SEGV_MAPERR)` before the `"MOD"` prefix check or clean `ModDevice.restartForMultiplayer()` (`trustedOnly=true`) could execute.
- **Evidence** (identical across 3 live Android ARM64 crash tombstones: `PID 936`, `PID 5630`, `PID 21171`, `Thread-3`):
  ```text
  signal 11 (SIGSEGV), code 1 (SEGV_MAPERR), fault addr 0x0000000018b6e0dc
  x0  0000000018b6e0d0  x1  0000000018b6e160  x2  0000000018b6e0d0  x3  0000000018b6e0cc
  x10 0000000010125380  (guest PC = libMBExpMobile.so + 0x125380)
  x27 0000000018b6d7c8  (guest SP)
  x28 00000075fd740000  (guest window base)
  backtrace:
    #00 pc 00000000018970a4  libmodlauncher.so
    #01 pc 000000000188dfa4  libmodlauncher.so
    #02 pc 00000000018858e0  libmodlauncher.so
  ```
- **Root Cause**:
  1. When `multiplayer.untrusted-mods=true` (`C0096t0.java:584`), `libmodlauncher.so` hooks `Network_manager::start_client` (`0x00143770` -> `0x188df78`) and `Network_manager::start_server` (`0x00142330` -> `0x188dff4`).
  2. At `0x188df90..0x188dfa4` (`mov x0, x2; bl 0x1897084`), `0x188df78` passed the raw 32-bit guest stack pointer `x2` (`const rglString&` server name) directly to `0x1897084` **without translating it via `decode_pointer` or adding `x28`**.
  3. At `#00 0x18970a4`: `ldr w8, [x0, #0xc]` dereferenced `0x18b6e0d0 + 0xc = 0x18b6e0dc` -> immediate `SIGSEGV (SEGV_MAPERR)`.
- **Fix & Design Note**:
  1. **Crash fix (confirmed for next release)**: Translate `x2` via `decode_pointer(x2, sizeof(rglString), MF_MEMORY_READ)` inside `0x188df78` before calling `0x1897084`.
  2. **Policy feedback (`multiplayer.untrusted-mods`)**: `remixer-dec` noted that `"MOD"`-prefixed servers allow external mods and that proper client/server mod negotiation is a future goal. For joining third-party public servers whose admins will not rename them to `MOD...`, an opt-in setting (disabled by default, with a warning) allowing local UI/QoL external mods on regular servers would also be helpful.

### 1e. [Feature Request — Not Planned] Optional Legitimate PC CD-Key (`chkserial`) Support for Public PC 1.174 Servers
- **Upstream Status**: **Not planned** (`remixer-dec` declined).
- **Technical Summary**: The Tegra Android engine (`libMBExpMobile.so`) contains only Google Play LVL auth (`chkserialandroid`) and zero PC CD-key (`serial_key` / `chkserial`) code. `builtin.multiplayer-master-server-fix` (`0x1994b54..0x1994bc0`, `0x199511c`) intercepts `chkserialandroid` locally and returns `"0|<multiplayer.player_id>"`, which allows LAN and unsecured (`use_secure_connection = 0`) servers while `use_secure_connection = 1` public PC 1.174 servers reject with *"Incorrect serial number"*. Supporting legitimate user-supplied PC keys via TaleWorlds' HTTPS `chkserial` endpoint (`mb_warband.exe` `0x407ae0..0x407d68`) is not planned upstream.

### 2. [Info — By Design] `library.json` Default Master Server (`http://oreo922.cn`)
- **Upstream Status**: **By design**. `remixer-dec` confirmed `library.json` `"servers"` intentionally sets the default community master server (`http://oreo922.cn`) until the player configures a custom address in **Custom Server**.

### 3. [High — Redirects Fixed in Next Release] `ModDevice.httpsGet()` Cross-Scheme Redirects & Error Bodies
- **Upstream Status**: **Redirects fixed in next release**; returning HTTP 4xx/5xx error streams as response bytes is kept **intentionally** to match the stock game's HTTP client behavior.
- **Root Cause**: Android `HttpURLConnection`'s default redirect handling refused `http↔https` (`301`/`302`/`303`/`307`/`308`) cross-protocol redirects.

### 4. [Info — By Design] Android Query Filter (`&gametype=android*`) & Protocol Default
- **Upstream Status**: **By design** (the TaleWorlds master list is intentionally unused by M&BTool).

### 5. [Medium — Planned in Next Release] Missing "Filter by Module" Checkbox in Multiplayer Server Browser
- **Upstream Status**: **Planned as a built-in option in the next release** (`remixer-dec` confirmed adding module filtering as an option).
- **Root Cause**: TaleWorlds compiled PC Warband's `is_version_compatible_checkbox` (`ui_is_version_compatible`) out of the Tegra binary (`libMBExpMobile.so`) even though its layout coordinates (`x=0.03, y=0.046`) remain in `game_variables.txt` lines 1301–1304, leaving a blank gap between *Has players* (`y=0.071`) and *No password* (`y=0.021`).
- **Community Mod Reference (`native/mp-module-filter/`, `v1.1.2`)**: Clean-room C++17 ModFramework mod (`schemaVersion: 2`, `ANDROID_ABI=both`) verified working on real Android ARM64 (`PID 30424`, `936`, `5630`, `6815`) for both Native and Napoleonic Wars. *(Note: v1.0.0 had failed with `mod_start returned -4` due to overlapping `original`/`previous_core` slots; fixed by migrating to the official `remixer-dec/MNBTool/modframework` SDK.)*

### 6. [Critical — Fixed in Next Release] Total M&BTool UI Failure on Android 7.0–8.1 (API 24–27)
- **Upstream Status**: **Confirmed & fixed in next release** (affects Android 7.0–8.1 only).
- **Root Cause**: `AndroidManifest.xml` declares `minSdkVersion=24`, yet `e/g.e(Context)` unconditionally passed `PackageManager.GET_SIGNING_CERTIFICATES` (`0x08000000`, API 28+) and read `PackageInfo.signingInfo` without guarding `Build.VERSION.SDK_INT >= 28` (unlike `e/g.g()` and `SessionHost.b()`).

### 7. [Retracted — Not a Bug] `builtin.ai-mod` `read-options` and Dialog `"end"` Events
- **Upstream Status**: **Retracted (Not a bug)** — `remixer-dec` was right.
- **Smali Re-Verification (`smali/d/b.smali`, `f/C0134c.java`)**:
  - `"read-options"` is `"Read chosen options"` (*"The hero says the option you choose"*), which is spoken via the `"choice"` event (`d/b.smali` L3024–3099 -> `d/p.d(choiceText, speakerVoice, /*flush=*/true)`).
  - `"end"` intentionally lets the current utterance finish speaking.
  - The discarded `c(player)` (L2975) and `c(s.i)` (L3016) calls were simply R8-inlined empty `{}` no-op methods on the phone-TTS backend (`d/p`) for callbacks used by the separate neural `ai-tts` backend.

### 8. [High — Fixed in Next Release] Destructive Non-Atomic SAF Rename in `GameFiles.k()`
- **Upstream Status**: **Confirmed & fixed in next release**.
- **Root Cause**: When renaming/moving a file on SAF custom storage (`GameFiles.k(src, dst)`), if `dst` already existed as a file, it deleted `dst` via `GameFiles.c(dst)` **before** validating that `src` existed (`f(src) != null`) or that parent document URIs (`j(src)`, `j(dst)`) were non-null.

### 9. [Medium — Acknowledged] Cross-Process `SharedPreferences` Inconsistency & `onDestroy()` `killProcess()` Race
- **Upstream Status**: **Acknowledged (unchanged for now)**.
- **Root Cause**: Seven input-saving methods in `C0067n0` (`Y`, `Z`, `a`, `b0`, `c0`, `i0`, `j0`) open `"game_manager"` with `MODE_PRIVATE` while the rest use `Z2.a0()` (`MODE_MULTI_PROCESS`). `GameActivity` clears `mod_start_failure` via asynchronous `.apply()`, while `onDestroy()` calls `Process.killProcess(Process.myPid())` right after `super.onDestroy()`.

### 10. [Medium — Acknowledged] External Savedata Migration Edge Cases (`AbstractC0111w0`)
- **Upstream Status**: **Acknowledged (unchanged for now)**.
- **Root Cause**: `AbstractC0111w0.k()` catches and ignores exceptions (`catch (Throwable) {}`), and rollback on `Os.symlink()` failure in `AbstractC0111w0.e()` moves all pre-existing external saves into internal storage and deletes `<root>/savedata`.

### 11. [Low/Med — Fixed in Next Release] `GameActivity` Missing `smallestScreenSize` in `AndroidManifest.xml`
- **Upstream Status**: **Confirmed & fixed in next release**.
- **Root Cause**: `MainActivity` declared `smallestScreenSize` in `android:configChanges`, while `GameActivity` omitted it, causing Android to recreate `GameActivity` and kill `:game` on foldable/multi-window resizes crossing `smallestScreenSize`.

### 12. [Low — Acknowledged] Minor Asset & Config Cleanups
- **Upstream Status**: **Acknowledged (unchanged for now)**.
- `assets/config/rgl_config.txt`: Duplicate `enable_blood = 1` on lines 15 and 43.
- `assets/resources/fonts/th/`: Contains `OFL-Vazirmatn.txt` alongside Thai Noto Sans (`OFL.txt` / `README.md`).
- `c/b.java`: Cold-start Cloudflare Web Analytics RUM beacon (`https://cloudflareinsights.com/cdn-cgi/rum`) has no user opt-out toggle.

---

### Bonus Upstream Fix Noted by `remixer-dec`
- **ARM64 LAN server search & tab refresh**: Fixed in the next release (caused by a search-thread startup bug on ARM64).
