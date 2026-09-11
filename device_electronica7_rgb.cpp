#include "core_web/FSWebServerLib.h"

#include "core_ntp/NtpClientLib.h"
#include "core_json/core_json.h"

#include "device_electronica7_rgb.h"
#include "common_module.h"
#include "common/common.h"
#include "common/TimeLib.h"
#include "device_electronica7_rgb_version.h"
#include "core_sys/eertos.h"

#include <math.h>

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

// Отложенное применение изменений (паттерн module_rgb):
// вызывается только из main-loop, поэтому Show() здесь безопасен.
void CLASS_DEVICE_E7RGB::deferredApplyTask() {
    CLASS_DEVICE_E7RGB& d = device_electronica7_rgb;

    if (d._pendingReinit) {
        if (d._pendingDataPin < 0) { d._pendingDataPin = d._config.dataPin; }
        d._config.dataPin = d._pendingDataPin;
        d.destroyMatrix();
        d.initMatrix();
        d.saveConfig();
        d._pendingReinit = false;
        d._pendingSave = false;
        d._pendingApply = false;
        if (d._fs != NULL) { d.loadFont(); }
        d.applyMode();
        return;
    }

    if (d._pendingSave) {
        d.saveConfig();
        d._pendingSave = false;
    }
    if (d._fs != NULL) { d.loadFont(); }

    if (d._pendingApply) {
        // Скорректировать состояние отображения под изменённые настройки
        bool synced = (NTP.getLastNTPSync() > 0);
        if (d._config.mode != E7_MODE_WORK) {
            d._view = E7_VIEW_NORMAL;
        } else if (d._config.rainEnabled == 0 &&
                   (d._view == E7_VIEW_RAIN || d._view == E7_VIEW_SETTLE)) {
            d._view = E7_VIEW_NORMAL;
            d._forceRedraw = true;
            d._lastMinute = (uint8_t)minute();
        } else if (d._config.rainEnabled == 1 && d._view == E7_VIEW_NORMAL && synced == false) {
            d.startRain();
        }
        d.applyMode();
        d._pendingApply = false;
    }
}

// ============================================================
// Конфиг
// ============================================================

void CLASS_DEVICE_E7RGB::defaultConfig() {
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

// ============================================================
// Конкретная логика модуля
// ============================================================

bool CLASS_DEVICE_E7RGB::loadFont() {
    if (_fs == NULL) { return false; }
    if (ns_device_electronica7_rgb::e7FontPathOk(_config.fontFile) == false) { _config.fontFile = E7_FONT_DEFAULT; }
    // каталог шрифтов должен существовать для записи файла
    _fs->mkdir(E7_FONT_DIR);
    if (_fonts.loadOrCreate(*_fs, _config.fontFile.c_str())) { return true; }
    _fonts.loadDefault();
    return false;
}

void CLASS_DEVICE_E7RGB::initMatrix() {
    _matrix.setTopology(_config.origin, _config.direction, _config.layout);
    _matrix.init(_config.dataPin);
}

void CLASS_DEVICE_E7RGB::destroyMatrix() {
    _matrix.destroy();
}

void CLASS_DEVICE_E7RGB::applyMode() {
    if (_matrix.ready() == false) { return; }
    // Применяем актуальную топологию при каждой отрисовке: изменения
    // origin/direction/layout из веб-настроек вступают в силу сразу.
    _matrix.setTopology(_config.origin, _config.direction, _config.layout);
    if (_config.mode == E7_MODE_WORK) {
        if (_view == E7_VIEW_RAIN)      { drawRain(false, 0.0f, NULL); return; }
        if (_view == E7_VIEW_SETTLE)    { drawSettle(); return; }
        if (_view == E7_VIEW_TRANS)     { drawTransition(); return; }
        redrawFrame();
    } else {
        _view = E7_VIEW_NORMAL;
        renderManual();
    }
}

void CLASS_DEVICE_E7RGB::redrawFrame() {
    if (_matrix.ready() == false) { return; }
    if (NTP.getLastNTPSync() > 0) {
        renderDigits((uint8_t)hour(), (uint8_t)minute());
    } else {
        // NTP ещё не синхронизирован — прочерки («нет времени»)
        renderNoTime();
    }
}

void CLASS_DEVICE_E7RGB::renderDigits(uint8_t h, uint8_t m) {
    if (_matrix.ready() == false) { return; }
    _curH = h;
    _curM = m;
    uint8_t mask[E7_HEIGHT][E7_WIDTH];
    buildTimeMask(h, m, mask);
    drawMaskFx(mask);
}

void CLASS_DEVICE_E7RGB::renderNoTime() {
    if (_matrix.ready() == false) { return; }
    uint8_t mask[E7_HEIGHT][E7_WIDTH];
    memset(mask, 0, sizeof(mask));
    for (uint8_t d = 0; d < 4; d++) {
        blitGlyph((uint8_t)(d * E7_GLYPH_W), _fonts.glyph('-'), mask);
    }
    drawMaskFx(mask);
}

// Ручной режим: показывает 4 символа из _config.manualText
// ('0'..'9', '-' = прочерк, ' ' = пусто) для визуальной проверки цифр/шрифта.
void CLASS_DEVICE_E7RGB::renderManual() {
    if (_matrix.ready() == false) { return; }
    uint8_t mask[E7_HEIGHT][E7_WIDTH];
    memset(mask, 0, sizeof(mask));
    for (uint8_t d = 0; d < 4; d++) {
        char ch = (d < _config.manualText.length()) ? _config.manualText.charAt(d) : ' ';
        if (ch == ' ') { continue; }
        blitGlyph((uint8_t)(d * E7_GLYPH_W), _fonts.glyph(ch), mask);
    }
    drawMaskFx(mask);
}

// Наложить глиф в логическую маску: ряд 0 глифа = верх панели (y = E7_HEIGHT-1)
void CLASS_DEVICE_E7RGB::blitGlyph(uint8_t x0, const uint8_t* glyph, uint8_t mask[E7_HEIGHT][E7_WIDTH]) {
    if (glyph == NULL) { return; }
    for (int r = 0; r < E7_GLYPH_H; r++) {
        for (int c = 0; c < E7_GLYPH_W; c++) {
            if (glyph[r * E7_GLYPH_W + c] == 0) { continue; }
            int px = (int)x0 + c;
            int py = (E7_HEIGHT - 1) - r;   // логический y: 0 = низ
            if (px >= 0 && px < E7_WIDTH && py >= 0 && py < E7_HEIGHT) {
                mask[py][px] = 1;
            }
        }
    }
}

// ============================================================
// Переходы смены времени
// ============================================================

void CLASS_DEVICE_E7RGB::buildTimeMask(uint8_t h, uint8_t m, uint8_t mask[E7_HEIGHT][E7_WIDTH]) {
    memset(mask, 0, (size_t)(E7_HEIGHT * E7_WIDTH));
    uint8_t digits[4];
    digits[0] = (uint8_t)(h / 10);
    digits[1] = (uint8_t)(h % 10);
    digits[2] = (uint8_t)(m / 10);
    digits[3] = (uint8_t)(m % 10);
    for (uint8_t d = 0; d < 4; d++) {
        char ch = (char)('0' + (digits[d] % 10));
        blitGlyph((uint8_t)(d * E7_GLYPH_W), _fonts.glyph(ch), mask);
    }
}

// Вывести один пиксель с пер-пиксельным масштабом уровня (0..1)
void CLASS_DEVICE_E7RGB::compositePixel(int x, int y, uint32_t rgb, float level) {
    if (level <= 0.003f) { return; }
    if (level > 1.0f) { level = 1.0f; }
    uint8_t b = (uint8_t)((float)_config.brightness * level);
    _matrix.setPixel(x, y, rgb, b);
}

void CLASS_DEVICE_E7RGB::drawMaskFx(const uint8_t mask[E7_HEIGHT][E7_WIDTH]) {
    if (_matrix.ready() == false) { return; }
    _matrix.clear();
    for (int y = 0; y < E7_HEIGHT; y++) {
        for (int x = 0; x < E7_WIDTH; x++) {
            if (mask[y][x] == 0) { continue; }
            uint32_t color = ns_device_electronica7_rgb::e7FxColor(_config, x, y, _animPhase, _fxOrder);
            compositePixel(x, y, color, 1.0f);
        }
    }
    _matrix.show();
}

void CLASS_DEVICE_E7RGB::startTransition(uint8_t h, uint8_t m) {
    _prevH = _curH;
    _prevM = _curM;
    _curH = h;
    _curM = m;
    _transType = _config.timeFx;
    _transStartMs = millis();
    for (int y = 0; y < E7_HEIGHT; y++) {
        for (int x = 0; x < E7_WIDTH; x++) {
            _transRand[y][x] = (uint8_t)(ns_device_electronica7_rgb::e7Rand() % 255);
        }
    }
    _view = E7_VIEW_TRANS;
    drawTransition();
}

void CLASS_DEVICE_E7RGB::drawTransition() {
    if (_matrix.ready() == false) { return; }
    uint8_t prevMask[E7_HEIGHT][E7_WIDTH];
    uint8_t curMask[E7_HEIGHT][E7_WIDTH];
    buildTimeMask(_prevH, _prevM, prevMask);
    buildTimeMask(_curH, _curM, curMask);

    uint32_t dur = (uint32_t)_config.timeFxDur * 1000u;
    if (dur == 0) { dur = 1000u; }
    float t = (float)(millis() - _transStartMs) / (float)dur;
    if (t < 0.0f) { t = 0.0f; }
    if (t > 1.0f) { t = 1.0f; }

    // Фоновая заливка всего поля: плавный разгон в начале (~10%) и
    // гашение в конце (~25%), чтобы к финалу остались только цифры.
    float bgLevel = (float)(_config.timeFxBg > 10 ? 10 : _config.timeFxBg) / 10.0f;
    float bgFade = 1.0f;
    if (t < 0.10f)      { bgFade = t / 0.10f; }
    else if (t > 0.75f) { bgFade = (1.0f - t) / 0.25f; }
    if (bgFade < 0.0f) { bgFade = 0.0f; }
    if (bgFade > 1.0f) { bgFade = 1.0f; }
    float bg = bgLevel * bgFade;

    // Кадры перехода: цифры на полной яркости, всё остальное поле — фон
    float prevFrame[E7_HEIGHT][E7_WIDTH];
    float curFrame[E7_HEIGHT][E7_WIDTH];
    for (int y = 0; y < E7_HEIGHT; y++) {
        for (int x = 0; x < E7_WIDTH; x++) {
            prevFrame[y][x] = prevMask[y][x] ? 1.0f : bg;
            curFrame[y][x] = curMask[y][x] ? 1.0f : bg;
        }
    }

    _matrix.clear();
    for (int y = 0; y < E7_HEIGHT; y++) {
        for (int x = 0; x < E7_WIDTH; x++) {
            float level = 0.0f;
            bool white = false;
            switch (_transType) {
                case E7_TFX_FADE: {
                    if (t < 0.5f) { level = prevFrame[y][x] * (1.0f - 2.0f * t); }
                    else          { level = curFrame[y][x]  * (2.0f * t - 1.0f); }
                } break;
                case E7_TFX_WIPE_H:
                    level = (x < (int)(t * E7_WIDTH)) ? curFrame[y][x] : prevFrame[y][x];
                    break;
                case E7_TFX_WIPE_V:
                    // сверху вниз: верхние строки переключаются первыми
                    level = (y >= E7_HEIGHT - (int)(t * E7_HEIGHT)) ? curFrame[y][x] : prevFrame[y][x];
                    break;
                case E7_TFX_DISSOLVE:
                    level = (_transRand[y][x] < (uint8_t)(t * 255.0f)) ? curFrame[y][x] : prevFrame[y][x];
                    break;
                case E7_TFX_SLIDE_UP: {
                    int off = (int)(t * E7_HEIGHT + 0.5f);
                    level = ns_device_electronica7_rgb::e7SampleSlideUp(prevFrame, curFrame, x, y, off);
                } break;
                case E7_TFX_SLIDE_DOWN: {
                    int off = (int)(t * E7_HEIGHT + 0.5f);
                    if (y >= E7_HEIGHT - off) {
                        int sy = y - (E7_HEIGHT - off);
                        level = (sy >= 0 && sy < E7_HEIGHT) ? curFrame[sy][x] : 0.0f;
                    } else {
                        int sy = y + off;
                        level = (sy < E7_HEIGHT) ? prevFrame[sy][x] : 0.0f;
                    }
                } break;
                case E7_TFX_SLIDE_LEFT: {
                    int off = (int)(t * E7_WIDTH + 0.5f);
                    if (x >= E7_WIDTH - off) {
                        int sx = x - (E7_WIDTH - off);
                        level = (sx >= 0 && sx < E7_WIDTH) ? curFrame[y][sx] : 0.0f;
                    } else {
                        int sx = x + off;
                        level = (sx < E7_WIDTH) ? prevFrame[y][sx] : 0.0f;
                    }
                } break;
                case E7_TFX_SLIDE_RIGHT: {
                    int off = (int)(t * E7_WIDTH + 0.5f);
                    if (x < off) {
                        int sx = x + (E7_WIDTH - off);
                        level = (sx >= 0 && sx < E7_WIDTH) ? curFrame[y][sx] : 0.0f;
                    } else {
                        int sx = x - off;
                        level = (sx >= 0) ? prevFrame[y][sx] : 0.0f;
                    }
                } break;
                case E7_TFX_FLASH: {
                    if (t < 0.45f)      { level = prevFrame[y][x]; }
                    else if (t < 0.65f) { white = true; level = 1.0f; }
                    else                { level = curFrame[y][x]; }
                } break;
                case E7_TFX_DIGIT_FADE: {
                    int d = x / E7_GLYPH_W;
                    float seg = 1.0f / 4.0f;
                    float local = (t - (float)d * seg) / seg;
                    if (local <= 0.0f)      { level = prevFrame[y][x]; }
                    else if (local >= 1.0f) { level = curFrame[y][x]; }
                    else if (local < 0.5f)  { level = prevFrame[y][x] * (1.0f - 2.0f * local); }
                    else                    { level = curFrame[y][x]  * (2.0f * local - 1.0f); }
                } break;
                case E7_TFX_DIGIT_SLIDE: {
                    int d = x / E7_GLYPH_W;
                    float seg = 1.0f / 4.0f;
                    float local = (t - (float)d * seg) / seg;
                    if (local <= 0.0f)      { level = prevFrame[y][x]; }
                    else if (local >= 1.0f) { level = curFrame[y][x]; }
                    else {
                        int off = (int)(local * E7_HEIGHT + 0.5f);
                        level = ns_device_electronica7_rgb::e7SampleSlideUp(prevFrame, curFrame, x, y, off);
                    }
                } break;
                default:
                    level = curFrame[y][x];
                    break;
            }
            if (level <= 0.0f) { continue; }
            uint32_t color = white ? 0xFFFFFF : ns_device_electronica7_rgb::e7FxColor(_config, x, y, _animPhase, _fxOrder);
            compositePixel(x, y, color, level);
        }
    }
    _matrix.show();
}

// ============================================================
// «Дождь»: заставка до синхронизации NTP и оседание в цифры
// ============================================================

void CLASS_DEVICE_E7RGB::startRain() {
    _view = E7_VIEW_RAIN;
    _rainStartMs = millis();
    _rainRampStartMs = _rainStartMs;
    int activeTarget = ((int)_config.rainIntensity * E7_WIDTH) / 10;
    if (activeTarget < 1) { activeTarget = 1; }
    if (activeTarget > E7_WIDTH) { activeTarget = E7_WIDTH; }
    for (int i = 0; i < E7_WIDTH; i++) {
        _rainHead[i] = (float)(ns_device_electronica7_rgb::e7Rand() % (E7_HEIGHT + 3));
        _rainSpeed[i] = 0.35f + 0.05f * (float)_config.rainIntensity;
        _rainColOn[i] = false;
    }
    // Равномерно распределяем активные столбцы по ширине матрицы
    for (int k = 0; k < activeTarget; k++) {
        int col = (k * E7_WIDTH) / activeTarget;
        if (col >= E7_WIDTH) { col = E7_WIDTH - 1; }
        _rainColOn[col] = true;
    }
    drawRain(false, 0.0f, NULL);
}

void CLASS_DEVICE_E7RGB::drawRain(bool settleMode, float settleP, const uint8_t target[E7_HEIGHT][E7_WIDTH]) {
    if (_matrix.ready() == false) { return; }
    float levels[E7_HEIGHT][E7_WIDTH];
    memset(levels, 0, sizeof(levels));

    float ramp = (float)(millis() - _rainRampStartMs) / (float)E7_RAIN_RAMP_MS;
    if (ramp < 0.0f) { ramp = 0.0f; }
    if (ramp > 1.0f) { ramp = 1.0f; }
    if (settleP < 0.0f) { settleP = 0.0f; }
    if (settleP > 1.0f) { settleP = 1.0f; }

    float rainMul = settleMode ? (1.0f - settleP) : 1.0f;
    float digitMul = settleMode ? settleP : 0.0f;

    float dtTicks = (float)E7_FX_RENDER_MS / 50.0f;
    int trail = 2 + ((int)_config.rainIntensity / 3);

    for (int i = 0; i < E7_WIDTH; i++) {
        if (_rainColOn[i] == false) { continue; }
        _rainHead[i] -= _rainSpeed[i] * dtTicks;
        if (_rainHead[i] < -3.0f) {
            _rainHead[i] = (float)E7_HEIGHT + (float)(ns_device_electronica7_rgb::e7Rand() % 5);
        }
        int headY = (int)(_rainHead[i] + 0.5f);
        for (int k = 0; k < trail; k++) {
            int py = headY + k;
            if (py < 0 || py >= E7_HEIGHT) { continue; }
            float lv = (1.0f - (float)k / (float)trail) * ramp * rainMul;
            if (lv > levels[py][i]) { levels[py][i] = lv; }
        }
    }

    if (settleMode && target != NULL) {
        // Капли, совпавшие с будущими цифрами, «прилипают»: берём максимум уровней
        for (int y = 0; y < E7_HEIGHT; y++) {
            for (int x = 0; x < E7_WIDTH; x++) {
                if (target[y][x] && digitMul > levels[y][x]) { levels[y][x] = digitMul; }
            }
        }
    }

    _matrix.clear();
    for (int y = 0; y < E7_HEIGHT; y++) {
        for (int x = 0; x < E7_WIDTH; x++) {
            if (levels[y][x] <= 0.0f) { continue; }
            uint32_t color = ns_device_electronica7_rgb::e7FxColor(_config, x, y, _animPhase, _fxOrder);
            compositePixel(x, y, color, levels[y][x]);
        }
    }
    _matrix.show();
}

void CLASS_DEVICE_E7RGB::drawSettle() {
    uint8_t target[E7_HEIGHT][E7_WIDTH];
    buildTimeMask(_curH, _curM, target);
    uint32_t dur = (uint32_t)_config.rainSettleDur * 1000u;
    float p = (dur == 0) ? 1.0f : (float)(millis() - _settleStartMs) / (float)dur;
    if (p < 0.0f) { p = 0.0f; }
    if (p > 1.0f) { p = 1.0f; }
    drawRain(true, p, target);
}

// ============================================================
// Периодическая задача рендера (main-loop, E7_FX_RENDER_MS).
// Продвигает фазу цвета раз в E7RGB_ANIM_MS и рисует текущее
// состояние: дождь / оседание / переход / обычный кадр.
// Show() безопасен: loop-контекст.
// ============================================================
void e7rgbAnimTask() {
    CLASS_DEVICE_E7RGB& d = device_electronica7_rgb;

    bool phaseAdvanced = false;
    d._fxAccumMs += E7_FX_RENDER_MS;
    if (d._fxAccumMs >= E7RGB_ANIM_MS) {
        d._fxAccumMs -= E7RGB_ANIM_MS;
        phaseAdvanced = true;
        float next = d._animPhase + (float)d._config.animSpeed;
        if (next >= 360.0f) {
            next -= 360.0f;
            d._fxLap++;
            // при «случайном» обходе палитры каждый круг — новый порядок
            if (d._config.effect == E7_EFFECT_COLORCYCLE && d._config.cycleMode == E7_CYCLE_RANDOM) {
                uint8_t n = d._config.colorsCount;
                if (n < 1) { n = 1; }
                if (n > E7_FX_MAX_COLORS) { n = E7_FX_MAX_COLORS; }
                ns_device_electronica7_rgb::e7ShuffleOrder(d._fxOrder, n);
            }
        }
        d._animPhase = next;
    }

    if (d._view == E7_VIEW_RAIN) {
        d.drawRain(false, 0.0f, NULL);
    } else if (d._view == E7_VIEW_SETTLE) {
        d.drawSettle();
    } else if (d._view == E7_VIEW_TRANS) {
        d.drawTransition();
    } else if (phaseAdvanced && ns_device_electronica7_rgb::e7FxAnimated(d._config.effect)) {
        // обычный кадр перерисовываем только при смене фазы цвета
        d.applyMode();
    }

    SetTimerTask(e7rgbAnimTask, E7_FX_RENDER_MS);
}

// ============================================================
// Периодическая задача (1 сек, main-loop)
// Конечный автомат отображения: дождь -> оседание -> обычный
// режим с переходами при смене времени.
// ============================================================
void e7rgbSecondTask() {
    CLASS_DEVICE_E7RGB& d = device_electronica7_rgb;

    if (d._config.mode != E7_MODE_WORK) {
        SetTimerTask(e7rgbSecondTask, 1000);
        return;
    }

    bool synced = (NTP.getLastNTPSync() > 0);

    if (d._view == E7_VIEW_RAIN) {
        if (synced) {
            d._ntpWasSynced = true;
            uint32_t elapsed = millis() - d._rainStartMs;
            if (elapsed >= (uint32_t)d._config.rainDurMin * 1000u) {
                d._curH = (uint8_t)hour();
                d._curM = (uint8_t)minute();
                d._settleStartMs = millis();
                if (d._config.rainSettleDur == 0) {
                    d._view = E7_VIEW_NORMAL;
                    d._lastMinute = d._curM;
                    d.applyMode();
                } else {
                    d._view = E7_VIEW_SETTLE;
                }
            }
        }
        SetTimerTask(e7rgbSecondTask, 1000);
        return;
    }

    if (d._view == E7_VIEW_SETTLE) {
        uint32_t dur = (uint32_t)d._config.rainSettleDur * 1000u;
        if (dur == 0 || (millis() - d._settleStartMs) >= dur) {
            d._view = E7_VIEW_NORMAL;
            d._curH = (uint8_t)hour();
            d._curM = (uint8_t)minute();
            d._lastMinute = d._curM;
            d.applyMode();
        }
        SetTimerTask(e7rgbSecondTask, 1000);
        return;
    }

    if (d._view == E7_VIEW_TRANS) {
        uint32_t dur = (uint32_t)d._config.timeFxDur * 1000u;
        if (dur == 0 || (millis() - d._transStartMs) >= dur) {
            d._view = E7_VIEW_NORMAL;
            d._lastMinute = d._curM;
            d.applyMode();
        }
        SetTimerTask(e7rgbSecondTask, 1000);
        return;
    }

    if (synced && !d._ntpWasSynced) {
        d._ntpWasSynced = true;
        d._forceRedraw = true;
    }
    uint8_t h = (uint8_t)hour();
    uint8_t m = (uint8_t)minute();
    if (d._forceRedraw || m != d._lastMinute) {
        bool immediate = d._forceRedraw;   // первое время/смена настроек — без перехода
        d._forceRedraw = false;
        if (!immediate && synced && d._config.timeFx != E7_TFX_OFF && d._config.timeFxFreq > 0 &&
            (m % d._config.timeFxFreq) == 0) {
            d.startTransition(h, m);
        } else {
            d._lastMinute = m;
            d._curH = h;
            d._curM = m;
            d.redrawFrame();
        }
    }

    SetTimerTask(e7rgbSecondTask, 1000);
}
