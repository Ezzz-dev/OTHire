-- This file is part of Jiddo's advanced NpcSystem v3.0x. This NpcSystem is free to use by anyone, for any purpose. 
-- Initial release date: 2007-02-21
-- Credits: Jiddo, honux(I'm using a modified version of his Find function).
-- Please include full credits where ever you use this system, or parts of it.
-- For support, questions and updates, please consult the following thread:
-- http://opentibia.net/topic/59592-release-advanced-npc-system-v30a/

if(NpcHandler == nil) then
	
	-- Constant talkDelay behaviors.
	TALKDELAY_NONE = 0 -- No talkDelay. NPC will reply immediately.
	TALKDELAY_ONTHINK = 1 -- talkDelay handled through the onThink callback function. (Default)
	TALKDELAY_EVENT = 2 -- Not yet implemented
	
	-- Currently applied talkDelay behavior. TALKDELAY_ONTHINK is default.
	NPCHANDLER_TALKDELAY = TALKDELAY_ONTHINK
	
	
	
	-- Constant indexes for defining default messages.
	MESSAGE_GREET 		= 1 -- When the player greets the NPC.
	MESSAGE_FAREWELL 	= 2 -- When the player unGreets the NPC.
	MESSAGE_BUY 		= 3 -- When the NPC asks the player if he wants to buy something.
	MESSAGE_SELL 		= 4 -- When the NPC asks the player if he wants to sell something.
	MESSAGE_ONBUY 		= 5 -- When the player successfully buys something
	MESSAGE_ONSELL 		= 6 -- When the player successfully sells something
	MESSAGE_NEEDMOREMONEY = 7 -- When the player does not have enough money
	MESSAGE_NOTHAVEITEM = 8 -- When the player is trying to sell an item he does not have.
	MESSAGE_IDLETIMEOUT = 9 -- When the player has been idle for longer then idleTime allows.
	MESSAGE_WALKAWAY 	= 10 -- When the player walks out of the talkRadius of the NPC.
	MESSAGE_ALREADYFOCUSED = 11 -- When the player already has the focus of this NPC.
	MESSAGE_PLACEDINQUEUE = 12 -- When the player has been placed in the costumer queue. 
	MESSAGE_DECLINE 	= 13 -- When the player says no to something.
	
	-- Constant indexes for callback functions. These are also used for module callback ids.
	CALLBACK_CREATURE_APPEAR 	= 1
	CALLBACK_CREATURE_DISAPPEAR = 2
	CALLBACK_CREATURE_SAY 		= 3
	CALLBACK_ONTHINK 			= 4
	CALLBACK_GREET 				= 5
	CALLBACK_FAREWELL 			= 6
	CALLBACK_MESSAGE_DEFAULT 	= 7
	
	-- Additional module callback IDs
	CALLBACK_MODULE_INIT		= 10
	CALLBACK_MODULE_RESET		= 11
	
	
	-- Constant strings defining the keywords to replace in the default messages.
	TAG_PLAYERNAME = '|PLAYERNAME|'
	TAG_ITEMCOUNT = '|ITEMCOUNT|'
	TAG_TOTALCOST = '|TOTALCOST|'
	TAG_ITEMNAME = '|ITEMNAME|'
	TAG_QUEUESIZE = '|QUEUESIZE|'
	TAG_TIME = '|TIME|'
	TAG_TRAVELCOST = '|TRAVELCOST|'
	
	
	NpcHandler = {
		keywordHandler = nil,
		queue = nil,
		focus = 0,
		talkStart = 0,
		idleTime = 30,
		talkRadius = 5,
		talkDelay = nil,
		talkEvent = nil,
		callbackFunctions = nil,
		modules = nil,
		messages = {
		
			-- These are the default replies of all NPC's. They can/should be changed individually for each NPC.
			[MESSAGE_GREET] 		= 'Welcome, |PLAYERNAME|! I have been expecting you.',
			[MESSAGE_FAREWELL] 		= 'Good bye, |PLAYERNAME|!',
			[MESSAGE_BUY] 			= 'Do you want to buy |ITEMCOUNT| |ITEMNAME| for |TOTALCOST| gold coins?',
			[MESSAGE_SELL] 			= 'Do you want to sell |ITEMCOUNT| |ITEMNAME| for |TOTALCOST| gold coins?',
			[MESSAGE_ONBUY] 		= 'It was a pleasure doing business with you.',
			[MESSAGE_ONSELL] 		= 'Thank you for this item, |PLAYERNAME|.',
			[MESSAGE_NEEDMOREMONEY] = 'You do not have enough money.',
			[MESSAGE_NOTHAVEITEM] 	= 'You don\'t even have that item!',
			[MESSAGE_IDLETIMEOUT] 	= 'Next please!',
			[MESSAGE_WALKAWAY] 		= 'How rude!',
			[MESSAGE_ALREADYFOCUSED]= '|PLAYERNAME|, I am already talking to you.',
			[MESSAGE_PLACEDINQUEUE] = '|PLAYERNAME|, please wait for your turn. There are |QUEUESIZE| customers before you.',
			[MESSAGE_DECLINE]		= 'Not good enough, is it?'
		}
	}
	
	
	-- Creates a new NpcHandler with an empty callbackFunction stack. 
	function NpcHandler:new(keywordHandler)
		local obj = {}
		obj.callbackFunctions = {}
		obj.modules = {}
		obj.talkDelay = {
				message = nil,
				time = nil
			}
        obj.talkEvent = {}
		obj.queue = Queue:new(obj)
		obj.keywordHandler = keywordHandler
		obj.messages = {}
		setmetatable(obj.messages, self.messages)
		self.messages.__index = self.messages
		
		setmetatable(obj, self)
		self.__index = self
		return obj
	end
	
	-- Re-defines the maximum idle time allowed for a player when talking to this npc.
	function NpcHandler:setMaxIdleTime(newTime)
		self.idleTime = newTime
	end
	
	-- Attaches a new costumer queue to this npcHandler.
	function NpcHandler:setQueue(newQueue)
		self.queue = newQueue
		self.queue:setHandler(self)
	end
	
	-- Attaches a new keyword handler to this npcHandler
	function NpcHandler:setKeywordHandler(newHandler)
		self.keywordHandler = newHandler
	end
	
	-- Function used to change the focus of this NPC. 
	function NpcHandler:changeFocus(newFocus)
		self.focus = newFocus
		self:updateFocus()
	end
	
	-- This function should be called on each onThink and makes sure the NPC faces the player it is talking to.
	--	Should also be called whenever a new player is focused.
	function NpcHandler:updateFocus()
		doNpcSetCreatureFocus(self.focus)
	end
	
	-- Used when the NPC should release focus from the player. 
	function NpcHandler:releaseFocus()
		if self.talkEvent[self.focus] then
			self:cancelStory(self.talkEvent[self.focus])
		end	
		return self:changeFocus(0)
	end
	
	-- Returns the callback function with the specified id or nil if no such callback function exists.
	function NpcHandler:getCallback(id)
		local ret = nil
		if(self.callbackFunctions ~= nil) then
			ret = self.callbackFunctions[id]
		end
		return ret
	end
	
	-- Changes the callback function for the given id to callback.
	function NpcHandler:setCallback(id, callback)
		if(self.callbackFunctions ~= nil) then
			self.callbackFunctions[id] = callback
		end
	end
	
	-- Adds a module to this npcHandler and initiates it.
	function NpcHandler:addModule(module)
		if(self.modules ~= nil) then
			table.insert(self.modules, module)
			module:init(self)
		end
	end
	
	-- Calls the callback function represented by id for all modules added to this npcHandler with the given arguments.
	function NpcHandler:processModuleCallback(id, ...)
		local ret = true
		for i, module in pairs(self.modules) do
			local tmpRet = true
			if(id == CALLBACK_CREATURE_APPEAR and module.callbackOnCreatureAppear ~= nil) then
				tmpRet = module:callbackCreatureAppear(unpack(arg))
				
			elseif(id == CALLBACK_CREATURE_DISAPPEAR and module.callbackOnCreatureDisappear ~= nil) then
				tmpRet = module:callbackCreatureDisappear(unpack(arg))
				
			elseif(id == CALLBACK_CREATURE_SAY and module.callbackOnCreatureSay ~= nil) then
				tmpRet = module:callbackCreatureSay(unpack(arg))
				
			elseif(id == CALLBACK_ONTHINK and module.callbackOnThink ~= nil) then
				tmpRet = module:callbackOnThink(unpack(arg))
				
			elseif(id == CALLBACK_GREET and module.callbackOnGreet ~= nil) then
				tmpRet = module:callbackOnGreet(unpack(arg))
				
			elseif(id == CALLBACK_FAREWELL and module.callbackOnFarewell ~= nil) then
				tmpRet = module:callbackOnFarewell(unpack(arg))
				
			elseif(id == CALLBACK_MESSAGE_DEFAULT and module.callbackOnMessageDefault ~= nil) then
				tmpRet = module:callbackOnMessageDefault(unpack(arg))
				
			elseif(id == CALLBACK_MODULE_RESET and module.callbackOnModuleReset ~= nil) then
				tmpRet = module:callbackOnModuleReset(unpack(arg))
			end
			if(not tmpRet) then
				ret = false
				break
			end
		end
		return ret
	end
	
	-- Returns the message represented by id.
	function NpcHandler:getMessage(id)
		local ret = nil
		if(self.messages ~= nil) then
			ret = self.messages[id]
		end
		return ret
	end
	
	-- Changes the default response message with the specified id to newMessage.
	function NpcHandler:setMessage(id, newMessage)
		if(self.messages ~= nil) then
			self.messages[id] = newMessage
		end
	end
	
	-- Translates all message tags found in MSG using parseInfo
	function NpcHandler:parseMessage(msg, parseInfo)
		local ret = msg
		for search, replace in pairs(parseInfo) do
			ret = string.gsub(ret, search, replace)
		end
		return ret
	end
	
	-- Makes sure the NPC unfocused the currently focused player, and greets the next player in the queue is it is not empty.
	function NpcHandler:unGreet()
		if(self.focus == 0) then
			return
		end
		local callback = self:getCallback(CALLBACK_FAREWELL)
		if(callback == nil or callback()) then
			if(self:processModuleCallback(CALLBACK_FAREWELL)) then
				if(self.queue == nil or not self.queue:greetNext()) then
					local msg = self:getMessage(MESSAGE_FAREWELL)
					local parseInfo = { [TAG_PLAYERNAME] = getPlayerName(self.focus) }
					msg = self:parseMessage(msg, parseInfo)
					self:say(msg)
					self:releaseFocus()
				end
			end
		end
	end
	
	-- Greets a new player. 
	function NpcHandler:greet(cid)
		if(cid ~= 0) then
			local callback = self:getCallback(CALLBACK_GREET)
			if(callback == nil or callback(cid)) then
				if(self:processModuleCallback(CALLBACK_GREET, cid)) then
					local msg = self:getMessage(MESSAGE_GREET)
					local parseInfo = { [TAG_PLAYERNAME] = getPlayerName(cid) }
					msg = self:parseMessage(msg, parseInfo)
					self:say(msg)
				else
					return
				end
			else
				return
			end
		end
		self:changeFocus(cid)
	end
	
	-- Handles onCreatureAppear events. If you with to handle this yourself, please use the CALLBACK_CREATURE_APPEAR callback.
	function NpcHandler:onCreatureAppear(cid)
		local callback = self:getCallback(CALLBACK_CREATURE_APPEAR)
		if(callback == nil or callback(cid)) then
			if(self:processModuleCallback(CALLBACK_CREATURE_APPEAR, cid)) then
				
			end
		end
	end
	
	-- Handles onCreatureDisappear events. If you with to handle this yourself, please use the CALLBACK_CREATURE_DISAPPEAR callback.
	function NpcHandler:onCreatureDisappear(cid)
		local callback = self:getCallback(CALLBACK_CREATURE_DISAPPEAR)
		if(callback == nil or callback(cid)) then
			if(self:processModuleCallback(CALLBACK_CREATURE_DISAPPEAR, cid)) then
				if(self.focus == cid) then
					self:unGreet()
				end
			end
		end
	end
	
	-- Handles onCreatureSay events. If you with to handle this yourself, please use the CALLBACK_CREATURE_SAY callback.
	function NpcHandler:onCreatureSay(cid, msgtype, msg)
		local callback = self:getCallback(CALLBACK_CREATURE_SAY)
		if(callback == nil or callback(cid, msgtype, msg)) then
			if(self:processModuleCallback(CALLBACK_CREATURE_SAY, cid, msgtype, msg)) then
				if(not self:isInRange(cid)) then
					return
				end
				if(self.keywordHandler ~= nil) then
					local ret = self.keywordHandler:processMessage(cid, msg)
					if(not ret) then
						local callback = self:getCallback(CALLBACK_MESSAGE_DEFAULT)
						if(callback ~= nil and callback(cid, msgtype, msg)) then
							self.talkStart = os.time()
						end
					else
						self.talkStart = os.time()
					end
				end
			end
		end
	end
	
	-- Handles onThink events. If you with to handle this yourself, please use the CALLBACK_ONTHINK callback.
	function NpcHandler:onThink()
		local callback = self:getCallback(CALLBACK_ONTHINK)
		if(callback == nil or callback()) then
			
			if(NPCHANDLER_TALKDELAY == TALKDELAY_ONTHINK and self.talkDelay.time ~= nil and self.talkDelay.message ~= nil and os.time() >= self.talkDelay.time) then
				selfSay(self.talkDelay.message)
				self.talkDelay.time = nil
				self.talkDelay.message = nil
			end
			
			if(self:processModuleCallback(CALLBACK_ONTHINK)) then
				if(self.focus ~= 0) then
					if(not self:isInRange(self.focus)) then
						self:onWalkAway(self.focus)
					elseif(os.time() - self.talkStart > self.idleTime) then
						if self.queue:greetNext() then 
							self.talkStart = self.talkStart + self.idleTime
						else
							self:unGreet()
						end
					else
						self:updateFocus()
					end
				end
			end
		end
	end
	
	-- Tries to greet the player with the given CID. This function does not override queue order, current focus etc.
	function NpcHandler:onGreet(cid)
		if(self:isInRange(cid)) then
			if(self.focus == 0) then
				self:greet(cid)
			elseif(cid == self.focus) then
				local msg = self:getMessage(MESSAGE_ALREADYFOCUSED)
				local parseInfo = { [TAG_PLAYERNAME] = getPlayerName(cid) }
				msg = self:parseMessage(msg, parseInfo)
				self:say(msg)
			else
				if(not self.queue:isInQueue(cid)) then
					self.queue:push(cid)
				end
				
				if self.talkEvent[self.focus] then
					return 
				end
				
				local msg = self:getMessage(MESSAGE_PLACEDINQUEUE)
				local parseInfo = { [TAG_PLAYERNAME] = getPlayerName(cid), [TAG_QUEUESIZE] = self.queue:getSize() }
				msg = self:parseMessage(msg, parseInfo)
				self:say(msg)
			end
		end
	end
	
	-- Simply calls the underlying unGreet function. 
	function NpcHandler:onFarewell()
		self:unGreet()
	end
	
	-- Should be called on this NPC's focus if the distance to focus is greater then talkRadius.
	function NpcHandler:onWalkAway(cid)
		if(cid == self.focus) then
			local callback = self:getCallback(CALLBACK_CREATURE_DISAPPEAR)
			if(callback == nil or callback()) then
				if(self:processModuleCallback(CALLBACK_CREATURE_DISAPPEAR, cid)) then
					if(self.queue == nil or not self.queue:greetNext()) then
						local msg = self:getMessage(MESSAGE_WALKAWAY)
						local parseInfo = { [TAG_PLAYERNAME] = getPlayerName(self.focus) }
						msg = self:parseMessage(msg, parseInfo)
						self:say(msg)
						self:releaseFocus()
					end
				end
			end
		end
	end
	
	-- Returns true if CID is within the talkRadius of this NPC.
	function NpcHandler:isInRange(cid)
		local playerPos = getPlayerPosition(cid)
		if playerPos == LUA_ERROR or playerPos == LUA_NO_ERROR then
			return false
		end
		
		local sx = selfGetPosition().x
		local sy = selfGetPosition().y
		local sz = selfGetPosition().z
		
		local dx = math.abs(sx-playerPos.x)
		local dy = math.abs(sy-playerPos.y)
		local dz = math.abs(sz-playerPos.z)
		
		local dist = (dx^2 + dy^2)^0.5
		
		return (dist <= self.talkRadius and dz == 0)
	end
	
	-- Resets the NPC into it's initial state (in regard of the keywordHandler). 
	--	All modules are also receiving a reset call through their callbackOnModuleReset function.
	function NpcHandler:resetNpc()
		if(self:processModuleCallback(CALLBACK_MODULE_RESET)) then
			self.keywordHandler:reset()
		end
	end
	
	function NpcHandler:cancelStory(event)
		if event ~= nil then
			stopEvent(event)
			event = nil
		end
	end
	
	function NpcHandler:story(messages, npc, delay)

		if doCreatureSay(npc, messages[1], 1) then
			table.remove(messages, 1)
		end
		
		if messages[1] then
			self.talkEvent[self.focus] = addEvent(
				function(messages, npc, delay)
					return self:story(messages, npc, delay)
				end, delay, messages, npc, delay
			)
			self.talkDelay.time = os.time() + math.max(0, delay / 1000)
			return 
		end	
		self.talkEvent[self.focus] = nil
	end
		
	-- Makes the NPC represented by this instance of NpcHandler say something. 
	--	This implements the currently set type of talkDelay.
	function NpcHandler:say(message, delay)
	
		if self.talkEvent[self.focus] then
			self:cancelStory(self.talkEvent[self.focus])
		end
		
		if not tonumber(delay) or delay < 0 then
			delay = 1000
		end
									
		if type(message) == 'table' then			
			self.talkEvent[self.focus] = addEvent(
				function(message, npc, delay)
					return self:story(message, npc, delay)
				end, 1000, message, getNpcCid(), delay		
			)
		else		
			self.talkDelay.message = message
		end
		
		self.talkDelay.time = os.time() + math.max(0, delay / 1000)
	end
end