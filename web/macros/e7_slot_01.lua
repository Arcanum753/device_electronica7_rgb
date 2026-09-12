-- E7 RGB: слот 5-10 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 5-10: effect 1",
    rules = {
        { cron = "0 5 * * * *", calls = {
            { name = "e7.effect",     args = { 1 } },
            { name = "e7.speed",      args = { 25 } },
            { name = "e7.brightness", args = { 30 } },
            { name = "e7.color",      args = { 65280 } },
        } },
        { cron = "30 7 * * * *", call = "e7.color", args = { 16711935 } },
    },
}
