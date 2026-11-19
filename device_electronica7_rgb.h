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
    bool setColonBlink(uint8_t s);
    bool setSpeed(uint8_t s);
    bool setDigitsColor(uint32_t c);
    bool setManualText(const String& t);
    bool setBusMode(uint8_t m);
    bool setDisplayMode(uint8_t d);
    bool setTickerText(const String& t);
    bool setShowText(const String& t);   // текст: <=4 симв. статично, длиннее — ticker
    bool setTickerSpeed(uint8_t ms);
    bool setTickerDirection(uint8_t dir);
    bool setDigitFlip(uint8_t f);
    bool setDigitFlipSpeed(uint8_t s);
    uint8_t getBusMode();

    // Явное сохранение текущего in-memory конфига (для терминала и макросов).
    void saveNow();
    // Сохранение только режима шины (команда e7.save_mode).
    bool saveModeConfig();

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

    // Терминал
    void terminalRegister();
    static void s_terminalCmd();
    static void s_cmdShow();
    static void s_cmdEffect();
    static void s_cmdBrightness();
    static void s_cmdSpeed();
    static void s_cmdText();
    static void s_cmdTicker();
    static void s_cmdFlip();
    static void s_cmdSave();
    static void s_cmdSaveMode();

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

    // Бегущая строка (displayMode == E7_DISP_TICKER)
    void drawTicker();                     // нарисовать текущий кадр строки
    void tickerScroll();                   // сдвинуть строку по времени (tickerSpeed)

    // Перелистывание цифр (digitFlip при смене минут в режиме часов)
    void startDigitFlip(uint8_t h, uint8_t m);
    void drawFlipFrame();                  // нарисовать текущий шаг перелистывания

    // Переходы смены времени и «дождь»
    void buildTimeMask(uint8_t h, uint8_t m, uint8_t mask[E7_HEIGHT][E7_WIDTH]);
    void drawMaskFx(const uint8_t mask[E7_HEIGHT][E7_WIDTH], bool withDots = true);
    void compositePixel(int x, int y, uint32_t rgb, float level);
    void drawTransition();
    void startTransition(uint8_t h, uint8_t m);
    void startExitTransition();                 // часы -> пустой кадр (перед текстом)
    void startReturnTransition(uint8_t h, uint8_t m);  // пустой кадр -> часы
    void startRain();
    void drawRain(bool settleMode, float settleP, const uint8_t target[E7_HEIGHT][E7_WIDTH]);
    void drawSettle();

    // Разделительные точки ЧЧ/ММ (LED 112/113)
    bool dotBlinkOn();                                 // текущая фаза моргания
    void drawDots(float level, bool white = false, uint32_t colorOverride = 0xFFFFFFFF);    // зажечь/погасить обе точки

    static void deferredApplyTask();

    friend void e7rgbSecondTask();
    friend void e7rgbAnimTask();

protected:
    fs::LittleFSFS* _fs;
    strE7RgbConfig _config;
    E7Matrix _matrix;
    E7Fonts _fonts;
    String  _loadedFontPath;  // путь уже загруженного шрифта (защита от повторных open)
    uint32_t _loadedFontSize = 0; // размер файла при загрузке (для перезагрузки при замене)

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
    bool     _transBlankPrev; // предыдущий кадр перехода — пустой (приезд)
    bool     _transBlankCur;  // текущий кадр перехода — пустой (уезд)
    uint32_t _transPrevColor; // цвет предыдущего кадра перехода
    uint32_t _transCurColor;  // цвет текущего кадра перехода
    uint32_t _colorPrev;      // цвет, который был на экране до последней смены
    uint8_t  _transRand[E7_HEIGHT][E7_WIDTH];  // пороги растворения
    bool     _toClockFx;      // при возврате на часы проиграть timeFx
    bool     _toTextFx;       // при уходе с часов проиграть timeFx (в пустоту)
    bool     _tickerOneShot;  // ticker от e7.show: один проход, затем пусто
    uint32_t _settleStartMs;  // момент старта оседания
    uint32_t _rainStartMs;    // момент старта дождя (для минимума показа)
    uint32_t _rainRampStartMs;// момент старта разгона интенсивности
    float    _rainHead[E7_WIDTH];   // Y головы капли в столбце (0 = низ)
    float    _rainSpeed[E7_WIDTH];  // скорость падения (пикс/тик)
    bool     _rainColOn[E7_WIDTH];  // активность столбца
    uint32_t _fxAccumMs;      // накопитель времени фазы цвета
    bool     _dotOn;          // фактически нарисованное состояние точек

    // Бегущая строка
    int16_t  _tickerOffset;   // сдвиг строки в пикселях
    uint32_t _tickerLastMs;   // момент последнего шага прокрутки
    bool     _tickerReset;    // сбросить сдвиг при следующем применении

    // Перелистывание цифр
    uint8_t  _flipCur[4];     // что сейчас на экране (разряды)
    uint8_t  _flipNxt[4];     // куда листаем
    bool     _flipOn;         // перелистывание активно
    uint32_t _flipStartMs;    // момент старта перелистывания
    uint8_t  _flipDir;        // E7_FLIP_UP / E7_FLIP_DOWN
    uint8_t  _flipStepMs;     // мс на шаг (60/35/20)
};

extern CLASS_DEVICE_E7RGB device_electronica7_rgb;

#endif // _DEVICE_E7RGB_h
