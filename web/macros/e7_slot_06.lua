-- E7 RGB: слот 30-35 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 30-35: effect 1",
    rules = {
        { cron = "0 30 * * * *", body = function()
            set("e7.effect", 1)
            set("e7.speed", 25)
            set("e7.brightness", 40)
            set("e7.color", 16744448)
        end },
        { cron = "30 32 * * * *", body = function()
            set("e7.color", 16711808)
        end },
    }
}
