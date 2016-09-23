local settings = {
	inFightBlock = true,
	onlyLeader = true
}

function onSay(cid, words, param)
	local members = getPartyMembers(cid)
	if not members then
		doPlayerSendCancel(cid, "You are not in a party.")
		return false
	end

	if settings.onlyLeader and not isPartyLeader(cid) then
		doPlayerSendCancel(cid, "Only party leader can enable or disable shared experience.")
		return false
	end

	if settings.inFightBlock and hasCondition(cid, CONDITION_INFIGHT) then
		doPlayerSendCancel(cid, "You need to be out of fight.")
		return false
	end

	local boolean = not isPartySharedExperienceActive(cid)
	if setPartySharedExperience(cid, boolean) then
		for _, pid in ipairs(members) do
			if not isPartyLeader(pid) then
				doPlayerSendTextMessage(pid, MESSAGE_EVENT_ADVANCE, "Shared Experience has been "..(boolean and "activated" or "deactivated")..".")
			end
		end
	else
		doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, "There was an error. Try again in few seconds.")
	end

	return false
end
