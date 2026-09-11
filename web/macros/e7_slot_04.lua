-- E7 RGB: слот 20-25 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 20-25: effect 4",
    rules = {
        { cron = "0 20 * * * *", body = function()
            set("e7.effect", 4)
            set("e7.speed", 40)
            set("e7.brightness", 20)
            set("e7.color", 65535)
        end },
        { cron = "30 22 * * * *", body = function()
            set("e7.color", 16777215)
        end },
    }
}
