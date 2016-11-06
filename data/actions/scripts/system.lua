local specialQuests = {
	[2001] = 30015 --Annihilator
}

local questsExperience = {
	[30015] = 10000
}

function onUse(cid, item, fromPosition, itemEx, toPosition)
	local storage = specialQuests[item.uid]
	if(not storage) then
		storage = item.uid
		if(storage > 65535) then
			return false
		end
	end

	if(getPlayerStorageValue(cid, storage) > 0) then
		doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, "It is empty.")
		return true
	end

	local items = {}
	local reward = 0

	local size = isContainer(item.uid) and getContainerSize(item.uid) or 0
	if(size == 0) then
		reward = doCopyItem(item, false)
	else
		for i = 0, size do
			local tmp = getContainerItem(item.uid, i)
			if(tmp.itemid > 0) then
				table.insert(items, tmp)
			end
		end
	end

	size = table.maxn(items)
	if(size == 1) then
		reward = doCopyItem(items[1], true)
	end

	local result = ""
	if reward ~= 0 then
		local ret = getItemDescriptions(reward.itemid)
		if (reward.type > 0 and isItemRune(reward.itemid)) then
			result = reward.type .. " charges " .. ret.name
		elseif(reward.type > 1 and isItemStackable(reward.itemid)) then
			result = reward.type .. " " .. ret.plural
		else
			result = ret.article .. " " .. ret.name
		end
	else
		if(size > 20) then
			reward = doCopyItem(item, false)
		elseif(size > 8) then
			reward = getThing(doCreateItemEx(1988, 1))
			result = "a backpack"
		else
			reward = getThing(doCreateItemEx(1987, 1))
			result = "a bag"
		end

		for i = 1, size do
			local tmp = doCopyItem(items[i], true)
			if(doAddContainerItemEx(reward.uid, tmp.uid) ~= RETURNVALUE_NOERROR) then
				print("[Warning] QuestSystem:", "Could not add quest reward")
			end
		end
	end

	if(doPlayerAddItemEx(cid, reward.uid, false) ~= RETURNVALUE_NOERROR) then
		if getPlayerFreeCap(cid) < getItemWeight(reward.uid) then
			result = "You have found a reward weighing " .. string.format("%.2f",getItemWeight(reward.uid)) .. " oz. It is too heavy."
		else
			result = "You have not enough space."
		end
	else
		result = "You have found " .. result .. "."
		setPlayerStorageValue(cid, storage, 1)
		if(questsExperience[storage] ~= nil) then
			doPlayerAddExp(cid, questsExperience[storage])
			doSendAnimatedText(getCreaturePosition(cid), questsExperience[storage], TEXTCOLOR_WHITE)
		end
	end

	doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, result)
	return true
end