#ifndef _E7RGB_MATRIX_h
#define _E7RGB_MATRIX_h

// ============================================================
// e7rgb_matrix.h — драйвер адресной RGB-матрицы 7x16 (112 LEDs)
// для device_electronica7_rgb (файл общего назначения).
// Вся лента — 114 LED: 112 матрицы + 2 разделительные точки
// ЧЧ/ММ в конце цепочки (112 — нижняя, 113 — верхняя).
//
// Вывод: NeoPixelBus. Метод переключается дефайном:
//   - по умолчанию I2S+DMA (NeoEsp32I2s0Ws2812xMethod), ESP32 classic;
//   - -D E7_USE_RMT — RMT (NeoEsp32Rmt0Ws2812xMethod, как module_rgb;
//     нужен для ESP32-C3/S3 и др.).
//
// Развёртка координат — точный порт getPixNumber() из
// example/microLED-main (библиотека microLED AVR-only и не
// используется, переносится только алгоритм формирования матрицы):
// угол подключения + направление + зигзаг/параллельно.
//
// Логические координаты (как в microLED): x = 0..15 слева направо,
// y = 0..6 снизу вверх. По умолчанию (под спаянную матрицу) первый
// светодиод — внизу слева, порядок — столбцами снизу вверх,
// слева направо (зигзаг по столбцам).
// ============================================================

#include <Arduino.h>
#include <NeoPixelBus.h>

#define E7_WIDTH         16
#define E7_HEIGHT         7
#define E7_MATRIX_LEDS    (E7_WIDTH * E7_HEIGHT)   // 112 (сама матрица)
#define E7_COLON_DOTS     2                        // разделительные точки ЧЧ/ММ
#define E7_LEDS           (E7_MATRIX_LEDS + E7_COLON_DOTS)   // 114 (вся лента)

// Точки физически последние в цепочке: 112 — нижняя, 113 — верхняя.
#define E7_COLON_LOWER_IDX E7_MATRIX_LEDS          // 112, логическая y=4
#define E7_COLON_UPPER_IDX (E7_MATRIX_LEDS + 1)    // 113, логическая y=2

// Начало координат: угол панели, где стоит первый светодиод
#define E7_ORIGIN_TOP_LEFT      0   // ВЛ
#define E7_ORIGIN_TOP_RIGHT     1   // ВП
#define E7_ORIGIN_BOTTOM_LEFT   2   // НЛ (дефолт)
#define E7_ORIGIN_BOTTOM_RIGHT  3   // НП

// Направление первого ряда от угла подключения
#define E7_DIR_RIGHT  0
#define E7_DIR_UP     1               // дефолт
#define E7_DIR_LEFT   2
#define E7_DIR_DOWN   3

// Тип развёртки
#define E7_LAYOUT_ZIGZAG    0         // змейка (дефолт)
#define E7_LAYOUT_PARALLEL  1         // все ряды в одну сторону

// Метод вывода: по умолчанию I2S+DMA, через -D E7_USE_RMT — RMT
#if defined(E7_USE_RMT)
typedef NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod> E7Strip;
#else
typedef NeoPixelBus<NeoGrbFeature, NeoEsp32I2s0Ws2812xMethod> E7Strip;
#endif

class E7Matrix {
public:
    E7Matrix();
    void init(int16_t dataPin);                       // создать ленту и Begin()
    void destroy();                                   // удалить ленту
    void setTopology(uint8_t origin, uint8_t direction, uint8_t layout);
    uint16_t ledIndex(int x, int y);                  // логические координаты -> LED
    void setPixel(int x, int y, uint32_t rgb, uint8_t brightness);
    void setColonDot(bool upper, uint32_t rgb, uint8_t brightness);  // точка ЧЧ/ММ
    void clear();
    void show();
    bool ready() const;

private:
    E7Strip* _strip;
    uint8_t _origin;      // E7_ORIGIN_*
    uint8_t _direction;   // E7_DIR_*
    uint8_t _layout;      // E7_LAYOUT_*
};

#endif // _E7RGB_MATRIX_h
