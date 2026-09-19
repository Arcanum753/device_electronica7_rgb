# device_electronica7_rgb — часы «Электроника-7» RGB

> **Опциональное устройство.** Подключается через `src_filter` + `build_flags`.
> **Работоспособно только в составе сборки, содержащей ядро** (см. `../../TRS.md` §2.1).
> **Для сборки необходимо клонировать само устройство и все указанные в его `.ini` модули**
> (см. `../../TRS.md` §2.2).

Вывод HH:MM (или ручного 4-символьного текста) на матрице WS2812 7×16 со шрифтами из ФС;
цветовые эффекты, 12 типов переходных эффектов при смене времени, эффект «дождя»/«оседания»
до синхронизации NTP; режимы шины off/auto/macro.

- **Репозиторий:** https://github.com/Arcanum753/device_electronica7_rgb
- **Папка:** `src/device_electronica7_rgb/`
- **Флаг активации:** `-D DEVICE_E7RGB=1`
- **Registry:** `object=device_electronica7_rgb`, `define=DEVICE_E7RGB`, `namespace=e7`, `web=1`, `loop=0`, `res=1`, `prio=70`
- **Env:** `esp32_electronica7_rgb` и `esp32_electronica7_rgb_macros` (с `module_macros` и таблицей `partitions_esp32_macro.csv`)
- **Только для платформы:** ESP32
- **Зависит от модулей:** `module_udp`, `module_otaclient`, (`module_macros` — только в macros-env)
- **Зависит от ядра:** `core_web`, `core_sys`, `core_json`, `core_ntp`, `core_state`, `core_led`

## Назначение

Вывод HH:MM (или ручного 4-символьного текста) на матрице WS2812 7×16 со шрифтами из ФС;
цветовые эффекты, 12 типов переходных эффектов при смене времени, эффект «дождя»/«оседания»
до синхронизации NTP; режимы шины off/auto/macro.

## Функциональные требования

- FR-E7RGB-1: Отображать время HH:MM или `manualText`; шрифт из `fontFile` (по умолчанию `/e7fonts/digital7.fnt`), кэшировать путь+размер.
- FR-E7RGB-2: Эффекты `effect` (моно, радуга, градиент статичный/динамичный, цикл цвета) с `effectDir`, `animSpeed`, `palette[8]`, `colorsCount`, `cycleMode`.
- FR-E7RGB-3: Переходные эффекты `timeFx` (12 типов), `timeFxFreq`, `timeFxDur`, `timeFxBg`.
- FR-E7RGB-4: Вступительный «дождь»: `rainEnabled`, `rainDurMin`, `rainIntensity`, `rainSettleDur`.
- FR-E7RGB-5: Публичные сеттеры (`setEffect`, `setBrightness`, `setSpeed`, `setDigitsColor`, `setManualText`, `setBusMode`) применяют изменения в RAM/на экране, но **не сохраняют** конфиг; сигналят на шину; планируют отложенное применение.
- FR-E7RGB-6: `saveNow()` и web `handleSave` сохраняют конфиг; bus-функция `e7.save` ставит отложенное сохранение.
- FR-E7RGB-7: Режимы шины `off/auto/macro` (`busMode`); bus-запись (кроме `mode`) разрешена только в `macro`, иначе `BUS_ERR_DENIED`.
- FR-E7RGB-8: Конфиг `/config_e7rgb.json`: `busMode`, `mode`, `dataPin` (16), `brightness` (25), `effect`, `effectDir`, `digitsColor`, `digitsColor2`, `animSpeed`, `colorsCount`, `cycleMode`, `palette[8]`, `origin`, `direction`, `layout`, `timeFx`, `timeFxFreq`, `timeFxDur`, `timeFxBg`, `rainEnabled`, `rainDurMin`, `rainIntensity`, `rainSettleDur`, `fontFile`, `manualText`.
- FR-E7RGB-9: Аппаратно 112 светодиодов WS2812 (7×16) через NeoPixelBus; RMT при `-D E7_USE_RMT=1`, иначе I2S0; `dataPin=-1` — выключено.

## Аппаратные интерфейсы

| Интерфейс | Выводы по умолчанию | Примечание |
|-----------|---------------------|------------|
| WS2812 (матрица) | DATA=16 (`dataPin`) | 112 светодиодов 7×16; NeoPixelBus (RMT при `-D E7_USE_RMT=1`, иначе I2S0); `dataPin=-1` — выключено |

## Веб-интерфейс

Маршруты: `GET /e7rgb/save`, `/e7rgb/info`, `/e7rgb/fonts`, `/e7rgb/ver`.
Страница `electronica7-rgb.html` (пункт меню — `web/_menu.html`), шрифт
`e7fonts/digital7.fnt`, примеры макросов `web/macros/*.lua`.

## Конфигурация

`/config_e7rgb.json` — поля: см. FR-E7RGB-8.

## Ресурсы шины (namespace `e7`)

- состояния: `e7.mode` (ENUM off/auto/macro), `e7.effect` (ENUM), `e7.brightness` (I32), `e7.speed` (I32), `e7.color` (I32), `e7.text` (STR);
- функции: `e7.mode`, `e7.effect`, `e7.brightness`, `e7.speed`, `e7.color`, `e7.text`, `e7.save`.

## Слоистая структура

Из `../../LAYERS.md`: `device_electronica7_rgb` — матрица E7 RGB: рендер, эффекты, переходы,
«дождь»; уже есть `e7rgb_matrix.*`, `e7rgb_fonts.*`, `common_module.*`; выделить `_types.h`,
`_engine.cpp`. Локальные stateless-хелперы (`e7*`: HSV/Lerp/ГПСЧ/эффекты/сэмплер) — в
`common_module.*`, namespace `ns_device_electronica7_rgb`.

## Зависимости модулей

Указаны в `.ini` (`src_filter`) и должны быть склонированы для сборки:
- `module_udp` — `../module_udp/AGENTS.md`
- `module_otaclient` — `../module_otaclient/AGENTS.md`
- `module_macros` — `../module_macros/AGENTS.md` (только env `esp32_electronica7_rgb_macros`)

## Критерии приёмки

- AC-12: `device_electronica7_rgb`: `call("e7.save")` и веб-Save персистят конфиг, а `set("e7.effect", ...)` — нет; режимы `off/auto/macro` соблюдаются.

## Ссылки

- Ядро и конвенции: `../../TRS.md`
- Слоистая структура: `../../LAYERS.md`
- Общие утилиты: `../../TRS.md` §3.1.12 (`common/`)
- Сборка: `../../BUILD.md`
- Реестр компонентов: `../../INVENTORY.md`

> Если устройство читается вне дерева ядра (standalone), корневые документы доступны в
> репозитории ядра avr-fota.
