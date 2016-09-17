-- by JDB @otland.net
-- http://otland.net/threads/first-items.40857/

function onLogin(cid)
	local lastLogin = getPlayerLastLogin(cid)
	local config = {
		voc_items = {
			{ -- SORC
				{2190} -- wand of vortex
			},
			{ -- DRUID
				{2182} -- snakebite rod
			},
			{ -- PALADIN
				{2389, 10} -- spear
			},
			{ -- KNIGHT
				{2398} -- mace
			}
		},
		all_items = {
			{2480}, -- legion helmet
			{2484}, -- studded armor
			{2530}, -- cooper shield
			{2649}, -- leather legs
			{2643} -- leather boots
		},
		extra_items = {
			{2667, 2}, -- fish
			{2050}, -- torch
			{2412} -- katana
		},
		knight_weapons = {
			{2388} -- hatchet
		}
	}
	if not (lastLogin ~= 0) then
		if getPlayerVocation(cid) < 11 and getPlayerVocation(cid) > 0 then
			local common = config.voc_items[getPlayerVocation(cid)]
			if common ~= nil then
				for _, v in ipairs(common) do
					doPlayerAddItem(cid, v[1], v[2] or 1)
				end
			end
			
			local all = config.all_items
			if all ~= nil then
				for _, v in ipairs(all) do
					doPlayerAddItem(cid, v[1], v[2] or 1)
				end
			end
			
			local extra = config.extra_items
			local bp = doPlayerAddItem(cid, 1987, 1) -- container
			if extra ~= nil then
				for _, v in ipairs(extra) do
					doAddContainerItem(bp, v[1], v[2] or 1)
				end
			end
			
			local weapons = config.knight_weapons
			if weapons ~= nil then
				for _, w in ipairs(weapons) do
					if getPlayerVocation(cid) == 4 then
						doAddContainerItem(bp, w[1], w[2] or 1)
					end
				end
			end
			
		elseif getPlayerVocation(cid) == 0 then 
			if (getPlayerSex(cid) == 0) then
				doPlayerAddItem(cid, 2651, 1)
			else
				doPlayerAddItem(cid, 2650, 1) -- jacket
			end
			local club = doCreateItemEx(2382, 1) -- club
			doPlayerAddItemEx(cid, club, true, CONST_SLOT_LEFT)
			local bp = doPlayerAddItem(cid, 1987, 1) -- bag
			doAddContainerItem(bp, 2050, 1) -- torch
			doAddContainerItem(bp, 2674, 2) -- apples
		end
		
	end
	return true
end