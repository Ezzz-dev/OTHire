local keywordHandler = KeywordHandler:new()
local npcHandler = NpcHandler:new(keywordHandler)
NpcSystem.parseParameters(npcHandler)

-- OTServ event handling functions start
function onCreatureAppear(cid)				npcHandler:onCreatureAppear(cid) end
function onCreatureDisappear(cid) 			npcHandler:onCreatureDisappear(cid) end
function onCreatureSay(cid, type, msg) 	npcHandler:onCreatureSay(cid, type, msg) end
function onThink() npcHandler:onThink() end

npcHandler:setMessage(MESSAGE_FAREWELL, "Bye, bye.")

function onPlayerGreet(cid)
	npcHandler:setMessage(MESSAGE_GREET, "Hey |PLAYERNAME|. What do you need?")
	return true
end

npcHandler:setCallback(CALLBACK_GREET, onPlayerGreet)
npcHandler:setMessage(MESSAGE_PLACEDINQUEUE, "Hey, |PLAYERNAME|. Would ya mind not interruptin'? Thanks.")
npcHandler:setMessage(MESSAGE_WALKAWAY, "Yeah, sod off...")

function creatureSayCallback(cid, type, msg)
	if(npcHandler.focus ~= cid) then
		return false
	end

	if msgcontains(msg, "job") then
		npcHandler:say("No job really.")
		return true
	end
	if msgcontains(msg, "name") then
		npcHandler:say("I'm a test NPC.")
		return true
	end
	
    return true
end

npcHandler:setCallback(CALLBACK_MESSAGE_DEFAULT, creatureSayCallback)

local focus = FocusModule:new()
npcHandler:addModule(focus)