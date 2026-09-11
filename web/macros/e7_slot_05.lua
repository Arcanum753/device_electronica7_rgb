-- E7 RGB: слот 25-30 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 25-30: effect 0",
    rules = {
        { cron = "0 25 * * * *", body = function()
            set("e7.effect", 0)
            set("e7.speed", 20)
            set("e7.brightness", 30)
            set("e7.color", 16711935)
        end },
        { cron = "30 27 * * * *", body = function()
            set("e7.color", 65408)
        end },
    }
}
