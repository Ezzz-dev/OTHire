function getContentDescription(uid, comma)
local ret, i, containers = '', 0, {}
while i < getContainerSize(uid) do
	local v, s = getContainerItem(uid, i), ''
	local k = getItemDescriptions(v.itemid)
if k.name ~= '' then
	if v.type > 1 and k.stackable and k.showCount then
		s = v.type .. ' ' .. getItemDescriptions(v.itemid).plural
		else
		local article = k.article
		s = (article == '' and '' or article .. ' ') .. k.name
	end
	ret = ret .. (i == 0 and not comma and '' or ', ') .. s
	if isContainer(v.uid) and getContainerSize(v.uid) > 0 then
		
	end
	else
		ret = ret .. (i == 0 and not comma and '' or ', ') .. 'an item of type ' .. v.itemid .. ', please report it to gamemaster'
	end
	i = i + 1
	end
	for i = 1, #containers do
		ret = ret .. getContentDescription(containers[i], true)
	end
return ret
end

local function send(cid, pos, name, party)
local corpse = getTileItemByType(pos, ITEM_TYPE_CONTAINER).uid
local ret = isContainer(corpse) and getContentDescription(corpse)
doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, 'Loot of ' .. name .. ': ' .. (ret ~= '' and ret or 'nothing'))
if party then
	for _, pid in ipairs(getPartyMembers(cid)) do
	--doPlayerSendChannelMessage(pid, '', 'Loot of ' .. name .. ': ' .. (ret ~= '' and ret or 'nothing'), TALKTYPE_CHANNEL_W, CHANNEL_PARTY)
  --not working in OTHire, Fix pending, using workaround below
  doPlayerSendTextMessage(pid, MESSAGE_INFO_DESCR, 'Loot of ' .. name .. ': ' .. (ret ~= '' and ret or 'nothing'))
end
end
end

function onKill(cid, target, lastHit)
if not isPlayer(target) then
	addEvent(send, 0, cid, getThingPos(target), getCreatureName(target), getPartyMembers(cid))
end
return true
end
