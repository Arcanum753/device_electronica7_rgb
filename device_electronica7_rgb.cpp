#include "core_web/FSWebServerLib.h"

#include "core_ntp/NtpClientLib.h"
#include "core_json/core_json.h"

#include "device_electronica7_rgb.h"
#include "common_module.h"
#include "common/common.h"
#include "common/TimeLib.h"
#include "device_electronica7_rgb_version.h"
#include "core_state/core_state.h"
#include "core_sys/eertos.h"

// ============================================================
// Глобальные объекты и переменные
// ============================================================

CLASS_DEVICE_E7RGB device_electronica7_rgb;

CLASS_DEVICE_E7RGB::CLASS_DEVICE_E7RGB() {
    _fs = NULL;
    _lastMinute = 0xFF;
    _ntpWasSynced = false;
    _forceRedraw = false;
    _pendingReinit = false;
    _pendingSave = false;
    _pendingApply = false;
    _pendingDataPin = -1;
    _animPhase = 0.0f;
    _fxLap = 0;
    for (int i = 0; i < E7_FX_MAX_COLORS; i++) { _fxOrder[i] = (uint8_t)i; }

    _view = E7_VIEW_NORMAL;
    _curH = 0; _curM = 0;
    _prevH = 0; _prevM = 0;
    _transType = E7_TFX_OFF;
    _transStartMs = 0;
    _settleStartMs = 0;
    _rainStartMs = 0;
    _rainRampStartMs = 0;
    _fxAccumMs = 0;
    for (int i = 0; i < E7_WIDTH; i++) {
        _rainHead[i] = 0.0f;
        _rainSpeed[i] = 0.0f;
        _rainColOn[i] = false;
    }
    memset(_transRand, 0, sizeof(_transRand));
}

// Forward-объявления свободных функций, используемых в шаблонном блоке
void e7rgbSecondTask();
void e7rgbAnimTask();

// ============================================================
// setFs()
// ============================================================
void CLASS_DEVICE_E7RGB::setFs(fs::LittleFSFS* fs) {
    _fs = fs;
}

// ============================================================
// begin()
// ============================================================
void CLASS_DEVICE_E7RGB::begin() {
    DEBUGE7RGB("%s\r\n", __FUNCTION__);

    _lastMinute = 0xFF;
    _ntpWasSynced = false;
    _forceRedraw = false;
    _pendingReinit = false;
    _pendingSave = false;
    _pendingApply = false;
    _fxLap = 0;
    for (int i = 0; i < E7_FX_MAX_COLORS; i++) { _fxOrder[i] = (uint8_t)i; }
    ns_device_electronica7_rgb::e7SeedRng();

    _view = E7_VIEW_NORMAL;
    _curH = 0; _curM = 0;
    _prevH = 0; _prevM = 0;
    _transType = E7_TFX_OFF;
    _settleStartMs = 0;
    _fxAccumMs = 0;
    memset(_transRand, 0, sizeof(_transRand));

    defaultConfig();
    if (loadConfig() == false) { saveConfig(); }

    // Публикуем фактические значения конфига после его загрузки.
    core_state.signal("e7.mode", BusValue::en(_config.busMode));
    core_state.signal("e7.effect", BusValue::en(_config.effect));
    core_state.signal("e7.brightness", BusValue::i32(_config.brightness));
    core_state.signal("e7.speed", BusValue::i32(_config.animSpeed));
    core_state.signal("e7.color", BusValue::i32((int32_t)_config.digitsColor));
    core_state.signal("e7.text", BusValue::str(_config.manualText));

    if (_fs != NULL) { loadFont(); }

    initMatrix();

    _animPhase = 0.0f;
    if (_config.mode == E7_MODE_WORK && _config.rainEnabled == 1 && NTP.getLastNTPSync() == 0) {
        startRain();
    } else {
        applyMode();
    }

    SetTimerTask(e7rgbSecondTask, 1000);
    SetTimerTask(e7rgbAnimTask, E7_FX_RENDER_MS);
}

void CLASS_DEVICE_E7RGB::begin(ModContext& ctx) {
    _fs = ctx.fs;
    begin();
}

// ============================================================
// register_resources() — публикация ресурсов на шине core_state
// ============================================================
static const char* const e7BusModeNames[3] = { "off", "auto", "macro" };
static const char* const e7EffectNames[5] = {
    "mono", "rainbow", "grad_static", "grad_dynamic", "colorcycle",
};

static int e7ArgInt(const BusValue* a) {
    if (a == nullptr) { return 0; }
    if (a->kind == BusValue::F32) { return (int)a->f; }
    if (a->kind == BusValue::BOOL) { return a->b ? 1 : 0; }
    return (int)a->i;
}

static int e7BusEffect(void*, int argc, const BusValue* a, BusValue&) {
    if (argc < 1) { return BUS_ERR_BAD_ARGC; }
    return device_electronica7_rgb.setEffect((uint8_t)e7ArgInt(&a[0])) ? BUS_OK : BUS_ERR_BAD_VALUE;
}
static int e7BusBrightness(void*, int argc, const BusValue* a, BusValue&) {
    if (argc < 1) { return BUS_ERR_BAD_ARGC; }
    return device_electronica7_rgb.setBrightness((uint8_t)e7ArgInt(&a[0])) ? BUS_OK : BUS_ERR_BAD_VALUE;
}
static int e7BusSpeed(void*, int argc, const BusValue* a, BusValue&) {
    if (argc < 1) { return BUS_ERR_BAD_ARGC; }
    return device_electronica7_rgb.setSpeed((uint8_t)e7ArgInt(&a[0])) ? BUS_OK : BUS_ERR_BAD_VALUE;
}
static int e7BusColor(void*, int argc, const BusValue* a, BusValue&) {
    if (argc < 1) { return BUS_ERR_BAD_ARGC; }
    return device_electronica7_rgb.setDigitsColor((uint32_t)e7ArgInt(&a[0])) ? BUS_OK : BUS_ERR_BAD_VALUE;
}
static int e7BusText(void*, int argc, const BusValue* a, BusValue&) {
    if (argc < 1) { return BUS_ERR_BAD_ARGC; }
    return device_electronica7_rgb.setManualText(a[0].s) ? BUS_OK : BUS_ERR_BAD_VALUE;
}
static int e7BusMode(void*, int argc, const BusValue* a, BusValue&) {
    if (argc < 1) { return BUS_ERR_BAD_ARGC; }
    return device_electronica7_rgb.setBusMode((uint8_t)e7ArgInt(&a[0])) ? BUS_OK : BUS_ERR_BAD_VALUE;
}
static int e7BusSave(void*, int argc, const BusValue* a, BusValue&) {
    (void)argc; (void)a;
    device_electronica7_rgb.saveNow();
    return BUS_OK;
}

void CLASS_DEVICE_E7RGB::register_resources() {
    DEBUGE7RGB("%s\r\n", __FUNCTION__);

    core_state.regEnum("mode", 3, e7BusModeNames, "device control mode (off/auto/macro)");
    core_state.regEnum("effect", 5, e7EffectNames, "color effect");
    core_state.regState("brightness", BusValue::I32, "brightness 0..255", true);
    core_state.regState("speed", BusValue::I32, "animation speed 1..50", true);
    core_state.regState("color", BusValue::I32, "digits color 0xRRGGBB", true);
    core_state.regState("text", BusValue::STR, "manual text (4 chars)", true);

    core_state.regFunc("mode", "i->", "set control mode", e7BusMode, nullptr);
    core_state.regFunc("effect", "i->", "set effect", e7BusEffect, nullptr);
    core_state.regFunc("brightness", "i->", "set brightness", e7BusBrightness, nullptr);
    core_state.regFunc("speed", "i->", "set animation speed", e7BusSpeed, nullptr);
    core_state.regFunc("color", "i->", "set digits color", e7BusColor, nullptr);
    core_state.regFunc("text", "s->", "set manual text", e7BusText, nullptr);
    core_state.regFunc("save", "->", "persist current settings to config", e7BusSave, nullptr);
    // Значения публикуются в begin() после loadConfig().
}

// Публичные сеттеры для шины: меняют значения в памяти и применяют отложенно,
// но НЕ сохраняют конфиг (сохранение — handleSave/терминал/явный e7.save).
bool CLASS_DEVICE_E7RGB::setEffect(uint8_t e) {
    if (e > E7_EFFECT_COLORCYCLE) { return false; }
    _config.effect = e;
    _forceRedraw = true;
    _pendingApply = true;
    SetTask(deferredApplyTask);
    core_state.signal("e7.effect", BusValue::en(e));
    return true;
}

bool CLASS_DEVICE_E7RGB::setBrightness(uint8_t b) {
    _config.brightness = b;
    _forceRedraw = true;
    _pendingApply = true;
    SetTask(deferredApplyTask);
    core_state.signal("e7.brightness", BusValue::i32(b));
    return true;
}

bool CLASS_DEVICE_E7RGB::setSpeed(uint8_t s) {
    if (s < 1 || s > 50) { return false; }
    _config.animSpeed = s;
    _pendingApply = true;
    SetTask(deferredApplyTask);
    core_state.signal("e7.speed", BusValue::i32(s));
    return true;
}

bool CLASS_DEVICE_E7RGB::setDigitsColor(uint32_t c) {
    _config.digitsColor = c & 0xFFFFFF;
    _forceRedraw = true;
    _pendingApply = true;
    SetTask(deferredApplyTask);
    core_state.signal("e7.color", BusValue::i32((int32_t)_config.digitsColor));
    return true;
}

bool CLASS_DEVICE_E7RGB::setManualText(const String& t) {
    String s = t.substring(0, 4);
    _config.manualText = s;
    _config.mode = E7_MODE_MANUAL;
    _forceRedraw = true;
    _pendingApply = true;
    SetTask(deferredApplyTask);
    core_state.signal("e7.text", BusValue::str(s));
    return true;
}

bool CLASS_DEVICE_E7RGB::setBusMode(uint8_t m) {
    if (m > E7_BUS_MACRO) { return false; }
    _config.busMode = m;
    _forceRedraw = true;
    _pendingApply = true;
    SetTask(deferredApplyTask);
    core_state.signal("e7.mode", BusValue::en(m));
    return true;
}

uint8_t CLASS_DEVICE_E7RGB::getBusMode() {
    return _config.busMode;
}

void CLASS_DEVICE_E7RGB::saveNow() {
    _pendingSave = true;
    SetTask(deferredApplyTask);
}

// ============================================================
// web_Init()
// Все эндпоинты — GET (только по необходимости возможен POST,
// для этого устройства ничего требующего POST нет).
// ============================================================
void CLASS_DEVICE_E7RGB::web_Init() {
    DEBUGE7RGB("%s\r\n", __FUNCTION__);

    ESPHTTPServer.on("/e7rgb/save", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!ESPHTTPServer.checkAuth(request)) { return request->requestAuthentication(); }
        this->handleSave(request);
    });

    ESPHTTPServer.on("/e7rgb/info", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!ESPHTTPServer.checkAuth(request)) { return request->requestAuthentication(); }
        this->handleInfo(request);
    });

    ESPHTTPServer.on("/e7rgb/fonts", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if (!ESPHTTPServer.checkAuth(request)) { return request->requestAuthentication(); }
        this->handleFonts(request);
    });

    ESPHTTPServer.on("/e7rgb/ver", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->html_ver_get(request);
    });
}

// ============================================================
// Веб-обработчики
// ============================================================

void CLASS_DEVICE_E7RGB::handleInfo(AsyncWebServerRequest *request) {
    DEBUGE7RGB("%s\r\n", __FUNCTION__);
    String values = "";

    values += "busMode|"       + String(_config.busMode)       + "|input\n";
    values += "mode|"          + String(_config.mode)          + "|input\n";
    values += "dataPin|"       + String(_config.dataPin)       + "|input\n";
    values += "brightness|"    + String(_config.brightness)    + "|input\n";
    values += "effect|"        + String(_config.effect)        + "|input\n";
    values += "effectDir|"     + String(_config.effectDir)     + "|input\n";
    values += "digitsColor|"   + ns_device_electronica7_rgb::e7HexColor(_config.digitsColor) + "|input\n";
    values += "digitsColor2|"  + ns_device_electronica7_rgb::e7HexColor(_config.digitsColor2) + "|input\n";
    values += "animSpeed|"     + String(_config.animSpeed)     + "|input\n";
    values += "colorsCount|"   + String(_config.colorsCount)   + "|input\n";
    values += "cycleMode|"     + String(_config.cycleMode)     + "|input\n";
    for (uint8_t i = 0; i < E7_FX_MAX_COLORS; i++) {
        String pn = "palette" + String(i);
        values += pn + "|" + ns_device_electronica7_rgb::e7HexColor(_config.palette[i]) + "|input\n";
    }
    values += "origin|"        + String(_config.origin)        + "|input\n";
    values += "direction|"     + String(_config.direction)     + "|input\n";
    values += "layout|"        + String(_config.layout)        + "|input\n";
    values += "fontFile|"      + _config.fontFile              + "|input\n";
    values += "manualText|"    + _config.manualText            + "|input\n";
    values += "timeFx|"        + String(_config.timeFx)        + "|input\n";
    values += "timeFxFreq|"    + String(_config.timeFxFreq)    + "|input\n";
    values += "timeFxDur|"     + String(_config.timeFxDur)     + "|input\n";
    values += "timeFxBg|"      + String(_config.timeFxBg)      + "|input\n";
    values += "rainEnabled|"   + String(_config.rainEnabled)   + "|input\n";
    values += "rainDurMin|"    + String(_config.rainDurMin)    + "|input\n";
    values += "rainIntensity|" + String(_config.rainIntensity) + "|input\n";
    values += "rainSettleDur|" + String(_config.rainSettleDur) + "|input\n";

    bool synced = (NTP.getLastNTPSync() > 0);
    values += "x_ntp_sync|"    + String(synced ? 1 : 0)        + "|div\n";
    values += "x_time|"        + (synced ? ns_device_electronica7_rgb::e7TimeHhMm() : String("—")) + "|div\n";

    request->send(200, "text/plain", values);
}

void CLASS_DEVICE_E7RGB::handleSave(AsyncWebServerRequest *request) {
    DEBUGE7RGB("%s\r\n", __FUNCTION__);

    if (request->args() == 0) { request->send(400, "text/plain", "No args"); return; }

    for (uint8_t i = 0; i < request->args(); i++) {
        String name = request->argName(i);
        String val  = request->arg(i);
        DEBUGE7RGB("Arg %d: %s %s\r\n", i, name.c_str(), val.c_str());

        if (name == "busMode") {
            // TODO: уточнить — восстанавливать config при выходе из macro?
            _config.busMode = (uint8_t)constrain(val.toInt(), E7_BUS_OFF, E7_BUS_MACRO);
            core_state.signal("e7.mode", BusValue::en(_config.busMode));
            continue;
        }
        if (name == "mode") {
            _config.mode = (uint8_t)constrain(val.toInt(), E7_MODE_WORK, E7_MODE_MANUAL);
            continue;
        }
        if (name == "dataPin") {
            int16_t pin = (int16_t)val.toInt();
            if (pin >= 0 && pin < 34 && pin != _config.dataPin) {
                _pendingDataPin = pin;
                _pendingReinit = true;
            }
            continue;
        }
        if (name == "brightness") {
            _config.brightness = (uint8_t)constrain(val.toInt(), 0, 255);
            continue;
        }
        if (name == "effect") {
            _config.effect = (uint8_t)constrain(val.toInt(), E7_EFFECT_MONO, E7_EFFECT_COLORCYCLE);
            continue;
        }
        if (name == "effectDir") {
            _config.effectDir = (uint8_t)constrain(val.toInt(), 0, 5);
            continue;
        }
        if (name == "colorsCount") {
            _config.colorsCount = (uint8_t)constrain(val.toInt(), 1, E7_FX_MAX_COLORS);
            continue;
        }
        if (name == "cycleMode") {
            _config.cycleMode = (uint8_t)constrain(val.toInt(), E7_CYCLE_SEQUENTIAL, E7_CYCLE_RAINBOW);
            continue;
        }
        if (name.startsWith("palette")) {
            int idx = name.substring(7).toInt();   // palette0..palette7
            if (idx >= 0 && idx < E7_FX_MAX_COLORS) {
                _config.palette[idx] = ns_device_electronica7_rgb::e7HexStringToUint32(val);
            }
            continue;
        }
        if (name == "digitsColor") {
            _config.digitsColor = ns_device_electronica7_rgb::e7HexStringToUint32(val);
            continue;
        }
        if (name == "digitsColor2") {
            _config.digitsColor2 = ns_device_electronica7_rgb::e7HexStringToUint32(val);
            continue;
        }
        if (name == "animSpeed") {
            _config.animSpeed = (uint8_t)constrain(val.toInt(), 1, 50);
            continue;
        }
        if (name == "origin") {
            _config.origin = (uint8_t)constrain(val.toInt(), 0, 3);
            continue;
        }
        if (name == "direction") {
            _config.direction = (uint8_t)constrain(val.toInt(), 0, 3);
            continue;
        }
        if (name == "layout") {
            _config.layout = (uint8_t)constrain(val.toInt(), 0, 1);
            continue;
        }
        if (name == "fontFile") {
            if (ns_device_electronica7_rgb::e7FontPathOk(val)) { _config.fontFile = val; }
            continue;
        }
        if (name == "manualText") {
            // Ручной режим: ровно 4 символа из {'0'..'9','-',' '}
            String out;
            for (uint16_t i = 0; i < val.length() && out.length() < 4; i++) {
                char ch = val.charAt(i);
                if ((ch >= '0' && ch <= '9') || ch == '-' || ch == ' ') { out += ch; }
            }
            while (out.length() < 4) { out += ' '; }
            _config.manualText = out;
            continue;
        }
        if (name == "timeFx") {
            _config.timeFx = (uint8_t)constrain(val.toInt(), 0, E7_TFX_COUNT - 1);
            continue;
        }
        if (name == "timeFxFreq") {
            int f = val.toInt();
            if (ns_device_electronica7_rgb::e7TfxFreqOk(f) == false) { f = E7_TFX_FREQ_DEFAULT; }
            _config.timeFxFreq = (uint8_t)f;
            continue;
        }
        if (name == "timeFxDur") {
            _config.timeFxDur = (uint8_t)constrain(val.toInt(), 1, 10);
            continue;
        }
        if (name == "timeFxBg") {
            _config.timeFxBg = (uint8_t)constrain(val.toInt(), 0, 10);
            continue;
        }
        if (name == "rainEnabled") {
            _config.rainEnabled = (uint8_t)constrain(val.toInt(), 0, 1);
            continue;
        }
        if (name == "rainDurMin") {
            _config.rainDurMin = (uint8_t)constrain(val.toInt(), 1, 10);
            continue;
        }
        if (name == "rainIntensity") {
            _config.rainIntensity = (uint8_t)constrain(val.toInt(), 1, 10);
            continue;
        }
        if (name == "rainSettleDur") {
            _config.rainSettleDur = (uint8_t)constrain(val.toInt(), 0, 10);
            continue;
        }
    }

    if (_pendingReinit) {
        _pendingApply = true;
    } else {
        // TODO: уточнить — сохранять только изменённые поля?
        _pendingSave = true;
        _pendingApply = true;
    }

    request->send(200, "text/plain", "OK");
    if (_pendingApply || _pendingSave) {
        SetTask(deferredApplyTask);
    }
}

void CLASS_DEVICE_E7RGB::handleFonts(AsyncWebServerRequest *request) {
    String out = "digital7.fnt\n";   // гарантия, что список не пуст
    if (_fs != NULL) {
        _fonts.listFonts(*_fs, E7_FONT_DIR, out);
        if (out.length() == 0) { out = "digital7.fnt\n"; }
    }
    request->send(200, "text/plain", out);
}

// ============================================================
// Конфиг
// ============================================================

void CLASS_DEVICE_E7RGB::defaultConfig() {
    _config.busMode     = E7_BUS_AUTO;
    _config.mode        = E7_MODE_WORK;
    _config.dataPin     = 16;
    _config.brightness  = 25;
    _config.effect      = E7_EFFECT_MONO;
    _config.effectDir   = E7_FXDIR_TL;
    _config.digitsColor = 0xFF0000;
    _config.digitsColor2 = 0x0000FF;
    _config.animSpeed   = 45;
    _config.colorsCount = 4;
    _config.cycleMode   = E7_CYCLE_SEQUENTIAL;
    _config.palette[0]  = 0xFF0000;
    _config.palette[1]  = 0xFFFF00;
    _config.palette[2]  = 0x00FF00;
    _config.palette[3]  = 0x00FFFF;
    _config.palette[4]  = 0x0000FF;
    _config.palette[5]  = 0x8000FF;
    _config.palette[6]  = 0xFF8000;
    _config.palette[7]  = 0xFFFFFF;
    _config.origin      = E7_ORIGIN_BOTTOM_LEFT;   // спаянная матрица: первый LED внизу слева
    _config.direction   = E7_DIR_UP;               // порядок: снизу вверх, слева направо
    _config.layout      = E7_LAYOUT_PARALLEL;      // развёртка: параллельно (по умолчанию)
    _config.timeFx      = E7_TFX_FADE;
    _config.timeFxFreq  = E7_TFX_FREQ_DEFAULT;
    _config.timeFxDur   = 2;
    _config.timeFxBg    = 3;
    _config.rainEnabled = 1;
    _config.rainDurMin  = 2;
    _config.rainIntensity = 6;
    _config.rainSettleDur = 1;
    _config.fontFile    = E7_FONT_DEFAULT;
    _config.manualText  = "0000";
}

bool CLASS_DEVICE_E7RGB::loadConfig() {
    DEBUGE7RGB("%s\r\n", __FUNCTION__);
    JsonDocument doc;
    if (core_json.jsonFileLoadDoc(CONFIG_FILE_E7RGB, doc) == false) { return false; }

    _config.busMode     = (uint8_t)constrain(doc["busMode"].as<int>(), E7_BUS_OFF, E7_BUS_MACRO);
    _config.mode        = (uint8_t)constrain(doc["mode"].as<int>(), E7_MODE_WORK, E7_MODE_MANUAL);
    _config.dataPin     = (int16_t)constrain(doc["dataPin"].as<int>(), -1, 33);
    _config.brightness  = (uint8_t)constrain(doc["brightness"].as<int>(), 0, 255);
    _config.effect      = (uint8_t)constrain(doc["effect"].as<int>(), E7_EFFECT_MONO, E7_EFFECT_COLORCYCLE);
    _config.effectDir   = (uint8_t)constrain(doc["effectDir"].as<int>(), 0, 5);
    _config.digitsColor = doc["digitsColor"].as<uint32_t>() & 0xFFFFFF;
    _config.digitsColor2 = doc["digitsColor2"].as<uint32_t>() & 0xFFFFFF;
    _config.animSpeed   = (uint8_t)constrain(doc["animSpeed"].as<int>(), 1, 50);
    _config.colorsCount = (uint8_t)constrain(doc["colorsCount"].as<int>(), 1, E7_FX_MAX_COLORS);
    _config.cycleMode   = (uint8_t)constrain(doc["cycleMode"].as<int>(), E7_CYCLE_SEQUENTIAL, E7_CYCLE_RAINBOW);

    if (doc["palette"].is<JsonArray>()) {
        JsonArray arr = doc["palette"].as<JsonArray>();
        for (uint8_t i = 0; i < E7_FX_MAX_COLORS; i++) {
            if (i < arr.size()) { _config.palette[i] = arr[i].as<uint32_t>() & 0xFFFFFF; }
        }
    }
    _config.origin      = (uint8_t)constrain(doc["origin"].as<int>(), 0, 3);
    _config.direction   = (uint8_t)constrain(doc["direction"].as<int>(), 0, 3);
    _config.layout      = (uint8_t)constrain(doc["layout"].as<int>(), 0, 1);

    // Новые поля читаем только при наличии ключа: у старых конфигов их нет,
    // поэтому должны сохраниться значения из defaultConfig().
    if (doc["timeFx"].is<int>()) {
        _config.timeFx = (uint8_t)constrain(doc["timeFx"].as<int>(), 0, E7_TFX_COUNT - 1);
    }
    if (doc["timeFxFreq"].is<int>()) {
        int f = doc["timeFxFreq"].as<int>();
        if (ns_device_electronica7_rgb::e7TfxFreqOk(f) == false) { f = E7_TFX_FREQ_DEFAULT; }
        _config.timeFxFreq = (uint8_t)f;
    }
    if (doc["timeFxDur"].is<int>()) {
        _config.timeFxDur = (uint8_t)constrain(doc["timeFxDur"].as<int>(), 1, 10);
    }
    if (doc["timeFxBg"].is<int>()) {
        _config.timeFxBg = (uint8_t)constrain(doc["timeFxBg"].as<int>(), 0, 10);
    }
    if (doc["rainEnabled"].is<int>()) {
        _config.rainEnabled = (uint8_t)constrain(doc["rainEnabled"].as<int>(), 0, 1);
    }
    if (doc["rainDurMin"].is<int>()) {
        _config.rainDurMin = (uint8_t)constrain(doc["rainDurMin"].as<int>(), 1, 10);
    }
    if (doc["rainIntensity"].is<int>()) {
        _config.rainIntensity = (uint8_t)constrain(doc["rainIntensity"].as<int>(), 1, 10);
    }
    if (doc["rainSettleDur"].is<int>()) {
        _config.rainSettleDur = (uint8_t)constrain(doc["rainSettleDur"].as<int>(), 0, 10);
    }

    _config.fontFile = doc["fontFile"].as<String>();
    if (ns_device_electronica7_rgb::e7FontPathOk(_config.fontFile) == false) { _config.fontFile = E7_FONT_DEFAULT; }

    _config.manualText = doc["manualText"].as<String>();
    if (_config.manualText.length() != 4) { _config.manualText = "0000"; }

    DEBUGE7RGB("dataPin: %d, mode: %d, brightness: %d, font: %s\r\n",
               _config.dataPin, _config.mode, _config.brightness, _config.fontFile.c_str());
    return true;
}

bool CLASS_DEVICE_E7RGB::saveConfig() {
    DEBUGE7RGB("%s\r\n", __FUNCTION__);
    JsonDocument doc;
    core_json.jsonFileLoadDoc(CONFIG_FILE_E7RGB, doc);

    doc["busMode"]     = _config.busMode;
    doc["mode"]        = _config.mode;
    doc["dataPin"]     = _config.dataPin;
    doc["brightness"]  = _config.brightness;
    doc["effect"]      = _config.effect;
    doc["effectDir"]   = _config.effectDir;
    doc["digitsColor"] = _config.digitsColor;
    doc["digitsColor2"] = _config.digitsColor2;
    doc["animSpeed"]   = _config.animSpeed;
    doc["colorsCount"] = _config.colorsCount;
    doc["cycleMode"]   = _config.cycleMode;

    JsonArray palArr = doc["palette"].to<JsonArray>();
    palArr.clear();
    for (uint8_t i = 0; i < E7_FX_MAX_COLORS; i++) {
        palArr.add(_config.palette[i]);
    }

    doc.remove("cells");   // старые поля координатной сетки не сохраняем
    doc["origin"]      = _config.origin;
    doc["direction"]   = _config.direction;
    doc["layout"]      = _config.layout;
    doc["timeFx"]        = _config.timeFx;
    doc["timeFxFreq"]    = _config.timeFxFreq;
    doc["timeFxDur"]     = _config.timeFxDur;
    doc["timeFxBg"]      = _config.timeFxBg;
    doc["rainEnabled"]   = _config.rainEnabled;
    doc["rainDurMin"]    = _config.rainDurMin;
    doc["rainIntensity"] = _config.rainIntensity;
    doc["rainSettleDur"] = _config.rainSettleDur;
    doc["fontFile"]    = _config.fontFile;
    doc["manualText"]  = _config.manualText;

    return core_json.jsonFileSaveDoc(CONFIG_FILE_E7RGB, doc);
}

// ============================================================
// Версионные методы
// ============================================================

String CLASS_DEVICE_E7RGB::getVersionStr() {
    return String(DEVICE_ELECTRONICA7_RGB_VERSION);
}

String CLASS_DEVICE_E7RGB::getGeneratedTime() {
    return String(DEVICE_ELECTRONICA7_RGB_GENERATED_TIME);
}

String CLASS_DEVICE_E7RGB::getCommitDateStr() {
    return String(DEVICE_ELECTRONICA7_RGB_COMMIT_DATE_STR);
}

void CLASS_DEVICE_E7RGB::html_ver_get(AsyncWebServerRequest *request) {
    DEBUGE7RGB("%s\r\n", __FUNCTION__);
    String values = "";
    values += "e7rgbversion|" + getVersionStr()    + "|div\n";
    values += "e7rgbgentime|" + getGeneratedTime() + "|div\n";
    values += "e7rgbgendate|" + getCommitDateStr() + "|div\n";
    request->send(200, "text/plain", values);
}
