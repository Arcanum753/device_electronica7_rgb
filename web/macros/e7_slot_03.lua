-- E7 RGB: слот 15-20 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 15-20: effect 3",
    rules = {
        { when = { cron = "0 15 * * * *" }, calls = {
            { name = "e7.effect",     args = { 3 } },
            { name = "e7.speed",      args = { 35 } },
            { name = "e7.brightness", args = { 50 } },
            { name = "e7.color",      args = { 16776960 } },
        } },
        { when = { cron = "30 17 * * * *" }, call = "e7.color", args = { 8388863 } },
    },
}