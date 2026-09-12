-- E7 RGB: слот 30-35 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 30-35: effect 1",
    rules = {
        { cron = "0 30 * * * *", calls = {
            { name = "e7.effect",     args = { 1 } },
            { name = "e7.speed",      args = { 25 } },
            { name = "e7.brightness", args = { 40 } },
            { name = "e7.color",      args = { 16744448 } },
        } },
        { cron = "30 32 * * * *", call = "e7.color", args = { 16711808 } },
    },
}
