-- E7 RGB: слот 35-40 мин. Эффект отображения + смена цвета.
return {
    desc = "E7 slot 35-40: effect 2",
    rules = {
        { when = { cron = "0 35 * * * *" }, calls = {
            { name = "e7.effect",     args = { 2 } },
            { name = "e7.speed",      args = { 30 } },
            { name = "e7.brightness", args = { 50 } },
            { name = "e7.color",      args = { 8388863 } },
        } },
        { when = { cron = "30 37 * * * *" }, call = "e7.color", args = { 8404992 } },
    },
}