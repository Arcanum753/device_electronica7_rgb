-- E7 RGB: С†РёРєР» С†РІРµС‚Р°, РІС‚РѕСЂР°СЏ РїРѕР»РѕРІРёРЅР° (12 С€Р°РіРѕРІ СЂР°Р·Р±РёС‚Рѕ РЅР° 2 С„Р°Р№Р»Р° РїРѕ Р»РёРјРёС‚Сѓ РїСЂР°РІРёР»).
-- РќРѕРІС‹Р№ СЃС‚РёР»СЊ: РєР°Р¶РґС‹Р№ С€Р°Рі вЂ” РѕС‚РґРµР»СЊРЅС‹Р№ cron-РѕР±СЂР°Р±РѕС‚С‡РёРє.
meta{ desc = "E7 color cycle (steps 2/2)" }

every("30 * * * * *", function() e7.color("#00FFFF") end)
every("35 * * * * *", function() e7.color("#0080FF") end)
every("40 * * * * *", function() e7.color("#0000FF") end)
every("45 * * * * *", function() e7.color("#8000FF") end)
every("50 * * * * *", function() e7.color("#FF00FF") end)
every("55 * * * * *", function() e7.color("#FF0080") end)