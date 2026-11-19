# device_electronica7_rgb — часы «Электроника-7» RGB

> **Опциональное устройство.** Подключается через `src_filter` + `build_flags`.
> **Работоспособно только в составе сборки, содержащей ядро** (см. `../../TRS.md` §2.1).
> **Для сборки необходимо клонировать само устройство и все указанные в его `.ini` модули**
> (см. `../../TRS.md` §2.2).

Вывод ЧЧ:ММ, ручного текста (4 символа) или бегущей строки (ticker) на матрице WS2812 7×16
со шрифтами из ФС (45 глифов 4×7: цифры `0-9`, `A-Z`, пунктуация); цветовые эффекты, 12 типов
переходных эффектов при смене времени, перелистывание цифр при смене минут, эффект
«дождя»/«осадания» до появления валидного времени (`core_sys.timeValid()`); режимы шины
off/auto/macro.

- **Репозиторий:** https://github.com/Arcanum753/device_electronica7_rgb
- **Папка:** `src/device_electronica7_rgb/`
- **Флаг активации:** `-D DEVICE_E7RGB=1`
- **Registry:** `object=device_electronica7_rgb`, `define=DEVICE_E7RGB`, `namespace=e7`, `web=1`, `loop=0`, `res=1`, `prio=70`
- **Env:** `esp32_electronica7_rgb` и `esp32_electronica7_rgb_macros` (с `module_macros` и единой таблицей `partitions_esp32.csv`)
- **Только для платформы:** ESP32
- **Зависит от модулей:** `module_udp`, `module_otaclient`, `module_bmp280`, `module_ccs811`,
  `module_hdc1080`, (`module_macros` — только в macros-env)
- **Зависит от ядра:** `core_web`, `core_sys`, `core_json`, `core_state`, `core_led`

## Назначение

Вывод ЧЧ:ММ, ручного текста (4 символа) или бегущей строки (ticker) на матрице WS2812 7×16
со шрифтами из ФС (45 глифов 4×7: цифры `0-9`, `A-Z`, пунктуация); цветовые эффекты, 12 типов
переходных эффектов при смене времени, перелистывание цифр при смене минут, эффект
«дождя»/«осадания» до появления валидного времени (`core_sys.timeValid()`); режимы шины
off/auto/macro. В сборку включены датчики BMP280, CCS811, HDC1080 (ресурсы шины читаются
макросами).

## Функциональные требования

- FR-E7RGB-1: Режим отображения `displayMode`: `clock` — время ЧЧ:ММ (`core_sys.timeNow()`,
  показ при `core_sys.timeValid() || core_sys.timeFallback()`, шрифт из `fontFile`, кэш
  путь+размер); `manual` — 4-символьный `manualText`; `ticker` — бегущая строка.
- FR-E7RGB-2: Эффекты `effect` (моно, радуга, градиент статичный/динамичный, цикл цвета) с `effectDir`, `animSpeed`, `palette[8]`, `colorsCount`, `cycleMode`.
- FR-E7RGB-3: Переходные эффекты `timeFx` (12 типов), `timeFxFreq`, `timeFxDur`, `timeFxBg`.
- FR-E7RGB-4: «Анимация синхронизации» (`rainEnabled`): при `1` — вступительный «дождь» до
  валидного времени; при `0` — показывать часы с 00:00 (внутренние часы ядра), а при
  синхронизации — анимированный переход (`timeFx`) к актуальному времени. Параметры дождя:
  `rainDurMin`, `rainIntensity`, `rainSettleDur`.
- FR-E7RGB-5: Публичные сеттеры (`setEffect`, `setBrightness`, `setSpeed`, `setDigitsColor`, `setManualText`, `setBusMode`, `setDisplayMode`, `setTickerText`, `setTickerSpeed`, `setTickerDirection`, `setDigitFlip`, `setDigitFlipSpeed`) применяют изменения в RAM/на экране, но **не сохраняют** конфиг; сигналят на шину; планируют отложенное применение. При смене `e7.mode` через шину `core_state` вызывает `e7.save_mode`, которая сохраняет только `busMode`.
- FR-E7RGB-6: `saveNow()`/`saveConfig()` и web `handleSave` сохраняют полный конфиг; bus-функция `e7.save` ставит отложенное полное сохранение, `e7.save_mode` сохраняет только режим. `saveConfig()` удаляет устаревшее поле `mode`.
- FR-E7RGB-7: Режимы шины `off/auto/macro` (`busMode`); bus-запись (кроме `mode` и `save_mode`) разрешена только в `macro`, иначе `BUS_ERR_DENIED`. `busMode=off` гасит матрицу и точки при любом `displayMode`.
- FR-E7RGB-8: Конфиг `/config_e7rgb.json`: `busMode`, `displayMode` (0 clock / 1 manual / 2 ticker), `tickerText` (до 64 символов), `tickerSpeed` (мс/px, 10..200, по умолчанию 160), `tickerDirection` (0 left / 1 right), `digitFlip` (0 none / 1 up / 2 down), `digitFlipSpeed` (0 slow / 1 normal / 2 fast), `dataPin` (16), `brightness` (25), `colonBlink` (1, 0..10 с), `effect`, `effectDir`, `digitsColor`, `digitsColor2`, `animSpeed`, `colorsCount`, `cycleMode`, `palette[8]`, `origin`, `direction`, `layout`, `timeFx`, `timeFxFreq`, `timeFxDur`, `timeFxBg`, `rainEnabled`, `rainDurMin`, `rainIntensity`, `rainSettleDur`, `fontFile`, `manualText`. `digitsColor` по умолчанию `#00FF00` (зелёный). Цвета — строки `#RRGGBB` (регистр ввода любой, вывод в верхнем); числа не принимаются, ошибка чтения строки даёт `#FFFFFF`. Старое поле `mode` не поддерживается и удаляется при сохранении.
- FR-E7RGB-9: Аппаратно 114 светодиодов WS2812 через NeoPixelBus: 112 матрицы 7×16 + 2 разделительные точки ЧЧ/ММ в конце цепочки (индекс 112 — нижняя, 113 — верхняя). RMT при `-D E7_USE_RMT=1`, иначе I2S0; `dataPin=-1` — выключено.
- FR-E7RGB-10: Разделительные точки ЧЧ/ММ горят цветом текущего эффекта (`e7FxColor` при `x=8`, `y=2`/`y=4`) и общей яркостью. `colonBlink`: 0 — горят всегда; `N>0` — полный цикл N секунд (N/2 горят, N/2 погашены). В кадре часов (включая перелистывание цифр и прочерки «нет времени») точки мигают по
`colonBlink`; в переходе смены времени мигание приостановлено, в `FADE` точки гаснут вместе с
цифрами, в `FLASH` вспыхивают белым; при оседании дождя проявляются вместе с цифрами; в чистом
«дожде», в режимах `manual`/`ticker` и при `busMode=off` погашены.
- FR-E7RGB-11: `displayMode` — что показывается: `clock` (часы, дождь/переходы/перелистывание), `manual` (`manualText`), `ticker` (бегущая строка). Переключение сбрасывает `_view` в `NORMAL`; дождь запускается только в `clock` при невалидном времени и `rainEnabled=1`. Возврат в `clock` при невалидном времени возобновляет дождь.
- FR-E7RGB-12: Ticker — посимвольный вывод `tickerText`: глиф E7_GLYPH_W (4 px) + всегда пустая колонка E7_TICKER_GAP (1 px) между символами (итого E7_TICKER_CHAR_W = 5 px; пробел и неизвестный символ — тоже 5 px); строчные буквы приводятся к заглавным, неизвестный символ пропускается. Направление `left/right`, скорость `tickerSpeed` мс/px; смещение растёт от времени в `e7rgbAnimTask`, после `ширина текста + E7_WIDTH` сбрасывается в 0. Цвет — текущий эффект, яркость — общая, точки погашены. Пустой текст — пустая матрица. Длина
ограничена 64 символами (конфиг, сеттеры, веб). `e7.show` для текста длиннее 4 символов
включает ticker автоматически (см. ресурсы шины). Ticker от `e7.show` — **один проход**
(`_tickerOneShot`): после полного ухода строки кадр остаётся пустым до возврата часов; явный
`e7.ticker`/`e7.display=ticker` — цикличная прокрутка.
- FR-E7RGB-13: Перелистывание цифр (`digitFlip` `none/up/down`, `digitFlipSpeed` slow/normal/fast = 60/35/20 мс) при смене минут в `clock`: листаются только изменившиеся разряды; «лента» — 7 строк старой цифры, 1 строка зазора, 7 строк новой (8 шагов). Пока перелистывание активно, `e7rgbAnimTask` перепланируется через 20 мс, новая смена минут ждёт завершения. Перелистывание имеет приоритет над `timeFx`; `timeFx` остаётся для первой синхронизации NTP и для минут, кратных `timeFxFreq`, если flip выключен. Ведущий ноль часа сохраняется.
- FR-E7RGB-14: Шрифт — 45 глифов 4×7 (`0-9`, `A-Z`, `.`, `,`, `:`, `=`, `-`, `%`, `°`, `+`, пробел) в PROGMEM и в `/e7fonts/digital7.fnt` формата `# FORMAT=4`. Файл другого формата или неполный пересоздаётся из встроенной таблицы.
- FR-E7RGB-15: При смене `displayMode` между `clock` и текстом (`manual`/`ticker`) при
  `timeFx != OFF` и `timeFxDur > 0` проигрывается переход `timeFx`: уход с часов — кадр часов →
  пустая матрица, приезд — пустая матрица → актуальное время. Завершение перехода обрабатывает
  `e7rgbAnimTask()` по 50-мс тику независимо от `displayMode`; для `ticker` прокрутка стартует
  после ухода часов. Пока есть `_pendingApply` или идёт `_view == TRANS`, `e7rgbSecondTask()` не
  вмешивается (иначе мгновенная перерисовка часов по `_forceRedraw` даёт вспышку перед плавным
  приездом). Цвета кадров берутся из `_transPrevColor`/`_transCurColor`: при уходе старый
  кадр сохраняет прежний цвет, при приезде новый — свой (FADE монотонный, без вспышек и чёрной
  паузы; фон в переходах «часы↔текст» отключён). При `timeFx OFF` переключение мгновенное.

## Аппаратные интерфейсы

| Интерфейс | Выводы по умолчанию | Примечание |
|-----------|---------------------|------------|
| WS2812 (матрица) | DATA=16 (`dataPin`) | 114 светодиодов: 112 матрицы 7×16 + 2 точки ЧЧ/ММ (112 — нижняя, 113 — верхняя); NeoPixelBus (RMT при `-D E7_USE_RMT=1`, иначе I2S0); `dataPin=-1` — выключено |

## Веб-интерфейс

Маршруты: `GET /e7rgb/save`, `/e7rgb/info`, `/e7rgb/fonts`, `/e7rgb/ver`.
Страница `electronica7-rgb.html` (пункт меню — `web/_menu.html`) с полями `displayMode`,
ticker (текст/скорость/направление) и flip (направление/скорость); шрифт
`e7fonts/digital7.fnt` (FORMAT=4), примеры макросов `web/macros/*.lua`.

## Конфигурация

`/config_e7rgb.json` — поля: см. FR-E7RGB-8.

## Ресурсы шины (namespace `e7`)

- состояния: `e7.mode` (ENUM off/auto/macro), `e7.display` (ENUM clock/manual/ticker),
  `e7.effect` (ENUM), `e7.brightness` (I32), `e7.colon_blink` (I32, 0..10), `e7.speed` (I32),
  `e7.color` (STR, `#RRGGBB`), `e7.text` (STR, 4 символа), `e7.show` (STR, «показать текст»),
  `e7.ticker` (STR, до 64 символов), `e7.ticker_speed` (I32, 10..200 мс/px),
  `e7.ticker_dir` (ENUM left/right), `e7.flip` (ENUM none/up/down), `e7.flip_speed` (I32, 0..2);
- функции: `e7.mode`, `e7.display`, `e7.effect`, `e7.brightness`, `e7.colon_blink`, `e7.speed`,
  `e7.color`, `e7.text`, `e7.show`, `e7.ticker`, `e7.ticker_speed`, `e7.ticker_dir`, `e7.flip`,
  `e7.flip_speed`, `e7.save`, `e7.save_mode`.
- `e7.show` — универсальная «покажи текст»: пустая строка → часы; до 4 символов — статично в
  `manual` (дополняется пробелами); длиннее — автоматически `ticker` с этим текстом. Режим
  переключается сам, скорость/направление берутся из `tickerSpeed`/`tickerDirection`.
- `e7.color` принимает только строку `#RRGGBB`; невалидная строка -> `BUS_ERR_BAD_VALUE`,
  текущий цвет не меняется.
- `e7.text` переключает `displayMode` на `manual` (4 символа); `e7.show` — универсальный показ
  текста с авто-переключением на ticker; `e7.ticker` меняет только текст (режим — через
  `e7.display`). Запись `e7.ticker*` сбрасывает прокрутку в начало; `e7.flip`/`e7.flip_speed`
  применяются со следующей смены минут (`flip=none` останавливает текущее перелистывание). Все
  записи, кроме `e7.mode`/`e7.save_mode`, гейтятся `e7BusAllowed()` (только `macro`).
- `e7.display`, `e7.ticker`, `e7.flip` — dual-ресурсы (состояние + write-функция): в Lua читаются
  через `get("e7.display")`, пишутся как `e7.display = "ticker"`.

## Терминал

`device_electronica7_rgb_terminal.cpp`; регистрация — `terminalRegister()` из `begin()` после
загрузки конфига. `component = "device_electronica7_rgb"`, команда верхнего уровня — `e7`,
`helpText` в `PROGMEM`, вывод только латиницей. Все изменения только применяются; на FS пишут
только `save`/`save_mode`.

| Подкоманда | Назначение |
|---|---|
| `e7 show` | busMode, displayMode, effect, brightness, colon, speed, color, text, ticker, flip, font, dataPin, время/NTP |
| `e7 effect <0..4\|имя>` | моно/радуга/градиенты/цикл цвета; без аргумента — текущий |
| `e7 brightness <0..255>` | яркость |
| `e7 speed <1..50>` | скорость перелива |
| `e7 text <текст>` | показать текст: до 4 символов статично (`manual`), длиннее — ticker |
| `e7 ticker <текст>` | задать текст и включить `ticker` |
| `e7 ticker off` | очистить текст и вернуться на часы |
| `e7 ticker speed <10..200>` | мс на пиксель |
| `e7 ticker dir <left\|right>` | направление строки |
| `e7 flip <none\|up\|down> [slow\|normal\|fast]` | перелистывание цифр; без аргумента — текущее |
| `e7 save` | записать `/config_e7rgb.json` |
| `e7 save_mode` | записать только `busMode` |
| `e7 help` | справка |

## Слоистая структура

Из `../../LAYERS.md`: `device_electronica7_rgb` — матрица E7 RGB: рендер, эффекты, переходы,
«дождь», ticker, перелистывание. Слои выделены: `device_electronica7_rgb_types.h`,
`device_electronica7_rgb.cpp` (шаблонный блок), `device_electronica7_rgb_engine.cpp`
(рендер/эффекты/задачи), `device_electronica7_rgb_terminal.cpp` (команда `e7`), а также
`e7rgb_matrix.*`, `e7rgb_fonts.*`, `e7rgb_font.h`, `common_module.*`. Локальные
stateless-хелперы (`e7*`: HSV/Lerp/ГПСЧ/эффекты/сэмплер) — в `common_module.*`, namespace
`ns_device_electronica7_rgb`.

## Зависимости модулей

Указаны в `.ini` (`src_filter`) и должны быть склонированы для сборки:
- `module_udp` — `../module_udp/AGENTS.md`
- `module_otaclient` — `../module_otaclient/AGENTS.md`
- `module_bmp280` — `../module_bmp280/AGENTS.md` (оба env; I2C `Wire`, SDA=21/SCL=22)
- `module_ccs811` — `../module_ccs811/AGENTS.md` (оба env; компенсация t/RH через `hdc1080.*`)
- `module_hdc1080` — `../module_hdc1080/AGENTS.md` (оба env)
- `module_macros` — `../module_macros/AGENTS.md` (только env `esp32_electronica7_rgb_macros`)

## Примеры макросов

`web/macros/e7_sensors_cycle_15s.lua` — раз в 30 с показывает одно показание датчика с подписью
(`T1=23.4C`, `P=1013HPA`, `CO2=412PPM`, `TVOC=3PPB`, `T2=29.1C`, `H=73.0%`) по кругу, каждое
своим цветом (`e7.color`: `#FF8000`, `#00FFFF`, `#FF00FF`, `#FFFF00`, `#FF0000`, `#0080FF`);
между показаниями — часы зелёные (`#00FF00`, точки мигают). Показ идёт через `e7.show` (длиннее
4 символов → автоматически ticker) и длится `max(5 с, полный проход строки)`; время прохода —
`ceil((#текст*5 + E7_WIDTH) * get("e7.ticker_speed") / 1000)` с + 4 с (переход «часы уезжают»
~2 с, запас 1–2 с). Ticker от `e7.show` идёт одним проходом и не зацикливается.
После прохода — `e7.display = "clock"` (часы приезжают через `timeFx`). Форматы:
температура/влажность — 1 знак, давление/CO₂/TVOC — целые.
Недоступные датчики (`*.available == false`) исключаются; если не доступен ни один — `clock`.
Скрипт ничего не сохраняет; для записей нужен `e7.mode="macro"` (в `off/auto` — `DENIED`).
Форматирование — только `//`, `%`, `..` (в Lua нет `math`/`string`/`tostring`).

## Критерии приёмки

- AC-12: `device_electronica7_rgb`: `call("e7.save")` и веб-Save персистят полный конфиг (включая
  `displayMode`/ticker/flip и без старого `mode`), `call("e7.save_mode")` — только `busMode`, а
  `set("e7.effect", ...)` — нет; режимы `off/auto/macro` соблюдаются и переживают перезагрузку;
  `busMode=off` гасит матрицу при любом `displayMode`.
- AC-13: точки ЧЧ/ММ (LED 112 — нижняя, 113 — верхняя) горят цветом эффекта; `colonBlink=0` —
  постоянно, `1` — 0.5 с / 0.5 с, `10` — 5 с / 5 с; значение сохраняется веб-Save и `e7.save`, а
  `set("e7.colon_blink", ...)` — нет; в переходах смены времени мигание приостановлено; в ticker и
  во время перелистывания точки мигают по `colonBlink`.
- AC-14: `displayMode` переключает часы/ручной текст/ticker; ticker прокручивается в обе стороны с
  заданной скоростью, при невалидном времени дождь не запускается; `digitFlip` листает только
  изменившиеся разряды (8 шагов, 60/35/20 мс), новая смена минут ждёт завершения; шрифт
  `FORMAT=4` (45 глифов) пересоздаётся из PROGMEM при старом файле; `e7_sensors_cycle_15s.lua`
  показывает значения доступных датчиков и ticker, не сохраняя конфиг.

## Ссылки

- Ядро и конвенции: `../../TRS.md`
- Слоистая структура: `../../LAYERS.md`
- Общие утилиты: `../../TRS.md` §3.1.12 (`common/`)
- Сборка: `../../BUILD.md`
- Реестр компонентов: `../../INVENTORY.md`

> Если устройство читается вне дерева ядра (standalone), корневые документы доступны в
> репозитории ядра avr-fota.
