-- Include external classes.
dofile(getDataDir() .. 'npc/scripts/lib/npcsystem/npcsystem.lua')

dofile(getDataDir() .. 'npc/scripts/lib/custom modules.lua')

-- Callback for isPremium(cid) so Jiddo's npcsystem works
function isPlayerPremiumCallback(cid)
	if EVERYONE_PREMIUM_FOR_NPCS then
		return true
	else
		return isPremium(cid)
	end
end

-- get the distance to a creature
-- deprecated function
function getDistanceToCreature(id)
	debugPrint('getDistanceToCreature(): deprecated function. Use getDistanceTo()')
	return getDistanceTo(id)	
end

-- move to a creature
function moveToCreature(id)
	if(isCreature(id) == false) then
		debugPrint('moveToCreature(): creature not found.')
		return false
	end

	local pos = getCreaturePosition(id)
	selfMoveTo(pos.x, pos.y, pos.z)
	return true
end

function selfGotoIdle()
	debugPrint('selfGotoIdle(): deprecated function. Do not use it anymore!')
	return nil
end

function msgcontains(message, keyword)
	message = message:lower()
	local a, b = string.find(message, keyword)
	if a == nil or b == nil then
		return false
	end
	return true
end

function doCreatureSayWithDelay(cid,text,type,delay,e)
	if delay<=0 then
		doCreatureSay(cid,text,type)
	else
		local func=function(pars)
			doCreatureSay(pars.cid,pars.text,pars.type)
			pars.e.done=true
		end
		e.done=false
		e.event=addEvent(func,delay,{cid=cid, text=text, type=type, e=e})
	end
end
 
--returns how many msgs he have said already
function cancelNPCTalk(events)
	local ret = 1
	for aux=1,table.getn(events) do
		if events[aux].done==false then
			stopEvent(events[aux].event)
		else
			ret=ret+1
		end
	end
	events = nil
	return (ret)
end
 
function doNPCTalkALot(msgs,interval)
	local e={}
	local ret={}
	if interval==nil then 
		interval=3000 --3 seconds is default time between messages
	end 
	for aux=1,table.getn(msgs) do
		e[aux]={}
		doCreatureSayWithDelay(getNpcCid(),msgs[aux],TALKTYPE_SAY,(aux-1)*interval,e[aux])
		table.insert(ret,e[aux])
	end
	return(ret)
end

function getMoneyCount(string)
	local b, e = string:find("%d+")
	local money = b and e and tonumber(string:sub(b, e)) or -1
	if isValidMoney(money) then
		return money
	end
	return -1
end

function isValidMoney(money)
	return isNumber(money) and money > 0 and money < 4294967296
end

function getVocationByPlayerGUID(guid)
	local vocationQuery = db.storeQuery("SELECT `vocation` FROM `players` WHERE `id` = " .. guid .. ";")
	if not vocationQuery then
		return false
	end
	local vocation = result.getDataInt(vocationQuery, "vocation")
	result.free(vocationQuery)
	return tonumber(vocation) or vocation
end

function sellPlayerEmptyVials(cid)
    local emptyvialmoney = 0
    while doPlayerRemoveItem(cid, 2006, 1, 0) == true do
        emptyvialmoney = 5 + emptyvialmoney
    end
    if emptyvialmoney > 0 then
        doPlayerAddMoney(cid, emptyvialmoney)
        return true
    else
        return false
    end
end