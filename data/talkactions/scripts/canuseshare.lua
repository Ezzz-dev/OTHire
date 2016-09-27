function onSay(cid, words, param)

	local members = getPartyMembers(cid)
	if not members then
		doPlayerSendCancel(cid, "You are not in a party.")
		return false
	end

	local boolean = not canUseSharedExperience(cid)
	doPlayerSendTextMessage(cid, MESSAGE_INFO_DESCR, "You are " .. (boolean and "not" or "") .. " meeting the requirements to be benefited by party shared experience.")

	return false
end
