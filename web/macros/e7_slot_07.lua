-- E7 RGB: слот 35-40 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 35-40: effect 2",
    rules = {
        { cron = "0 35 * * * *", body = function()
            set("e7.effect", 2)
            set("e7.speed", 30)
            set("e7.brightness", 50)
            set("e7.color", 8388863)
        end },
        { cron = "30 37 * * * *", body = function()
            set("e7.color", 8404992)
        end },
    }
}
