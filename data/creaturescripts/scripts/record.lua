function onLogin(cid)
	local PlayersRecordStorage = 14301
	local i = 1
	
	for _, pid in ipairs(getPlayersOnlineList()) do
		i = i + 1
	end
	local current = i - 1
	
	if getGlobalStorageValue(PlayersRecordStorage) == nil then
		setGlobalStorageValue(PlayersRecordStorage, 0)
	end	
	
	oldRecord = getGlobalStorageValue(PlayersRecordStorage)
	
	if current > oldRecord then
		addEvent(doBroadcastMessage, 150, "New record: " .. current .. " players are logged in.", MESSAGE_STATUS_DEFAULT)
		setGlobalStorageValue(PlayersRecordStorage, current)
	end
	
	return true
end
