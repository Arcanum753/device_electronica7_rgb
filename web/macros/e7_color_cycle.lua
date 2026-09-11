-- E7 RGB: медленный цикл цвета (12 шагов по кругу).
return {
    desc = "E7 color cycle (12 steps)",
    rules = {
        { cron = "0 * * * * *", body = function()
            set("e7.color", 16711680)
        end },
        { cron = "5 * * * * *", body = function()
            set("e7.color", 16744448)
        end },
        { cron = "10 * * * * *", body = function()
            set("e7.color", 16776960)
        end },
        { cron = "15 * * * * *", body = function()
            set("e7.color", 8453888)
        end },
        { cron = "20 * * * * *", body = function()
            set("e7.color", 65280)
        end },
        { cron = "25 * * * * *", body = function()
            set("e7.color", 65408)
        end },
        { cron = "30 * * * * *", body = function()
            set("e7.color", 65535)
        end },
        { cron = "35 * * * * *", body = function()
            set("e7.color", 33023)
        end },
        { cron = "40 * * * * *", body = function()
            set("e7.color", 255)
        end },
        { cron = "45 * * * * *", body = function()
            set("e7.color", 8388863)
        end },
        { cron = "50 * * * * *", body = function()
            set("e7.color", 16711935)
        end },
        { cron = "55 * * * * *", body = function()
            set("e7.color", 16711808)
        end },
    }
}
