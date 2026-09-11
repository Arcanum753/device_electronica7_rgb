-- E7 RGB: слот 10-15 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 10-15: effect 2",
    rules = {
        { cron = "0 10 * * * *", body = function()
            set("e7.effect", 2)
            set("e7.speed", 30)
            set("e7.brightness", 40)
            set("e7.color", 255)
        end },
        { cron = "30 12 * * * *", body = function()
            set("e7.color", 16744448)
        end },
    }
}
