-- E7 RGB: слот 40-45 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 40-45: effect 3",
    rules = {
        { cron = "0 40 * * * *", calls = {
            { name = "e7.effect",     args = { 3 } },
            { name = "e7.speed",      args = { 35 } },
            { name = "e7.brightness", args = { 20 } },
            { name = "e7.color",      args = { 16777215 } },
        } },
        { cron = "30 42 * * * *", call = "e7.color", args = { 16711680 } },
    },
}
