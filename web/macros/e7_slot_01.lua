-- E7 RGB: слот 5-10 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 5-10: effect 1",
    rules = {
        { cron = "0 5 * * * *", body = function()
            set("e7.effect", 1)
            set("e7.speed", 25)
            set("e7.brightness", 30)
            set("e7.color", 65280)
        end },
        { cron = "30 7 * * * *", body = function()
            set("e7.color", 16711935)
        end },
    }
}
