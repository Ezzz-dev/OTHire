-- othire mccheck
function onSay(cid, words, param)

	doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, "Multiclient Check List:")
	
	local ipList = {}
	local players = getPlayersOnlineList()

	for i = 1, #players do
		local tmpPlayer = players[i]
		local ip = getPlayerIp(tmpPlayer)
		if ip ~= 0 then
			local list = ipList[ip]
			if not list then
				ipList[ip] = {}
				list = ipList[ip]
			end
			list[#list + 1] = tmpPlayer
		end
	end

	for ip, list in pairs(ipList) do
		local listLength = #list
		if listLength > 1 then
			local tmpPlayer = list[1]
			local message = ("%s: %s [%d]"):format(convertIntToIP(ip), getPlayerName(tmpPlayer), getPlayerLevel(tmpPlayer))
			for i = 2, listLength do
				tmpPlayer = list[i]
				message = ("%s, %s [%d]"):format(message, getPlayerName(tmpPlayer), getPlayerLevel(tmpPlayer))
			end
			doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, message .. ".")
		end
	end
	return false
end