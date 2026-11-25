# M&BTool 4.3 — Мод фильтра модулей для мультиплеера и технический баг-репорт

> **[English version (README.md)](README.md)** | **[Технический баг-репорт (BUG_REPORT.md)](BUG_REPORT.md)** | **[Авторы и благодарности (CREDITS.md)](CREDITS.md)**

Нативный AArch64-мод (clean-room C11), возвращающий недостающий флажок **«Совместимость модуля»** в окно списка серверов мультиплеера **Mount & Blade: Warband** под **M&BTool 4.3** (`com.taleworlds.mbwarband.m1p`), а также независимый технический баг-репорт для оригинального автора и рабочее решение для мультиплеера PC 1.174.

> **Политика clean-room**: В этом репозитории публикуются **только оригинальный исходный код на C11, сборочные файлы, CI workflow и техническая документация** (лицензия MIT). Здесь **нет** smali-патчей, **нет** бинарных патчеров, **нет** APK/библиотек M&BTool и **нет** ресурсов Mount & Blade: Warband. Все права и благодарности за M&BTool 4.3 принадлежат оригинальному автору и участникам — см. ниже и [CREDITS.md](CREDITS.md).

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

## Быстрое решение проблемы «0 серверов» (на официальном немодифицированном APK)

Чтобы список серверов PC 1.174 заработал **прямо сейчас без переподписи официального APK**:

1. Откройте **M&BTool 4.3** -> **Native Mods** -> **Custom Server** (`builtin.custom-server`).
2. Вручную задайте **Server address** (адрес главного сервера):
   ```
   https://warbandmain.taleworlds.com/
   ```
   - **Почему `https://` решает проблему**:
     - Явно сохранённый адрес перекрывает тихую подмену дефолтного сервера из `library.json` (`http://oreo922.cn`, отдаёт лишь 2 сервера) в `C0031g.c()`.
     - Использование **`https://`** направляет `type=list` из `builtin.custom-server` напрямую в Java `ModDevice.httpsGet()`, минуя ошибку в `builtin.multiplayer-master-server-fix` (`0x01995118`), из-за которой HTTP-запросы к `warbandmain.taleworlds.com/handlerservers.ashx?type=list*` отбрасываются с пустым ответом (`""`).
   - **Проверенное поведение**: Живые проверки подтверждают, что `https://warbandmain.taleworlds.com/handlerservers.ashx?type=list&gametype=android` напрямую отдаёт HTTP 200 (~185 серверов; полный PC-список без `&gametype=android` — ~278–288 серверов). При схеме `https://` запросы `chkserialandroid` также идут напрямую на HTTPS TaleWorlds, минуя локальную заглушку `master_fix`.
3. Выставьте **Host version** = **`1.174 (PC)`** и **Start in host version** = **`true`** (или добавьте `compatible_multiplayer_version_no = 1170` в `/sdcard/mnb/patch/Modules/Native/module.ini`), чтобы клиент использовал протокол PC 1.174 (`1170`).

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

1. **Почему показывается 0 серверов**:
   - `builtin.multiplayer-master-server-fix` (`0x01995118` в `libmodlauncher.so`) отбрасывает все HTTP-запросы к `warbandmain.taleworlds.com/handlerservers.ashx?type=` без `chkserialandroid`, возвращая `""`.
   - `C0031g.c()` тихо подменяет дефолтный мастер-сервер адресом `http://oreo922.cn` из `library.json` (всего 2 сервера).
   - `ModDevice.httpsGet()` возвращает HTML-страницы ошибок HTTP 4xx/5xx как успешный ответ `byte[]` и не обрабатывает редиректы `http↔https`.
   - Движок Tegra дописывает `&gametype=android` (~185 серверов вместо ~280+ PC-серверов) или `&gametype=androidonly` (0 байт), а `builtin.custom-server` по умолчанию ставит протокол `1157` (`1.158`) вместо `1170` (`1.174`).
2. **Почему отсутствует «Фильтр по модулю»**:
   - Окно списка серверов написано на C++ (`Multiplayer_client_window` в `libMBExpMobile.so`). TaleWorlds вырезали 3-й флажок ПК-версии (`is_version_compatible_checkbox`, `ui_is_version_compatible`) в Tegra-порте, хотя его координаты (`x=0.03, y=0.046`) до сих пор лежат в `game_variables.txt` (строки 1301–1304), оставляя пустой промежуток между «Есть игроки» и «Без пароля».
3. **Другие найденные ошибки лаунчера (описаны в `BUG_REPORT.md`)**:
   - Полный отказ интерфейса на Android 7.0–8.1 (API 24–27) из-за вызова API-28 `GET_SIGNING_CERTIFICATES` без проверки версии SDK в `e/g.e()`.
   - Неработающие ветки `read-options` и `"end"` в `builtin.ai-mod`.
   - Опасное переименование в `GameFiles.k()` (SAF): удаление `dst` до проверки `src` и родительских URI.
   - Смешанное использование `MODE_PRIVATE` и `Z2.a0()` для `"game_manager"` и гонка `apply()` с `Process.killProcess()` в `GameActivity.onDestroy()`.
   - Проглатывание ошибок переноса сохранений в `AbstractC0111w0.k()`, чрезмерный откат в `AbstractC0111w0.e()`, отсутствие `smallestScreenSize` у `GameActivity` и дубликат `enable_blood = 1` в `rgl_config.txt`.

---

## Нативный мод: `native/mp-module-filter/`

Реализация на чистом C11, восстанавливающая `is_version_compatible_checkbox` (`ui_is_version_compatible`) в штатных координатах ПК-версии (`x=0.03, y=0.046`) и фильтрующая строки серверов без учёта регистра по текущему модулю (с учётом главного флажка «Фильтр серверов» и совместимостью с `builtin.custom-server`):

- `native/mp-module-filter/mf_abi.h` — проверенные через `_Static_assert` структуры `modAbi: 1` и смещения/символы Warband.
- `native/mp-module-filter/mod_etrel.c` — AArch64 `ET_REL` (`mod.arm64.o`), строго соблюдающий белый список символов `libmodlauncher.so`.
- `native/mp-module-filter/mod_so.c` — вариант разделяемой библиотеки для ShadowHook (`libmp_module_filter.so`).
- `native/mp-module-filter/manifest.json` — манифест мода M&BTool (`id: "mbtool.mp-module-filter"`, русская/английская локализации, настройка `default_enabled`).
- **Доверие в мультиплеере (для апстрима)**: При размещении `mp-module-filter/{manifest.json,mod.arm64.o}` в `assets/mods/` официального APK мод автоматически получает статус доверенного (`AssetManager.list("mods")` -> `InstalledMod.bundled = true`) без каких-либо правок Java. Сторонние непроверенные моды при ручной установке вызывают предупреждение `multiplayer.untrusted-mods` до включения в официальный релиз/библиотеку.

---

## Артефакты сборки GitHub Actions и локальная сборка

[`.github/workflows/build.yml`](.github/workflows/build.yml) компилирует `native/mp-module-filter` через Android NDK (`aarch64-linux-android24-clang`), проверяет белый список символов (`make check-whitelist`), валидирует `manifest.json` и публикует артефакт **`mp-module-filter-arm64`** (а на тегах `v*` — релиз GitHub):

- `mp-module-filter.zip` — пакет мода M&BTool (`mp-module-filter/manifest.json` + `mp-module-filter/mod.arm64.o`)
- `mod.arm64.o` — объектный файл AArch64 `ET_REL` для `assets/mods/mp-module-filter/`
- `libmp_module_filter.so` — вариант разделяемой библиотеки для ShadowHook
- `SHA256SUMS.txt` — контрольные суммы SHA-256

Локальная сборка с Android NDK:
```bash
make -C native/mp-module-filter \
  CC="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang" \
  all check-whitelist
```
