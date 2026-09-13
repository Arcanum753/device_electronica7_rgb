-- E7 RGB: слот 45-50 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 45-50: effect 4",
    rules = {
        { when = { cron = "0 45 * * * *" }, calls = {
            { name = "e7.effect",     args = { 4 } },
            { name = "e7.speed",      args = { 40 } },
            { name = "e7.brightness", args = { 30 } },
            { name = "e7.color",      args = { 65408 } },
        } },
        { when = { cron = "30 47 * * * *" }, call = "e7.color", args = { 65280 } },
    },
}