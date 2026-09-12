-- E7 RGB: слот 25-30 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 25-30: effect 0",
    rules = {
        { cron = "0 25 * * * *", calls = {
            { name = "e7.effect",     args = { 0 } },
            { name = "e7.speed",      args = { 20 } },
            { name = "e7.brightness", args = { 30 } },
            { name = "e7.color",      args = { 16711935 } },
        } },
        { cron = "30 27 * * * *", call = "e7.color", args = { 65408 } },
    },
}
