-- E7 RGB: слот 20-25 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 20-25: effect 4",
    rules = {
        { cron = "0 20 * * * *", calls = {
            { name = "e7.effect",     args = { 4 } },
            { name = "e7.speed",      args = { 40 } },
            { name = "e7.brightness", args = { 20 } },
            { name = "e7.color",      args = { 65535 } },
        } },
        { cron = "30 22 * * * *", call = "e7.color", args = { 16777215 } },
    },
}
