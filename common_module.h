
#ifndef _DEVICE_E7RGB_COMMON_MODULE_h
#define _DEVICE_E7RGB_COMMON_MODULE_h

#include <stdint.h>
#include <Arduino.h>

class AsyncWebServerRequest;

#include "device_electronica7_rgb.h"

// Вспомогательные функции устройства «Электроника-7» RGB (чистые, без состояния).
namespace ns_device_electronica7_rgb {

// Проверка пути к файлу шрифта (каталог /e7fonts, расширение .fnt, безопасные символы)
bool e7FontPathOk(const String& p);

// Текущее время в формате ЧЧ:ММ
String e7TimeHhMm();

// Цвет 0xRRGGBB -> "#RRGGBB"
String e7HexColor(uint32_t c);

// Допустимая кратность минут для эффекта смены времени
bool e7TfxFreqOk(int f);

// Преобразование hex-строки (#RRGGBB, 0xRRGGBB или RRGGBB) в uint32
uint32_t e7HexStringToUint32(const String& hexStr);

// Помощники спецэффектов окраски
uint32_t e7HsvToRgb(float h, float s, float v);
uint32_t e7LerpColor(uint32_t c1, uint32_t c2, float t);
void     e7SeedRng();
uint32_t e7Rand();
void     e7ShuffleOrder(uint8_t* arr, int n);

// Нормированная позиция (0..1) вдоль оси выбранного направления
float e7PosOnAxis(int x, int y, uint8_t dir);

// Цвет пикселя (x,y) под текущим эффектом; phaseDeg — фаза анимации 0..360
uint32_t e7FxColor(const strE7RgbConfig& cfg, int x, int y, float phaseDeg, const uint8_t* order);
bool     e7FxAnimated(uint8_t effect);

// Выборка пикселя для сдвига вверх (общая логика E7_TFX_SLIDE_UP / E7_TFX_DIGIT_SLIDE)
float e7SampleSlideUp(const float prevFrame[E7_HEIGHT][E7_WIDTH],
                      const float curFrame[E7_HEIGHT][E7_WIDTH],
                      int x, int y, int off);

} // namespace ns_device_electronica7_rgb

#endif // _DEVICE_E7RGB_COMMON_MODULE_h
