-- E7 RGB: слот 45-50 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 45-50: effect 4",
    rules = {
        { cron = "0 45 * * * *", body = function()
            set("e7.effect", 4)
            set("e7.speed", 40)
            set("e7.brightness", 30)
            set("e7.color", 65408)
        end },
        { cron = "30 47 * * * *", body = function()
            set("e7.color", 65280)
        end },
    }
}
