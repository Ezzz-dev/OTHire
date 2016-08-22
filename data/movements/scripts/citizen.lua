function onStepIn(cid, item, position, fromPosition)
	if(item.actionid > 30020 and item.actionid < 30100) then
		local townId = (item.actionid - 30020)
		local townPos = getTownTemplePosition(townId)
		doPlayerSetTown(cid, townId)
		doTeleportThing(cid, townPos)
		doSendMagicEffect(townPos, CONST_ME_TELEPORT)
		doSetCreatureDirection(cid, SOUTH)
		doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, "You are now citizen of " .. getTownNameById(townId) .. ".")
	end

	return true
end