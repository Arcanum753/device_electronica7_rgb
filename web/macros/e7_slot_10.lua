-- E7 RGB: слот 50-55 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 50-55: effect 0",
    rules = {
        { cron = "0 50 * * * *", body = function()
            set("e7.effect", 0)
            set("e7.speed", 20)
            set("e7.brightness", 40)
            set("e7.color", 16711808)
        end },
        { cron = "30 52 * * * *", body = function()
            set("e7.color", 255)
        end },
    }
}
