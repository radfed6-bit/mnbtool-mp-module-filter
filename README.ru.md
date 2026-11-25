# M&BTool 4.3 — Мод фильтра модулей для мультиплеера и технический баг-репорт

> **[English version (README.md)](README.md)** | **[Технический баг-репорт (BUG_REPORT.md)](BUG_REPORT.md)** | **[Авторы и благодарности (CREDITS.md)](CREDITS.md)**

Нативный мод (clean-room C++17, официальный ModFramework SDK, ARMv7 + ARM64), возвращающий недостающий флажок **«Совместимость модуля»** в окно списка серверов мультиплеера **Mount & Blade: Warband** под **M&BTool 4.3** (`com.taleworlds.mbwarband.m1p`), и исправляющий проблему «0 серверов» для мультиплеера PC 1.174 (v1.1.1), а также независимый технический баг-репорт для оригинального автора.

> **Политика clean-room**: В этом репозитории публикуются **только оригинальный исходный код на C++17, сборочные файлы, CI workflow и техническая документация** (лицензия MIT). Здесь **нет** smali-патчей, **нет** бинарных патчеров, **нет** APK/библиотек M&BTool и **нет** ресурсов Mount & Blade: Warband. Все права и благодарности за M&BTool 4.3 принадлежат оригинальному автору и участникам — см. ниже и [CREDITS.md](CREDITS.md).

---

## Оригинальный автор и ссылки

- **Автор M&BTool 4.3**: **remixer-dec**
  - Официальный сайт: https://remixer-dec.github.io
  - Telegram-канал релизов: https://t.me/rdreleases
  - GitHub: https://github.com/remixer-dec/MNBTool
  - Манифест библиотеки: https://remixer-dec.github.io/MNBTool/library.json
- **Участники, указанные в M&BTool 4.3**: **leakman**, **Oreo922**, **M&BTool contributors**
- **Поддержавшие проект (из `library.json`)**: **Oreo922**, **Corukae**, **LenHa**
- **Оригинальная игра**: **TaleWorlds Entertainment** (Mount & Blade: Warband)
- Полный список сторонних библиотек (ByteHook/ShadowHook), шрифтов SIL OFL 1.1 и Readium Speech приведён в **[CREDITS.md](CREDITS.md)**.

---

## Почему одних настроек `https://warbandmain.taleworlds.com/` + `1.174` недостаточно

Значения **Server address** = `https://warbandmain.taleworlds.com/`, **Host version** = `1.174 (PC)` и **Start in host version** = `true` в `builtin.custom-server` на немодифицированном официальном APK **не решают проблему** (подтверждено на реальном телефоне, logcat `Nov 25 00:35`):

1. **Ошибка порядка запуска** — `builtin.custom-server::mod_start()` выполняется примерно на 42 мс **раньше** `builtin.resource-overrides::mod_start()`:
   ```
   00:35:33.246 I/CustomServer:       master servers: 1, hosting on http://warbandmain.taleworlds.com/; version 1157
   00:35:33.288 I/ResourceOverrides:  layer files=122 base module=Native stacked=0 archive files=0
   ```
   `CustomServer` переключается на протокол `1170` только если `ResourceOverrides::g_archive_ready` установлен, а на этом этапе он всегда `0`. **Поэтому `Start in host version` никогда не срабатывает, и игра всегда стартует в `1157`.**
2. **Штатный фильтр скрывает все серверы `1170`** — пока движок работает в `1157`, `apply_filter` (`cmp w20, #0x485`) отбрасывает все строки PC 1.174. Для сторонних модулей (например, *Napoleonic Wars*) `CustomServer` вообще не подменяет версию.
3. **URL `http://` по умолчанию «глотается»** — при встроенном адресе `http://warbandmain.taleworlds.com/` `builtin.multiplayer-master-server-fix` отвечает на `type=list` пустой строкой (успех HTTP, 0 серверов). Явный `https://` обходит только этот слой.
4. `Internet (Android Only)` отправляет `gametype=androidonly` (TaleWorlds отвечает 0 байт), а обычный *Internet* — `gametype=android` (~156 серверов) вместо полного PC-списка (`gametype=warband`, ~228 серверов).

### Решение: `mp-module-filter.zip` v1.1.1 (без патча APK и переподписи)

Мод v1.1.1 (собран официальным SDK `remixer-dec/MNBTool/modframework` для `arm64` и `armv7`) исправляет всё это одним внешним модом:

- Переписывает запросы `warbandmain.taleworlds.com/handlerservers.ashx?type=list` в `https://warbandmain.taleworlds.com/handlerservers.ashx?gametype=warband&type=list` (перестановка параметров обходит блокировку `?type=` в `master-server-fix` даже при одном лишь штатном *LAN Multiplayer Fix*) (хук `Request::SendHTTP`, `0x001d2718`, `CHAIN`), чтобы они шли через штатный `ModDevice.httpsGet()` M&BTool; `chkserialandroid` и сторонние хосты не затрагиваются.
- Нормализует PC-серверы `1170` вокруг штатного `apply_filter` (хук `0x0013ea98`), чтобы серверы PC 1.174 больше не скрывались.
- Возвращает недостающий флажок **«Совместимость модуля»** (снят — все модули, установлен — только текущий).

Установка: M&BTool -> **Моды движка** -> **Добавить** -> **С устройства (.zip)** -> выберите `mp-module-filter.zip`, включите мод и перезапустите игру. Это непроверенный внешний мод, поэтому M&BTool считает мультиплеер *untrusted-mods*, пока автор не включит его в сборку (см. ниже).

> **Примечание**: v1.0.0 на реальных устройствах падал с `mbtool.mp-module-filter: mod_start returned -4 (not found)` (пересекающиеся слоты `original`/`previous_core`, отклонённые `libmodlauncher.so`, ошибка скрывалась под кодом `-4`). v1.1.1 переписан под официальный SDK; присылайте результаты проверки на реальных устройствах.

---

## Как устроен M&BTool 4.3 (кратко)

- **Двухпроцессная архитектура (`com.taleworlds.mbwarband.m1p`, `minSdk=24`, `targetSdk=29`, `compileSdk=35`)**:
  - **`MainActivity` (основной процесс)**: Полноэкранный `WebView`, загружающий зашифрованный интерфейс (`assets/cache-index`) через `libmodlauncher.so` (`SessionNative`) с JS-мостами (`GameLauncher`, `GameManager`, `SavedataManager`, `NativeMods`, `Library`, `AiMod`, `MbImages`).
  - **`GameLaunchActivity` -> `GameActivity` (процесс `:game`, `NativeActivity`)**: Монтирует ПК-файлы Warband (`main/` + `patch/` из `/sdcard/mnb`, папки приложения или SAF), применяет PC-совместимые ресурсы (`assets/resources/pc-base/`), загружает 46 встроенных и внешних `assets/mods/*` модов через `libmodlauncher.so` (`ByteHook` + `ShadowHook`) и запускает `libMBExpMobile.so` (статически перекомпилированный под ARM64 движок Tegra Warband 1.158).
- **Аттестация подписи (`SessionNative`)**:
  - `libmodlauncher.so` криптографически привязывает интерфейс `MainActivity` (`assets/cache-index`) и аттестацию `SessionHost` в `:game` к SHA-256 сертификата подписи оригинального APK (`f5d6bdaa9a19f3c87be2ba5cdbc4e89ec74e9a9dc1b4daf166fd46baa140c026`). Переподписанный другим ключом APK намеренно не загружает интерфейс (*«no fallback by design»*).
  - **Этот репозиторий не обходит аттестацию**: все найденные ошибки лаунчера и движка подробно описаны в **[BUG_REPORT.md](BUG_REPORT.md)** для исправления автором (`remixer-dec`), а мод фильтра модулей написан строго под официальный API `NativeMods` (`modAbi: 1`).

---

## Основные причины проблем (подробности в [BUG_REPORT.md](BUG_REPORT.md))

1. **Почему показывается 0 серверов** (несколько наложившихся причин):
   - `builtin.multiplayer-master-server-fix` (`0x01995118` в `libmodlauncher.so`) отбрасывает все HTTP-запросы к `warbandmain.taleworlds.com/handlerservers.ashx?type=` без `chkserialandroid`, возвращая `""`.
   - **Порядок запуска**: `builtin.custom-server` стартует примерно на 42 мс раньше `builtin.resource-overrides`, поэтому `Start in host version = true` не срабатывает (игра стартует в `1157`).
   - **Штатный `apply_filter`** скрывает все серверы PC `1170`, пока движок работает в `1157`.
   - `C0031g.c()` тихо подменяет дефолтный мастер-сервер адресом `http://oreo922.cn` из `library.json` (всего 2 сервера).
   - `ModDevice.httpsGet()` возвращает HTML-страницы ошибок HTTP 4xx/5xx как успешный `byte[]` и не обрабатывает редиректы `http<->https`.
   - Движок Tegra дописывает `&gametype=android` (~156 серверов) или `&gametype=androidonly` (0 байт) вместо полного PC-списка.
2. **Почему отсутствует «Фильтр по модулю»**:
   - Окно списка серверов написано на C++ (`Multiplayer_client_window` в `libMBExpMobile.so`). TaleWorlds вырезали 3-й флажок ПК-версии (`is_version_compatible_checkbox`, `ui_is_version_compatible`) в Tegra-порте, хотя его координаты (`x=0.03, y=0.046`) до сих пор лежат в `game_variables.txt` (строки 1301–1304), оставляя пустой промежуток между «Есть игроки» и «Без пароля».
3. **Другие найденные ошибки лаунчера (описаны в `BUG_REPORT.md`)**:
   - Полный отказ интерфейса на Android 7.0–8.1 (API 24–27) из-за вызова API-28 `GET_SIGNING_CERTIFICATES` без проверки версии SDK в `e/g.e()`.
   - Неработающие ветки `read-options` и `"end"` в `builtin.ai-mod`.
   - Опасное переименование в `GameFiles.k()` (SAF): удаление `dst` до проверки `src` и родительских URI.
   - Смешанное использование `MODE_PRIVATE` и `Z2.a0()` для `"game_manager"` и гонка `apply()` с `Process.killProcess()` в `GameActivity.onDestroy()`.
   - Проглатывание ошибок переноса сохранений в `AbstractC0111w0.k()`, чрезмерный откат в `AbstractC0111w0.e()`, отсутствие `smallestScreenSize` у `GameActivity` и дубликат `enable_blood = 1` в `rgl_config.txt`.

---

## Нативный мод: `native/mp-module-filter/` (v1.1.1)

Clean-room мод на C++17 под официальный SDK `remixer-dec/MNBTool/modframework` (`mod_api.h`, `build-mod.sh`, автоматические ARM64-привязки через `generate-bindings.py`). Он восстанавливает `is_version_compatible_checkbox` (`ui_is_version_compatible`) в штатных координатах ПК-версии (`x=0.03, y=0.046`), фильтрует строки серверов без учёта регистра по текущему модулю (с учётом главного флажка «Фильтр серверов» и совместимостью с `builtin.custom-server`) и содержит описанные выше исправления списка серверов.

- `native/mp-module-filter/mod.cpp` — исходный код мода (`MF_MOD_START`, хуки `MF_HOOK_CHAIN`, возвращает реальные коды `mf_result_v1`).
- `native/mp-module-filter/manifest.json` — `schemaVersion: 2`, `minLauncherVersion: "4.3"`, `modAbi: 1`, `gameBuilds: ["default"]`, русская/английская локализации, настройка `default_enabled`.
- `native/mp-module-filter/Makefile` — тонкая обёртка над официальным `build-mod.sh`.
- **Доверие в мультиплеере (для апстрима)**: При размещении папки мода в `assets/mods/` официального APK он автоматически получает статус доверенного (`AssetManager.list("mods")` -> `InstalledMod.bundled = true`) без правок Java. Сторонние непроверенные моды при ручной установке вызывают предупреждение `multiplayer.untrusted-mods` до включения в официальный релиз/библиотеку.

---

## Артефакты сборки GitHub Actions и локальная сборка

[`.github/workflows/build.yml`](.github/workflows/build.yml) получает официальный SDK (`remixer-dec/MNBTool`, зафиксирован коммит релиза 4.3), собирает **оба** объекта `mod.armv7.o` и `mod.arm64.o` через `ANDROID_ABI=both ./modframework/build-mod.sh` (проверка правил загрузчика и флаги SafeStack включены), валидирует `manifest.json` и публикует артефакт **`mp-module-filter`** (а на тегах `v*` — релиз GitHub):

- `mp-module-filter.zip` — пакет мода M&BTool (`manifest.json` в корне архива + `mod.arm64.o` + `mod.armv7.o` + `source/`)
- `mod.arm64.o`, `mod.armv7.o` — перемещаемые объектные файлы
- `SHA256SUMS.txt` — контрольные суммы SHA-256

Локальная сборка с Android NDK:
```bash
git clone --depth 1 https://github.com/remixer-dec/MNBTool.git
export ANDROID_NDK_ROOT=/path/to/android-ndk
make -C native/mp-module-filter MNBTOOL_SDK=$PWD/MNBTool/modframework package
```
