-- E7 RGB: слот 0-5 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 0-5: effect 0",
    rules = {
        { cron = "0 0 * * * *", calls = {
            { name = "e7.effect",     args = { 0 } },
            { name = "e7.speed",      args = { 20 } },
            { name = "e7.brightness", args = { 20 } },
            { name = "e7.color",      args = { 16711680 } },
        } },
        { cron = "30 2 * * * *", call = "e7.color", args = { 65535 } },
    },
}
