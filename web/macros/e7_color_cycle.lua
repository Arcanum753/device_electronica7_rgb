-- E7 RGB: С†РёРєР» С†РІРµС‚Р°, РїРµСЂРІР°СЏ РїРѕР»РѕРІРёРЅР° (12 С€Р°РіРѕРІ СЂР°Р·Р±РёС‚Рѕ РЅР° 2 С„Р°Р№Р»Р° РїРѕ Р»РёРјРёС‚Сѓ РїСЂР°РІРёР»).
-- РќРѕРІС‹Р№ СЃС‚РёР»СЊ: РєР°Р¶РґС‹Р№ С€Р°Рі вЂ” РѕС‚РґРµР»СЊРЅС‹Р№ cron-РѕР±СЂР°Р±РѕС‚С‡РёРє.
meta{ desc = "E7 color cycle (steps 1/2)" }

every("0 * * * * *",  function() e7.color("#FF0000") end)
every("5 * * * * *",  function() e7.color("#FF8000") end)
every("10 * * * * *", function() e7.color("#FFFF00") end)
every("15 * * * * *", function() e7.color("#80FF00") end)
every("20 * * * * *", function() e7.color("#00FF00") end)
every("25 * * * * *", function() e7.color("#00FF80") end)