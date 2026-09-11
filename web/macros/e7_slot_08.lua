-- E7 RGB: слот 40-45 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 40-45: effect 3",
    rules = {
        { cron = "0 40 * * * *", body = function()
            set("e7.effect", 3)
            set("e7.speed", 35)
            set("e7.brightness", 20)
            set("e7.color", 16777215)
        end },
        { cron = "30 42 * * * *", body = function()
            set("e7.color", 16711680)
        end },
    }
}
