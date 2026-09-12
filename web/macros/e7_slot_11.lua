-- E7 RGB: слот 55-60 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 55-60: effect 1",
    rules = {
        { cron = "0 55 * * * *", calls = {
            { name = "e7.effect",     args = { 1 } },
            { name = "e7.speed",      args = { 25 } },
            { name = "e7.brightness", args = { 50 } },
            { name = "e7.color",      args = { 8404992 } },
        } },
        { cron = "30 57 * * * *", call = "e7.color", args = { 16776960 } },
    },
}
