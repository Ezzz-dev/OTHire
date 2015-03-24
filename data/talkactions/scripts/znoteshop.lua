-- Znote Shop v1.0 for Znote AAC on TFS 1.0+.
function onSay(cid, words, param)
        local storage = 54073 -- Make sure to select non-used storage. This is used to prevent SQL load attacks.
        local cooldown = 15 -- in seconds.
 
        if getPlayerStorageValue(cid, storage) > os.time() then
                doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, "Can only be executed once every " .. cooldown .. " seconds. Remaining cooldown: " .. getPlayerStorageValue(cid, storage) - os.time())
                return false
        end
 
        setPlayerStorageValue(cid, storage, os.time() + cooldown)
 
        -- Create the query
        local orderQuery = db.storeQuery("SELECT `id`, `type`, `itemid`, `count` FROM `znote_shop_orders` WHERE `account_id` = " .. getPlayerAccountId(cid) .. " LIMIT 1;")
 
        -- Detect if we got any results
        if orderQuery == false then
                doPlayerSendTextMessage(cid, MESSAGE_STATUS_WARNING, "You have no orders.")
                return false
        end
 
        -- Fetch order values
        local q_id = result.getDataInt(orderQuery, "id")
        local q_type = result.getDataInt(orderQuery, "type")
        local q_itemid = result.getDataInt(orderQuery, "itemid")
        local q_count = result.getDataInt(orderQuery, "count")
        result.free(orderQuery)
 
        -- ORDER TYPE 1 (Regular item shop products)
        if q_type == 1 then
                local item = doCreateItemEx(q_itemid, q_count)
                if doPlayerAddItemEx(cid, item) ~= RETURNVALUE_NOERROR then
                        if getPlayerFreeCap(cid) < getItemWeight(item) then
                                doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, "Need more CAP!")
                        else
                                doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, "Need more SPACE!")
                        end
                        return false
                end
 
                db.query("DELETE FROM `znote_shop_orders` WHERE `id` = " .. q_id)
                doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, "Congratulations! You have received " .. q_count .. " x " .. getItemName(q_itemid) .. "!")
        end
 
        -- Add custom order types here
        -- Type 2 is reserved for premium days and is handled on website, not needed here.
        -- Type 3 is reserved for character gender(sex) change and is handled on website as well.
        -- So use type 4+ for custom stuff, like etc packages.
        -- if q_type == 4 then
        -- end
        return false
end
