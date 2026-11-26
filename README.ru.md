# M&BTool 4.3 — Мод фильтра модулей для мультиплеера и технический баг-репорт

> **[English version (README.md)](README.md)** | **[Технический баг-репорт (BUG_REPORT.md)](BUG_REPORT.md)** | **[Авторы и благодарности (CREDITS.md)](CREDITS.md)**

Нативный мод (clean-room C++17, официальный ModFramework SDK, ARMv7 + ARM64), возвращающий недостающий флажок **«Совместимость модуля»** в окно списка серверов мультиплеера **Mount & Blade: Warband** под **M&BTool 4.3** (`com.taleworlds.mbwarband.m1p`) и включающий получение списка PC-серверов 1.174 с мастер-сервера TaleWorlds (`v1.1.2`), а также независимый технический баг-репорт для оригинального автора.

> **Политика clean-room**: В этом репозитории публикуются **только оригинальный исходный код на C++17, сборочные файлы, CI workflow и техническая документация** (лицензия MIT). Здесь **нет** smali-патчей, **нет** бинарных патчеров, **нет** APK/библиотек M&BTool и **нет** ресурсов Mount & Blade: Warband. Все права и благодарности за M&BTool 4.3 принадлежат оригинальному автору и участникам — см. ниже и [CREDITS.md](CREDITS.md).

> **Статус в апстриме (`2026-11-26`)**: `remixer-dec` рассмотрел [BUG_REPORT.md](BUG_REPORT.md) и подтвердил исправление в следующем релизе M&BTool **краша ARM64 при входе с внешними модами (#1d)**, **редиректов HTTP↔HTTPS (#3)**, **сбоя на Android 7.0–8.1 (#6)**, **переименования SAF (#8)**, **`smallestScreenSize` в `GameActivity` (#11)** и **поиска серверов по LAN на ARM64**, а также пообещал добавить **встроенную фильтрацию модулей (#5)** как опцию и проверить **цепочку `SendHTTP` для нескольких мастер-серверов (#1)**.

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

## Почему одной настройки `https://warbandmain.taleworlds.com/` недостаточно

M&BTool 4.3 намеренно использует мастер-сервер сообщества (`http://oreo922.cn` из `library.json`), а не мастер-сервер TaleWorlds. Если вручную вписать **Server address** = `https://warbandmain.taleworlds.com/` в `builtin.custom-server`, список на официальном APK всё равно останется пустым (**0 серверов**):

1. **FIFO-перехват в `builtin.multiplayer-master-server-fix`** — `libmodlauncher.so` (`0x1898368..0x1898390`) вызывает цепочку `MF_HOOK_CHAIN` строго в порядке FIFO. `builtin.multiplayer-master-server-fix` (`0x19950a4`) ставит хук на `Request::SendHTTP` (`0x001d2718`) **первым**, намеренно возвращает `""` (ноль DNS/сетевых запросов) для `warbandmain.taleworlds.com/handlerservers.ashx?type=` по **обоим** протоколам `http://` и `https://` и не вызывает `previous` — что заодно делает `CustomServer::SendHTTP` (`0x19168bc`) недостижимым при настройке нескольких (`>= 2`) мастер-серверов.
2. **Штатный фильтр скрывает серверы `1170` в режиме `1157` / сторонних модулях** — когда движок работает в протоколе `1157` (например, в сторонних модулях вроде *Napoleonic Wars*, где в `module.ini` нет `compatible_multiplayer_version_no = 1170`), штатный `apply_filter` (`cmp w20, #0x485`) отбрасывает все строки PC `1170` (1.174).
3. **Ограничение по `gametype`** — `Internet (Android Only)` отправляет `gametype=androidonly` (TaleWorlds отвечает 0 байт), а обычный *Internet* — `gametype=android` (~156–185 серверов) вместо полного PC-списка (`gametype=warband`, ~228–280 серверов).

### Внешний мод: `mp-module-filter.zip` `v1.1.2` (без патча APK и переподписи)

Собран официальным SDK `remixer-dec/MNBTool/modframework` (`ANDROID_ABI=both` для `arm64` и `armv7`), `v1.1.2` включает:

- **FIFO-первый хук запроса списка серверов (`Network_manager::send_http_request`, `0x001487ec`, `CHAIN`)**: срабатывает до `builtin.multiplayer-master-server-fix` на `0x001d2718` и переписывает `warbandmain.taleworlds.com/handlerservers.ashx?type=list` в `https://warbandmain.taleworlds.com/handlerservers.ashx?gametype=warband&type=list`. Перестановка параметров обходит проверку подстроки `?type=` в `master-server-fix` и направляет запрос напрямую в штатный `ModDevice.httpsGet()` M&BTool — работает даже при одном лишь включённом по умолчанию *LAN Multiplayer Fix*. Проверка `api->is_mod_active()` гарантирует, что старый `libcurl` никогда не получит `https://`; `chkserialandroid` и сторонние мастер-серверы не затрагиваются.
- **Нормализация серверов PC `1170..1174` (`0x0013ea98`, `CHAIN`)**: временно нормализует версию строк `1170..1174` (повтор с `1157` / `1174` и немедленным восстановлением исходного значения) вокруг штатного `apply_filter`, чтобы серверы PC 1.174 отображались и в Native, и в сторонних модулях.
- **Восстановление флажка «Совместимость модуля» (`0x0012f06c`, `0x0012f040`, `0x00122c38`, `CHAIN`)**: возвращает `is_version_compatible_checkbox` (`ui_is_version_compatible`) в штатные координаты ПК-версии из `game_variables.txt` (`x=0.03, y=0.046`): снят — все модули, установлен — только текущий (без учёта регистра, с учётом главного флажка «Фильтр серверов»).

**Установка**: M&BTool -> **Моды движка** -> **Добавить** -> **С устройства (.zip)** -> выберите `mp-module-filter.zip`, включите мод (оставьте **Исправление сетевой игры по LAN** включённым) и перезапустите игру.

> **Статус проверки на реальном устройстве (`v1.1.2`) и ограничение M&BTool 4.3 ARM64 при подключении**:
> - **Подтверждено на реальном Android ARM64**: мод загружается без ошибок, получает полный список PC-серверов 1.174 (~228–280 серверов), отображает флажок **«Совместимость модуля и версии»** на `y=0.046` и фильтрует серверы по текущему модулю (Native и *Napoleonic Wars*).
> - **Известный баг M&BTool 4.3 ARM64 при подключении ([BUG_REPORT.md #1d](BUG_REPORT.md#1d-critical--fixed-in-next-release-arm64-sigsegv-crash-in-libmodlauncherso-when-joining-any-server-with-any-external-mod-enabled) — исправлен в следующем релизе M&BTool)**: Когда включён *любой* внешний мод в M&BTool 4.3 ARM64, перехватчик `Network_manager::start_client` внутри `libmodlauncher.so` (`0x188df78 -> 0x18970a4`) передаёт непереведённый 32-битный гостевой указатель `x2` без `decode_pointer` / `+ x28`, что вызывает мгновенный `SIGSEGV (SEGV_MAPERR)` при нажатии **Присоединиться** к **любому** серверу. `remixer-dec` уже исправил этот краш для следующего релиза и запланировал встроенную фильтрацию модулей.

---

## Как устроен M&BTool 4.3 (кратко)

- **Двухпроцессная архитектура (`com.taleworlds.mbwarband.m1p`, `minSdk=24`, `targetSdk=29`, `compileSdk=35`)**:
  - **`MainActivity` (основной процесс)**: Полноэкранный `WebView`, загружающий зашифрованный интерфейс (`assets/cache-index`) через `libmodlauncher.so` (`SessionNative`) с JS-мостами (`GameLauncher`, `GameManager`, `SavedataManager`, `NativeMods`, `Library`, `AiMod`, `MbImages`).
  - **`GameLaunchActivity` -> `GameActivity` (процесс `:game`, `NativeActivity`)**: Монтирует ПК-файлы Warband (`main/` + `patch/` из `/sdcard/mnb`, папки приложения или SAF), применяет PC-совместимые ресурсы (`assets/resources/pc-base/`), загружает встроенные и внешние `assets/mods/*` моды через `libmodlauncher.so` (`ByteHook` + `ShadowHook`) и запускает `libMBExpMobile.so` (статически перекомпилированный под ARM64 движок Tegra Warband 1.158).
- **Аттестация подписи (`SessionNative`)**:
  - `libmodlauncher.so` криптографически привязывает интерфейс `MainActivity` (`assets/cache-index`) и аттестацию `SessionHost` в `:game` к SHA-256 сертификата подписи оригинального APK (`f5d6bdaa9a19f3c87be2ba5cdbc4e89ec74e9a9dc1b4daf166fd46baa140c026`). Переподписанный другим ключом APK намеренно не загружает интерфейс (*«no fallback by design»*).
  - **Этот репозиторий не обходит аттестацию**: все найденные проблемы подробно описаны в **[BUG_REPORT.md](BUG_REPORT.md)** для апстрима (`remixer-dec`), а мод фильтра модулей написан строго под официальный `ModFramework` SDK (`modAbi: 1`).

---

## Сводка ключевых находок (полная таблица статусов в [BUG_REPORT.md](BUG_REPORT.md))

1. **Цепочка `SendHTTP` для нескольких мастер-серверов (`#1`, на проверке у автора)**:
   - Один мастер-сервер сообщества (`http://oreo922.cn`) работает за счёт прямой перезаписи `main_server_url` в `CustomServer::mod_start`, а отказ от мастер-сервера TaleWorlds сделан намеренно. Однако из-за того, что `builtin.multiplayer-master-server-fix` (`0x19950a4`) стоит первым в FIFO-цепочке `0x001d2718` и не вызывает `previous`, функция `CustomServer::SendHTTP` (`0x19168bc`) недостижима — это ломает опрос нескольких (`>= 2`) мастер-серверов и её логирование ошибок.
2. **Краш ARM64 при подключении с любым внешним модом (`#1d`, исправлено в следующем релизе)**:
   - В `libmodlauncher.so` проверка `multiplayer.untrusted-mods` на `Network_manager::start_client` (`0x00143770` -> `0x188df78`) передавала непереведённый 32-битный гостевой указатель `x2` в `0x1897084` без `decode_pointer` / `+ x28`, вызывая мгновенный `SIGSEGV` на `0x18970a4`.
3. **Отсутствующий флажок «Фильтр по модулю» (`#5`, запланирован как встроенная опция в следующем релизе)**:
   - TaleWorlds вырезали 3-й флажок ПК-версии (`is_version_compatible_checkbox`, `ui_is_version_compatible`) в Tegra-порте, хотя его координаты (`x=0.03, y=0.046`) до сих пор лежат в `game_variables.txt` (строки 1301–1304).
4. **Другие подтверждённые исправления для следующего релиза (`BUG_REPORT.md`)**:
   - **#6**: Отказ интерфейса на Android 7.0–8.1 (API 24–27) из-за вызова `GET_SIGNING_CERTIFICATES` без проверки версии SDK в `e/g.e()`.
   - **#3**: Редиректы `http↔https` в `ModDevice.httpsGet()`.
   - **#8**: Удаление `dst` до проверки `src` и родительских URI в SAF-переименовании `GameFiles.k()`.
   - **#11**: Отсутствие `smallestScreenSize` у `GameActivity`.
   - *(Пункты #9, #10, #12 приняты к сведению; #1b и #7 перепроверены, не являются багами и отозваны.)*

---

## Нативный мод: `native/mp-module-filter/` (`v1.1.2`)

Clean-room мод на C++17 под официальный SDK `remixer-dec/MNBTool/modframework` (`mod_api.h`, `build-mod.sh`, автоматические ARM64-привязки через `generate-bindings.py`).

- `native/mp-module-filter/mod.cpp` — `MF_MOD_START`, 5 хуков `MF_HOOK_CHAIN` (`0x12f06c`, `0x12f040`, `0x122c38`, `0x13ea98`, `0x1487ec`), защита `is_mod_active` для HTTPS-моста, нормализация `1157`/`1174`, сравнение модулей без учёта регистра и возврат реальных кодов `mf_result_v1`.
- `native/mp-module-filter/manifest.json` — `schemaVersion: 2`, `minLauncherVersion: "4.3"`, `version: "1.1.2"`, `modAbi: 1`, `gameBuilds: ["default"]`, `compatibility.loadAfter: ["builtin.custom-server", "builtin.multiplayer-master-server-fix"]`, русская/английская локализации, настройка `default_enabled` и все 5 объявлений хуков.
- `native/mp-module-filter/Makefile` — тонкая обёртка над официальным `build-mod.sh`.
- **Доверие в мультиплеере (для апстрима)**: При размещении папки мода в `assets/mods/` официального APK он автоматически получает статус доверенного (`AssetManager.list("mods")` -> `InstalledMod.bundled = true`) без правок Java.

---

## Артефакты сборки GitHub Actions и локальная сборка

[`.github/workflows/build.yml`](.github/workflows/build.yml) получает официальный SDK (`remixer-dec/MNBTool`, зафиксирован коммит релиза 4.3 `2797afa175ae1763635f265fa348798770cb7e1e`), собирает **оба** объекта `mod.armv7.o` и `mod.arm64.o` через `ANDROID_ABI=both ./modframework/build-mod.sh` (проверка правил загрузчика и флаги SafeStack включены), валидирует `manifest.json` (5 хуков) и публикует артефакт **`mp-module-filter`** (а на тегах `v*` — релиз GitHub):

- `mp-module-filter.zip` — пакет мода M&BTool (`manifest.json` в корне архива + `mod.arm64.o` + `mod.armv7.o` + `source/mod.cpp`)
- `mod.arm64.o`, `mod.armv7.o` — перемещаемые объектные файлы
- `SHA256SUMS.txt` — контрольные суммы SHA-256

Локальная сборка с Android NDK:
```bash
git clone --depth 1 https://github.com/remixer-dec/MNBTool.git
export ANDROID_NDK_ROOT=/path/to/android-ndk
make -C native/mp-module-filter MNBTOOL_SDK=$PWD/MNBTool/modframework package
```
