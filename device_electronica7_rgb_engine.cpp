#include "core_web/FSWebServerLib.h"

#include <math.h>

#include "device_electronica7_rgb.h"
#include "common_module.h"
#include "common/common.h"
#include "common/TimeLib.h"
#include "core_sys/core_sys.h"
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
    _colorPrev = _config.digitsColor;   // что фактически нарисовано сейчас

    // Режим off: матрица и точки погашены, кадр не рисуется.
    if (_config.busMode == E7_BUS_OFF) {
        _matrix.clear();
        _matrix.show();
        _dotOn = false;
        return;
    }

    if (_config.displayMode == E7_DISP_TICKER) {
        // Бегущая строка: дождь и переходы не используются.
        _view = E7_VIEW_NORMAL;
        drawTicker();
        return;
    }
    if (_config.displayMode == E7_DISP_MANUAL) {
        _view = E7_VIEW_NORMAL;
        renderManual();
        return;
    }

    // Часы: дождь/оседание/переход/обычный кадр.
    if (_view == E7_VIEW_RAIN)      { drawRain(false, 0.0f, NULL); return; }
    if (_view == E7_VIEW_SETTLE)    { drawSettle(); return; }
    if (_view == E7_VIEW_TRANS)     { drawTransition(); return; }
    redrawFrame();
}

void CLASS_DEVICE_E7RGB::redrawFrame() {
    if (_matrix.ready() == false) { return; }
    // Время показывается всегда: если валидного источника нет, идут внутренние
    // часы ядра (стартуют с 00:00). Дождь — опциональная анимация синхронизации.
    if (core_sys.timeValid() || core_sys.timeFallback()) {
        time_t t = core_sys.timeNow();
        renderDigits((uint8_t)hour(t), (uint8_t)minute(t));
    } else {
        // Время ещё не валидно — прочерки («нет времени»)
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
    drawMaskFx(mask, false);
}

// Бегущая строка: каждый символ занимает E7_GLYPH_W пикселей (пробел и
// неизвестный символ — тоже, чтобы текст не «слипался»).
void CLASS_DEVICE_E7RGB::drawTicker() {
    if (_matrix.ready() == false) { return; }
    _matrix.clear();

    int len = (int)_config.tickerText.length();
    int textW = len * E7_GLYPH_W;
    int baseX = (_config.tickerDirection == E7_TICKER_RIGHT)
                ? (_tickerOffset - textW)
                : (E7_WIDTH - _tickerOffset);

    for (int i = 0; i < len; i++) {
        const uint8_t* g = _fonts.glyph(_config.tickerText.charAt(i));
        if (g == NULL) { continue; }
        int x0 = baseX + i * E7_GLYPH_W;
        for (int r = 0; r < E7_GLYPH_H; r++) {
            int py = (E7_HEIGHT - 1) - r;
            for (int c = 0; c < E7_GLYPH_W; c++) {
                if (g[r * E7_GLYPH_W + c] == 0) { continue; }
                int px = x0 + c;
                if (px < 0 || px >= E7_WIDTH) { continue; }
                uint32_t color = ns_device_electronica7_rgb::e7FxColor(_config, px, py, _animPhase, _fxOrder);
                compositePixel(px, py, color, 1.0f);
            }
        }
    }

    // В режиме бегущей строки точки погашены (не часы).
    drawDots(0.0f);
    _matrix.show();
}

// Сдвиг строки по времени: шаг tickerSpeed мс на пиксель, полный цикл —
// ширина текста плюс ширина матрицы.
void CLASS_DEVICE_E7RGB::tickerScroll() {
    int len = (int)_config.tickerText.length();
    if (len == 0) { return; }
    int limit = len * E7_GLYPH_W + E7_WIDTH;

    // Один проход (e7.show): строка доехала до конца — остаёмся на пустом
    // кадре до возврата часов, без «вылезания» первых символов снова.
    if (_tickerOneShot && _tickerOffset >= limit) { return; }

    uint32_t now = millis();
    uint32_t stepMs = _config.tickerSpeed;
    if (stepMs < E7_TICKER_SPEED_MIN) { stepMs = E7_TICKER_SPEED_MIN; }
    uint32_t steps = (uint32_t)(now - _tickerLastMs) / stepMs;
    if (steps == 0) { return; }
    // Тик может быть реже tickerSpeed: сдвигаем сразу на несколько пикселей.
    // Большой «долг» (блокировка главного цикла) не догоняем — сбрасываем.
    if (steps > 16) { steps = 16; _tickerLastMs = now; }
    _tickerLastMs += steps * stepMs;

    _tickerOffset = (int16_t)(_tickerOffset + (int)steps);
    if (_tickerOffset >= limit) {
        _tickerOffset = _tickerOneShot ? (int16_t)limit : 0;
    }
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

void CLASS_DEVICE_E7RGB::drawMaskFx(const uint8_t mask[E7_HEIGHT][E7_WIDTH], bool withDots) {
    if (_matrix.ready() == false) { return; }
    _matrix.clear();
    for (int y = 0; y < E7_HEIGHT; y++) {
        for (int x = 0; x < E7_WIDTH; x++) {
            if (mask[y][x] == 0) { continue; }
            uint32_t color = ns_device_electronica7_rgb::e7FxColor(_config, x, y, _animPhase, _fxOrder);
            compositePixel(x, y, color, 1.0f);
        }
    }
    // Ручной текст/бегущая строка — не часы: точки не мигают и погашены.
    drawDots(withDots ? (dotBlinkOn() ? 1.0f : 0.0f) : 0.0f);
    _matrix.show();
}

void CLASS_DEVICE_E7RGB::startTransition(uint8_t h, uint8_t m) {
    _prevH = _curH;
    _prevM = _curM;
    _curH = h;
    _curM = m;
    _transType = _config.timeFx;
    _transStartMs = millis();
    _transBlankPrev = false;
    _transBlankCur = false;
    _transPrevColor = _config.digitsColor;
    _transCurColor = _config.digitsColor;
    for (int y = 0; y < E7_HEIGHT; y++) {
        for (int x = 0; x < E7_WIDTH; x++) {
            _transRand[y][x] = (uint8_t)(ns_device_electronica7_rgb::e7Rand() % 255);
        }
    }
    _view = E7_VIEW_TRANS;
    drawTransition();
}

// Уход с часов: старый кадр часов -> пустой кадр (эффект timeFx).
// После завершения e7rgbSecondTask вызовет applyMode() и покажет текст.
void CLASS_DEVICE_E7RGB::startExitTransition() {
    _prevH = _curH;
    _prevM = _curM;
    _transType = _config.timeFx;
    _transStartMs = millis();
    _transBlankPrev = false;
    _transBlankCur = true;
    // Старый кадр — ещё прежним цветом (нет вспышки нового цвета).
    _transPrevColor = _colorPrev;
    _transCurColor = _config.digitsColor;
    for (int y = 0; y < E7_HEIGHT; y++) {
        for (int x = 0; x < E7_WIDTH; x++) {
            _transRand[y][x] = (uint8_t)(ns_device_electronica7_rgb::e7Rand() % 255);
        }
    }
    _view = E7_VIEW_TRANS;
    drawTransition();
}

// Возврат на часы: пустой кадр -> актуальное время (эффект timeFx).
void CLASS_DEVICE_E7RGB::startReturnTransition(uint8_t h, uint8_t m) {
    _prevH = _curH;
    _prevM = _curM;
    _curH = h;
    _curM = m;
    _transType = _config.timeFx;
    _transStartMs = millis();
    _transBlankPrev = true;
    _transBlankCur = false;
    _transPrevColor = _config.digitsColor;
    _transCurColor = _config.digitsColor;
    for (int y = 0; y < E7_HEIGHT; y++) {
        for (int x = 0; x < E7_WIDTH; x++) {
            _transRand[y][x] = (uint8_t)(ns_device_electronica7_rgb::e7Rand() % 255);
        }
    }
    _view = E7_VIEW_TRANS;
    drawTransition();
}

// Линейная интерполяция цвета RGB (для плавной смены цвета в переходах).
static uint32_t e7LerpColor(uint32_t a, uint32_t b, float t) {
    if (t <= 0.0f) { return a; }
    if (t >= 1.0f) { return b; }
    int ar = (int)((a >> 16) & 0xFF), ag = (int)((a >> 8) & 0xFF), ab = (int)(a & 0xFF);
    int br = (int)((b >> 16) & 0xFF), bg = (int)((b >> 8) & 0xFF), bb = (int)(b & 0xFF);
    int r = ar + (int)((float)(br - ar) * t);
    int g = ag + (int)((float)(bg - ag) * t);
    int bl = ab + (int)((float)(bb - ab) * t);
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
}

void CLASS_DEVICE_E7RGB::drawTransition() {
    if (_matrix.ready() == false) { return; }
    uint8_t prevMask[E7_HEIGHT][E7_WIDTH];
    uint8_t curMask[E7_HEIGHT][E7_WIDTH];
    buildTimeMask(_prevH, _prevM, prevMask);
    buildTimeMask(_curH, _curM, curMask);
    // Переходы «часы -> пусто» (перед текстом) и «пусто -> часы» (возврат):
    // один из кадров — пустая матрица; эффект остаётся тем же (timeFx).
    if (_transBlankPrev) { memset(prevMask, 0, sizeof(prevMask)); }
    if (_transBlankCur)  { memset(curMask, 0, sizeof(curMask)); }

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
    // Для переходов «часы <-> пусто» фон не нужен: он давал лишнюю вспышку.
    if (_transBlankPrev || _transBlankCur) { bg = 0.0f; }

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
                    // «часы <-> пусто»: монотонное гашение/проявление без
                    // чёрной паузы и без обратной вспышки во второй половине.
                    if (_transBlankCur)       { level = prevFrame[y][x] * (1.0f - t); }
                    else if (_transBlankPrev) { level = curFrame[y][x]  * t; }
                    else if (t < 0.5f)        { level = prevFrame[y][x] * (1.0f - 2.0f * t); }
                    else                      { level = curFrame[y][x]  * (2.0f * t - 1.0f); }
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

            // Цвет: у старого и нового кадра свои цвета, поэтому смена
            // цвета не даёт вспышки «новым цветом» на старом кадре.
            // FADE/FLASH — по фазе, остальные эффекты — линейный переход.
            uint32_t color;
            if (white) {
                color = 0xFFFFFF;
            } else if (_transBlankCur) {
                // Уезд: виден только старый кадр — держим его прежний цвет
                // для любого эффекта (нет вспышки новым цветом).
                color = _transPrevColor;
            } else if (_transBlankPrev) {
                // Приезд: виден только новый кадр — его цвет.
                color = _transCurColor;
            } else if (_transType == E7_TFX_FADE) {
                color = (t < 0.5f) ? _transPrevColor : _transCurColor;
            } else if (_transType == E7_TFX_FLASH) {
                color = (t < 0.55f) ? _transPrevColor : _transCurColor;
            } else {
                color = e7LerpColor(_transPrevColor, _transCurColor, t);
            }
            compositePixel(x, y, color, level);
        }
    }

    // Точки в переходе: мигание приостановлено, участвуют в анимации.
    // Для «часы <-> пусто» точки гаснут/проявляются вместе с кадром.
    float dotLevel = 1.0f;
    bool  dotWhite = false;
    if (_transType == E7_TFX_FADE) {
        if (_transBlankPrev)      { dotLevel = t; }
        else if (_transBlankCur)  { dotLevel = 1.0f - t; }
        else                      { dotLevel = (t < 0.5f) ? (1.0f - 2.0f * t) : (2.0f * t - 1.0f); }
    } else if (_transType == E7_TFX_FLASH && t >= 0.45f && t < 0.65f) {
        dotWhite = true;
    } else if (_transBlankPrev) {
        dotLevel = t;
    } else if (_transBlankCur) {
        dotLevel = 1.0f - t;
    }
    uint32_t dotColor;
    if (_transBlankCur) {
        dotColor = _transPrevColor;
    } else if (_transBlankPrev) {
        dotColor = _transCurColor;
    } else if (_transType == E7_TFX_FADE) {
        dotColor = (t < 0.5f) ? _transPrevColor : _transCurColor;
    } else if (_transType == E7_TFX_FLASH) {
        dotColor = (t < 0.5f) ? _transPrevColor : _transCurColor;
    } else {
        dotColor = e7LerpColor(_transPrevColor, _transCurColor, t);
    }
    drawDots(dotLevel, dotWhite, dotColor);
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
    // При оседании точки проявляются вместе с цифрами; в чистом дожде погашены.
    if (settleMode) { drawDots(settleP); }
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
// Перелистывание цифр (digitFlip): при смене минут разряды, которые
// меняются, прокручиваются вверх/вниз через «ленту» из 7 строк старой
// цифры, 1 строки зазора и 7 строк новой (8 шагов).
// ============================================================

void CLASS_DEVICE_E7RGB::startDigitFlip(uint8_t h, uint8_t m) {
    _flipCur[0] = (uint8_t)(_curH / 10);
    _flipCur[1] = (uint8_t)(_curH % 10);
    _flipCur[2] = (uint8_t)(_curM / 10);
    _flipCur[3] = (uint8_t)(_curM % 10);
    _flipNxt[0] = (uint8_t)(h / 10);
    _flipNxt[1] = (uint8_t)(h % 10);
    _flipNxt[2] = (uint8_t)(m / 10);
    _flipNxt[3] = (uint8_t)(m % 10);

    _flipDir = _config.digitFlip;
    switch (_config.digitFlipSpeed) {
        case E7_FLIP_SPEED_SLOW:   _flipStepMs = 60; break;
        case E7_FLIP_SPEED_FAST:   _flipStepMs = 20; break;
        default:                   _flipStepMs = 35; break;
    }
    _flipStartMs = millis();
    _flipOn = true;
    _view = E7_VIEW_NORMAL;
    drawFlipFrame();
}

void CLASS_DEVICE_E7RGB::drawFlipFrame() {
    if (_matrix.ready() == false) { return; }

    uint32_t stepMs = (_flipStepMs == 0) ? 35u : (uint32_t)_flipStepMs;
    uint32_t k = (millis() - _flipStartMs) / stepMs + 1;
    if (k > 8) {
        // Доехали: на экране новое время.
        _curH = (uint8_t)(_flipNxt[0] * 10 + _flipNxt[1]);
        _curM = (uint8_t)(_flipNxt[2] * 10 + _flipNxt[3]);
        _lastMinute = _curM;
        _flipOn = false;
        redrawFrame();
        return;
    }

    uint8_t mask[E7_HEIGHT][E7_WIDTH];
    memset(mask, 0, sizeof(mask));
    bool up = (_flipDir == E7_FLIP_UP);

    for (uint8_t d = 0; d < 4; d++) {
        uint8_t x0 = (uint8_t)(d * E7_GLYPH_W);
        const uint8_t* cg = _fonts.glyph((char)('0' + _flipCur[d]));
        const uint8_t* ng = _fonts.glyph((char)('0' + _flipNxt[d]));

        // Разряд не меняется — стоит на месте.
        if (_flipCur[d] == _flipNxt[d]) {
            blitGlyph(x0, cg, mask);
            continue;
        }
        if (cg == NULL || ng == NULL) { continue; }

        for (int r = 0; r < E7_GLYPH_H; r++) {
            const uint8_t* g = NULL;
            int srcRow = -1;
            if (up) {
                int idx = r + (int)k;
                if (idx < E7_GLYPH_H)      { g = cg; srcRow = idx; }
                else if (idx == E7_GLYPH_H) { continue; }   // зазор
                else                       { g = ng; srcRow = idx - E7_GLYPH_H - 1; }
            } else {
                int p = r - (int)k;
                if (p >= 0)             { g = cg; srcRow = p; }
                else if (p == -1)       { continue; }       // зазор
                else                    { g = ng; srcRow = p + E7_GLYPH_H + 1; }
            }
            if (g == NULL || srcRow < 0 || srcRow >= E7_GLYPH_H) { continue; }
            int py = (E7_HEIGHT - 1) - r;   // логический y: 0 = низ
            for (int c = 0; c < E7_GLYPH_W; c++) {
                if (g[srcRow * E7_GLYPH_W + c] == 0) { continue; }
                mask[py][x0 + c] = 1;
            }
        }
    }

    drawMaskFx(mask);
}

// ============================================================
// Разделительные точки ЧЧ/ММ (LED 112 — нижняя, 113 — верхняя)
// ============================================================

// Моргание: 0 — горят всегда; N — полный цикл N секунд,
// первые N/2 горят, вторые N/2 погашены. Фаза от millis().
bool CLASS_DEVICE_E7RGB::dotBlinkOn() {
    if (_config.colonBlink == 0) { return true; }
    float cycle = (float)_config.colonBlink;
    float phase = fmodf((float)millis() / 1000.0f, cycle);
    return phase < cycle * 0.5f;
}

// Уровень 0..1: 0 — точки погашены. white — белый цвет (вспышка перехода).
void CLASS_DEVICE_E7RGB::drawDots(float level, bool white, uint32_t colorOverride) {
    if (_matrix.ready() == false) { return; }
    if (level < 0.0f) { level = 0.0f; }
    if (level > 1.0f) { level = 1.0f; }
    uint8_t b = (uint8_t)((float)_config.brightness * level);
    bool useOverride = (colorOverride != 0xFFFFFFFF);
    uint32_t cUpper = white ? 0xFFFFFF
                            : (useOverride ? colorOverride
                                           : ns_device_electronica7_rgb::e7FxColor(_config, 8, 2, _animPhase, _fxOrder));
    uint32_t cLower = white ? 0xFFFFFF
                            : (useOverride ? colorOverride
                                           : ns_device_electronica7_rgb::e7FxColor(_config, 8, 4, _animPhase, _fxOrder));
    _matrix.setColonDot(true,  cUpper, b);
    _matrix.setColonDot(false, cLower, b);
    _dotOn = (level > 0.003f);
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
            d._dotOn = false;
            d._toClockFx = false;
            d._toTextFx = false;
            d._pendingApply = false;
            return;
        }
        // Смена текста/скорости/направления — строку с начала.
        if (d._tickerReset) {
            d._tickerOffset = 0;
            d._tickerLastMs = millis();
            d._tickerReset = false;
        }
        // Скорректировать состояние отображения под изменённые настройки
        bool synced = core_sys.timeValid();
        if (d._config.displayMode != E7_DISP_CLOCK) {
            d._view = E7_VIEW_NORMAL;
            d._flipOn = false;
        } else if (d._config.rainEnabled == 0 &&
                   (d._view == E7_VIEW_RAIN || d._view == E7_VIEW_SETTLE)) {
            d._view = E7_VIEW_NORMAL;
            d._forceRedraw = true;
            d._lastMinute = (uint8_t)minute(core_sys.timeNow());
        } else if (d._config.rainEnabled == 1 && d._view == E7_VIEW_NORMAL && synced == false) {
            d.startRain();
        }

        bool toClockFx = d._toClockFx;
        bool toTextFx = d._toTextFx;
        d._toClockFx = false;
        d._toTextFx = false;
        bool fxOk = synced && d._config.timeFx != E7_TFX_OFF && d._config.timeFxDur > 0 &&
                    d._view == E7_VIEW_NORMAL;

        if (toClockFx && fxOk && d._config.displayMode == E7_DISP_CLOCK) {
            // Приезд часов: пустой кадр -> актуальное время.
            time_t t = core_sys.timeNow();
            d._forceRedraw = false;
            d.startReturnTransition((uint8_t)hour(t), (uint8_t)minute(t));
        } else if (toTextFx && fxOk && d._config.displayMode != E7_DISP_CLOCK) {
            // Уезд часов: старый кадр -> пусто, затем applyMode() покажет текст.
            d._forceRedraw = false;
            d.startExitTransition();
        } else {
            d.applyMode();
        }
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

    // Есть неприменённые настройки (цвет/режим): не перерисовываем старый
    // кадр новым цветом — дождёмся deferredApplyTask, чтобы не было вспышки.
    if (d._pendingApply) {
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

    // Переходы (в т.ч. «уезд» часов при displayMode=ticker/manual) имеют
    // приоритет над обычным рендером режима.
    if (d._view == E7_VIEW_TRANS) {
        d.drawTransition();
        // Завершение — по 50-мс тику (а не раз в секунду), чтобы переход
        // заканчивался ровно в timeFxDur и текст стартовал вовремя.
        uint32_t dur = (uint32_t)d._config.timeFxDur * 1000u;
        if (dur == 0 || (millis() - d._transStartMs) >= dur) {
            d._view = E7_VIEW_NORMAL;
            d._lastMinute = d._curM;
            if (d._config.displayMode == E7_DISP_TICKER) {
                d._tickerOffset = 0;
                d._tickerLastMs = millis();
            }
            d.applyMode();
        }
    } else if (d._view == E7_VIEW_RAIN) {
        d.drawRain(false, 0.0f, NULL);
    } else if (d._view == E7_VIEW_SETTLE) {
        d.drawSettle();
    } else if (d._config.displayMode == E7_DISP_TICKER) {
        // Бегущая строка: прокрутка по tickerSpeed, перерисовка каждый тик
        // (точки и анимированный цвет обновляются здесь же).
        d.tickerScroll();
        d.drawTicker();
    } else if (d._config.displayMode == E7_DISP_MANUAL) {
        // Точки в ручном режиме погашены: кадр обновляем только при
        // анимированном эффекте (иначе он статичен).
        if (phaseAdvanced && ns_device_electronica7_rgb::e7FxAnimated(d._config.effect)) {
            d.applyMode();
        }
    } else if (d._flipOn) {
        // Перелистывание цифр: шаг 20..60 мс, перерисовка каждый тик.
        d.drawFlipFrame();
    } else if (phaseAdvanced && ns_device_electronica7_rgb::e7FxAnimated(d._config.effect)) {
        // обычный кадр перерисовываем только при смене фазы цвета
        d.applyMode();
    } else {
        // Обычный кадр: для неанимированных эффектов перерисовки нет,
        // поэтому точки обновляем отдельно (только при смене фазы моргания).
        bool on = d.dotBlinkOn();
        if (on != d._dotOn) {
            d.drawDots(on ? 1.0f : 0.0f);
            d._matrix.show();
        }
    }

    // Пока идёт перелистывание, тик ускоряется, чтобы шаг 20 мс не «проскакивал».
    SetTimerTask(e7rgbAnimTask, d._flipOn ? 20 : E7_FX_RENDER_MS);
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

    bool synced = core_sys.timeValid();

    // Есть неприменённые изменения или идёт переход: не вмешиваемся, иначе
    // мгновенная перерисовка часов даст вспышку перед плавным «приездом».
    if (d._pendingApply || d._view == E7_VIEW_TRANS) {
        SetTimerTask(e7rgbSecondTask, 1000);
        return;
    }

    // Часы/дождь актуальны только для режима отображения clock.
    if (d._config.displayMode != E7_DISP_CLOCK) {
        SetTimerTask(e7rgbSecondTask, 1000);
        return;
    }

    if (d._view == E7_VIEW_RAIN) {
        if (synced) {
            d._ntpWasSynced = true;
            uint32_t elapsed = millis() - d._rainStartMs;
            if (elapsed >= (uint32_t)d._config.rainDurMin * 1000u) {
                time_t t = core_sys.timeNow();
                d._curH = (uint8_t)hour(t);
                d._curM = (uint8_t)minute(t);
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
            time_t t = core_sys.timeNow();
            d._curH = (uint8_t)hour(t);
            d._curM = (uint8_t)minute(t);
            d._lastMinute = d._curM;
            d.applyMode();
        }
        SetTimerTask(e7rgbSecondTask, 1000);
        return;
    }

    // Пока идёт перелистывание, новая смена минут ждёт его завершения
    // (drawFlipFrame() сам обновит _lastMinute и перерисует кадр).
    if (d._flipOn) {
        SetTimerTask(e7rgbSecondTask, 1000);
        return;
    }

    if (synced && !d._ntpWasSynced) {
        d._ntpWasSynced = true;
        // Переход от внутренних часов (00:00) к актуальному времени — анимированно
        // (если включён timeFx), иначе мгновенная перерисовка.
        time_t tnow = core_sys.timeNow();
        uint8_t h = (uint8_t)hour(tnow);
        uint8_t m = (uint8_t)minute(tnow);
        if (d._config.timeFx != E7_TFX_OFF && d._config.timeFxDur > 0) {
            d.startTransition(h, m);
        } else {
            d._lastMinute = m;
            d._curH = h;
            d._curM = m;
            d.redrawFrame();
        }
        SetTimerTask(e7rgbSecondTask, 1000);
        return;
    }
    time_t tnow = core_sys.timeNow();
    uint8_t h = (uint8_t)hour(tnow);
    uint8_t m = (uint8_t)minute(tnow);
    if (d._forceRedraw || m != d._lastMinute) {
        bool immediate = d._forceRedraw;   // первое время/смена настроек — без перехода
        d._forceRedraw = false;

        // Перелистывание цифр имеет приоритет над timeFx при смене минут.
        bool flipChanged = false;
        if (!immediate && synced && d._config.digitFlip != E7_FLIP_NONE && d._view == E7_VIEW_NORMAL) {
            uint8_t cur[4] = { (uint8_t)(d._curH / 10), (uint8_t)(d._curH % 10),
                               (uint8_t)(d._curM / 10), (uint8_t)(d._curM % 10) };
            uint8_t nxt[4] = { (uint8_t)(h / 10), (uint8_t)(h % 10),
                               (uint8_t)(m / 10), (uint8_t)(m % 10) };
            for (int i = 0; i < 4; i++) {
                if (cur[i] != nxt[i]) { flipChanged = true; break; }
            }
        }

        if (flipChanged) {
            d.startDigitFlip(h, m);
        } else if (!immediate && synced && d._config.timeFx != E7_TFX_OFF && d._config.timeFxFreq > 0 &&
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
