function checkExhausted(cid, storage, seconds)
local v = exhaustion.get(cid, storage)
    if(not v) then
        exhaustion.set(cid, storage, seconds)
    else
        doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, "Please wait " .. v .. " seconds before trying this command again.")
    return false
end
    return true
end
 
function onSay(cid, words, param)
    if(not checkExhausted(cid, 674, 2)) then
        return false
    end
    local guild = getPlayerGuildId(cid)
    if(not guild or getPlayerGuildLevel(cid) < 3) then
        doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, "You are not in a guild or you are not the leader.")
        return false
    end
 
    local t = string.explode(param, ",")
    if(not t[2]) then
        doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, "Not enough param(s).")
        return false
    end
 
    local enemy = getGuildId(t[2])
    if(enemy == false) then
        doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, "Guild \"" .. t[2] .. "\" does not exists.")
        return false
    end
 
    if(enemy == guild) then
        doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, "You cannot perform war action against your own guild.")
        return false
    end
 
    local enemyName, tmp = "", db.getResult("SELECT `name` FROM `guilds` WHERE `id` = " .. enemy)
    if(tmp:getID() ~= -1) then
        enemyName = tmp:getDataString("name")
        tmp:free()
    end
 
    if t[1] == "accept" or t[1] == "reject" or t[1] == "cancel" then
        local query = "`guild_id` = " .. enemy .. " AND `opponent_id` = " .. guild
        if(t[1] == "cancel") then
            query = "`guild_id` = " .. guild .. " AND `opponent_id` = " .. enemy
        end
 
        tmp = db.getResult("SELECT `id`, `declaration_date`, `end_date`, `guild_fee` FROM `guild_wars` WHERE " .. query .. " AND `status` = -1")
        if(tmp:getID() == -1) then
            doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, "Currently there's no pending invitation for a war with " .. enemyName .. ".")
            return false
        end
 
        query = "UPDATE `guild_wars` SET "
        local msg = "accepted Guild " .. enemyName .. " invitation to war."
        if(t[1] == "reject") then
            query = "DELETE FROM `guild_wars`"
            msg = "rejected Guild " .. enemyName .. " invitation to war."
        elseif(t[1] == "cancel") then
            query = "DELETE FROM `guild_wars`"
            msg = "canceled invitation to a war with Guild " .. enemyName .. "."
        else
            query = query .. "`declaration_date` = " .. os.time() .. ", `status` = 1"
        end
 
        query = query .. " WHERE `id` = " .. tmp:getDataInt("id")
        db.executeQuery(query)
        if(t[1] == "accept") then
            if doUpdateGuildWar(tmp:getDataInt("id")) then
                doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, 'Guild War started!')
            else
                doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, 'Sorry but you or your opponent doesnt\'t have enough cash on bank')
            return false
            end
         end
        tmp:free()    
        doBroadcastMessage("Guild ".. getPlayerGuildName(cid) .. " has " .. msg, MESSAGE_EVENT_ADVANCE)
        return false
    end
 
    if(t[1] == "invite") then
    local str = ""
    tmp = db.getResult("SELECT `guild_id`, `status` FROM `guild_wars` WHERE `guild_id` IN (" .. guild .. "," .. enemy .. ") AND `opponent_id` IN (" .. enemy .. "," .. guild .. ") AND `status` IN (0,1)")
        if(tmp:getID() ~= -1) then
            if(tmp:getDataInt("status") == 0) then
                if(tmp:getDataInt("guild_id") == guild) then
                    str = "You have already invited Guild " .. enemyName .. " to war."
                else
                    str = "Guild ".. enemyName .. " have already invited you to war."
                end
            else
                str = "You are already on a war with Guild " .. enemyName .. "."
            end
            tmp:free()
        end
 
        if(str ~= "") then
            doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, str)
            return false
        end
 
        local frags = tonumber(t[3])
        if(frags ~= nil) then
            frags = math.max(10, math.min(1000, frags))
        else
            frags = 100
        end
 
        local payment = tonumber(t[4])
        if(payment ~= nil) then
            payment = math.max(0, math.min(1000000000, payment))
        else
            payment = 0
        end
        local begining, ending = os.time(), tonumber(t[5])
        if(ending ~= nil and ending ~= 0 and ending <= 31) then
            ending = begining + (ending * 86400)
        else
            ending = begining + (7 * 86400)
        end
       
        db.executeQuery("INSERT INTO `guild_wars` (`guild_id`, `opponent_id`, `declaration_date`, `end_date`, `frag_limit`, `guild_fee`, `opponent_fee`, `status`) VALUES (" .. guild .. ", " .. enemy .. ", " .. begining .. ", " .. ending .. ", " .. frags .. ", " .. payment .. ", " .. payment .. ", -1);")
        doBroadcastMessage("Guild ".. getPlayerGuildName(cid) .. " has invited Guild " .. enemyName .. " to war until " .. frags .. " frags.", MESSAGE_EVENT_ADVANCE)
        return false
    end
 
    doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, "Currently there's no active war with " .. enemyName .. ".")
    return true
end