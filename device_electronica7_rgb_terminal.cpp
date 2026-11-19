#include "core_web/FSWebServerLib.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "core_json/common_module.h"
#include "core_sys/core_sys.h"

#include "device_electronica7_rgb.h"
#include "common_module.h"

#include "core_terminal/core_terminal.h"
#include "core_terminal/common_module.h"

// ============================================================
// Терминальные команды device_electronica7_rgb (команда `e7`)
//
//  * обработчики — статические методы класса, обращаются к приватному
//    состоянию через глобальный объект device_electronica7_rgb;
//  * подкоманды проверяют число аргументов и диапазоны
//    (ns_core_terminal::parseIntStrict);
//  * изменения только применяются; на FS пишут только save/save_mode
//    (правило apply/save из корневого AGENTS.md);
//  * helpText лежит в PROGMEM, вывод — только латиница.
// ============================================================

static const char* const kE7Commands[] = {
    "show", "effect", "brightness", "speed", "text", "ticker", "flip",
    "save", "save_mode", nullptr
};

// Строковые пределы для справки/usage:
#define E7_STR_(x) #x
#define E7_STR(x) E7_STR_(x)

static const char kE7Help[] PROGMEM =
    "e7 - Electronica7 RGB clock (changes are not saved automatically)\r\n"
    "  e7 show                          current state\r\n"
    "  e7 effect <0..4|name>            color effect (mono/rainbow/grad_static/grad_dynamic/colorcycle)\r\n"
    "  e7 brightness <0..255>           brightness\r\n"
    "  e7 speed <1..50>                 animation speed\r\n"
    "  e7 text <text>                   show text (<=4 static, longer -> ticker)\r\n"
    "  e7 ticker <text>                 set ticker text and show ticker\r\n"
    "  e7 ticker off                    clear ticker text and show clock\r\n"
    "  e7 ticker speed <" E7_STR(E7_TICKER_SPEED_MIN) ".." E7_STR(E7_TICKER_SPEED_MAX) ">        ticker speed, ms per pixel\r\n"
    "  e7 ticker dir <left|right>       ticker direction\r\n"
    "  e7 flip <none|up|down> [speed]   digit flip; speed = slow/normal/fast\r\n"
    "  e7 save                          persist " CONFIG_FILE_E7RGB "\r\n"
    "  e7 save_mode                     persist only bus mode\r\n"
    "  e7 help                          this help\r\n";

static const char* const kE7EffectNames[] = {
    "mono", "rainbow", "grad_static", "grad_dynamic", "colorcycle"
};
static const char* const kE7DisplayNames[] = { "clock", "manual", "ticker" };
static const char* const kE7BusNames[] = { "off", "auto", "macro" };
static const char* const kE7FlipNames[] = { "none", "up", "down" };
static const char* const kE7FlipSpeedNames[] = { "slow", "normal", "fast" };
static const char* const kE7TickerDirNames[] = { "left", "right" };

// Индекс имени в таблице (без учёта регистра) или -1.
static int e7NameIndex(const String& value, const char* const* names, int count) {
    String s = value;
    s.toLowerCase();
    for (int i = 0; i < count; i++) {
        if (s == names[i]) { return i; }
    }
    return -1;
}

// Разделить строку на первый токен и остаток.
static void e7SplitFirst(const String& line, String& head, String& tail) {
    int sp = line.indexOf(' ');
    if (sp < 0) {
        head = line;
        tail = "";
    } else {
        head = line.substring(0, sp);
        tail = ns_core_terminal::trimCopy(line.substring(sp + 1));
    }
}

void CLASS_DEVICE_E7RGB::terminalRegister() {
    TerminalRegisterModule("device_electronica7_rgb", "e7", "Electronica7 RGB clock",
                           &CLASS_DEVICE_E7RGB::s_terminalCmd,
                           kE7Commands, kE7Help, true);
}

void CLASS_DEVICE_E7RGB::s_terminalCmd() {
    char* sub = term.getNext();
    if (sub == nullptr || strcmp(sub, "help") == 0) {
        TerminalPrintText(kE7Help);
        return;
    }

    if (strcmp(sub, "show") == 0)       { s_cmdShow();       return; }
    if (strcmp(sub, "effect") == 0)     { s_cmdEffect();     return; }
    if (strcmp(sub, "brightness") == 0) { s_cmdBrightness(); return; }
    if (strcmp(sub, "speed") == 0)      { s_cmdSpeed();      return; }
    if (strcmp(sub, "text") == 0)       { s_cmdText();       return; }
    if (strcmp(sub, "ticker") == 0)     { s_cmdTicker();     return; }
    if (strcmp(sub, "flip") == 0)       { s_cmdFlip();       return; }
    if (strcmp(sub, "save") == 0)       { s_cmdSave();       return; }
    if (strcmp(sub, "save_mode") == 0)  { s_cmdSaveMode();   return; }

    Serial.printf("Unknown e7 subcommand '%s'. Type 'e7 help'.\r\n", sub);
}

// ============================================================
// show — текущее состояние
// ============================================================

void CLASS_DEVICE_E7RGB::s_cmdShow() {
    if (term.getNext() != nullptr) {
        Serial.println("Usage: e7 show");
        return;
    }

    const strE7RgbConfig& cfg = device_electronica7_rgb._config;
    bool synced = core_sys.timeValid();

    Serial.printf("Bus mode:     %s\r\n", kE7BusNames[cfg.busMode]);
    Serial.printf("Display:      %s\r\n", kE7DisplayNames[cfg.displayMode]);
    Serial.printf("Effect:       %s\r\n", kE7EffectNames[cfg.effect]);
    Serial.printf("Brightness:   %u\r\n", (unsigned)cfg.brightness);
    if (cfg.colonBlink == 0) {
        Serial.println("Colon blink:  0 s (always on)");
    } else {
        Serial.printf("Colon blink:  %u s\r\n", (unsigned)cfg.colonBlink);
    }
    Serial.printf("Anim speed:   %u\r\n", (unsigned)cfg.animSpeed);
    Serial.printf("Color:        %s\r\n", ns_core_json::colorToHex(cfg.digitsColor).c_str());
    Serial.printf("Manual text:  '%s'\r\n", cfg.manualText.c_str());
    Serial.printf("Ticker text:  '%s'\r\n", cfg.tickerText.c_str());
    Serial.printf("Ticker:       %u ms/px, dir=%s\r\n",
                  (unsigned)cfg.tickerSpeed, kE7TickerDirNames[cfg.tickerDirection]);
    Serial.printf("Flip:         %s, speed=%s\r\n",
                  kE7FlipNames[cfg.digitFlip], kE7FlipSpeedNames[cfg.digitFlipSpeed]);
    Serial.printf("Font:         %s\r\n", cfg.fontFile.c_str());
    Serial.printf("Data pin:     %d\r\n", (int)cfg.dataPin);
    if (synced) {
        Serial.printf("Time:         %s (ntp sync ok)\r\n",
                      ns_device_electronica7_rgb::e7TimeHhMm(core_sys.timeNow()).c_str());
    } else {
        Serial.println("Time:         no valid source");
    }
}

// ============================================================
// effect <0..4|name> — число или имя эффекта
// ============================================================

void CLASS_DEVICE_E7RGB::s_cmdEffect() {
    String line = TerminalGetArgLine();
    if (line.length() == 0) {
        Serial.printf("Effect: %s\r\n", kE7EffectNames[device_electronica7_rgb._config.effect]);
        return;
    }

    long idx = -1;
    if (ns_core_terminal::parseIntStrict(line, idx, 0, E7_EFFECT_COLORCYCLE) == false) {
        idx = e7NameIndex(line, kE7EffectNames, 5);
    }
    if (idx < 0) {
        Serial.println("Usage: e7 effect <0..4|mono|rainbow|grad_static|grad_dynamic|colorcycle>");
        return;
    }

    device_electronica7_rgb.setEffect((uint8_t)idx);
    Serial.printf("Effect: %s (not saved).\r\n", kE7EffectNames[idx]);
}

// ============================================================
// brightness <0..255>
// ============================================================

void CLASS_DEVICE_E7RGB::s_cmdBrightness() {
    char* arg = term.getNext();
    long value = 0;
    if (arg == nullptr || term.getNext() != nullptr ||
        ns_core_terminal::parseIntStrict(String(arg), value, 0, 255) == false) {
        Serial.println("Usage: e7 brightness <0..255>");
        return;
    }

    device_electronica7_rgb.setBrightness((uint8_t)value);
    Serial.printf("Brightness: %ld (not saved).\r\n", value);
}

// ============================================================
// speed <1..50>
// ============================================================

void CLASS_DEVICE_E7RGB::s_cmdSpeed() {
    char* arg = term.getNext();
    long value = 0;
    if (arg == nullptr || term.getNext() != nullptr ||
        ns_core_terminal::parseIntStrict(String(arg), value, 1, 50) == false) {
        Serial.println("Usage: e7 speed <1..50>");
        return;
    }

    device_electronica7_rgb.setSpeed((uint8_t)value);
    Serial.printf("Anim speed: %ld (not saved).\r\n", value);
}

// ============================================================
// text <4 chars> — ручной текст ('0'..'9', '-', пробел)
// ============================================================

void CLASS_DEVICE_E7RGB::s_cmdText() {
    String raw = ns_core_terminal::trimCopy(TerminalGetArgLine());
    if (raw.length() == 0) {
        Serial.printf("Manual text: '%s'\r\n",
                      device_electronica7_rgb._config.manualText.c_str());
        return;
    }

    // Короткий текст — статично, длинный — автоматически бегущей строкой.
    device_electronica7_rgb.setShowText(raw);
    if (raw.length() <= E7_GLYPH_W) {
        Serial.printf("Manual text: '%s' (not saved).\r\n",
                      device_electronica7_rgb._config.manualText.c_str());
    } else {
        Serial.printf("Ticker text: '%s' (not saved).\r\n",
                      device_electronica7_rgb._config.tickerText.c_str());
    }
}

// ============================================================
// ticker [<text>|off|speed <ms>|dir <left|right>]
// ============================================================

void CLASS_DEVICE_E7RGB::s_cmdTicker() {
    String line = ns_core_terminal::trimCopy(TerminalGetArgLine());
    if (line.length() == 0) {
        const strE7RgbConfig& cfg = device_electronica7_rgb._config;
        Serial.printf("Ticker text:  '%s'\r\n", cfg.tickerText.c_str());
        Serial.printf("Ticker:       %u ms/px, dir=%s\r\n",
                      (unsigned)cfg.tickerSpeed, kE7TickerDirNames[cfg.tickerDirection]);
        return;
    }

    String head;
    String tail;
    e7SplitFirst(line, head, tail);
    String keyword = head;
    keyword.toLowerCase();

    if (keyword == "off") {
        if (tail.length() != 0) {
            Serial.println("Usage: e7 ticker off");
            return;
        }
        device_electronica7_rgb.setTickerText("");
        device_electronica7_rgb.setDisplayMode(E7_DISP_CLOCK);
        Serial.println("Ticker: off, clock shown (not saved).");
        return;
    }

    if (keyword == "speed") {
        long ms = 0;
        if (tail.length() == 0 || tail.indexOf(' ') >= 0 ||
            ns_core_terminal::parseIntStrict(tail, ms, E7_TICKER_SPEED_MIN, E7_TICKER_SPEED_MAX) == false) {
            Serial.printf("Usage: e7 ticker speed <%d..%d>\r\n", E7_TICKER_SPEED_MIN, E7_TICKER_SPEED_MAX);
            return;
        }
        device_electronica7_rgb.setTickerSpeed((uint8_t)ms);
        Serial.printf("Ticker speed: %ld ms/px (not saved).\r\n", ms);
        return;
    }

    if (keyword == "dir") {
        long dir = -1;
        if (tail.length() != 0 && tail.indexOf(' ') < 0) {
            dir = e7NameIndex(tail, kE7TickerDirNames, 2);
        }
        if (dir < 0) {
            Serial.println("Usage: e7 ticker dir <left|right>");
            return;
        }
        device_electronica7_rgb.setTickerDirection((uint8_t)dir);
        Serial.printf("Ticker direction: %s (not saved).\r\n", kE7TickerDirNames[dir]);
        return;
    }

    // Всё остальное — текст строки (пробелы допустимы).
    device_electronica7_rgb.setTickerText(line);
    device_electronica7_rgb.setDisplayMode(E7_DISP_TICKER);
    Serial.printf("Ticker text: '%s' (not saved).\r\n",
                  device_electronica7_rgb._config.tickerText.c_str());
}

// ============================================================
// flip <none|up|down> [slow|normal|fast]
// ============================================================

void CLASS_DEVICE_E7RGB::s_cmdFlip() {
    String line = ns_core_terminal::trimCopy(TerminalGetArgLine());
    if (line.length() == 0) {
        const strE7RgbConfig& cfg = device_electronica7_rgb._config;
        Serial.printf("Flip: %s, speed=%s\r\n",
                      kE7FlipNames[cfg.digitFlip], kE7FlipSpeedNames[cfg.digitFlipSpeed]);
        return;
    }

    String head;
    String tail;
    e7SplitFirst(line, head, tail);

    int dir = e7NameIndex(head, kE7FlipNames, 3);
    if (dir < 0) {
        Serial.println("Usage: e7 flip <none|up|down> [slow|normal|fast]");
        return;
    }

    int speed = device_electronica7_rgb._config.digitFlipSpeed;
    if (tail.length() != 0) {
        if (tail.indexOf(' ') >= 0) {
            Serial.println("Usage: e7 flip <none|up|down> [slow|normal|fast]");
            return;
        }
        speed = e7NameIndex(tail, kE7FlipSpeedNames, 3);
        if (speed < 0) {
            Serial.println("Usage: e7 flip <none|up|down> [slow|normal|fast]");
            return;
        }
    }

    device_electronica7_rgb.setDigitFlip((uint8_t)dir);
    if (tail.length() != 0) {
        device_electronica7_rgb.setDigitFlipSpeed((uint8_t)speed);
    }
    Serial.printf("Flip: %s, speed=%s (not saved).\r\n",
                  kE7FlipNames[dir], kE7FlipSpeedNames[speed]);
}

// ============================================================
// save / save_mode — единственные подкоманды, пишущие на FS
// ============================================================

void CLASS_DEVICE_E7RGB::s_cmdSave() {
    if (term.getNext() != nullptr) {
        Serial.println("Usage: e7 save");
        return;
    }
    bool ok = device_electronica7_rgb.saveConfig();
    Serial.println(ok ? "Saved." : "Save failed.");
}

void CLASS_DEVICE_E7RGB::s_cmdSaveMode() {
    if (term.getNext() != nullptr) {
        Serial.println("Usage: e7 save_mode");
        return;
    }
    bool ok = device_electronica7_rgb.saveModeConfig();
    Serial.println(ok ? "Bus mode saved." : "Save failed.");
}
