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

#define CONFIG_FILE_E7RGB   "/config_e7rgb.json"
#define E7_FONT_DIR         "/e7fonts"
#define E7_FONT_DIR_SLASH   "/e7fonts/"
#define E7_FONT_DEFAULT     "/e7fonts/digital7.fnt"

#define E7_MODE_WORK        0   // рабочий режим: часы ЧЧ:ММ (NTP)
#define E7_MODE_MANUAL      1   // ручной режим: свой символ в каждой из 4 цифр

// Эффекты окраски отображаемого
#define E7_EFFECT_MONO          0   // моноцвет (digitsColor)
#define E7_EFFECT_RAINBOW       1   // радуга с переливом
#define E7_EFFECT_GRAD_STATIC   2   // градиент из 2 цветов, статичный
#define E7_EFFECT_GRAD_DYNAMIC  3   // градиент из 2 цветов, динамический
#define E7_EFFECT_COLORCYCLE    4   // плавная смена цвета по палитре

// Режимы обхода палитры для E7_EFFECT_COLORCYCLE
#define E7_CYCLE_SEQUENTIAL 0      // по порядку, заданному пользователем
#define E7_CYCLE_RANDOM     1      // произвольно (случайный порядок на круг)
#define E7_CYCLE_RAINBOW    2      // по радуге (палитра не используется)

#define E7_FX_MAX_COLORS    8      // максимум цветов в палитре

// Направления перелива/градиента
#define E7_FXDIR_TL   0   // из верхнего левого угла (по диагонали)
#define E7_FXDIR_TR   1   // из верхнего правого угла
#define E7_FXDIR_BL   2   // из нижнего левого угла
#define E7_FXDIR_BR   3   // из нижнего правого угла
#define E7_FXDIR_TB   4   // сверху вниз
#define E7_FXDIR_BT   5   // снизу вверх

#define E7RGB_ANIM_MS  100   // период продвижения фазы динамических эффектов
#define E7_FX_RENDER_MS 50   // период рендера переходов/дождя (плавность)

// Эффекты смены времени (E7_TFX_*)
#define E7_TFX_OFF         0   // выкл
#define E7_TFX_FADE        1   // затухание: гаснет прошлое -> проявляется новое
#define E7_TFX_WIPE_H      2   // шторка слева-вправо
#define E7_TFX_WIPE_V      3   // шторка сверху-вниз
#define E7_TFX_DISSOLVE    4   // растворение (порог по случайным пикселям)
#define E7_TFX_SLIDE_UP    5   // сдвиг вверх (новое выезжает снизу)
#define E7_TFX_SLIDE_DOWN  6   // сдвиг вниз (новое выезжает сверху)
#define E7_TFX_SLIDE_LEFT  7   // сдвиг влево (новое выезжает справа)
#define E7_TFX_SLIDE_RIGHT 8   // сдвиг вправо (новое выезжает слева)
#define E7_TFX_FLASH       9   // вспышка белым в середине перехода
#define E7_TFX_DIGIT_FADE  10  // по цифрам: глифы переходят поочерёдно (затухание)
#define E7_TFX_DIGIT_SLIDE 11  // по цифрам: глифы сдвигаются поочерёдно
#define E7_TFX_COUNT       12

// Кратности минут для срабатывания эффекта смены времени
#define E7_TFX_FREQ_DEFAULT 1

#define E7_RAIN_RAMP_MS    1000  // плавный разгон интенсивности дождя при старте

// Состояния отображения
#define E7_VIEW_NORMAL  0   // обычный режим (цифры/прочерки), переходы смены времени
#define E7_VIEW_RAIN    1   // заставка-дождь (до синхронизации NTP)
#define E7_VIEW_SETTLE  2   // оседание дождя в цифры
#define E7_VIEW_TRANS   3   // переход смены времени

typedef struct {
    uint8_t  mode;          // E7_MODE_WORK / E7_MODE_MANUAL
    int16_t  dataPin;       // пин данных (по умолчанию 16, -1 = выкл.)
    uint8_t  brightness;    // 0..255 (по умолчанию 25; масштабирование каналов)
    uint8_t  effect;        // E7_EFFECT_*
    uint8_t  effectDir;     // E7_FXDIR_* — направление перелива/градиента
    uint32_t digitsColor;   // 0xRRGGBB — цвет 1 (моноцвет / начало градиента)
    uint32_t digitsColor2;  // 0xRRGGBB — цвет 2 (конец градиента)
    uint8_t  animSpeed;     // 1..50 — скорость перелива (градус фазы за тик)
    uint8_t  colorsCount;   // 1..8 — кол-во цветов палитры (смена цвета)
    uint8_t  cycleMode;     // E7_CYCLE_* — режим обхода палитры
    uint32_t palette[E7_FX_MAX_COLORS];   // палитра для плавной смены цвета
    uint8_t  origin;        // E7_ORIGIN_* (дефолт НЛ — спаянная матрица)
    uint8_t  direction;     // E7_DIR_* (дефолт вверх)
    uint8_t  layout;        // E7_LAYOUT_* (дефолт параллельно)
    uint8_t  timeFx;        // E7_TFX_* — эффект смены времени (0 = выкл)
    uint8_t  timeFxFreq;    // кратность минуты: 1,3,5,10,15,20,40
    uint8_t  timeFxDur;     // 1..10 с — длительность перехода
    uint8_t  timeFxBg;      // 0..10 — яркость фоновой заливки поля в переходе
    uint8_t  rainEnabled;   // 0/1 — «дождь» при включении
    uint8_t  rainDurMin;    // 1..10 с — минимальная длительность дождя
    uint8_t  rainIntensity; // 1..10 — интенсивность дождя
    uint8_t  rainSettleDur; // 0..10 с — длительность оседания дождя в цифры
    String   fontFile;      // путь шрифта в ФС
    String   manualText;    // 4 символа для ручного режима ('0'..'9','-',' ')
} strE7RgbConfig;

class CLASS_DEVICE_E7RGB {
public:
    CLASS_DEVICE_E7RGB();
    void setFs(fs::LittleFSFS* fs);
    void begin();
    void begin(ModContext& ctx);
    void web_Init();

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
