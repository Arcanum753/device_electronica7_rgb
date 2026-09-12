#ifndef _DEVICE_E7RGB_h
#define _DEVICE_E7RGB_h

#include "main.h"

#include "mod_context.h"

#include "e7rgb_matrix.h"
#include "e7rgb_fonts.h"

#ifdef DEBUG_E7RGB
#define DEBUGE7RGB(...) DBG_MOD("[D_E7RGB] ", __VA_ARGS__)
#else
#define DEBUGE7RGB(...)
#endif

#include <LittleFS.h>

#include "device_electronica7_rgb_types.h"

class CLASS_DEVICE_E7RGB {
public:
    CLASS_DEVICE_E7RGB();
    void setFs(fs::LittleFSFS* fs);
    void begin();
    void begin(ModContext& ctx);
    void register_resources();
    void web_Init();

    // Публичное API для ресурсной шины
    bool setEffect(uint8_t e);
    bool setBrightness(uint8_t b);
    bool setSpeed(uint8_t s);
    bool setDigitsColor(uint32_t c);
    bool setManualText(const String& t);
    bool setBusMode(uint8_t m);
    uint8_t getBusMode();

private:
    // Версионные методы
    String getVersionStr();
    String getGeneratedTime();
    String getCommitDateStr();
    void html_ver_get(AsyncWebServerRequest *request);

    // Веб-обработчики
    void handleInfo(AsyncWebServerRequest *request);
    void handleSave(AsyncWebServerRequest *request);
    void handleFonts(AsyncWebServerRequest *request);

    // Конфиг
    void defaultConfig();
    bool loadConfig();
    bool saveConfig();

    // Логика устройства
    bool loadFont();                       // загрузка шрифта из ФС (с автосозданием)
    void initMatrix();                     // создать ленту по конфигу
    void destroyMatrix();
    void applyMode();                      // применить текущий режим на матрицу
    void redrawFrame();                    // перерисовать рабочий кадр (время/прочерки)
    void renderDigits(uint8_t h, uint8_t m);
    void renderNoTime();
    void renderManual();                   // ручной режим: показать manualText
    void blitGlyph(uint8_t x0, const uint8_t* glyph, uint8_t mask[E7_HEIGHT][E7_WIDTH]);

    // Переходы смены времени и «дождь»
    void buildTimeMask(uint8_t h, uint8_t m, uint8_t mask[E7_HEIGHT][E7_WIDTH]);
    void drawMaskFx(const uint8_t mask[E7_HEIGHT][E7_WIDTH]);
    void compositePixel(int x, int y, uint32_t rgb, float level);
    void drawTransition();
    void startTransition(uint8_t h, uint8_t m);
    void startRain();
    void drawRain(bool settleMode, float settleP, const uint8_t target[E7_HEIGHT][E7_WIDTH]);
    void drawSettle();

    static void deferredApplyTask();

    friend void e7rgbSecondTask();
    friend void e7rgbAnimTask();

protected:
    fs::LittleFSFS* _fs;
    strE7RgbConfig _config;
    E7Matrix _matrix;
    E7Fonts _fonts;

    uint8_t  _lastMinute;      // последняя нарисованная минута
    bool    _ntpWasSynced;    // первая синхронизация NTP (переход на часы)
    bool    _forceRedraw;     // принудительная перерисовка (настройки/режим)
    bool    _pendingReinit;   // отложенное пересоздание ленты (смена пина)
    bool    _pendingSave;     // отложенное сохранение конфига
    bool    _pendingApply;    // отложенное применение
    int16_t _pendingDataPin;  // новый пин (для _pendingReinit)
    float   _animPhase;       // фаза анимации эффекта (0..360 градусов)
    uint32_t _fxLap;          // номер «круга» обхода палитры (для случайного порядка)
    uint8_t  _fxOrder[E7_FX_MAX_COLORS];  // текущий порядок обхода палитры

    uint8_t  _view;           // E7_VIEW_* — текущее состояние отображения
    uint8_t  _curH, _curM;    // отображаемое время
    uint8_t  _prevH, _prevM;  // время-источник перехода
    uint8_t  _transType;      // E7_TFX_* активного перехода
    uint32_t _transStartMs;   // момент старта перехода
    uint8_t  _transRand[E7_HEIGHT][E7_WIDTH];  // пороги растворения
    uint32_t _settleStartMs;  // момент старта оседания
    uint32_t _rainStartMs;    // момент старта дождя (для минимума показа)
    uint32_t _rainRampStartMs;// момент старта разгона интенсивности
    float    _rainHead[E7_WIDTH];   // Y головы капли в столбце (0 = низ)
    float    _rainSpeed[E7_WIDTH];  // скорость падения (пикс/тик)
    bool     _rainColOn[E7_WIDTH];  // активность столбца
    uint32_t _fxAccumMs;      // накопитель времени фазы цвета
};

extern CLASS_DEVICE_E7RGB device_electronica7_rgb;

#endif // _DEVICE_E7RGB_h
