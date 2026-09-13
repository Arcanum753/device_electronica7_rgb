-- E7 RGB: цикл цвета, вторая половина (12 шагов разбито на 2 файла по лимиту правил).
return {
    desc = "E7 color cycle (steps 2/2)",
    rules = {
        { when = { cron = "30 * * * * *" }, call = "e7.color", args = { 65535 } },
        { when = { cron = "35 * * * * *" }, call = "e7.color", args = { 33023 } },
        { when = { cron = "40 * * * * *" }, call = "e7.color", args = { 255 } },
        { when = { cron = "45 * * * * *" }, call = "e7.color", args = { 8388863 } },
        { when = { cron = "50 * * * * *" }, call = "e7.color", args = { 16711935 } },
        { when = { cron = "55 * * * * *" }, call = "e7.color", args = { 16711808 } },
    },
}