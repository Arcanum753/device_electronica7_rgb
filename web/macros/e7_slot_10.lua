-- E7 RGB: слот 50-55 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 50-55: effect 0",
    rules = {
        { when = { cron = "0 50 * * * *" }, calls = {
            { name = "e7.effect",     args = { 0 } },
            { name = "e7.speed",      args = { 20 } },
            { name = "e7.brightness", args = { 40 } },
            { name = "e7.color",      args = { 16711808 } },
        } },
        { when = { cron = "30 52 * * * *" }, call = "e7.color", args = { 255 } },
    },
}