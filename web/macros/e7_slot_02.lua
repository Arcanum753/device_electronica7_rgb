-- E7 RGB: слот 10-15 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 10-15: effect 2",
    rules = {
        { cron = "0 10 * * * *", calls = {
            { name = "e7.effect",     args = { 2 } },
            { name = "e7.speed",      args = { 30 } },
            { name = "e7.brightness", args = { 40 } },
            { name = "e7.color",      args = { 255 } },
        } },
        { cron = "30 12 * * * *", call = "e7.color", args = { 16744448 } },
    },
}
