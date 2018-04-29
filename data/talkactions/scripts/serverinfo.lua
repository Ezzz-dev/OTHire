-- You should multiply rate values by player rate if you use stages
local config = {
	rate_exp = getConfigValue('rate_exp'),
	rate_skill = getConfigValue('rate_skill'),
	rate_loot = getConfigValue('rate_loot'),
	rate_mag = getConfigValue('rate_mag'),
	rent_period = getConfigValue('houserentperiod'),
	protection_level = getConfigValue('min_pvp_level')
}

local function getWorldTypeName()
	local worldType = getWorldType()
	if worldType == WORLD_TYPE_NO_PVP then
		return "Non-PvP"
	elseif worldType == WORLD_TYPE_PVP then
		return "PvP"
	elseif worldType == WORLD_TYPE_PVP_ENFORCED  then
		return "PvP-Enforced"
	else
		return "null"
	end	
end

function onSay(cid, words, param)
	-- Add basic info
	local str = "Server Information:\n\n"
	str = str .. "World Type: " .. getWorldTypeName() .. "\n"
	str = str .. "Experience Rate: " .. config.rate_exp .. "x\n"
	str = str .. "Magic Rate: " .. config.rate_mag .. "x\n"
	str = str .. "Skill Rate: " .. config.rate_skill .. "x\n"
	str = str .. "Loot Rate: " .. config.rate_loot .. "x\n"
	str = str .. "Protection Level: " .. config.protection_level .. "\n"
	
	-- Add house renting info
	if(config.rent_period ~= "never") then
		str = str .. "\nHouse Rent: House rents are paid " .. config.rent_period .. "."
	end

	doPlayerSendTextMessage(cid, MESSAGE_STATUS_CONSOLE_BLUE, str)
	return false
end