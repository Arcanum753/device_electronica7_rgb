-- E7 RGB: one sensor reading every 30 s, clock (green) in between.
-- Each reading is labeled (e.g. "T1=27.0C") and gets its own color; the
-- label is longer than 4 chars, so e7.show switches to ticker automatically.
-- The line runs one full pass (duration = max(5 s, pass time at current
-- speed + transition time)); then the clock comes back and dots blink.
-- Missing sensors (*.available == false) are skipped.
-- Requires e7.mode = "macro": writes are denied in off/auto.
meta{ desc = "E7 sensors 30s, colors, clock" }

-- Lua stdlib is disabled: no math/string/tostring. Only //, %, .. and #.
local D = {"0","1","2","3","4","5","6","7","8","9"}

local CLOCK_COLOR = "#00FF00"   -- время — зелёное

-- Первый запуск: часы зелёные (дальше пользователь может менять).
if not state.init then
    state.init = true
    e7.color = CLOCK_COLOR
end

local function abs(v)
    if v < 0 then return -v end
    return v
end

-- Integer to string (truncates toward zero).
local function u(v)
    local neg = (v < 0)
    local n = abs(v) // 1
    local s = "0"
    if n > 0 then
        s = ""
        while n > 0 do
            local d = n % 10
            s = D[d + 1] .. s
            n = n // 10
        end
    end
    if neg then s = "-" .. s end
    return s
end

-- One decimal digit, rounded.
local function f1(v)
    local neg = (v < 0)
    local r = (abs(v) * 10 + 0.5) // 1
    local whole = r // 10
    local frac = r % 10
    local s = u(whole) .. "." .. D[frac + 1]
    if neg then s = "-" .. s end
    return s
end

-- Integer, rounded.
local function i0(v)
    local neg = (v < 0)
    local r = (abs(v) + 0.5) // 1
    local s = u(r)
    if neg and r > 0 then s = "-" .. s end
    return s
end

-- Labeled readings with individual colors.
local function readings()
    local items = {}

    if bmp280.available == true then
        items[#items + 1] = { s = "T1=" .. f1(bmp280.temperature) .. "C", c = "#FF8000" }
        items[#items + 1] = { s = "P=" .. i0(bmp280.pressure) .. "HPA",   c = "#00FFFF" }
    end

    if ccs811.available == true then
        items[#items + 1] = { s = "CO2=" .. i0(ccs811.eco2) .. "PPM",      c = "#FF00FF" }
        items[#items + 1] = { s = "TVOC=" .. i0(ccs811.tvoc) .. "PPB",    c = "#FFFF00" }
    end

    if hdc1080.available == true then
        items[#items + 1] = { s = "T2=" .. f1(hdc1080.temperature) .. "C", c = "#FF0000" }
        items[#items + 1] = { s = "H=" .. f1(hdc1080.humidity) .. "%",     c = "#0080FF" }
    end

    return items
end

every(30, function()
    local items = readings()
    local n = #items

    if n == 0 then
        e7.color = CLOCK_COLOR
        e7.display = "clock"
        state.i = 0
        return
    end

    local i = state.i or 0
    if i >= n then i = 0 end
    local it = items[i + 1]
    state.i = i + 1

    -- Цвет — до показа: часы уезжают уже в цвете значения.
    e7.color = it.c
    e7.show = it.s

    -- Длительность: не меньше 5 с; полный проход строки при текущей
    -- скорости + 4 с (переход «часы уезжают» ~2 с и запас >1 с).
    local speed = get("e7.ticker_speed") or 160
    if speed < 10 then speed = 10 end
    local secs = ((#it.s * 5 + 16) * speed + 999) // 1000 + 4
    if secs < 5 then secs = 5 end

    after(secs, function()
        e7.color = CLOCK_COLOR
        e7.display = "clock"
    end)
end)
