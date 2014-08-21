function onUse(cid, item, fromPosition, itemEx, toPosition)
	if itemEx.itemid == 2147 and item.itemid == 2342 then
		doTransformItem(item.uid, 2343)
		doDecayItem(item.uid)
		doRemoveItem(itemEx.uid,1)
		doSendMagicEffect(toPosition, CONST_ME_MAGIC_RED)
		return true
	end
	return false
end