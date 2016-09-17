function onLook(cid, thing, itemId)

  local settings = {
    anonymous = false,
    hiddenAccess = 2
  }

  if(isPlayer(thing.uid) and cid ~= thing.uid and getPlayerAccess(cid) < settings.hiddenAccess) then
    doPlayerSendTextMessage(thing.uid, MESSAGE_EVENT_DEFAULT, (settings.anonymous and "Someone" or getPlayerName(cid)) .. " is looking at you.")
  end

  return true
end
