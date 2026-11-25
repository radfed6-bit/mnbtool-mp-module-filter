# Credits, Attribution & Original Resources

All rights and credit for **M&BTool 4.3** (`com.taleworlds.mbwarband.m1p`, `mnb_noobb_v_4_3_armv8.apk`) and **Mount & Blade: Warband** belong strictly to their respective original authors and copyright holders. This repository contains only independent technical analysis, bug reports, and original clean-room C++17 mod code built with the official M&BTool ModFramework SDK.

---

## M&BTool 4.3 — Original Author & Contributors

- **Primary Author**: **remixer-dec**
  - Official Website: https://remixer-dec.github.io
  - Telegram Channel (Releases & Updates): https://t.me/rdreleases
  - GitHub Repository / Asset Releases: https://github.com/remixer-dec/MNBTool
  - Live Library Manifest: https://remixer-dec.github.io/MNBTool/library.json
  - Author of M&BTool 4.3, its core built-in mods, and all 8 bundled mods (`mbtool.color-config`, `mbtool.debug-overlay`, `mbtool.dynamic-clouds`, `mbtool.postfx-restore`, `mbtool.screen-hdr`, `mbtool.taa`, `mbtool.vibration`, `mbtool.wind`).

- **Contributors Credited Inside M&BTool 4.3 (`f/C0134c`)**:
  - **leakman** — Author of `builtin.shadow-fixes`, `builtin.shadow-restore`, `builtin.shadow-depth-texture`, `builtin.shader-compatibility`, `builtin.gpu-resource-leak-fixes`, `builtin.multiplayer-master-server-fix`, `builtin.touch-active-rect`, `builtin.touch-history-fix`.
  - **Oreo922** — Co-author of `builtin.custom-server` v1.3.14; operator of `http://oreo922.cn`; listed in `library.json` supporters.
  - **M&BTool contributors** — Co-credited across multiple built-in mods.

- **Supporters (Credited in Live `library.json`)**:
  - **Oreo922**, **Corukae**, **LenHa**

---

## Original Game & Engine

- **TaleWorlds Entertainment** — Creators of *Mount & Blade: Warband* (`com.taleworlds.mbwarband`) and original game assets/master server (`http://warbandmain.taleworlds.com/`).

---

## Third-Party Open-Source Libraries & Assets Referenced in M&BTool 4.3

- **ByteDance ByteHook** (`com.bytedance.android.bytehook`, `libbytehook.so`) — Android PLT hooking library
- **ByteDance ShadowHook** (`com.bytedance.shadowhook`, `libshadowhook.so`) — Android inline hooking library
- **Fonts (SIL Open Font License 1.1, `assets/resources/fonts/`)**:
  - **Rubik**: https://github.com/googlefonts/rubik
  - **Vazirmatn**: https://github.com/rastikerdar/vazirmatn
  - **Noto Sans Thai**: https://github.com/notofonts/thai / https://github.com/google/fonts/tree/main/ofl/notosansthai
- **Voice Metadata (`assets/ai/google_voices.txt`)**:
  - **Readium Speech** (`github.com/readium/speech/json`, CC0), cross-checked with RT-Voice PRO
- **AI / Translation Endpoint Defaults in M&BTool 4.3**:
  - OpenAI-compatible API (`https://api.openai.com/v1`), Hugging Face (`https://huggingface.co/`), ModelScope (`https://modelscope.cn/models/`)

---

## Official M&BTool ModFramework SDK (used to build `native/mp-module-filter`)

- **remixer-dec/MNBTool** `modframework/` (`mod_api.h`, `mod_compat.hpp`, `build-mod.sh`, `generate-bindings.py`) — fetched at build time from https://github.com/remixer-dec/MNBTool (pinned to the Release 4.3 commit); it is **not** redistributed in this repository. Docs: https://remixer-dec.github.io/MNBTool/web/ru/modframework-guide/
