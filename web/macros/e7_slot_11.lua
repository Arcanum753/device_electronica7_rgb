-- E7 RGB: слот 55-60 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 55-60: effect 1",
    rules = {
        { cron = "0 55 * * * *", body = function()
            set("e7.effect", 1)
            set("e7.speed", 25)
            set("e7.brightness", 50)
            set("e7.color", 8404992)
        end },
        { cron = "30 57 * * * *", body = function()
            set("e7.color", 16776960)
        end },
    }
}
