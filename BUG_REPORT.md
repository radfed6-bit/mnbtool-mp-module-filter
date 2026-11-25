# Upstream Technical Bug Report for M&BTool 4.3 (`remixer-dec`)

**Target version**: M&BTool 4.3 (`com.taleworlds.mbwarband.m1p`, `versionCode=2`, `arm64-v8a`)
**Purpose**: Clean technical root-cause report and recommended source-level fixes for the original author (**remixer-dec**) and M&BTool contributors. No proprietary binaries, smali excerpts, or binary patchers are included here.

---

## Summary Table

| # | Severity | Component | Summary |
|---|---|---|---|
| **1** | Critical | `builtin.multiplayer-master-server-fix` (`libmodlauncher.so`) | FIFO-first hook on `Request::SendHTTP` (`0x001d2718`) blackholes non-`chkserialandroid` requests matching `warbandmain.taleworlds.com/handlerservers.ashx?type=` (both `http://` and `https://`) with `""` and never calls `previous` -> **0 servers** |
| **1b** | Critical | `builtin.custom-server::mod_start()` vs `builtin.resource-overrides::mod_start()` (startup load order) | `CustomServer` checks `ResourceOverrides::g_archive_ready` ~42 ms **before** `ResourceOverrides` runs, so `Start in host version = true` + `Host version = 1.174` **always** silently fails and the game boots in protocol `1157` |
| **1c** | High | `builtin.custom-server::apply_filter` + stock `apply_filter` (`cmp w20, #0x485`) | While booted in `1157`, every PC `1170` (1.174) server is hidden by the stock filter unless `g_is_native_1157 && g_archive_ready`; true for non-Native modules (e.g. Napoleonic Wars) and, due to 1b, for Native too |
| **1d** | Critical | `libmodlauncher.so` (`multiplayer.untrusted-mods` guard on `Network_manager::start_client`, ARM64) | `0x188df78 -> 0x18970a4` passes untranslated 32-bit guest pointer `x2` (missing `+ x28` / `decode_pointer`), causing an immediate `SIGSEGV (SEGV_MAPERR)` when clicking **Join** on **any** server while **any** external (untrusted) mod is active |
| **1e** | Feature / Compat | `builtin.multiplayer-master-server-fix` / `builtin.custom-server` (PC CD-key `chkserial` auth) | Tegra `libMBExpMobile.so` has only `chkserialandroid` (no PC CD-key code), which `master-server-fix` stubs locally as `"0\|<player_id>"`; public PC 1.174 servers with `use_secure_connection = 1` reject with *"Incorrect serial number"* — optional user-supplied legitimate PC key (`chkserial`) support is needed |
| **2** | High | `builtin.custom-server` + `library.json` default override | Cached `library.json` `"servers"` silently replaces TaleWorlds default with `http://oreo922.cn` (2 entries, 4–6s latency) |
| **3** | High | `ModDevice.httpsGet()` | Reads `getErrorStream()` on HTTP `>= 400` and returns HTML error pages as valid `byte[]`; does not follow `http↔https` redirects; sends raw `Dalvik` UA |
| **4** | Medium | Engine query string (`&gametype=android*`) & protocol default (`1157` vs `1170`) | `&gametype=android` omits PC servers (~156–185 vs ~228–280; `&gametype=androidonly` returns 0 bytes); default `host-version="1.158"` / `start-host-version=false` keeps Native on `1157` when `module.ini` lacks `compatible_multiplayer_version_no` |
| **5** | Medium | Multiplayer server browser UI (`Multiplayer_client_window`) | Missing "Compatible with module" checkbox (`is_version_compatible_checkbox`); PC layout coordinates still exist in `game_variables.txt` (`y=0.046`) |
| **6** | Critical | Session/UI cert lookup on Android 7.0–8.1 (API 24–27) | `e/g.e(Context)` unconditionally uses API-28 `GET_SIGNING_CERTIFICATES` + `PackageInfo.signingInfo` despite `minSdkVersion=24`, breaking M&BTool UI on API 24–27 |
| **7** | High | `builtin.ai-mod` dialog event handler | `"read-options"` resolves player voice and discards it without speaking; `"end"` resolves narrator voice and discards it without stopping TTS |
| **8** | High | `GameFiles` SAF custom storage rename (`GameFiles.k`) | Deletes existing destination file **before** validating that source exists and parent URIs are non-null |
| **9** | Medium | Cross-process `SharedPreferences` & `GameActivity.onDestroy()` | 7 input-saving sites in `GameManager` open `"game_manager"` with `MODE_PRIVATE` instead of `Z2.a0()`; `mod_start_failure.edit().clear().apply()` races `Process.killProcess()` in `onDestroy()` |
| **10** | Medium | External savedata migration (`AbstractC0111w0`) | Storage-switch migration silently swallows exceptions (`catch (Throwable) {}`); symlink-failure rollback moves all pre-existing external saves back into internal storage |
| **11** | Low/Med | `AndroidManifest.xml` (`GameActivity`) | Missing `smallestScreenSize` in `android:configChanges` (present on `MainActivity`), recreating `GameActivity` and killing `:game` on foldable/multi-window resize |
| **12** | Low | Assets (`rgl_config.txt`, `en/ui.csv`, `fonts/th/`) | Duplicate `enable_blood = 1` (L15 & L43); `en/ui.csv` L742 still says `"Android games only"`; `assets/resources/fonts/th/` bundles `OFL-Vazirmatn.txt` |

---

## Detailed Findings & Recommended Upstream Fixes

### 1. [Critical] `builtin.multiplayer-master-server-fix` Drops `warbandmain.taleworlds.com` Server List Queries (FIFO `MF_HOOK_CHAIN` Blocking)
- **Symptom**: Selecting **Internet** in Multiplayer with TaleWorlds as the master server (whether `http://` or `https://`) yields **0 servers** with zero DNS/HTTP requests made by `:game`.
- **Root Cause**:
  - In `libmodlauncher.so` (`0x1898368..0x1898390`), `MF_HOOK_CHAIN` executes chained hooks in **strict FIFO order** (`stlr x9, [x8]` at `0x1898384`).
  - `builtin.multiplayer-master-server-fix` (`0x019950a4`) installs **first** on guest `Request::SendHTTP` (`0x001d2718`), ahead of `builtin.custom-server` (`0x019168bc`) and any external mod hooking `0x001d2718`.
  - Inside `master_fix` (`0x019950a4..0x0199520c`), it **never invokes `previous`**:
    1. It checks `strstr(url, "warbandmain.taleworlds.com/handlerservers.ashx?type=")` (`0x019950f0..0x019950fc`), which matches **both** `http://` and `https://`.
    2. Non-matching URLs branch to `0x01995130` -> Java `ModDevice.httpsGet()` (`0x01995150`).
    3. Matching URLs check `strstr(url, "chkserialandroid")`. Whenever `chkserialandroid` is absent (such as `type=list&gametype=android`, `type=list`, `type=ping`), `0x01995118: cbz x0, 0x19951f8` jumps straight to the epilogue returning `0` (HTTP success) with an empty response (`""`) — never calling `ModDevice.httpsGet()` and never invoking `builtin.custom-server` or later hooks on `0x001d2718`.
- **Recommended Upstream Fix**: Route non-`chkserialandroid` `warbandmain.taleworlds.com` requests to `ModDevice.httpsGet()` (`0x01995130`, upgrading `http://` -> `https://` because plain `http://` returns Cloudflare HTTP 403) instead of returning an empty string, or chain to `previous` when `builtin.custom-server` is active.

### 1b. [Critical] Startup Load-Order Bug: `Start in host version` Never Takes Effect
- **Symptom**: Even with `builtin.custom-server` configured to `Host version = 1.174` and `Start in host version = true`, the game still starts in protocol `1157`.
- **Evidence** (real-device logcat, `Nov 25 00:35:33`, `:game` process):
  - `00:35:33.246 I/CustomServer: master servers: 1, hosting on http://warbandmain.taleworlds.com/; version 1157`
  - `00:35:33.288 I/ResourceOverrides: layer files=122 base module=Native stacked=0 archive files=0`
  - Confirmed again in `PID 16402` (`17:18:28Z`): `CustomServer` logged `version 1157 (1.174 profile when the archive is readable)` immediately before `ResourceOverrides` initialized `archive files=24`.
- **Root Cause**: `builtin.custom-server::mod_start()` runs **before** `builtin.resource-overrides::mod_start()` and gates switching to `1170` (`0x492`, patching the six `0x485` sites including `0x0013eb18`) on `ResourceOverrides::g_archive_ready` (`[0x1b7c8b8]`). Because `ResourceOverrides` has not started yet, `[0x1b7c8b8]` is always `0` during `CustomServer::mod_start()`, so the game always boots in `1157`.
- **Recommended Upstream Fix**: Start `builtin.resource-overrides` before `builtin.custom-server` (add it to `custom-server`'s `loadAfter` or re-evaluate the `1170` switch lazily after all built-ins start / on first server-list open).

### 1c. [High] Stock `apply_filter` Hides All `1170` Servers While the Engine Runs `1157`
- **Root Cause**: `Multiplayer_client_window::apply_filter(rglTable_row*)` (`0x0013ea98`) compares each server row's `mb_version_no` (`cells[2]+0x2cc`) against the engine version (`0x6cc730: cmp w20, #0x485`). `CustomServer::apply_filter` (`0x01917cbc..0x01917d68`) only spoofs `1170 -> 1157` for rows matching the active module when `g_is_native_1157 == 1 && ResourceOverrides::g_archive_ready == 1`; otherwise it delegates to stock `apply_filter`, hiding every `1170` (PC 1.174) server. `g_is_native_1157` is `0` for any non-Native module (e.g., *Napoleonic Wars*), and `g_archive_ready` is `0` at startup due to Bug #1b.
- **Related UI note**: The second list type is `Internet (Android Only)` (`gametype=androidonly`), which TaleWorlds answers with 0 bytes (0 servers); use plain *Internet*.
- **Recommended Upstream Fix**: Normalize matching PC `1170..1174` rows around the stock filter call regardless of module and without gating on startup-time `g_archive_ready`.

### 1d. [Critical] ARM64 `SIGSEGV` Crash in `libmodlauncher.so` When Joining Any Server With Any External Mod Enabled
- **Symptom**: On M&BTool 4.3 ARM64, whenever **any** external (non-bundled/unreviewed) mod is enabled (`multiplayer.untrusted-mods=true`), clicking **Join** on **any** server — even a `"MOD..."` server — immediately crashes `:game` with `SIGSEGV (SEGV_MAPERR)` before the `"MOD"` prefix check or clean `ModDevice.restartForMultiplayer()` (`trustedOnly=true`) can execute.
- **Evidence** (100% identical across 2 live Android ARM64 crash tombstones, `PID 936` @ `17:43:36Z` and `PID 5630` @ `17:44:59Z`, `Thread-3`):
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
  2. At `0x188df90..0x188dfa4` (`mov x0, x2; bl 0x1897084`), `0x188df78` passes the raw 32-bit guest stack pointer `x2` (`const rglString&` server name, e.g. `0x18b6e0d0`) directly to `0x1897084` **without translating it via `decode_pointer` or adding the `x28` guest window base**. *(By contrast, `0x188dff4` `start_server` uses a pre-translated host pointer from `resolve_game_address`, and `0x18970b0..0x18970ec` does call `decode_pointer` on the inner `rglString` char buffer — translating `x2` itself in `0x188df78` was missed.)*
  3. At `#00 0x18970a4`: `ldr w8, [x0, #0xc]` reads `rglString::length` directly at `0x18b6e0d0 + 0xc = 0x18b6e0dc` -> immediate `SIGSEGV (SEGV_MAPERR)`.
- **Recommended Upstream Fix**: Translate `x2` via `decode_pointer(x2, sizeof(rglString), MF_MEMORY_READ)` (or add the guest window base) inside `0x188df78` before calling `0x1897084`.

### 1e. [Feature Request / Auth Compatibility] Optional Legitimate PC CD-Key (`chkserial`) Support for Public PC 1.174 Servers
- **Symptom**: Even after server list discovery and `1170` protocol compatibility are restored, joining standard public PC 1.174 servers running with `use_secure_connection = 1` fails with **"Incorrect serial number"**, while LAN and unsecured (`use_secure_connection = 0`) servers work.
- **Root Cause (`orig_libMBExpMobile.so` vs `mb_warband.exe` vs `libmodlauncher.so`)**:
  - The Tegra Android engine (`libMBExpMobile.so`) contains **only** Google Play LVL authentication (`chkserialandroid` with `&response=`, `&responsesigned=`, `&sign=`, `&ip=`, `&gametype=android&rand=` / `&gametype=nw`) and has zero PC CD-key (`serial_key` / `chkserial` / `ui_serial_key_caption`) code.
  - `builtin.multiplayer-master-server-fix` (`libmodlauncher.so` `0x1994b54..0x1994bc0`, `0x199511c`) unconditionally intercepts `chkserialandroid` locally and returns `"0|<multiplayer.player_id>"` (default `"0|1"`) without contacting TaleWorlds, and there is no setting or config key to supply a PC serial key or disable the local stub.
  - By contrast, PC Warband (`mb_warband.exe` `0x407ae0..0x407d68`) reads the player's legitimate CD-key (`serial_key`, `serial_key_nw`, `serial_key_vc`) and validates the session against TaleWorlds via `handlerservers.ashx?type=chkserial&serial=<KEY>&ip=<SERVER_IP>&rand=<RAND>` (plus `&gametype=nw` / `&gametype=vc`), returning `<status>|<unique_id>` (`0|0` on failure). Because the local `"0|<player_id>"` stub never registers a real key with TaleWorlds, `use_secure_connection = 1` servers always reject the join.
- **Recommended Upstream Enhancement**:
  - Add optional user-supplied **legitimate PC CD-key** settings (`serial_key`, `serial_key_nw`, `serial_key_vc`) in built-in `builtin.custom-server` (or `builtin.multiplayer-master-server-fix`).
  - When a player configures their own legitimate PC key, translate outgoing `chkserialandroid` requests into PC Warband's official HTTPS validation call (`https://warbandmain.taleworlds.com/handlerservers.ashx?type=chkserial&serial=<KEY>&ip=<SERVER_IP>&rand=<RAND>` plus `&gametype=nw` / `&gametype=vc` as applicable) via `ModDevice.httpsGet()`, and pass TaleWorlds' genuine `<status>|<unique_id>` response back to the engine (falling back to the existing `"0|<player_id>"` stub only when no key is configured).
  - *Scope note*: This enhancement is strictly for players authenticating with their **own legitimate purchased PC Warband / DLC serial keys** — not for bypassing or forging license verification. Because external mods cannot join `use_secure_connection = 1` public servers under M&BTool's `multiplayer.untrusted-mods` policy (and currently crash via Bug #1d on ARM64), this needs to live upstream in a built-in (or officially trusted) mod.

### 2. [High] `library.json` Silently Overrides Default TaleWorlds Master Server with `http://oreo922.cn`
- **Symptom**: Once the launcher fetches `https://remixer-dec.github.io/MNBTool/library.json`, `C0031g.c()` silently replaces `builtin.custom-server`'s default master server with `http://oreo922.cn`.
- **Verification (2026-11-24 / 2026-11-25)**:
  - `http://oreo922.cn/handlerservers.ashx?type=list` (Python `BaseHTTP/0.6`, HTTP/1.0, 4–6s latency) returns only **2 servers** (`41 B`, with trailing `|`).
  - Plain `http://warbandmain.taleworlds.com/handlerservers.ashx?type=list` returns **HTTP 403 Forbidden** (Cloudflare), while `https://warbandmain.taleworlds.com/handlerservers.ashx?gametype=warband&type=list` (and `?type=list`) returns **HTTP 200** directly with **~228–280 servers** (`~5.1 KB`, `~1.6s`) even with Android's `Dalvik/2.1.0` User-Agent.
- **Recommended Upstream Fix**: Do not overwrite `builtin.custom-server`'s default `server` setting from `library.json` `"servers"`. Present `library.json` servers only as selectable presets in the UI while keeping `https://warbandmain.taleworlds.com/` as the default.

### 3. [High] `ModDevice.httpsGet()` Returns HTTP Error Bodies as Valid Responses & Drops Cross-Scheme Redirects
- **Root Cause**:
  1. When `httpURLConnection.getResponseCode() >= 400`, `ModDevice.httpsGet()` reads `httpURLConnection.getErrorStream()` and returns the raw HTML error body (e.g., 403/404/502/Cloudflare page) as a non-null `byte[]`. Native callers treat non-null bytes as a successful HTTP response.
  2. Relies on Android `HttpURLConnection`'s default redirect handling, which refuses HTTP↔HTTPS (`301`/`302`/`303`/`307`/`308`) cross-protocol redirects.
  3. Sends default `Dalvik/...` User-Agent header.
- **Recommended Upstream Fix**:
  - Return `null` for any non-2xx HTTP status (`code < 200 || code >= 300`).
  - Disable `instanceFollowRedirects` and manually follow up to 5 `301`/`302`/`303`/`307`/`308` redirects across `http↔https` via `URL(currentUrl, location)`.
  - Set an explicit `User-Agent` header (e.g., `Mozilla/5.0 (Linux; Android) MBTool/4.3`).

### 4. [Medium] Android Query Filter (`&gametype=android*`) & PC 1.174 Protocol Default
- **Root Cause**:
  - The Tegra engine appends `&gametype=android` (~156–185 servers, missing ~70–100 PC servers) on **Internet** and `&gametype=androidonly` (0 bytes) on **Internet (Android Only)**.
  - `builtin.custom-server` defaults to `host-version="1.158"` and `start-host-version="false"`. Because PC Warband's `Modules/Native/module.ini` omits `compatible_multiplayer_version_no`, `Z2.b0()` defaults to protocol `1157` instead of PC 1.174's `1170`.
- **Recommended Upstream Fix**:
  - Replace `&gametype=android` / `&gametype=androidonly` with `gametype=warband` (or strip them) when querying `warbandmain.taleworlds.com/handlerservers.ashx` so all ~228–280 PC servers are returned.
  - Default `host-version` to `"1.174"` and `start-host-version` to `"true"` (once Bug #1b is fixed).
  - Ensure `assets/multiplayer/native-1174.zip` remains stored uncompressed (`STORED`) in release APKs so `AssetManager.openFd()` succeeds (`custom_server` hides 1.174 Native servers if `native-1174.zip` is unreadable).

### 5. [Medium] Missing "Filter by Module" Checkbox in Multiplayer Server Browser
- **Root Cause**:
  - In PC Warband, `Multiplayer_client_window::activate()` creates `is_version_compatible_checkbox` (`ui_is_version_compatible`) inside `server_filter_panel` at `x=0.03, y=0.046` (coordinates still present in `game_variables.txt` lines 1301–1304, leaving a blank gap between *Has players* at `y=0.071` and *No password* at `y=0.021`).
  - TaleWorlds compiled this checkbox out of the Tegra binary (`libMBExpMobile.so`) and replaced it with hardcoded `module_name != "Native"` (`0x0013eaec`). Once `builtin.custom-server` patched out `0x0013eaec` to support 3rd-party modules, users lost the ability to filter servers by their active module.
- **Recommended Upstream Fix**:
  - Bundle [`native/mp-module-filter/`](native/mp-module-filter/) (clean-room C++17 mod built with the official `remixer-dec/MNBTool/modframework` SDK, `schemaVersion: 2`, `ANDROID_ABI=both`) under `assets/mods/mp-module-filter/` (automatically trusted for multiplayer via `AssetManager.list("mods")` -> `InstalledMod.bundled = true`) or integrate it into `builtin.custom-server`.
  - **Real-device verification (`v1.1.2`)**: Confirmed working on Android ARM64 (`PID 30424`, `936`, `5630`): loads cleanly, fetches the full PC 1.174 server list via FIFO-head hook `Network_manager::send_http_request` (`0x001487ec`) -> `https://warbandmain.taleworlds.com/handlerservers.ashx?gametype=warband&type=list`, normalizes `1170..1174` rows (`1157`/`1174` retry), renders the checkbox at `y=0.046`, and filters rows by active module. *(Note: v1.0.0 had failed with `mod_start returned -4` due to overlapping `original`/`previous_core` hook slots rejected by `libmodlauncher.so` and masked as `-4`, fixed by migrating to the official SDK; joining servers while installed as an external untrusted mod is blocked/crashed by upstream Bug #1d until fixed or bundled upstream.)*
  - Update `assets/localization-stock/patch/languages/en/ui.csv` line 742 from `ui_is_version_compatible|Android games only` to `ui_is_version_compatible|Compatible with module` (`ru/ui.csv` line 742 already reads `Совместимость модуля и версии`).

### 6. [Critical] Total M&BTool UI Failure on Android 7.0–8.1 (API 24–27)
- **Root Cause**: `AndroidManifest.xml` declares `minSdkVersion=24`, yet the UI session verifier (`e/g.e(Context)`) unconditionally passes `PackageManager.GET_SIGNING_CERTIFICATES` (`0x08000000`, API 28+) and reads `PackageInfo.signingInfo` without checking `Build.VERSION.SDK_INT >= 28` (unlike `e/g.g()` and `SessionHost.b()`, which properly fall back to `GET_SIGNATURES` / `0x40`). On API 24–27, this catches an exception, passes 32 zero bytes to `SessionNative.onConfigurationChanged()`, and causes `MainActivity` to fail with no fallback.
- **Recommended Upstream Fix**: Guard with `if (Build.VERSION.SDK_INT >= 28)` and fall back to `PackageManager.GET_SIGNATURES` + `packageInfo.signatures` on API 24–27 (matching `SessionHost.b()`).

### 7. [High] `builtin.ai-mod` `read-options` and Dialog `"end"` Events Are No-Ops
- **Root Cause**:
  - In `d/C0129b.a(JSONObject)`:
    - For `"line"` events when `read-options` (default `true`) is enabled, the code parses `options`, filters non-blank strings, computes the player voice via `c(player)`, and discards the result without calling `tts.d(...)`.
    - For `"end"` events, the code computes `c(narrator)` and discards the result without stopping active `TextToSpeech` playback or resetting the utterance counter.
- **Recommended Upstream Fix**:
  - In `"read-options"`, iterate the filtered options and queue them via `tts.d(option, playerVoice, false)`.
  - In `"end"`, call `tts.stop()` and reset the active queue counter to `0`.

### 8. [High] Destructive Non-Atomic SAF Rename in `GameFiles.k()`
- **Root Cause**: When renaming/moving a file on SAF custom storage (`GameFiles.k(src, dst)`), if `dst` already exists as a file, the code deletes `dst` immediately via `GameFiles.c(dst)` **before** checking whether `src` exists (`f(src) != null`) or whether parent document URIs (`j(src)`, `j(dst)`) are valid. Any failure leaves `dst` permanently deleted.
- **Recommended Upstream Fix**: Validate `f(src) != null`, `j(src) != null`, and `j(dst) != null` before deleting `dst` (and ideally rename `dst` to a temporary backup name if the provider supports it, restoring on failure).

### 9. [Medium] Cross-Process `SharedPreferences` Inconsistency & `onDestroy()` `killProcess()` Race
- **Root Cause**:
  - Seven input-saving methods in `C0067n0` (`Y`, `Z`, `a`, `b0`, `c0`, `i0`, `j0`) open `"game_manager"` with `context.getSharedPreferences("game_manager", MODE_PRIVATE)` while the rest of `C0067n0`, `AbstractC0111w0`, and `:game` open `"game_manager"` via `Z2.a0()` (`MODE_MULTI_PROCESS`).
  - `GameActivity` clears `mod_start_failure` asynchronously via `.apply()`, while `GameActivity.onDestroy()` invokes `Process.killProcess(Process.myPid())` synchronously right after `super.onDestroy()`, risking dropped writes (and making the 5-second `game-finish` watchdog thread unreachable unless `super.onDestroy()` hangs).
- **Recommended Upstream Fix**:
  - Unify all `"game_manager"` accesses (or migrate cross-process IPC state away from deprecated `MODE_MULTI_PROCESS` to `ContentProvider` / atomic files).
  - Use synchronous `.commit()` for `mod_start_failure.edit().clear()` in `GameActivity`.

### 10. [Medium] External Savedata Migration Edge Cases (`AbstractC0111w0`)
- **Root Cause**:
  - `AbstractC0111w0.k()` (switching storage root with external savedata enabled) calls `e(context)` inside an empty `catch (Throwable) {}`, silently swallowing errors.
  - In `AbstractC0111w0.e()`, if `Os.symlink()` fails after moving internal saves into `<root>/savedata`, rollback calls `h(externalDir, internalDir)`, moving **all** pre-existing external saves into internal storage and deleting `<root>/savedata`.
- **Recommended Upstream Fix**: Log/surface migration failures in `k()`, and in `e()` track newly moved files (or test symlink creation before moving files).

### 11. [Low/Medium] `GameActivity` Missing `smallestScreenSize` in `AndroidManifest.xml`
- **Root Cause**: `MainActivity` declares `smallestScreenSize` in `android:configChanges`, but `GameActivity` omits it. On foldable devices or multi-window resizes crossing `smallestScreenSize`, Android recreates `GameActivity` and kills `:game`.
- **Recommended Upstream Fix**: Add `smallestScreenSize` to `GameActivity`'s `android:configChanges`.

### 12. [Low] Minor Asset & Config Cleanups
- `assets/config/rgl_config.txt`: Duplicate `enable_blood = 1` on lines 15 and 43.
- `assets/resources/fonts/th/`: Contains `OFL-Vazirmatn.txt` (Persian font license copied from `fonts/rtl/`) alongside Thai Noto Sans (`OFL.txt` / `README.md`).
- `c/b.java`: Cold-start Cloudflare Web Analytics RUM beacon (`https://cloudflareinsights.com/cdn-cgi/rum`) has no user opt-out toggle in settings.
