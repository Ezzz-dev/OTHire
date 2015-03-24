local function serverSave()
    doSaveServer(true)
end

local function secondSaveWarning()
    broadcastMessage("Server is saving game in one minute. Please mind it may freeze!", MESSAGE_STATUS_WARNING)
    addEvent(serverSave, 60000)
end

local function firstSaveWarning()
    broadcastMessage("Server is saving game in 3 minutes. Please mind it may freeze!", MESSAGE_STATUS_WARNING)
    addEvent(secondSaveWarning, 120000)
end

--function onThink(interval) <-- en caso de querer salvar por intervalos mas cortos
function onTime()
    broadcastMessage("Server is saving game in 5 minutes. Please mind it may freeze!", MESSAGE_STATUS_WARNING)
    addEvent(firstSaveWarning, 120000)
    return true
end
