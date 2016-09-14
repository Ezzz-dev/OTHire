local travelDiscounts = {
	['postman'] = {price = 10, storage = STORAGE_POSTMAN_BOAT, value = 1}
}

function StdModule.travelDiscount(cid, discounts)
	local discountPrice, discount = 0
	if type(discounts) == 'string' then
		discount = travelDiscounts[discounts]
		if discount and getPlayerStorageValue(cid, discount.storage) >= discount.value then
			return discount.price
		end
	else
		for i = 1, #discounts do
			discount = travelDiscounts[discounts[i]]
			if discount and getPlayerStorageValue(cid, discount.storage) >= discount.value then
				discountPrice = discountPrice + discount.price
			end
		end
	end

	return discountPrice
end