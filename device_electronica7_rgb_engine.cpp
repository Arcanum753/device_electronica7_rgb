#include "core_web/FSWebServerLib.h"

#include "core_ntp/NtpClientLib.h"

#include "device_electronica7_rgb.h"
#include "common_module.h"
#include "common/common.h"
#include "common/TimeLib.h"
#include "core_sys/eertos.h"

// ============================================================
// Конкретная логика модуля
// ============================================================

bool CLASS_DEVICE_E7RGB::loadFont() {
    if (_fs == NULL) { return false; }
    if (ns_device_electronica7_rgb::e7FontPathOk(_config.fontFile) == false) { _config.fontFile = E7_FONT_DEFAULT; }

    // Размер текущего файла: нужен и для проверки кэша, и для обнаружения замены.
    uint32_t size = 0;
    File sf = _fs->open(_config.fontFile, "r");
    if (sf) { size = (uint32_t)sf.size(); sf.close(); }

    // Этот же шрифт уже загружен и файл не изменился — не открываем повторно
    // (частые apply из макросов не должны дёргать FS).
    if (_loadedFontPath == _config.fontFile && size > 0 && size == _loadedFontSize) { return true; }

    // каталог шрифтов должен существовать для записи файла
    _fs->mkdir(E7_FONT_DIR);
    if (_fonts.loadOrCreate(*_fs, _config.fontFile.c_str()) == false) {
        _fonts.loadDefault();
        // Неудачу не кэшируем: следующая попытка снова прочитает файл.
        _loadedFontPath = "";
        _loadedFontSize = 0;
        return false;
    }
    _loadedFontPath = _config.fontFile;
    _loadedFontSize = size;
    return true;
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
        // Режим off: погасить матрицу и не рисовать кадр.
        if (d._config.busMode == E7_BUS_OFF) {
            d._matrix.clear();
            d._matrix.show();
            d._pendingApply = false;
            return;
        }
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
// Периодическая задача рендера (main-loop, E7_FX_RENDER_MS).
// Продвигает фазу цвета раз в E7RGB_ANIM_MS и рисует текущее
// состояние: дождь / оседание / переход / обычный кадр.
// Show() безопасен: loop-контекст.
// ============================================================
void e7rgbAnimTask() {
    CLASS_DEVICE_E7RGB& d = device_electronica7_rgb;

    // Режим off: матрица погашена, рендер не выполняется.
    if (d._config.busMode == E7_BUS_OFF) {
        SetTimerTask(e7rgbAnimTask, E7_FX_RENDER_MS);
        return;
    }

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

    // Режим off: логика времени не работает.
    if (d._config.busMode == E7_BUS_OFF) {
        SetTimerTask(e7rgbSecondTask, 1000);
        return;
    }

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
