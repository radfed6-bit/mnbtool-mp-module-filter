# Upstream Technical Bug Report for M&BTool 4.3 (`remixer-dec`)

**Target version**: M&BTool 4.3 (`com.taleworlds.mbwarband.m1p`, `versionCode=2`, `arm64-v8a`)
**Purpose**: Clean technical root-cause report and recommended source-level fixes for the original author (**remixer-dec**) and M&BTool contributors. No proprietary binaries, smali excerpts, or binary patchers are included here.

---

## Summary Table

| # | Severity | Component | Summary |
|---|---|---|---|
| **1** | Critical | `builtin.multiplayer-master-server-fix` (`libmodlauncher.so`) | Non-`chkserialandroid` requests to `warbandmain.taleworlds.com/handlerservers.ashx?type=` are dropped with an empty response (`""`) -> **0 servers** |
| **1b** | Critical | `builtin.custom-server::mod_start()` vs `builtin.resource-overrides::mod_start()` (startup load order) | `CustomServer` checks `ResourceOverrides::g_archive_ready` ~42 ms **before** `ResourceOverrides` runs, so `Start in host version = true` + `Host version = 1.174` **always** silently fails and the game boots in protocol `1157` |
| **1c** | High | `builtin.custom-server::apply_filter` + stock `apply_filter` (`cmp w20, #0x485`) | While booted in `1157`, every PC `1170` (1.174) server is hidden by the stock filter unless `g_is_native_1157 && g_archive_ready`; true for non-Native modules (e.g. Napoleonic Wars) and, due to 1b, for Native too |
| **2** | High | `builtin.custom-server` + `library.json` default override | Cached `library.json` `"servers"` silently replaces TaleWorlds default with `http://oreo922.cn` (2 entries, 4–6s latency) |
| **3** | High | `ModDevice.httpsGet()` | Reads `getErrorStream()` on HTTP `>= 400` and returns HTML error pages as valid `byte[]`; does not follow `http↔https` redirects; sends raw `Dalvik` UA |
| **4** | Medium | Engine query string (`&gametype=android*`) & protocol default (`1157` vs `1170`) | `&gametype=android` omits ~100 PC servers (`&gametype=androidonly` returns 0 bytes); default `host-version="1.158"` / `start-host-version=false` keeps Native on `1157` when `module.ini` lacks `compatible_multiplayer_version_no` |
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

### 1. [Critical] `builtin.multiplayer-master-server-fix` Drops `warbandmain.taleworlds.com` Server List Queries
- **Symptom**: Selecting **Internet** in Multiplayer with the default TaleWorlds master server yields **0 servers**.
- **Root Cause**:
  - `builtin.multiplayer-master-server-fix` hooks guest `Request::SendHTTP` (`0x001d2718`). `builtin.custom-server` chains on top of it.
  - When `builtin.custom-server` has `< 2` master servers and the URL uses `http://` (the built-in default is `http://warbandmain.taleworlds.com/`), it delegates to `master_fix`.
  - Inside `master_fix` (`0x019950a4`):
    1. Non-`warbandmain.taleworlds.com/handlerservers.ashx?type=` URLs branch to `0x01995130` (Java `ModDevice.httpsGet()`).
    2. For `warbandmain.taleworlds.com/handlerservers.ashx?type=` URLs, it checks `strstr(url, "chkserialandroid")`.
    3. When `chkserialandroid` is absent (such as `type=list&gametype=android`, `type=list`, `type=ping`), the branch at `0x01995118` jumps straight to the epilogue (`0x019951f8`) returning an empty response (`""`) instead of branching to `0x01995130` (`ModDevice.httpsGet()`).
  - Note: By contrast, when the user sets an `https://` URL, `builtin.custom-server` handles it directly without chaining to `master_fix`.
- **Recommended Upstream Fix**: Route non-`chkserialandroid` `warbandmain.taleworlds.com` requests to the `ModDevice.httpsGet()` path (`0x01995130`) instead of returning an empty string.

### 1b. [Critical] Startup Load-Order Bug: `Start in host version` Never Takes Effect
- **Symptom**: With `builtin.custom-server` set to `https://warbandmain.taleworlds.com/`, `Host version = 1.174`, `Start in host version = true`, the Internet list still shows **"0 servers and 0 players filtered"** on a real device.
- **Evidence** (real-device logcat, `Nov 25 00:35:33`, `:game` process):
  - `00:35:33.246 I/CustomServer: master servers: 1, hosting on http://warbandmain.taleworlds.com/; version 1157`
  - `00:35:33.288 I/ResourceOverrides: layer files=122 base module=Native stacked=0 archive files=0`
  - i.e. `builtin.custom-server::mod_start()` runs **~42 ms before** `builtin.resource-overrides::mod_start()`.
- **Root Cause**: `CustomServer::mod_start()` only switches the process to `1170` (`0x492`, patching the six `0x485` compare sites incl. `0x0013eb18`) when `ResourceOverrides::g_archive_ready` is true (this reflects the Java-side `Z2.b0()` rule: Native module + `start-host-version=true` + `host-version=1.174`, plus the `native-1174` overlay being ready). Because `ResourceOverrides` has not initialised yet, the flag is always `0` at that moment, so the game always starts in `1157`.
- **Recommended Upstream Fix**: Start `builtin.resource-overrides` before `builtin.custom-server` (add it to `custom-server`'s `requires`/`loadAfter`, or re-evaluate the version switch lazily on first server-list request instead of in `mod_start`).

### 1c. [High] Stock `apply_filter` Hides All `1170` Servers While the Engine Runs `1157`
- **Root Cause**: `Multiplayer_client_window::apply_filter(rglTable_row*)` (`0x0013ea98`) compares the row's `mb_version_no` with the engine version (`cmp w20, #0x485`). `CustomServer::apply_filter` only spoofs `1170 -> 1157` for rows whose module matches the active module when `g_is_native_1157 == 1 && ResourceOverrides::g_archive_ready == 1`; otherwise it calls the stock filter, which hides every `1170` (PC 1.174) row. `g_is_native_1157` is `0` for any non-Native module (the log shows `selected module directory: Napoleonic Wars`), and `g_archive_ready` is `0` for everyone because of 1b.
- **Related UI note**: the second list type is `Internet (Android Only)` -> `gametype=androidonly`, which TaleWorlds answers with 0 bytes (0 servers); use plain *Internet*.
- **Recommended Upstream Fix**: Normalise the version of matching PC rows around the stock filter call regardless of module; do not gate it on `g_archive_ready`.

### 2. [High] `library.json` Silently Overrides Default TaleWorlds Master Server with `http://oreo922.cn`
- **Symptom**: Even if Bug #1 is fixed, once the launcher fetches `https://remixer-dec.github.io/MNBTool/library.json`, the default master server silently changes from `http://warbandmain.taleworlds.com/` to `http://oreo922.cn`.
- **Verification (2026-11-24 / 2026-11-25)**:
  - `http://oreo922.cn/handlerservers.ashx?type=list` (Python `BaseHTTP/0.6`, HTTP/1.0, 4–6s latency) returns only **2 servers** (`41 B`, with trailing `|`).
  - `http://warbandmain.taleworlds.com/handlerservers.ashx?type=list` and `https://warbandmain.taleworlds.com/handlerservers.ashx?type=list` are both live and return HTTP 200 directly with **278–288 servers** (~5.1 KB, ~1.5s).
- **Recommended Upstream Fix**: Do not overwrite `builtin.custom-server`'s default `server` setting from `library.json` `"servers"`. Present `library.json` servers only as selectable presets in the UI while keeping `https://warbandmain.taleworlds.com/` (or `http://warbandmain.taleworlds.com/` once Bug #1 is fixed) as the default.

### 3. [High] `ModDevice.httpsGet()` Returns HTTP Error Bodies as Valid Responses & Drops Cross-Scheme Redirects
- **Root Cause**:
  1. When `httpURLConnection.getResponseCode() >= 400`, `ModDevice.httpsGet()` reads `httpURLConnection.getErrorStream()` and returns the raw HTML error body (e.g., 404/502/Cloudflare page) as a non-null `byte[]`. Native callers treat non-null bytes as a successful HTTP response.
  2. Relies on Android `HttpURLConnection`'s default redirect handling, which refuses HTTP↔HTTPS (`301`/`302`/`303`/`307`/`308`) cross-protocol redirects.
  3. Sends default `Dalvik/...` User-Agent header.
- **Recommended Upstream Fix**:
  - Return `null` for any non-2xx HTTP status (`code < 200 || code >= 300`).
  - Disable `instanceFollowRedirects` and manually follow up to 5 `301`/`302`/`303`/`307`/`308` redirects across `http↔https` via `URL(currentUrl, location)`.
  - Set an explicit `User-Agent` header (e.g., `Mozilla/5.0 (Linux; Android) MBTool/4.3`).

### 4. [Medium] Android Query Filter (`&gametype=android*`) & PC 1.174 Protocol Default
- **Root Cause**:
  - The Tegra engine appends `&gametype=android` (~185 servers, missing ~100 PC servers) on **Internet** and `&gametype=androidonly` (0 bytes) on **Internet (Android Only)**.
  - `builtin.custom-server` defaults to `host-version="1.158"` and `start-host-version="false"`. Because PC Warband's `Modules/Native/module.ini` omits `compatible_multiplayer_version_no`, `Z2.b0()` defaults to protocol `1157` instead of PC 1.174's `1170`.
- **Recommended Upstream Fix**:
  - Strip `&gametype=android` and `&gametype=androidonly` when querying `warbandmain.taleworlds.com/handlerservers.ashx?type=list` inside `ModDevice.httpsGet()` (or `custom_server`) so all ~280+ PC servers are returned.
  - Default `host-version` to `"1.174"` and `start-host-version` to `"true"` (or allow PC 1.174 as default for PC installs).
  - Ensure `assets/multiplayer/native-1174.zip` remains stored uncompressed (`STORED`) in release APKs so `AssetManager.openFd()` succeeds (`custom_server` hides 1.174 Native servers if `native-1174.zip` is unreadable).

### 5. [Medium] Missing "Filter by Module" Checkbox in Multiplayer Server Browser
- **Root Cause**:
  - In PC Warband, `Multiplayer_client_window::activate()` creates `is_version_compatible_checkbox` (`ui_is_version_compatible`) inside `server_filter_panel` at `x=0.03, y=0.046` (coordinates still present in `game_variables.txt` lines 1301–1304, leaving a blank gap between *Has players* at `y=0.071` and *No password* at `y=0.021`).
  - TaleWorlds compiled this checkbox out of the Tegra binary (`libMBExpMobile.so`) and replaced it with hardcoded `module_name != "Native"` (`0x0013eaec`). Once `builtin.custom-server` patched out `0x0013eaec` to support 3rd-party modules, users lost the ability to filter servers by their active module.
- **Recommended Upstream Fix**:
  - Bundle [`native/mp-module-filter/`](native/mp-module-filter/) (clean-room C++17 mod built with the official `remixer-dec/MNBTool/modframework` SDK) under `assets/mods/mp-module-filter/` (automatically trusted for multiplayer via `AssetManager.list("mods")` -> `InstalledMod.bundled = true`) or integrate it into `builtin.custom-server`.
  - Note: v1.0.0 of this mod failed on real devices with `mod_start returned -4 (not found)`. Cause: it passed the same slot for `mf_function_hook_v1.original` and `mf_game_hook_adapter_v1.previous_core`, which `libmodlauncher.so` rejects (overlapping slots), then masked the error as `-4`; it also used non-SDK helpers (`get_setting_string` instead of `get_launcher_config`, `resolve_game_address(..., 0)` instead of `MF_MEMORY_READ`). v1.1.0 is rewritten against the official SDK and returns real `mf_result_v1` codes.
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
