-- E7 RGB: слот 15-20 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 15-20: effect 3",
    rules = {
        { cron = "0 15 * * * *", body = function()
            set("e7.effect", 3)
            set("e7.speed", 35)
            set("e7.brightness", 50)
            set("e7.color", 16776960)
        end },
        { cron = "30 17 * * * *", body = function()
            set("e7.color", 8388863)
        end },
    }
}
