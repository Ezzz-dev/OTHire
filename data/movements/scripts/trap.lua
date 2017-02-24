local traps = {
	[1510] = {transformTo = 1511, damage = {-50, -100}},
	[1513] = {damage = {-50, -100}},
	[2579] = {transformTo = 2578, damage = {-15, -30}},
	[4208] = {transformTo = 4209, damage = {-15, -30}, type = COMBAT_EARTHDAMAGE}
}

function onStepIn(cid, item, position, fromPosition)
	local trap = traps[item.itemid]
	if trap == nil then
		return true
	end

	if not getTilePzInfo(position) then
		doTargetCombatHealth(0, cid, trap.type or COMBAT_PHYSICALDAMAGE, trap.damage[1], trap.damage[2], CONST_ME_NONE)
	end
	if trap.transformTo ~= nil then
		doTransformItem(item.uid, trap.transformTo)
		if item.actionid ~= 0 then
			doSetItemActionId(item.uid, item.actionid)
		end
	end

	return true
end

function onStepOut(cid, item, pos)
	doTransformItem(item.uid, item.itemid - 1)
	if item.actionid ~= 0 then
		doSetItemActionId(item.uid, item.actionid)
	end
	return true
end

function onRemoveItem(item, tile, pos)
	if (getDistanceBetween(getThingPos(item.uid), pos) > 0) then
		doTransformItem(item.uid, item.itemid - 1)
		if item.actionid ~= 0 then
			doSetItemActionId(item.uid, item.actionid)
		end
		doSendMagicEffect(getThingPos(item.uid), CONST_ME_POFF)
	end
	return true
end

function onAddItem(item, tileitem, pos)
	doTransformItem(tileitem.uid, tileitem.itemid - 1)
	if item.actionid ~= 0 then
		doSetItemActionId(item.uid, item.actionid)
	end
	doSendMagicEffect(pos, CONST_ME_POFF)
	return true
end
