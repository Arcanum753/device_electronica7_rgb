-- E7 RGB: слот 0-5 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 0-5: effect 0",
    rules = {
        { cron = "0 0 * * * *", body = function()
            set("e7.effect", 0)
            set("e7.speed", 20)
            set("e7.brightness", 20)
            set("e7.color", 16711680)
        end },
        { cron = "30 2 * * * *", body = function()
            set("e7.color", 65535)
        end },
    }
}
