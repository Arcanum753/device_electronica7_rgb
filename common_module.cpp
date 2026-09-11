
#include "common_module.h"

#include "common/TimeLib.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

// ============================================================
// Вспомогательные функции устройства «Электроника-7» RGB
// ============================================================

namespace ns_device_electronica7_rgb {

bool e7FontPathOk(const String& p) {
    if (!p.startsWith(E7_FONT_DIR_SLASH) || !p.endsWith(".fnt")) { return false; }
    // Разрешаем только безопасные символы: управляющие, '|', '#', DEL и
    // не-ASCII ломают CVT-ответ /e7rgb/info или путь в ФС.
    for (unsigned int i = 0; i < p.length(); i++) {
        char c = p.charAt(i);
        if ((unsigned char)c < 0x20 || c == 0x7F || c == '|' || c == '#' || (unsigned char)c >= 0x80) {
            return false;
        }
    }
    return true;
}

String e7TimeHhMm() {
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", hour(), minute());
    return String(buf);
}

String e7HexColor(uint32_t c) {
    char hex[8];
    snprintf(hex, sizeof(hex), "#%06X", (unsigned int)(c & 0xFFFFFF));
    return String(hex);
}

// Допустимая кратность минут для эффекта смены времени
bool e7TfxFreqOk(int f) {
    return (f == 1 || f == 3 || f == 5 || f == 10 || f == 15 || f == 20 || f == 40);
}

uint32_t e7HexStringToUint32(const String& hexStr) {
    if (hexStr.length() == 0) { return 0; }
    String clean = hexStr;
    clean.replace("#", "");
    clean.replace("0x", "");
    clean.replace("0X", "");
    return (uint32_t)strtoul(clean.c_str(), NULL, 16);
}

// ============================================================
// Помощники спецэффектов окраски
// ============================================================

uint32_t e7HsvToRgb(float h, float s, float v) {
    while (h < 0.0f) { h += 360.0f; }
    while (h >= 360.0f) { h -= 360.0f; }
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r = 0.0f, g = 0.0f, b = 0.0f;
    if (h < 60.0f)      { r = c; g = x; }
    else if (h < 120.0f) { r = x; g = c; }
    else if (h < 180.0f) { g = c; b = x; }
    else if (h < 240.0f) { g = x; b = c; }
    else if (h < 300.0f) { r = x; b = c; }
    else                { r = c; b = x; }
    uint8_t r8 = (uint8_t)((r + m) * 255.0f);
    uint8_t g8 = (uint8_t)((g + m) * 255.0f);
    uint8_t b8 = (uint8_t)((b + m) * 255.0f);
    return ((uint32_t)r8 << 16) | ((uint32_t)g8 << 8) | b8;
}

uint32_t e7LerpColor(uint32_t c1, uint32_t c2, float t) {
    if (t <= 0.0f) { return c1; }
    if (t >= 1.0f) { return c2; }
    uint8_t r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    uint8_t r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;
    uint8_t r = (uint8_t)(r1 + (r2 - r1) * t);
    uint8_t g = (uint8_t)(g1 + (g2 - g1) * t);
    uint8_t b = (uint8_t)(b1 + (b2 - b1) * t);
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// Простой xorshift-ГПСЧ для «случайного» порядка палитры
static uint32_t e7Rng = 0x9E3779B9;
void e7SeedRng() {
    e7Rng = micros() ^ 0x9E3779B9;
    if (e7Rng == 0) { e7Rng = 0x12345678; }
}
uint32_t e7Rand() {
    e7Rng ^= e7Rng << 13;
    e7Rng ^= e7Rng >> 17;
    e7Rng ^= e7Rng << 5;
    return e7Rng;
}
void e7ShuffleOrder(uint8_t* arr, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = (int)(e7Rand() % (uint32_t)(i + 1));
        uint8_t t = arr[i]; arr[i] = arr[j]; arr[j] = t;
    }
}

// Нормированная позиция (0..1) вдоль оси выбранного направления
float e7PosOnAxis(int x, int y, uint8_t dir) {
    float fx = x / (float)(E7_WIDTH - 1);
    float fy = y / (float)(E7_HEIGHT - 1);
    switch (dir) {
        case E7_FXDIR_TL: return (fx + fy) * 0.5f;
        case E7_FXDIR_TR: return ((1.0f - fx) + fy) * 0.5f;
        case E7_FXDIR_BL: return (fx + (1.0f - fy)) * 0.5f;
        case E7_FXDIR_BR: return ((1.0f - fx) + (1.0f - fy)) * 0.5f;
        case E7_FXDIR_TB: return fy;
        case E7_FXDIR_BT: return 1.0f - fy;
    }
    return fy;
}

// Цвет пикселя (x,y) под текущим эффектом; phaseDeg — фаза анимации 0..360.
// order — текущий порядок обхода палитры (только для плавной смены цвета).
uint32_t e7FxColor(const strE7RgbConfig& cfg, int x, int y, float phaseDeg, const uint8_t* order) {
    switch (cfg.effect) {
        case E7_EFFECT_RAINBOW: {
            float t = e7PosOnAxis(x, y, cfg.effectDir);
            float hue = fmodf(phaseDeg + t * 300.0f, 360.0f);
            return e7HsvToRgb(hue, 1.0f, 1.0f);
        }
        case E7_EFFECT_GRAD_STATIC: {
            float t = e7PosOnAxis(x, y, cfg.effectDir);
            return e7LerpColor(cfg.digitsColor, cfg.digitsColor2, t);
        }
        case E7_EFFECT_GRAD_DYNAMIC: {
            float t = e7PosOnAxis(x, y, cfg.effectDir);
            float ph = fmodf(phaseDeg, 360.0f) / 360.0f;
            float w = 0.5f - 0.5f * cosf(6.2831853f * (t - ph));
            return e7LerpColor(cfg.digitsColor, cfg.digitsColor2, w);
        }
        case E7_EFFECT_COLORCYCLE:
            // Плавная смена цвета: весь экран в одном плавно меняющемся цвете
            if (cfg.cycleMode == E7_CYCLE_RAINBOW) {
                return e7HsvToRgb(phaseDeg, 1.0f, 1.0f);
            }
            {
                uint8_t n = (cfg.colorsCount < 1) ? 1 : (cfg.colorsCount > E7_FX_MAX_COLORS ? E7_FX_MAX_COLORS : cfg.colorsCount);
                float segLen = 360.0f / (float)n;
                int seg = (int)(phaseDeg / segLen);
                if (seg >= n) { seg = n - 1; }
                float frac = phaseDeg - (float)seg * segLen;
                if (frac > segLen) { frac = segLen; }
                float t = frac / segLen;
                // плавный переход (сглаживание синусом)
                t = 0.5f - 0.5f * cosf(3.14159265f * t);
                int iFrom, iTo;
                if (cfg.cycleMode == E7_CYCLE_RANDOM && order != NULL) {
                    iFrom = order[seg];
                    iTo = order[(seg + 1) % n];
                } else {
                    iFrom = seg;
                    iTo = (seg + 1) % n;
                }
                return e7LerpColor(cfg.palette[iFrom], cfg.palette[iTo], t);
            }
        case E7_EFFECT_MONO:
        default:
            return cfg.digitsColor;
    }
}

bool e7FxAnimated(uint8_t effect) {
    return (effect == E7_EFFECT_RAINBOW || effect == E7_EFFECT_GRAD_DYNAMIC ||
            effect == E7_EFFECT_COLORCYCLE);
}

// Выборка пикселя для сдвига вверх (новое выезжает снизу):
// общая логика для E7_TFX_SLIDE_UP и E7_TFX_DIGIT_SLIDE
float e7SampleSlideUp(const float prevFrame[E7_HEIGHT][E7_WIDTH],
                      const float curFrame[E7_HEIGHT][E7_WIDTH],
                      int x, int y, int off) {
    if (y < off) {
        int sy = y + (E7_HEIGHT - off);
        return (sy >= 0 && sy < E7_HEIGHT) ? curFrame[sy][x] : 0.0f;
    }
    int sy = y - off;
    return (sy >= 0) ? prevFrame[sy][x] : 0.0f;
}

} // namespace ns_device_electronica7_rgb
