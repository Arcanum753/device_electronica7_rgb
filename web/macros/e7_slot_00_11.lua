-- E7 RGB: все 12 слотов (e7_slot_00..e7_slot_11) в одном сценарии.
-- Слоты идут по 5 минут (00-04, 05-09, ... 55-59) и повторяются каждый час.
-- Тело тикает раз в секунду: в 0-ю секунду минуты применяются эффект, скорость,
-- яркость и основной цвет слота, в 30-ю секунду второй минуты слота — второй цвет.
-- Обработчики every/after не регистрируются: 12 слотов x 2 события превышают
-- лимит MACRO_MAX_HANDLERS = 16 на файл.
meta{ desc = "E7 RGB: слоты 00-11" }

-- Таблица слотов создаётся один раз и хранится в state.
local slots = state.slots
if not slots then
    slots = {
        { effect = "mono",         speed = 20, brightness = 20, color = "#FF0000", color2 = "#00FFFF" },
        { effect = "rainbow",      speed = 25, brightness = 30, color = "#00FF00", color2 = "#FF00FF" },
        { effect = "grad_static",  speed = 30, brightness = 40, color = "#0000FF", color2 = "#FF8000" },
        { effect = "grad_dynamic", speed = 35, brightness = 50, color = "#FFFF00", color2 = "#8000FF" },
        { effect = "colorcycle",   speed = 40, brightness = 20, color = "#00FFFF", color2 = "#FFFFFF" },
        { effect = "mono",         speed = 20, brightness = 30, color = "#FF00FF", color2 = "#00FF80" },
        { effect = "rainbow",      speed = 25, brightness = 40, color = "#FF8000", color2 = "#FF0080" },
        { effect = "grad_static",  speed = 30, brightness = 50, color = "#8000FF", color2 = "#804000" },
        { effect = "grad_dynamic", speed = 35, brightness = 20, color = "#FFFFFF", color2 = "#FF0000" },
        { effect = "colorcycle",   speed = 40, brightness = 30, color = "#00FF80", color2 = "#00FF00" },
        { effect = "mono",         speed = 20, brightness = 40, color = "#FF0080", color2 = "#0000FF" },
        { effect = "rainbow",      speed = 25, brightness = 50, color = "#804000", color2 = "#FFFF00" },
    }
    state.slots = slots
end

local minute = time.minute
local slot = slots[minute // 5 + 1]
if slot == nil then
    return
end

if time.second == 0 then
    e7.effect = slot.effect
    e7.speed(slot.speed)
    e7.brightness = slot.brightness
    e7.color(slot.color)
elseif time.second == 30 and minute % 5 == 2 then
    e7.color(slot.color2)
end
