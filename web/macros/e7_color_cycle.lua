-- E7 RGB: цикл цвета, первая половина (12 шагов разбито на 2 файла по лимиту правил).
return {
    desc = "E7 color cycle (steps 1/2)",
    rules = {
        { when = { cron = "0 * * * * *"  }, call = "e7.color", args = { 16711680 } },
        { when = { cron = "5 * * * * *"  }, call = "e7.color", args = { 16744448 } },
        { when = { cron = "10 * * * * *" }, call = "e7.color", args = { 16776960 } },
        { when = { cron = "15 * * * * *" }, call = "e7.color", args = { 8453888 } },
        { when = { cron = "20 * * * * *" }, call = "e7.color", args = { 65280 } },
        { when = { cron = "25 * * * * *" }, call = "e7.color", args = { 65408 } },
    },
}