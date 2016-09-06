//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
// Lua script interface
//////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//////////////////////////////////////////////////////////////////////
#include "otpch.h"

#include "luascript.h"
#include "player.h"
#include "ioaccount.h"
#include "party.h"
#include "item.h"
#include "game.h"
#include "house.h"
#include "housetile.h"
#include "status.h"
#include "combat.h"
#include "spells.h"
#include "condition.h"
#include "monsters.h"
#include "baseevents.h"
#include "town.h"
#include "ioplayer.h"
#include "configmanager.h"
#include "teleport.h"
#include "ban.h"
#include "movement.h"
#include "tools.h"
#include "guild.h"
#include <string>
#include <iostream>
#include <sstream>

extern Game g_game;
extern Monsters g_monsters;
extern BanManager g_bans;
extern ConfigManager g_config;
extern MoveEvents* g_moveEvents;
extern Spells* g_spells;
extern Guilds g_guilds;

enum{
	EVENT_ID_LOADING = 1,
	EVENT_ID_USER = 1000
};

ScriptEnviroment::ThingMap ScriptEnviroment::m_globalMap;
ScriptEnviroment::AreaMap ScriptEnviroment::m_areaMap;
uint32_t ScriptEnviroment::m_lastAreaId = 0;
ScriptEnviroment::CombatMap ScriptEnviroment::m_combatMap;
uint32_t ScriptEnviroment::m_lastCombatId = 0;
ScriptEnviroment::ConditionMap ScriptEnviroment::m_conditionMap;
uint32_t ScriptEnviroment::m_lastConditionId = 0;
ScriptEnviroment::DBResultMap ScriptEnviroment::m_tempResults;
uint32_t ScriptEnviroment::m_lastResultId = 0;

ScriptEnviroment::StorageMap ScriptEnviroment::m_globalStorageMap;
ScriptEnviroment::TempItemListMap ScriptEnviroment::m_tempItems;

ScriptEnviroment::ScriptEnviroment()
: m_curNpc(NULL), m_curSpell(NULL)
{
	resetEnv();
	m_lastUID = 70000;
}

ScriptEnviroment::~ScriptEnviroment()
{
	resetEnv();

	for(CombatMap::iterator it = m_combatMap.begin(); it != m_combatMap.end(); ++it){
		delete it->second;
	}

	m_combatMap.clear();

	for(AreaMap::iterator it = m_areaMap.begin(); it != m_areaMap.end(); ++it){
		delete it->second;
	}

	m_areaMap.clear();
}

void ScriptEnviroment::resetEnv()
{
	m_scriptId = 0;
	m_callbackId = 0;
	m_timerEvent = false;
	m_interface = NULL;
	m_localMap.clear();

	for(TempItemListMap::iterator mit = m_tempItems.begin(); mit != m_tempItems.end(); ++mit){
		ItemList& itemList = mit->second;
		for(ItemList::iterator it = itemList.begin(); it != itemList.end(); ++it){
			if((*it)->getParent() == VirtualCylinder::virtualCylinder){
				g_game.FreeThing(*it);
			}
		}
	}
	m_tempItems.clear();
	
	if(!m_tempResults.empty())
	{
		Database* db = Database::instance();
		for(DBResultMap::iterator it = m_tempResults.begin(); it != m_tempResults.end(); ++it)
		{
			if(it->second)
				db->freeResult(it->second);
		}
	}
	
	m_tempResults.clear();

	m_realPos.x = 0;
	m_realPos.y = 0;
	m_realPos.z = 0;
	m_curNpc = NULL;
	m_curSpell = NULL;
}

bool ScriptEnviroment::saveGameState()
{
	Database* db = Database::instance();
	DBQuery query;

	if(!db->executeQuery("DELETE FROM `global_storage`")){
		return false;
	}

	DBInsert stmt(db);
	stmt.setQuery("INSERT INTO `global_storage` (`key`, `value`) VALUES ");

	for(StorageMap::const_iterator it = m_globalStorageMap.begin(); it != m_globalStorageMap.end(); ++it){
		query << it->first << ", " << it->second;

		if(!stmt.addRow(query)){
			return false;
		}
	}

	if(!stmt.execute()){
		return false;
	}

	return true;
}

bool ScriptEnviroment::loadGameState()
{
	Database* db = Database::instance();
	DBResult* result;
	DBQuery query;

	if(!(result = db->storeQuery("SELECT COUNT(*) AS `count` FROM `global_storage`"))){
		return false;
	}

	uint32_t count = result->getDataInt("count");
	db->freeResult(result);
	if(count == 0){
		return true;
	}

	if(!(result = db->storeQuery("SELECT * FROM `global_storage`"))){
		return false;
	}

	do{
		int32_t key = result->getDataInt("key");
		int32_t value = result->getDataInt("value");

		m_globalStorageMap[key] = value;
	}while(result->next());

	db->freeResult(result);
	return true;
}

bool ScriptEnviroment::setCallbackId(int32_t callbackId, LuaScriptInterface* scriptInterface)
{
	if(m_callbackId == 0){
		m_callbackId = callbackId;
		m_interface = scriptInterface;
		return true;
	}
	else{
		//nested callbacks are not allowed
		if(m_interface){
			m_interface->reportError(__FUNCTION__, "Nested callbacks!");
		}
		return false;
	}
}

void ScriptEnviroment::getEventInfo(int32_t& scriptId, std::string& desc, LuaScriptInterface*& scriptInterface, int32_t& callbackId, bool& timerEvent)
{
	scriptId = m_scriptId;
	desc = m_eventdesc;
	scriptInterface = m_interface;
	callbackId = m_callbackId;
	timerEvent = m_timerEvent;
}

void ScriptEnviroment::addUniqueThing(Thing* thing)
{
	Item* item = thing->getItem();
	if(item && item->getUniqueId() != 0 ){
		int32_t uid = item->getUniqueId();

		ThingMap::iterator it = m_globalMap.find(uid);
		if(it == m_globalMap.end()){
			m_globalMap[uid] = thing;
		}
		else{
			std::cout << "Duplicate uniqueId " <<  uid << std::endl;
		}
	}
}

void ScriptEnviroment::removeUniqueThing(Thing* thing)
{
	Item* item = thing->getItem();
	if(item && item->getUniqueId() != 0 ){
		int32_t uid = item->getUniqueId();
		ThingMap::iterator it = m_globalMap.find(uid);
		if(it != m_globalMap.end()){
			m_globalMap.erase(it);
		}
	}
}

uint32_t ScriptEnviroment::addThing(Thing* thing)
{
	if(thing && !thing->isRemoved()){
		ThingMap::iterator it;
		for(it = m_localMap.begin(); it != m_localMap.end(); ++it){
			if(it->second == thing){
				return it->first;
			}
		}

		uint32_t newUid;
		if(Creature* creature = thing->getCreature()){
			newUid = creature->getID();
		}
		else{
			if(Item* item = thing->getItem()){
				uint32_t uid = item->getUniqueId();
				if(uid && item->getTile() == item->getParent()){
					m_localMap[uid] = thing;
					return uid;
				}
			}

			++m_lastUID;
			if(m_lastUID > 0xFFFFFF)
				m_lastUID = 70000;

			while(m_localMap[m_lastUID]){
				++m_lastUID;
			}
			newUid = m_lastUID;
		}

		m_localMap[newUid] = thing;
		return newUid;
	}
	else{
		return 0;
	}
}

void ScriptEnviroment::insertThing(uint32_t uid, Thing* thing)
{
	ThingMap::iterator it = m_localMap.find(uid);
	if(it == m_localMap.end()){
		m_localMap[uid] = thing;
	}
	else{
		std::cout << std::endl << "Lua Script Error: Thing uid already taken.";
	}
}

Thing* ScriptEnviroment::getThingByUID(uint32_t uid)
{
	ThingMap::iterator it = m_localMap.find(uid);
	if(it != m_localMap.end() && !it->second->isRemoved()){
		return it->second;
	}
	
	it = m_globalMap.find(uid);
	if(it != m_globalMap.end() && !it->second->isRemoved()){
		return it->second;
	}
	
	if(uid >= PLAYER_ID_RANGE){ //is a creature id
		Thing* thing = g_game.getCreatureByID(uid);
		if(thing && !thing->isRemoved()){
			m_localMap[uid] = thing;
			return thing;
		}
	}
	
	return NULL;
}

Item* ScriptEnviroment::getItemByUID(uint32_t uid)
{
	Thing* tmp = getThingByUID(uid);
	if(tmp){
		if(Item* item = tmp->getItem()){
			return item;
		}
	}
	
	return NULL;
}

Container* ScriptEnviroment::getContainerByUID(uint32_t uid)
{
	Item* tmp = getItemByUID(uid);
	if(tmp){
		if(Container* container = tmp->getContainer())
			return container;
	}
	return NULL;
}

Creature* ScriptEnviroment::getCreatureByUID(uint32_t uid)
{
	Thing* tmp = getThingByUID(uid);
	if(tmp){
		if(Creature* creature = tmp->getCreature())
			return creature;
	}
	return NULL;
}

Player* ScriptEnviroment::getPlayerByUID(uint32_t uid)
{
	Thing* tmp = getThingByUID(uid);
	if(tmp){
		if(Creature* creature = tmp->getCreature())
			if(Player* player = creature->getPlayer())
				return player;
	}
	return NULL;
}

void ScriptEnviroment::removeItemByUID(uint32_t uid)
{
	ThingMap::iterator it;
	it = m_localMap.find(uid);
	if(it != m_localMap.end()){
		m_localMap.erase(it);
	}

	it = m_globalMap.find(uid);
	if(it != m_globalMap.end()){
		m_globalMap.erase(it);
	}
}

uint32_t ScriptEnviroment::addCombatArea(AreaCombat* area)
{
	uint32_t newAreaId = m_lastAreaId + 1;
	m_areaMap[newAreaId] = area;

	m_lastAreaId++;
	return newAreaId;
}

AreaCombat* ScriptEnviroment::getCombatArea(uint32_t areaId)
{
	AreaMap::const_iterator it = m_areaMap.find(areaId);
	if(it != m_areaMap.end()){
		return it->second;
	}

	return NULL;
}

uint32_t ScriptEnviroment::addCombatObject(Combat* combat)
{
	uint32_t newCombatId = m_lastCombatId + 1;
	m_combatMap[newCombatId] = combat;

	m_lastCombatId++;
	return newCombatId;
}


Combat* ScriptEnviroment::getCombatObject(uint32_t combatId)
{
	CombatMap::iterator it = m_combatMap.find(combatId);
	if(it != m_combatMap.end()){
		return it->second;
	}

	return NULL;
}

uint32_t ScriptEnviroment::addConditionObject(Condition* condition)
{
	uint32_t newConditionId = m_lastConditionId + 1;
	m_conditionMap[newConditionId] = condition;

	m_lastConditionId++;
	return m_lastConditionId;
}

Condition* ScriptEnviroment::getConditionObject(uint32_t conditionId)
{
	ConditionMap::iterator it = m_conditionMap.find(conditionId);
	if(it != m_conditionMap.end()){
		return it->second;
	}

	return NULL;
}

void ScriptEnviroment::addTempItem(ScriptEnviroment* env, Item* item)
{
	m_tempItems[env].push_back(item);
}

void ScriptEnviroment::removeTempItem(ScriptEnviroment* env, Item* item)
{
	ItemList& itemList = m_tempItems[env];
	ItemList::iterator it = std::find(itemList.begin(), itemList.end(), item);
	if(it != itemList.end()){
		itemList.erase(it);
	}
}

void ScriptEnviroment::removeTempItem(Item* item)
{
	for(TempItemListMap::iterator mit = m_tempItems.begin(); mit != m_tempItems.end(); ++mit){
		ItemList& itemList = mit->second;
		ItemList::iterator it = std::find(itemList.begin(), itemList.end(), item);
		if(it != itemList.end()){
			itemList.erase(it);
			break;
		}
	}
}

uint32_t ScriptEnviroment::addResult(DBResult* res)
{
	uint32_t newResultId = m_lastResultId + 1;
	m_tempResults[newResultId] = res;

	m_lastResultId++;
	return m_lastResultId;
}

bool ScriptEnviroment::removeResult(uint32_t id)
{
	DBResultMap::iterator it = m_tempResults.find(id);
	if(it == m_tempResults.end())
		return false;

	if(it->second)
		Database::instance()->freeResult(it->second);

	m_tempResults.erase(it);
	return true;
}

DBResult* ScriptEnviroment::getResultByID(uint32_t id)
{
	DBResultMap::iterator it = m_tempResults.find(id);
	if(it != m_tempResults.end())
		return it->second;

	return NULL;
}

void ScriptEnviroment::addGlobalStorageValue(const uint32_t key, const int32_t value)
{
	m_globalStorageMap[key] = value;
}

bool ScriptEnviroment::getGlobalStorageValue(const uint32_t key, int32_t& value) const
{
	StorageMap::const_iterator it;
	it = m_globalStorageMap.find(key);
	if(it != m_globalStorageMap.end()){
		value = it->second;
		return true;
	}
	else{
		value = 0;
		return false;
	}
}

bool ScriptEnviroment::eraseGlobalStorageValue(const uint32_t key)
{
	return (m_globalStorageMap.erase(key) > 0);
}


std::string LuaScriptInterface::getErrorDesc(ErrorCode_t code){
	switch(code){
	case LUA_ERROR_PLAYER_NOT_FOUND:
		return "Player not found";
		break;
	case LUA_ERROR_CREATURE_NOT_FOUND:
		return "Creature not found";
		break;
	case LUA_ERROR_ITEM_NOT_FOUND:
		return "Item not found";
		break;
	case LUA_ERROR_THING_NOT_FOUND:
		return "Thing not found";
		break;
	case LUA_ERROR_TILE_NOT_FOUND:
		return "Tile not found";
		break;
	case LUA_ERROR_HOUSE_NOT_FOUND:
		return "House not found";
		break;
	case LUA_ERROR_COMBAT_NOT_FOUND:
		return "Combat not found";
		break;
	case LUA_ERROR_CONDITION_NOT_FOUND:
		return "Condition not found";
		break;
	case LUA_ERROR_AREA_NOT_FOUND:
		return "Area not found";
		break;
	case LUA_ERROR_CONTAINER_NOT_FOUND:
		return "Container not found";
		break;
	case LUA_ERROR_VARIANT_NOT_FOUND:
		return "Variant not found";
	case LUA_ERROR_VARIANT_UNKNOWN:
		return "Unknown variant type";
		break;
	case LUA_ERROR_SPELL_NOT_FOUND:
		return "Spell not found";
		break;
	default:
		return "Wrong error code!";
		break;
	};
}

ScriptEnviroment LuaScriptInterface::m_scriptEnv[16];
int32_t LuaScriptInterface::m_scriptEnvIndex = -1;

LuaScriptInterface::LuaScriptInterface(std::string interfaceName)
{
	m_luaState = NULL;
	m_interfaceName = interfaceName;
	m_lastEventTimerId = 1000;
}

LuaScriptInterface::~LuaScriptInterface()
{
	for(LuaTimerEvents::iterator it = m_timerEvents.begin(); it != m_timerEvents.end(); ++it){
		g_scheduler.stopEvent(it->second.eventId);
	}

	closeState();
}

bool LuaScriptInterface::reInitState()
{
	closeState();
	return initState();
}

void LuaScriptInterface::dumpLuaStack()
{
	int a = lua_gettop(m_luaState);
	std::cout <<  "stack size: " << a << std::endl;
	for(int i = 1; i <= a ; ++i){
		std::cout << lua_typename(m_luaState, lua_type(m_luaState,-i)) << " " << lua_topointer(m_luaState, -i) << std::endl;
	}
}

int32_t LuaScriptInterface::loadFile(const std::string& file, bool reserveEnviroment /*= true*/)
{
	//loads file as a chunk at stack top
	int ret = luaL_loadfile(m_luaState, file.c_str());
	if(ret != 0){
		m_lastLuaError = popString(m_luaState);
		return -1;
	}
	//check that it is loaded as a function
	if(lua_isfunction(m_luaState, -1) == 0){
		return -1;
	}

	m_loadingFile = file;
	if(reserveEnviroment){
		reserveScriptEnv();
	}

	ScriptEnviroment* env = getScriptEnv();
	env->setScriptId(EVENT_ID_LOADING, this);

	//execute it
	ret = lua_pcall(m_luaState, 0, 0, 0);
	if(ret != 0){
		reportError(NULL, popString(m_luaState));
		if(reserveEnviroment){
			releaseScriptEnv();
		}
		return -1;
	}

	if(reserveEnviroment){
		releaseScriptEnv();
	}
	return 0;
}

int32_t LuaScriptInterface::loadBuffer(const std::string& text, bool reserveEnviroment /*= true*/)
{
	//loads file as a chunk at stack top
	const char* buffer = text.c_str();
	int ret = luaL_loadbuffer(m_luaState, buffer, strlen(buffer), "loadBuffer");
	if(ret != 0){
		m_lastLuaError = popString(m_luaState);
		reportError(NULL, m_lastLuaError);
		return -1;
	}
	//check that it is loaded as a function
	if(lua_isfunction(m_luaState, -1) == 0){
		return -1;
	}

	m_loadingFile = "loadBuffer";
	if(reserveEnviroment){
		reserveScriptEnv();
	}

	ScriptEnviroment* env = getScriptEnv();
	env->setScriptId(EVENT_ID_LOADING, this);

	//execute it
	ret = lua_pcall(m_luaState, 0, 0, 0);
	if(ret != 0){
		reportError(NULL, popString(m_luaState));
		if(reserveEnviroment){
			releaseScriptEnv();
		}
		return -1;
	}

	if(reserveEnviroment){
		releaseScriptEnv();
	}
	return 0;
}

int32_t LuaScriptInterface::getEvent(const std::string& eventName)
{
	//get our events table
	lua_getfield(m_luaState, LUA_REGISTRYINDEX, "EVENTS");
	if(lua_istable(m_luaState, -1) == 0){
		lua_pop(m_luaState, 1);
		return -1;
	}
	//get current event function pointer
	lua_getglobal(m_luaState, eventName.c_str());
	if(lua_isfunction(m_luaState, -1) == 0){
		lua_pop(m_luaState, 1);
		return -1;
	}
	//save in our events table
	lua_pushnumber(m_luaState, m_runningEventId);
	lua_pushvalue(m_luaState, -2);
	lua_rawset(m_luaState, -4);
	lua_pop(m_luaState, 2);
	//reset global value of this event
	lua_pushnil(m_luaState);
	lua_setglobal(m_luaState, eventName.c_str());

	m_cacheFiles[m_runningEventId] = m_loadingFile + ":" + eventName;
	++m_runningEventId;
	return m_runningEventId - 1;
}

const std::string& LuaScriptInterface::getFileById(int32_t scriptId)
{
	const static std::string unk = "(Unknown scriptfile)";
	if(scriptId != EVENT_ID_LOADING){
		ScriptsCache::iterator it = m_cacheFiles.find(scriptId);
		if(it != m_cacheFiles.end()){
			return it->second;
		}
		else{
			return unk;
		}
	}
	else{
		return m_loadingFile;
	}
}

void LuaScriptInterface::reportError(const char* function, const std::string& error_desc)
{
	ScriptEnviroment* env = getScriptEnv();
	int32_t scriptId;
	int32_t callbackId;
	bool timerEvent;
	std::string event_desc;
	LuaScriptInterface* scriptInterface;
	env->getEventInfo(scriptId, event_desc, scriptInterface, callbackId, timerEvent);

	std::cout << std::endl << "Lua Script Error: ";
	if(scriptInterface){
		std::cout << "[" << scriptInterface->getInterfaceName() << "] " << std::endl;
		if(timerEvent){
			std::cout << "in a timer event called from: " << std::endl;
		}
		if(callbackId){
			std::cout << "in callback: " << scriptInterface->getFileById(callbackId) << std::endl;
		}
		std::cout << scriptInterface->getFileById(scriptId) << std::endl;
	}
	std::cout << event_desc << std::endl;
	if(function)
		std::cout << function << "(). ";
	std::cout << error_desc << std::endl;
}

bool LuaScriptInterface::pushFunction(int32_t functionId)
{
	lua_getfield(m_luaState, LUA_REGISTRYINDEX, "EVENTS");
	if(lua_istable(m_luaState, -1) != 0){
		lua_pushnumber(m_luaState, functionId);
		lua_rawget(m_luaState, -2);
		lua_remove(m_luaState, -2);
		if(lua_isfunction(m_luaState, -1) != 0){
			return true;
		}
	}
	return false;
}

bool LuaScriptInterface::initState()
{
	m_luaState = luaL_newstate();
	if(!m_luaState){
		return false;
	}

#ifndef __USE_LUALIBRARIES__
	//Here you load only the "safe" libraries
	luaopen_base(m_luaState);
	luaopen_table(m_luaState);
	luaopen_os(m_luaState);
	luaopen_string(m_luaState);
	luaopen_math(m_luaState);
	// Unsafe, but immensely useful
	luaopen_debug(m_luaState);
#else
	//And here you load both "safe" and "unsafe" libraries
	luaL_openlibs(m_luaState);
#endif
	std::string datadir = g_config.getString(ConfigManager::DATA_DIRECTORY);

	registerFunctions();

	if(loadFile(std::string(datadir + "global.lua")) == -1){
		std::cout << "Warning: [LuaScriptInterface::initState] Can not load " << datadir << "global.lua." << std::endl;
	}

	lua_newtable(m_luaState);
	lua_setfield(m_luaState, LUA_REGISTRYINDEX, "EVENTS");

	m_runningEventId = EVENT_ID_USER;
	return true;
}

bool LuaScriptInterface::closeState()
{
	if(m_luaState){
		m_cacheFiles.clear();

		LuaTimerEvents::iterator it;
		for(it = m_timerEvents.begin(); it != m_timerEvents.end(); ++it){
			for(std::list<int>::iterator lt = it->second.parameters.begin(); lt != it->second.parameters.end(); ++lt){
				luaL_unref(m_luaState, LUA_REGISTRYINDEX, *lt);
			}
			it->second.parameters.clear();
			luaL_unref(m_luaState, LUA_REGISTRYINDEX, it->second.function);
		}
		m_timerEvents.clear();

		lua_close(m_luaState);
	}

	return true;
}

void LuaScriptInterface::executeTimerEvent(uint32_t eventIndex)
{
	LuaTimerEvents::iterator it = m_timerEvents.find(eventIndex);
	if(it != m_timerEvents.end()){
		//push function
		lua_rawgeti(m_luaState, LUA_REGISTRYINDEX, it->second.function);

		//push parameters
		for(std::list<int>::reverse_iterator rt = it->second.parameters.rbegin();
			rt != it->second.parameters.rend(); ++rt){
			lua_rawgeti(m_luaState, LUA_REGISTRYINDEX, *rt);
		}

		//call the function
		if(reserveScriptEnv()){
			ScriptEnviroment* env = getScriptEnv();
			env->setTimerEvent();
			env->setScriptId(it->second.scriptId, this);
			callFunction(it->second.parameters.size(), false);
			releaseScriptEnv();
		}
		else{
			std::cout << "[Error] Call stack overflow. LuaScriptInterface::executeTimerEvent" << std::endl;
		}

		//free resources
		for(std::list<int>::iterator lt = it->second.parameters.begin(); lt != it->second.parameters.end(); ++lt){
			luaL_unref(m_luaState, LUA_REGISTRYINDEX, *lt);
		}
		it->second.parameters.clear();
		luaL_unref(m_luaState, LUA_REGISTRYINDEX, it->second.function);
		m_timerEvents.erase(it);
	}
}

int LuaScriptInterface::luaErrorHandler(lua_State *L) {
	lua_getfield(L, LUA_GLOBALSINDEX, "debug");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return 1;
	}
	lua_getfield(L, -1, "traceback");
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 2);
		return 1;
	}
	lua_pushvalue(L, 1);
	lua_pushinteger(L, 2);
	lua_call(L, 2, 1);
	return 1;
}

bool LuaScriptInterface::callFunction(uint32_t nParams, bool getReturnValue /*=true*/)
{
	bool result = false;

	int size0 = lua_gettop(m_luaState);
	int error_index = size0 - nParams;
	lua_pushcfunction(m_luaState, luaErrorHandler);
	lua_insert(m_luaState, error_index);

	int qtdRet = 1;
	if (!getReturnValue){
		qtdRet = 0;
	}

	int ret = lua_pcall(m_luaState, nParams, qtdRet, error_index);
	if(ret != 0){
		LuaScriptInterface::reportError(NULL, LuaScriptInterface::popString(m_luaState));
	}
	else{
		if (getReturnValue){
			result = (int32_t)popBoolean(m_luaState);
		}
	}

	lua_remove(m_luaState, error_index);

	if(lua_gettop(m_luaState) + (int)nParams + 1 != size0){
		LuaScriptInterface::reportError(NULL, "Stack size changed!");
		std::cout << "Warning! stack size change: " << lua_gettop(m_luaState) + (int)nParams + 1 - size0 << std::endl;
		std::cout << "ret:" << ret << " getReturnValue:" << getReturnValue << std::endl;
		assert(false);
	}

	return result;
}

void LuaScriptInterface::pushVariant(lua_State *L, const LuaVariant& var)
{
	lua_newtable(L);
	setField(L, "type", var.type);

	switch(var.type){
		case VARIANT_NUMBER: setField(L, "number", var.number); break;
		case VARIANT_STRING: setField(L, "string", var.text); break;
		case VARIANT_TARGETPOSITION:
		case VARIANT_POSITION:
		{
				lua_pushstring(L, "pos");
				pushPosition(L, var.pos);
				lua_settable(L, -3);
				break;
		}
		case VARIANT_NONE:
			break;
	}
}

void LuaScriptInterface::pushThing(lua_State *L, Thing* thing, uint32_t thingid)
{
	lua_newtable(L);
	if(thing && thing->getItem()){
		const Item* item = thing->getItem();
		setField(L, "uid", thingid);
		setField(L, "itemid", item->getID());

		if(item->hasSubType())
			setField(L, "type", item->getSubType());
		else
			setField(L, "type", 0);

		setField(L, "actionid", item->getActionId());
	}
	else if(thing && thing->getCreature()){
		const Creature* creature = thing->getCreature();
		setField(L, "uid", thingid);
		setField(L, "itemid", 1);
		char type;
		if(creature->getPlayer()){
			type = 1;
		}
		else if(creature->getMonster()){
			type = 2;
		}
		else{//npc
			type = 3;
		}
		setField(L, "type", type);
		setField(L, "actionid", 0);
	}
	else{
		setField(L, "uid", 0);
		setField(L, "itemid", 0);
		setField(L, "type", 0);
		setField(L, "actionid", 0);
	}
}

void LuaScriptInterface::pushPosition(lua_State *L, const PositionEx& position)
{
	lua_newtable(L);
	setField(L, "z", position.z);
	setField(L, "y", position.y);
	setField(L, "x", position.x);
	setField(L, "stackpos", position.stackpos);
}

void LuaScriptInterface::pushPosition(lua_State *L, const Position& position, uint32_t stackpos)
{
	lua_newtable(L);
	setField(L, "z", position.z);
	setField(L, "y", position.y);
	setField(L, "x", position.x);
	setField(L, "stackpos", stackpos);
}

void LuaScriptInterface::pushCallback(lua_State *L, int32_t callback)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
}

LuaVariant LuaScriptInterface::popVariant(lua_State *L)
{
	uint32_t type = getField(L, "type");

	LuaVariant var;
	var.type = (LuaVariantType_t)type;

	switch(type){
		case VARIANT_NUMBER:
		{
			var.number = getFieldU32(L, "number");
			break;
		}

		case VARIANT_STRING:
		{
			var.text = getField(L, "string");
			break;
		}

		case VARIANT_POSITION:
		case VARIANT_TARGETPOSITION:
		{
			lua_pushstring(L, "pos");
			lua_gettable(L, -2);
			popPosition(L, var.pos);
			break;
		}

		default:
		{
			var.type = VARIANT_NONE;
			break;
		}
	}

	lua_pop(L, 1); //table

	return var;
}

void LuaScriptInterface::popPosition(lua_State *L, PositionEx& position)
{
	position.z = getField(L, "z");
	position.y = getField(L, "y");
	position.x = getField(L, "x");
	position.stackpos = getField(L, "stackpos");

	lua_pop(L, 1); //table
}

void LuaScriptInterface::popPosition(lua_State *L, Position& position, uint32_t& stackpos)
{
	position.z = getField(L, "z");
	position.y = getField(L, "y");
	position.x = getField(L, "x");
	stackpos = getField(L, "stackpos");
	lua_pop(L, 1); //table
}

bool LuaScriptInterface::popBoolean(lua_State *L, bool acceptIntegers /*=true*/)
{
	int tipo = lua_type(L,-1);

	lua_pop(L, 1);

	if (tipo == LUA_TBOOLEAN){
		return (lua_toboolean(L, 0) != 0);
	}
	else{
		if(tipo == LUA_TNUMBER && acceptIntegers){
			return (uint32_t(lua_tonumber(L, 0)) != 0);
		}
	else{
	if(g_config.getNumber(ConfigManager::LUA_EXCEPTED_TYPE_ERRORS_ENABLED)){
		reportErrorFunc("Error: Expected boolean type parameter.");
		}
		return false;
		}
	}
}

uint32_t LuaScriptInterface::popNumber(lua_State *L, bool acceptBooleans /*=false*/)
{
	uint32_t ret = 0;
	int tipo = lua_type(L,-1);
	lua_pop(L, 1);

	if (tipo == LUA_TNUMBER){
		ret = uint32_t(lua_tonumber(L, 0));
		}
	else{
		if ((tipo == LUA_TBOOLEAN) && acceptBooleans){
			ret = uint32_t((lua_toboolean(L, 0) != 0));
		}
		else{
		if(g_config.getNumber(ConfigManager::LUA_EXCEPTED_TYPE_ERRORS_ENABLED)){
			reportErrorFunc("Error: Expected an integer parameter.");
			}
		}
	}
	return ret;
}


double LuaScriptInterface::popFloatNumber(lua_State *L)
{
	lua_pop(L, 1);
	return (double)lua_tonumber(L, 0);
}

std::string LuaScriptInterface::popString(lua_State *L)
{
	lua_pop(L, 1);
	const char* str = "";
	if(lua_isstring(L, 0) != false){
		str = lua_tostring(L, 0);
	}
	return str;
}

int32_t LuaScriptInterface::popCallback(lua_State *L)
{
	return luaL_ref(L, LUA_REGISTRYINDEX);
}

void LuaScriptInterface::setField(lua_State *L, const char* index, double val)
{
	lua_pushstring(L, index);
	lua_pushnumber(L, val);
	lua_settable(L, -3);
}

void LuaScriptInterface::setField(lua_State *L, const char* index, const std::string& val)
{
	lua_pushstring(L, index);
	lua_pushstring(L, val.c_str());
	lua_settable(L, -3);
}

void LuaScriptInterface::setFieldBool(lua_State *L, const char* index, bool val)
{
	lua_pushstring(L, index);
	lua_pushboolean(L, val);
	lua_settable(L, -3);
}

int32_t LuaScriptInterface::getField(lua_State *L, const char *key)
{
	int32_t result;
	lua_pushstring(L, key);
	lua_gettable(L, -2);  // get table[key]
	result = (int32_t)lua_tonumber(L, -1);
	lua_pop(L, 1);  // remove number and key
	return result;
}

uint32_t LuaScriptInterface::getFieldU32(lua_State *L, const char *key)
{
	uint32_t result;
	lua_pushstring(L, key);
	lua_gettable(L, -2);  // get table[key]
	result = (uint32_t)lua_tonumber(L, -1);
	lua_pop(L, 1);  // remove number and key
	return result;
}

bool LuaScriptInterface::getFieldBool(lua_State *L, const char *key)
{
	bool result;
	lua_pushstring(L, key);
	lua_gettable(L, -2);  // get table[key]
	result = (lua_toboolean(L, -1) == 1);
	lua_pop(L, 1);  // remove number and key
	return result;
}

std::string LuaScriptInterface::getFieldString(lua_State *L, const char *key)
{
	std::string result = "";
	lua_pushstring(L, key);
	lua_gettable(L, -2);  // get table[key]
	if(lua_isstring(L, -1)){
		result = lua_tostring(L, -1);
	}
	lua_pop(L, 1);  // remove number and key
	return result;
}

void LuaScriptInterface::registerFunctions()
{
	//lua_register(L, "name", C_function);

	//getOTSYSTime()
	lua_register(m_luaState, "getOTSYSTime", LuaScriptInterface::luaGetOTSYSTime);

	//getConfigValue(key)
	lua_register(m_luaState, "getConfigValue", LuaScriptInterface::luaGetConfigValue);

	//getPlayerFood(cid)
	lua_register(m_luaState, "getPlayerFood", LuaScriptInterface::luaGetPlayerFood);

	//getPlayerMana(cid)
	lua_register(m_luaState, "getPlayerMana", LuaScriptInterface::luaGetPlayerMana);

	//getPlayerMaxMana(cid)
	lua_register(m_luaState, "getPlayerMaxMana", LuaScriptInterface::luaGetPlayerMaxMana);

	//getPlayerLevel(cid)
	lua_register(m_luaState, "getPlayerLevel", LuaScriptInterface::luaGetPlayerLevel);

	//getPlayerMagLevel(cid)
	lua_register(m_luaState, "getPlayerMagLevel", LuaScriptInterface::luaGetPlayerMagLevel);

	//getPlayerAccess(cid)
	lua_register(m_luaState, "getPlayerAccess", LuaScriptInterface::luaGetPlayerAccess);

	//getPlayerSkill(cid, skillid)
	lua_register(m_luaState, "getPlayerSkill", LuaScriptInterface::luaGetPlayerSkill);

	//getPlayerMasterPos(cid)
	lua_register(m_luaState, "getPlayerMasterPos", LuaScriptInterface::luaGetPlayerMasterPos);

	//getPlayerTown(cid)
	lua_register(m_luaState, "getPlayerTown", LuaScriptInterface::luaGetPlayerTown);

	//getPlayerVocation(cid)
	lua_register(m_luaState, "getPlayerVocation", LuaScriptInterface::luaGetPlayerVocation);

	//getPlayerItemCount(cid, itemid, subtype)
	lua_register(m_luaState, "getPlayerItemCount", LuaScriptInterface::luaGetPlayerItemCount);

	//getPlayerSoul(cid)
	lua_register(m_luaState, "getPlayerSoul", LuaScriptInterface::luaGetPlayerSoul);

	//getPlayerFreeCap(cid)
	lua_register(m_luaState, "getPlayerFreeCap", LuaScriptInterface::luaGetPlayerFreeCap);

	//getPlayerIp()
	lua_register(m_luaState, "getPlayerIp", LuaScriptInterface::luaGetPlayerIp);

	//getCreatureLight(cid)
	lua_register(m_luaState, "getCreatureLight", LuaScriptInterface::luaGetCreatureLight);

	//getCreatureLookDir(cid)
	lua_register(m_luaState, "getCreatureLookDir", LuaScriptInterface::luaGetCreatureLookDir);

	//getPlayerSlotItem(cid, slot)
	lua_register(m_luaState, "getPlayerSlotItem", LuaScriptInterface::luaGetPlayerSlotItem);

	//getPlayerItemById(cid, deepSearch, itemId, <optional> subType)
	lua_register(m_luaState, "getPlayerItemById", LuaScriptInterface::luaGetPlayerItemById);

	//getPlayerDepotItems(cid, depotid)
	lua_register(m_luaState, "getPlayerDepotItems", LuaScriptInterface::luaGetPlayerDepotItems);

	//getPlayerGuildId(cid)
	lua_register(m_luaState, "getPlayerGuildId", LuaScriptInterface::luaGetPlayerGuildId);

	//getPlayerGuildName(cid)
	lua_register(m_luaState, "getPlayerGuildName", LuaScriptInterface::luaGetPlayerGuildName);

	//getPlayerGuildRank(cid)
	lua_register(m_luaState, "getPlayerGuildRank", LuaScriptInterface::luaGetPlayerGuildRank);

	//getPlayerGuildNick(cid)
	lua_register(m_luaState, "getPlayerGuildNick", LuaScriptInterface::luaGetPlayerGuildNick);

	//getPlayerGuildLevel(cid)
	lua_register(m_luaState, "getPlayerGuildLevel", LuaScriptInterface::luaGetPlayerGuildLevel);

	//getPlayerSex(cid)
	lua_register(m_luaState, "getPlayerSex", LuaScriptInterface::luaGetPlayerSex);

	//getPlayerGUID(cid)
	lua_register(m_luaState, "getPlayerGUID", LuaScriptInterface::luaGetPlayerGUID);

	//getPlayerNameByGUID(guid)
	lua_register(m_luaState, "getPlayerNameByGUID", LuaScriptInterface::luaGetPlayerNameByGUID);

	//getPlayerFlagValue(cid, flag)
	lua_register(m_luaState, "getPlayerFlagValue", LuaScriptInterface::luaGetPlayerFlagValue);

	//getPlayerLossPercent(cid, lossType)
	lua_register(m_luaState, "getPlayerLossPercent", LuaScriptInterface::luaGetPlayerLossPercent);

	//getPlayerPremiumDays(cid)
	lua_register(m_luaState, "getPlayerPremiumDays", LuaScriptInterface::luaGetPlayerPremiumDays);

	//getPlayerSkullType(cid, <optional> viewer)
	lua_register(m_luaState, "getPlayerSkullType", LuaScriptInterface::luaGetPlayerSkullType);

	//setPlayerSkullType(cid, skull_type)
	lua_register(m_luaState, "setPlayerSkullType", LuaScriptInterface::luaSetPlayerSkullType);

	//getPlayerSkullEndTime(cid)
	lua_register(m_luaState, "getPlayerSkullEndTime", LuaScriptInterface::luaGetPlayerSkullEndTime);

	//getPlayerUnjustKills(cid)
	lua_register(m_luaState, "getPlayerUnjustKills", LuaScriptInterface::luaGetPlayerUnjustKills);

	//getPlayerAccountBalance(cid)
	lua_register(m_luaState, "getPlayerAccountBalance", LuaScriptInterface::luaGetPlayerAccountBalance);

	//getPlayerByNameWildcard(name)
	lua_register(m_luaState, "getPlayerByNameWildcard", LuaScriptInterface::luaGetPlayerByNameWildcard);

	//playerLearnInstantSpell(cid, name)
	lua_register(m_luaState, "playerLearnInstantSpell", LuaScriptInterface::luaPlayerLearnInstantSpell);

	//canPlayerLearnInstantSpell(cid, name)
	lua_register(m_luaState, "canPlayerLearnInstantSpell", LuaScriptInterface::luaCanPlayerLearnInstantSpell);

	//getPlayerLearnedInstantSpell(cid, name)
	lua_register(m_luaState, "getPlayerLearnedInstantSpell", LuaScriptInterface::luaGetPlayerLearnedInstantSpell);

	//getPlayerInstantSpellInfo(cid, index)
	lua_register(m_luaState, "getPlayerInstantSpellInfo", LuaScriptInterface::luaGetPlayerInstantSpellInfo);

	//getPlayerInstantSpellCount(cid)
	lua_register(m_luaState, "getPlayerInstantSpellCount", LuaScriptInterface::luaGetPlayerInstantSpellCount);

	//getInstantSpellInfoByName(cid, name)
	lua_register(m_luaState, "getInstantSpellInfoByName", LuaScriptInterface::luaGetInstantSpellInfoByName);

	//getInstantSpellWords(name)
	lua_register(m_luaState, "getInstantSpellWords", LuaScriptInterface::luaGetInstantSpellWords);

	//getPlayerStorageValue(cid, valueid)
	lua_register(m_luaState, "getPlayerStorageValue", LuaScriptInterface::luaGetPlayerStorageValue);

	//setPlayerStorageValue(cid, valueid, newvalue)
	lua_register(m_luaState, "setPlayerStorageValue", LuaScriptInterface::luaSetPlayerStorageValue);

	//doErasePlayerStorageValue(cid, valueid)
	lua_register(m_luaState, "doErasePlayerStorageValue", LuaScriptInterface::luaDoErasePlayerStorageValue);

	//isPremium(cid)
	lua_register(m_luaState, "isPremium", LuaScriptInterface::luaIsPremium);

	//getPlayerLastLogin(cid)
	lua_register(m_luaState, "getPlayerLastLogin", LuaScriptInterface::luaGetPlayerLastLogin);

	//getGlobalStorageValue(valueid)
	lua_register(m_luaState, "getGlobalStorageValue", LuaScriptInterface::luaGetGlobalStorageValue);

	//setGlobalStorageValue(valueid, newvalue)
	lua_register(m_luaState, "setGlobalStorageValue", LuaScriptInterface::luaSetGlobalStorageValue);

	//doEraseGlobalStorageValue(valueid)
	lua_register(m_luaState, "doEraseGlobalStorageValue", LuaScriptInterface::luaDoEraseGlobalStorageValue);

	//doErasePlayerStorageValueByName(name, key)
	lua_register(m_luaState, "doErasePlayerStorageValueByName", LuaScriptInterface::luaDoErasePlayerStorageValueByName);

	//setPlayerStorageValueByName(name, key, newValue)
	lua_register(m_luaState, "setPlayerStorageValueByName", LuaScriptInterface::luaSetPlayerStorageValueByName);

	//getPlayerStorageValueByName(name, key)
	lua_register(m_luaState, "getPlayerStorageValueByName", LuaScriptInterface::luaGetPlayerStorageValueByName);

	//getTilePzInfo(pos)
	//1 is pz. 0 no pz.
	lua_register(m_luaState, "getTilePzInfo", LuaScriptInterface::luaGetTilePzInfo);

	//getTileHouseInfo(pos)
	//0 no house. != 0 house id
	lua_register(m_luaState, "getTileHouseInfo", LuaScriptInterface::luaGetTileHouseInfo);

	//getItemRWInfo(uid)
	lua_register(m_luaState, "getItemRWInfo", LuaScriptInterface::luaGetItemRWInfo);

	//getThingDefaultDescription(uid, lookDistance, <optional> canSeeXRay)
	lua_register(m_luaState, "getThingDefaultDescription", LuaScriptInterface::luaGetThingDefaultDescription);

	//getItemTypeDefaultDescription(itemid, <optional> count)
	lua_register(m_luaState, "getItemTypeDefaultDescription", LuaScriptInterface::luaGetItemTypeDefaultDescription);

	//getItemSpecialDescription(uid)
	lua_register(m_luaState, "getItemSpecialDescription", LuaScriptInterface::luaGetItemSpecialDescription);

	//getThingFromPos(pos)
	lua_register(m_luaState, "getThingFromPos", LuaScriptInterface::luaGetThingFromPos);

	//getThing(uid)
	lua_register(m_luaState, "getThing", LuaScriptInterface::luaGetThing);

	//queryTileAddThing(uid, pos, <optional> flags)
	lua_register(m_luaState, "queryTileAddThing", LuaScriptInterface::luaQueryTileAddThing);

	//getThingPos(uid)
	lua_register(m_luaState, "getThingPos", LuaScriptInterface::luaGetThingPos);

	//getTileStackItemsSize(pos)
	lua_register(m_luaState, "getTileStackItemsSize", LuaScriptInterface::luaGetTileStackItemsSize);

	//getTileItemById(pos, itemId, <optional> subType)
	lua_register(m_luaState, "getTileItemById", LuaScriptInterface::luaGetTileItemById);

	//getTileItemByType(pos, type)
	lua_register(m_luaState, "getTileItemByType", LuaScriptInterface::luaGetTileItemByType);

	//getTileThingByPos(pos)
	lua_register(m_luaState, "getTileThingByPos", LuaScriptInterface::luaGetTileThingByPos);

	//getTileThingByTopOrder(pos, topOrder)
	lua_register(m_luaState, "getTileThingByTopOrder", LuaScriptInterface::luaGetTileThingByTopOrder);

	//getTopCreature(pos)
	lua_register(m_luaState, "getTopCreature", LuaScriptInterface::luaGetTopCreature);

	//getAllCreatures(pos, <optional> flag)
	lua_register(m_luaState, "getAllCreatures", LuaScriptInterface::luaGetAllCreatures);

	//getWaypointPositionByName(name)
	lua_register(m_luaState, "getWaypointPositionByName", LuaScriptInterface::luaGetWaypointPositionByName);

	//doRemoveItem(uid, <optional> count)
	lua_register(m_luaState, "doRemoveItem", LuaScriptInterface::luaDoRemoveItem);

	//doPlayerFeed(cid, food)
	lua_register(m_luaState, "doPlayerFeed", LuaScriptInterface::luaDoFeedPlayer);

	//doPlayerSendCancel(cid, text)
	lua_register(m_luaState, "doPlayerSendCancel", LuaScriptInterface::luaDoSendCancel);

	//doPlayerSendDefaultCancel(cid, ReturnValue)
	lua_register(m_luaState, "doPlayerSendDefaultCancel", LuaScriptInterface::luaDoSendDefaultCancel);

	//doPlayerSetIdleTime(cid, time, warned)
	lua_register(m_luaState, "doPlayerSetIdleTime", LuaScriptInterface::luaDoPlayerSetIdleTime);

	//doTeleportThing(uid, newpos)
	lua_register(m_luaState, "doTeleportThing", LuaScriptInterface::luaDoTeleportThing);

	//doTransformItem(uid, toitemid, <optional> count/subtype)
	lua_register(m_luaState, "doTransformItem", LuaScriptInterface::luaDoTransformItem);

	//doCreatureSay(cid, text, type)
	lua_register(m_luaState, "doCreatureSay", LuaScriptInterface::luaDoCreatureSay);

	//doSendMagicEffect(pos, type[, player])
	lua_register(m_luaState, "doSendMagicEffect", LuaScriptInterface::luaDoSendMagicEffect);

	//doSendDistanceShoot(frompos, topos, type)
	lua_register(m_luaState, "doSendDistanceShoot", LuaScriptInterface::luaDoSendDistanceShoot);

	//doChangeTypeItem(uid, newtype)
	lua_register(m_luaState, "doChangeTypeItem", LuaScriptInterface::luaDoChangeTypeItem);

	//doSetItemActionId(uid, actionid)
	lua_register(m_luaState, "doSetItemActionId", LuaScriptInterface::luaDoSetItemActionId);

	//doSetItemText(uid, text)
	lua_register(m_luaState, "doSetItemText", LuaScriptInterface::luaDoSetItemText);

	//doSetItemSpecialDescription(uid, desc)
	lua_register(m_luaState, "doSetItemSpecialDescription", LuaScriptInterface::luaDoSetItemSpecialDescription);

	//doSendAnimatedText(pos, text, color)
	lua_register(m_luaState, "doSendAnimatedText", LuaScriptInterface::luaDoSendAnimatedText);

	//doPlayerAddInFightTicks(cid, ticks, <optional: default: 0> pzLock)
	lua_register(m_luaState, "doPlayerAddInFightTicks", LuaScriptInterface::luaDoPlayerAddInFightTicks);

	//doPlayerAddSkillTry(cid, skillid, n, <optional: default: 0> useMultiplier)
	lua_register(m_luaState, "doPlayerAddSkillTry", LuaScriptInterface::luaDoPlayerAddSkillTry);

	//doPlayerAddManaSpent(cid, mana, <optional: default: 0> useMultiplier)
	lua_register(m_luaState, "doPlayerAddManaSpent", LuaScriptInterface::luaDoPlayerAddManaSpent);

	//doCreatureAddHealth(cid, health, <optional: default: 1> filter))
	lua_register(m_luaState, "doCreatureAddHealth", LuaScriptInterface::luaDoCreatureAddHealth);

	//doPlayerAddMana(cid, mana, <optional: default: 1> withoutAnimation)
	lua_register(m_luaState, "doPlayerAddMana", LuaScriptInterface::luaDoPlayerAddMana);

	//doPlayerAddSoul(cid, soul)
	lua_register(m_luaState, "doPlayerAddSoul", LuaScriptInterface::luaDoPlayerAddSoul);

	//doPlayerAddItem(uid, itemid, <optional: default: 1> count/subtype)
	//doPlayerAddItem(cid, itemid, <optional: default: 1> count, <optional: default: 1> canDropOnMap, <optional: default: 1>subtype)
	//Returns uid of the created item
	lua_register(m_luaState, "doPlayerAddItem", LuaScriptInterface::luaDoPlayerAddItem);

	//doPlayerAddItemEx(cid, uid, <optional: default: 0> canDropOnMap, <optional> slot)
	lua_register(m_luaState, "doPlayerAddItemEx", LuaScriptInterface::luaDoPlayerAddItemEx);

	//doPlayerSendTextMessage(cid, MessageClasses, message)
	lua_register(m_luaState, "doPlayerSendTextMessage", LuaScriptInterface::luaDoPlayerSendTextMessage);

	//doPlayerRemoveMoney(cid, money)
	lua_register(m_luaState, "doPlayerRemoveMoney", LuaScriptInterface::luaDoPlayerRemoveMoney);

	//doPlayerAddMoney(cid, money)
	lua_register(m_luaState, "doPlayerAddMoney", LuaScriptInterface::luaDoPlayerAddMoney);

	//doPlayerWithdrawMoney(cid, money)
	lua_register(m_luaState, "doPlayerWithdrawMoney", LuaScriptInterface::luaDoPlayerWithdrawMoney);

	//doPlayerDepositMoney(cid, money)
	lua_register(m_luaState, "doPlayerDepositMoney", LuaScriptInterface::luaDoPlayerDepositMoney);

	//doPlayerTransferMoneyTo(cid, target, money)
	lua_register(m_luaState, "doPlayerTransferMoneyTo", LuaScriptInterface::luaDoPlayerTransferMoneyTo);
	
	//sendOutfitWindow(cid)
	lua_register(m_luaState, "sendOutfitWindow", LuaScriptInterface::luaSendOutfitWindow);

	//doShowTextWindow(cid, maxlen, canWrite)
	lua_register(m_luaState, "doShowTextWindow", LuaScriptInterface::luaDoShowTextWindow);

	//doShowTextDialog(cid, itemid, text)
	lua_register(m_luaState, "doShowTextDialog", LuaScriptInterface::luaDoShowTextDialog);

	//getTownIdByName(townName)
	lua_register(m_luaState, "getTownIdByName", LuaScriptInterface::luaGetTownIdByName);

	//getTownNameById(townId)
	lua_register(m_luaState, "getTownNameById", LuaScriptInterface::luaGetTownNameById);

	//getTownTemplePosition(townId)
	lua_register(m_luaState, "getTownTemplePosition", LuaScriptInterface::luaGetTownTemplePosition);

	//doDecayItem(uid)
	lua_register(m_luaState, "doDecayItem", LuaScriptInterface::luaDoDecayItem);

	//doCreateItem(itemid, <optional> type/count, pos)
	//Returns uid of the created item, only works on tiles.
	lua_register(m_luaState, "doCreateItem", LuaScriptInterface::luaDoCreateItem);

	//doCreateItemEx(itemid, <optional> count/subtype)
	lua_register(m_luaState, "doCreateItemEx", LuaScriptInterface::luaDoCreateItemEx);

	//doTileAddItemEx(pos, uid)
	lua_register(m_luaState, "doTileAddItemEx", LuaScriptInterface::luaDoTileAddItemEx);

	//doAddContainerItemEx(uid, virtuid)
	lua_register(m_luaState, "doAddContainerItemEx", LuaScriptInterface::luaAddContainerItemEx);

	//doRelocate(pos, posTo, <optional: default: false> moveUnmoveable, <optional: default: 0> maxAmount)
	//Moves all objects from pos to posTo
	lua_register(m_luaState, "doRelocate", LuaScriptInterface::luaDoRelocate);

	//doCreateTeleport(teleportID, positionToGo, createPosition)
	lua_register(m_luaState, "doCreateTeleport", LuaScriptInterface::luaDoCreateTeleport);

	//doSummonCreature(name, pos, <optional> extendedPosition, <optional> forceSpawn)
	lua_register(m_luaState, "doSummonCreature", LuaScriptInterface::luaDoSummonCreature);

	//doPlayerSummonCreature(cid, name, pos, <optional> extendedPosition, <optional> forceSpawn)
	lua_register(m_luaState, "doPlayerSummonCreature", LuaScriptInterface::luaDoPlayerSummonCreature);

	//doRemoveCreature(cid)
	lua_register(m_luaState, "doRemoveCreature", LuaScriptInterface::luaDoRemoveCreature);

	//doMoveCreature(cid, direction, <optional>flags)
	lua_register(m_luaState, "doMoveCreature", LuaScriptInterface::luaDoMoveCreature);

	//doSetCreatureDirection(cid, direction)
	lua_register(m_luaState, "doSetCreatureDirection", LuaScriptInterface::luaDoSetCreatureDirection);

	//doPlayerSetMasterPos(cid, pos)
	lua_register(m_luaState, "doPlayerSetMasterPos", LuaScriptInterface::luaDoPlayerSetMasterPos);

	//doPlayerSetTown(cid, townid)
	lua_register(m_luaState, "doPlayerSetTown", LuaScriptInterface::luaDoPlayerSetTown);

	//doPlayerSetVocation(cid, voc)
	lua_register(m_luaState, "doPlayerSetVocation", LuaScriptInterface::luaDoPlayerSetVocation);

	//doPlayerSetSex(cid, sex)
	lua_register(m_luaState, "doPlayerSetSex", LuaScriptInterface::luaDoPlayerSetSex);

	//doPlayerRemoveItem(cid, itemid, count, <optional> subtype, <optional> ignoreEquipped)
	lua_register(m_luaState, "doPlayerRemoveItem", LuaScriptInterface::luaDoPlayerRemoveItem);

	//doPlayerAddExp(cid, exp, <optional: default: 0> useRate, <optional: default: 0> useMultiplier)
	lua_register(m_luaState, "doPlayerAddExp", LuaScriptInterface::luaDoPlayerAddExp);

	//doPlayerRemoveExp(cid, exp, <optional: default: 0> useRate, <optional: default: 0> useMultiplier)
	lua_register(m_luaState, "doPlayerRemoveExp", LuaScriptInterface::luaDoPlayerRemoveExp);

	//getPlayerExperience(cid)
	lua_register(m_luaState, "getPlayerExperience", LuaScriptInterface::luaGetPlayerExperience);

	//doPlayerSetGuildRank(cid, rank)
	lua_register(m_luaState, "doPlayerSetGuildRank", LuaScriptInterface::luaDoPlayerSetGuildRank);

	//doPlayerSetGuildNick(cid, nick)
	lua_register(m_luaState, "doPlayerSetGuildNick", LuaScriptInterface::luaDoPlayerSetGuildNick);

	//doSetCreatureLight(cid, lightLevel, lightColor, time)
	lua_register(m_luaState, "doSetCreatureLight", LuaScriptInterface::luaDoSetCreatureLight);

	//doPlayerSetLossPercent(cid, lossType, newPercent)
	lua_register(m_luaState, "doPlayerSetLossPercent", LuaScriptInterface::luaDoPlayerSetLossPercent);

	//doSetCreatureDropLoot(cid, doDrop)
	lua_register(m_luaState, "doSetCreatureDropLoot", LuaScriptInterface::luaDoSetCreatureDropLoot);

	//isValidUID(uid)
	lua_register(m_luaState, "isValidUID", LuaScriptInterface::luaIsValidUID);

	//isCreatureImmuneToCondition(cid, conditionType)
	lua_register(m_luaState, "isCreatureImmuneToCondition", LuaScriptInterface::luaIsCreatureImmuneToCondition);

	//isCreature(cid)
	lua_register(m_luaState, "isCreature", LuaScriptInterface::luaIsCreature);

	//isItemTwoHandedByUID(uid)
	lua_register(m_luaState, "isItemTwoHandedByUID", LuaScriptInterface::luaIsItemTwoHandedByUID);

	//isItemTwoHanded(uid)
	lua_register(m_luaState, "isItemTwoHanded", LuaScriptInterface::luaIsItemTwoHanded);

	//isContainer(uid)
	lua_register(m_luaState, "isContainer", LuaScriptInterface::luaIsContainer);

	//isCorpse(uid)
	lua_register(m_luaState, "isCorpse", LuaScriptInterface::luaIsCorpse);

	//isMoveable(uid)
	lua_register(m_luaState, "isMoveable", LuaScriptInterface::luaIsMoveable);

	//getPlayerGUIDByName(name)
	lua_register(m_luaState, "getPlayerGUIDByName", LuaScriptInterface::luaGetPlayerGUIDByName);

	//registerCreatureEvent(uid, eventName)
	lua_register(m_luaState, "registerCreatureEvent", LuaScriptInterface::luaRegisterCreatureEvent);

	//getContainerSize(uid)
	lua_register(m_luaState, "getContainerSize", LuaScriptInterface::luaGetContainerSize);

	//getContainerCap(uid)
	lua_register(m_luaState, "getContainerCap", LuaScriptInterface::luaGetContainerCap);

	//getContainerItem(uid, slot)
	lua_register(m_luaState, "getContainerItem", LuaScriptInterface::luaGetContainerItem);

	//doAddContainerItem(uid, itemid, <optional> count/subtype)
	lua_register(m_luaState, "doAddContainerItem", LuaScriptInterface::luaDoAddContainerItem);

	//getDepotId(uid)
	lua_register(m_luaState, "getDepotId", LuaScriptInterface::luaGetDepotId);

	//getHouseOwner(houseid)
	lua_register(m_luaState, "getHouseOwner", LuaScriptInterface::luaGetHouseOwner);

	//getHouseName(houseid)
	lua_register(m_luaState, "getHouseName", LuaScriptInterface::luaGetHouseName);

	//getHouseEntry(houseid)
	lua_register(m_luaState, "getHouseEntry", LuaScriptInterface::luaGetHouseEntry);

	//getHouseRent(houseid)
	lua_register(m_luaState, "getHouseRent", LuaScriptInterface::luaGetHouseRent);

	//getHouseTown(houseid)
	lua_register(m_luaState, "getHouseTown", LuaScriptInterface::luaGetHouseTown);

	//getHouseAccessList(houseid, listid)
	lua_register(m_luaState, "getHouseAccessList", LuaScriptInterface::luaGetHouseAccessList);

	//getHouseByPlayerGUID(playerGUID)
	lua_register(m_luaState, "getHouseByPlayerGUID", LuaScriptInterface::luaGetHouseByPlayerGUID);

	//getHouseTilesSize(houseid)
	lua_register(m_luaState, "getHouseTilesSize", LuaScriptInterface::luaGetHouseTilesSize);

	//getHouseDoorCount(houseid)
	lua_register(m_luaState, "getHouseDoorCount", LuaScriptInterface::luaGetHouseDoorCount);

	//getHouseBedCount(houseid)
	lua_register(m_luaState, "getHouseBedCount", LuaScriptInterface::luaGetHouseBedCount);

	//isHouseGuildHall(houseid)
	lua_register(m_luaState, "isHouseGuildHall", LuaScriptInterface::luaIsHouseGuildHall);

	//setHouseAccessList(houseid, listid, listtext)
	lua_register(m_luaState, "setHouseAccessList", LuaScriptInterface::luaSetHouseAccessList);

	//setHouseOwner(houseid, ownerGUID)
	lua_register(m_luaState, "setHouseOwner", LuaScriptInterface::luaSetHouseOwner);

	//getHouseList(townid)
	lua_register(m_luaState, "getHouseList", LuaScriptInterface::luaGetHouseList);

	//cleanHouse(houseid)
	lua_register(m_luaState, "cleanHouse", LuaScriptInterface::luaCleanHouse);

	//getWorldType()
	lua_register(m_luaState, "getWorldType", LuaScriptInterface::luaGetWorldType);

	//getWorldTime()
	lua_register(m_luaState, "getWorldTime", LuaScriptInterface::luaGetWorldTime);

	//getWorldLight()
	lua_register(m_luaState, "getWorldLight", LuaScriptInterface::luaGetWorldLight);

	//getWorldCreatures(type)
	//0 players, 1 monsters, 2 npcs, 3 all
	lua_register(m_luaState, "getWorldCreatures", LuaScriptInterface::luaGetWorldCreatures);

	//getWorldUpTime()
	lua_register(m_luaState, "getWorldUpTime", LuaScriptInterface::luaGetWorldUpTime);

	//getPlayersOnlineList()
	lua_register(m_luaState, "getPlayersOnlineList", LuaScriptInterface::luaGetPlayersOnlineList);

	//doPlayerBroadcastMessage(cid, message)
	lua_register(m_luaState, "doPlayerBroadcastMessage", LuaScriptInterface::luaDoPlayerBroadcastMessage);

	//getGuildId(guild_name)
	lua_register(m_luaState, "getGuildId", LuaScriptInterface::luaGetGuildId);

	//createCombatArea( {area}, <optional> {extArea} )
	lua_register(m_luaState, "createCombatArea", LuaScriptInterface::luaCreateCombatArea);

	//createConditionObject(type)
	lua_register(m_luaState, "createConditionObject", LuaScriptInterface::luaCreateConditionObject);

	//setCombatArea(combat, area)
	lua_register(m_luaState, "setCombatArea", LuaScriptInterface::luaSetCombatArea);

	//setCombatCondition(combat, condition)
	lua_register(m_luaState, "setCombatCondition", LuaScriptInterface::luaSetCombatCondition);

	//setCombatParam(combat, key, value)
	lua_register(m_luaState, "setCombatParam", LuaScriptInterface::luaSetCombatParam);

	//setConditionParam(condition, key, value)
	lua_register(m_luaState, "setConditionParam", LuaScriptInterface::luaSetConditionParam);

	//addDamageCondition(condition, rounds, time, value)
	lua_register(m_luaState, "addDamageCondition", LuaScriptInterface::luaAddDamageCondition);

	//addOutfitCondition(condition, lookTypeEx, lookType, lookHead, lookBody, lookLegs, lookFeet)
	lua_register(m_luaState, "addOutfitCondition", LuaScriptInterface::luaAddOutfitCondition);

	//setCombatCallBack(combat, key, function_name)
	lua_register(m_luaState, "setCombatCallback", LuaScriptInterface::luaSetCombatCallBack);

	//setCombatFormula(combat, type, mina, minb, maxa, maxb)
	lua_register(m_luaState, "setCombatFormula", LuaScriptInterface::luaSetCombatFormula);

	//setConditionFormula(combat, mina, minb, maxa, maxb)
	lua_register(m_luaState, "setConditionFormula", LuaScriptInterface::luaSetConditionFormula);

	//doCombat(cid, combat, param)
	lua_register(m_luaState, "doCombat", LuaScriptInterface::luaDoCombat);

	//createCombatObject()
	lua_register(m_luaState, "createCombatObject", LuaScriptInterface::luaCreateCombatObject);

	//doAreaCombatHealth(cid, type, pos, area, min, max, effect)
	lua_register(m_luaState, "doAreaCombatHealth", LuaScriptInterface::luaDoAreaCombatHealth);

	//doTargetCombatHealth(cid, target, type, min, max, effect)
	lua_register(m_luaState, "doTargetCombatHealth", LuaScriptInterface::luaDoTargetCombatHealth);

	//doAreaCombatMana(cid, pos, area, min, max, effect)
	lua_register(m_luaState, "doAreaCombatMana", LuaScriptInterface::luaDoAreaCombatMana);

	//doTargetCombatMana(cid, target, min, max, effect)
	lua_register(m_luaState, "doTargetCombatMana", LuaScriptInterface::luaDoTargetCombatMana);

	//doAreaCombatCondition(cid, pos, area, condition, effect)
	lua_register(m_luaState, "doAreaCombatCondition", LuaScriptInterface::luaDoAreaCombatCondition);

	//doTargetCombatCondition(cid, target, condition, effect)
	lua_register(m_luaState, "doTargetCombatCondition", LuaScriptInterface::luaDoTargetCombatCondition);

	//doAreaCombatDispel(cid, pos, area, type, effect)
	lua_register(m_luaState, "doAreaCombatDispel", LuaScriptInterface::luaDoAreaCombatDispel);

	//doTargetCombatDispel(cid, target, type, effect)
	lua_register(m_luaState, "doTargetCombatDispel", LuaScriptInterface::luaDoTargetCombatDispel);

	//doChallengeCreature(cid, target)
	lua_register(m_luaState, "doChallengeCreature", LuaScriptInterface::luaDoChallengeCreature);

	//doConvinceCreature(cid, target)
	lua_register(m_luaState, "doConvinceCreature", LuaScriptInterface::luaDoConvinceCreature);

	//getMonsterTargetList(cid)
	lua_register(m_luaState, "getMonsterTargetList", LuaScriptInterface::luaGetMonsterTargetList);

	//getMonsterFriendList(cid)
	lua_register(m_luaState, "getMonsterFriendList", LuaScriptInterface::luaGetMonsterFriendList);

	//doSetMonsterTarget(cid, target)
	lua_register(m_luaState, "doSetMonsterTarget", LuaScriptInterface::luaDoSetMonsterTarget);

	//doMonsterChangeTarget(cid)
	lua_register(m_luaState, "doMonsterChangeTarget", LuaScriptInterface::luaDoMonsterChangeTarget);

	//doAddCondition(cid, condition)
	lua_register(m_luaState, "doAddCondition", LuaScriptInterface::luaDoAddCondition);

	//doRemoveCondition(cid, type, <optional> subId)
	lua_register(m_luaState, "doRemoveCondition", LuaScriptInterface::luaDoRemoveCondition);

	//numberToVariant(number)
	lua_register(m_luaState, "numberToVariant", LuaScriptInterface::luaNumberToVariant);

	//stringToVariant(string)
	lua_register(m_luaState, "stringToVariant", LuaScriptInterface::luaStringToVariant);

	//positionToVariant(pos)
	lua_register(m_luaState, "positionToVariant", LuaScriptInterface::luaPositionToVariant);

	//targetPositionToVariant(pos)
	lua_register(m_luaState, "targetPositionToVariant", LuaScriptInterface::luaTargetPositionToVariant);

	//variantToNumber(var)
	lua_register(m_luaState, "variantToNumber", LuaScriptInterface::luaVariantToNumber);

	//variantToString(var)
	lua_register(m_luaState, "variantToString", LuaScriptInterface::luaVariantToString);

	//variantToPosition(var)
	lua_register(m_luaState, "variantToPosition", LuaScriptInterface::luaVariantToPosition);

	//doChangeSpeed(cid, delta)
	lua_register(m_luaState, "doChangeSpeed", LuaScriptInterface::luaDoChangeSpeed);

	//doCreatureChangeOutfit(cid, outfit)
	lua_register(m_luaState, "doCreatureChangeOutfit", LuaScriptInterface::luaDoCreatureChangeOutfit);

	//doSetMonsterOutfit(cid, name, time)
	lua_register(m_luaState, "doSetMonsterOutfit", LuaScriptInterface::luaSetMonsterOutfit);

	//doSetItemOutfit(cid, item, time)
	lua_register(m_luaState, "doSetItemOutfit", LuaScriptInterface::luaSetItemOutfit);

	//doSetCreatureOutfit(cid, outfit, time)
	lua_register(m_luaState, "doSetCreatureOutfit", LuaScriptInterface::luaSetCreatureOutfit);

	//getCreatureOutfit(cid)
	lua_register(m_luaState, "getCreatureOutfit", LuaScriptInterface::luaGetCreatureOutfit);

	//getCreaturePosition(cid)
	lua_register(m_luaState, "getCreaturePosition", LuaScriptInterface::luaGetCreaturePosition);

	//getCreatureName(cid)
	lua_register(m_luaState, "getCreatureName", LuaScriptInterface::luaGetCreatureName);

	//getCreatureSpeed(cid)
	lua_register(m_luaState, "getCreatureSpeed", LuaScriptInterface::luaGetCreatureSpeed);

	//getCreatureBaseSpeed(cid)
	lua_register(m_luaState, "getCreatureBaseSpeed", LuaScriptInterface::luaGetCreatureBaseSpeed);

	//getCreatureTarget(cid)
	lua_register(m_luaState, "getCreatureTarget", LuaScriptInterface::luaGetCreatureTarget);

	//getCreatureHealth(cid)
	lua_register(m_luaState, "getCreatureHealth", LuaScriptInterface::luaGetCreatureHealth);

	//getCreatureMaxHealth(cid)
	lua_register(m_luaState, "getCreatureMaxHealth", LuaScriptInterface::luaGetCreatureMaxHealth);

	//getCreatureByName(name)
	lua_register(m_luaState, "getCreatureByName", LuaScriptInterface::luaGetCreatureByName);

	//getCreatureMaster(cid)
	//returns the creature's master or itself if the creature isn't a summon
	lua_register(m_luaState, "getCreatureMaster", LuaScriptInterface::luaGetCreatureMaster);

	//getCreatureSummons(cid)
	//returns a table with all the summons of the creature
	lua_register(m_luaState, "getCreatureSummons", LuaScriptInterface::luaGetCreatureSummons);

	//getSpectators(centerPos, rangex, rangey, multifloor)
	lua_register(m_luaState, "getSpectators", LuaScriptInterface::luaGetSpectators);

	//getPartyMembers(cid)
	lua_register(m_luaState, "getPartyMembers", LuaScriptInterface::luaGetPartyMembers);

	//hasCondition(cid, conditionid)
	lua_register(m_luaState, "hasCondition", LuaScriptInterface::luaHasCondition);

	//hasProperty(uid)
	lua_register(m_luaState, "hasProperty", LuaScriptInterface::luaHasProperty);

	//isItemStackable(itemid)
	lua_register(m_luaState, "isItemStackable", LuaScriptInterface::luaIsItemStackable);

	//isItemRune(itemid)
	lua_register(m_luaState, "isItemRune", LuaScriptInterface::luaIsItemRune);

	//isItemDoor(itemid)
	lua_register(m_luaState, "isItemDoor", LuaScriptInterface::luaIsItemDoor);

	//isItemContainer(itemid)
	lua_register(m_luaState, "isItemContainer", LuaScriptInterface::luaIsItemContainer);

	//isItemFluidContainer(itemid)
	lua_register(m_luaState, "isItemFluidContainer", LuaScriptInterface::luaIsItemFluidContainer);

	//isItemMoveable(itemid)
	lua_register(m_luaState, "isItemMoveable",LuaScriptInterface::luaIsItemMoveable);

	//getItemName(itemid)
	lua_register(m_luaState, "getItemName", LuaScriptInterface::luaGetItemName);

	//getItemDescriptions(itemid)
	lua_register(m_luaState, "getItemDescriptions", LuaScriptInterface::luaGetItemDescriptions);

	//getItemWeight(uid)
	lua_register(m_luaState, "getItemWeight", LuaScriptInterface::luaGetItemWeight);

	//getItemIdByName(name)
	lua_register(m_luaState, "getItemIdByName", LuaScriptInterface::luaGetItemIdByName);

	//isSightClear(fromPos, toPos, floorCheck)
	lua_register(m_luaState, "isSightClear", LuaScriptInterface::luaIsSightClear);

	//getFluidSourceType(type)
	lua_register(m_luaState, "getFluidSourceType", LuaScriptInterface::luaGetFluidSourceType);

	//isIntegerInArray(array, value)
	lua_register(m_luaState, "isIntegerInArray", LuaScriptInterface::luaIsIntegerInArray);

	//addEvent(callback, delay, ...)
	lua_register(m_luaState, "addEvent", LuaScriptInterface::luaAddEvent);

	//stopEvent(eventid)
	lua_register(m_luaState, "stopEvent", LuaScriptInterface::luaStopEvent);

	//addPlayerBan(playerName = 0xFFFFFFFF[, length = 0[, admin = 0[, comment = "No comment"]]])
	lua_register(m_luaState, "addPlayerBan", LuaScriptInterface::luaAddPlayerBan);

	//addAccountBan(accounNumber = 0xFFFFFFFF[, length = 0[, admin = 0[, comment = "No comment"]]])
	lua_register(m_luaState, "addAccountBan", LuaScriptInterface::luaAddAccountBan);

	//addIPBan(ip[, mask = 0xFFFFFFFF[, length = 0[, admin = 0[, comment = "No comment"]]]])
	lua_register(m_luaState, "addIPBan", LuaScriptInterface::luaAddIPBan);

	//removePlayerBan(playerName)
	lua_register(m_luaState, "removePlayerBan", LuaScriptInterface::luaRemovePlayerBan);

	//removeAccountBan(account)
	lua_register(m_luaState, "removeAccountBan", LuaScriptInterface::luaRemoveAccountBan);

	//removeIPBan(ip[, mask])
	lua_register(m_luaState, "removeIPBan", LuaScriptInterface::luaRemoveIPBan);

	//getPlayerBanList()
	lua_register(m_luaState, "getPlayerBanList", LuaScriptInterface::luaGetPlayerBanList);

	//getAccountBanList()
	lua_register(m_luaState, "getAccountBanList", LuaScriptInterface::luaGetAccountBanList);

	//getIPBanList()
	lua_register(m_luaState, "getIPBanList", LuaScriptInterface::luaGetIPBanList);

	//getPlayerByAccountNumber(account)
	lua_register(m_luaState, "getPlayerByAccountNumber", LuaScriptInterface::luaGetPlayerByAccountNumber);

	//getPlayerAccountId(cid)
	lua_register(m_luaState, "getPlayerAccountId", LuaScriptInterface::luaGetPlayerAccountId);

	//getAccountNumberByPlayerName(name)
	lua_register(m_luaState, "getAccountNumberByPlayerName", LuaScriptInterface::luaGetAccountNumberByPlayerName);

	//getIPByPlayerName(name)
	lua_register(m_luaState, "getIPByPlayerName", LuaScriptInterface::luaGetIPByPlayerName);

	//getPlayersByIPNumber(ip)
	lua_register(m_luaState, "getPlayersByIPAddress", LuaScriptInterface::luaGetPlayersByIPAddress);

	//getDataDir()
	lua_register(m_luaState, "getDataDir", LuaScriptInterface::luaGetDataDirectory);

	//doPlayerSetRate(cid, type, value)
	lua_register(m_luaState, "doPlayerSetRate", LuaScriptInterface::luaDoPlayerSetRate);

	//doPlayerSetVipLimit(cid, quantity)
	lua_register(m_luaState, "doPlayerSetVipLimit", LuaScriptInterface::luaDoPlayerSetVipLimit);

	//doPlayerSetDepotLimit(cid, quantity)
	lua_register(m_luaState, "doPlayerSetDepotLimit", LuaScriptInterface::luaDoPlayerSetDepotLimit);

	//isPzLocked(cid)
	lua_register(m_luaState, "isPzLocked", LuaScriptInterface::luaIsPzLocked);

	//doSaveServer(payHouses)
	lua_register(m_luaState, "doSaveServer", LuaScriptInterface::luaDoSaveServer);

	//doSetGameState(gameState)
	lua_register(m_luaState, "doSetGameState", LuaScriptInterface::luaDoSetGameState);

	//doReloadInfo(info)
	lua_register(m_luaState, "doReloadInfo", LuaScriptInterface::luaDoReloadInfo);

	//doRefreshMap()
	lua_register(m_luaState, "doRefreshMap", LuaScriptInterface::luaDoRefreshMap);

	//debugPrint(text)
	lua_register(m_luaState, "debugPrint", LuaScriptInterface::luaDebugPrint);

	//isMonsterName(name)
	lua_register(m_luaState, "isMonsterName", LuaScriptInterface::luaIsMonsterName);

	//isValidItemId(itemid)
	lua_register(m_luaState, "isValidItemId", LuaScriptInterface::luaIsValidItemId);

	//isNpcName(name)
	lua_register(m_luaState, "isNpcName", LuaScriptInterface::luaIsNpcName);

	//getMonsterParameter(name, key)
	lua_register(m_luaState, "getMonsterParameter", LuaScriptInterface::luaGetMonsterParameter);

	//getNpcParameterByName(name, key)
	lua_register(m_luaState, "getNpcParameterByName", LuaScriptInterface::luaGetNpcParameterByName);

	//getItemWeaponType(itemid)
	lua_register(m_luaState, "getItemWeaponType", LuaScriptInterface::luaGetItemWeaponType);

	//getItemAttack(itemid)
	lua_register(m_luaState, "getItemAttack", LuaScriptInterface::luaGetItemAttack);

	//getItemDefense(itemid)
	lua_register(m_luaState, "getItemDefense", LuaScriptInterface::luaGetItemDefense);

	//getItemExtraDef(itemid)
	lua_register(m_luaState, "getItemExtraDef", LuaScriptInterface::luaGetItemExtraDef);

	//getItemArmor(itemid)
	lua_register(m_luaState, "getItemArmor", LuaScriptInterface::luaGetItemArmor);

	//getItemWeaponTypeByUID(uid)
	lua_register(m_luaState, "getItemWeaponTypeByUID", LuaScriptInterface::luaGetItemWeaponTypeByUID);

	//getItemAttackByUID(uid)
	lua_register(m_luaState, "getItemAttackByUID", LuaScriptInterface::luaGetItemAttackByUID);

	//getItemDefenseByUID(uid)
	lua_register(m_luaState, "getItemDefenseByUID", LuaScriptInterface::luaGetItemDefenseByUID);

	//getItemExtraDefByUID(uid)
	lua_register(m_luaState, "getItemExtraDefByUID", LuaScriptInterface::luaGetItemExtraDefByUID);

	//getItemArmor(uid)
	lua_register(m_luaState, "getItemArmorByUID", LuaScriptInterface::luaGetItemArmorByUID);

	//bit operations for Lua, based on bitlib project release 24
	//bit.bnot, bit.band, bit.bor, bit.bxor, bit.lshift, bit.rshift
	luaL_register(m_luaState, "bit", LuaScriptInterface::luaBitReg);
	
	//db table
	luaL_register(m_luaState, "db", LuaScriptInterface::luaDatabaseTable);

	//result table
	luaL_register(m_luaState, "result", LuaScriptInterface::luaResultTable);
	
	//isGmInvisible(cid)
	lua_register(m_luaState, "isGmInvisible", LuaScriptInterface::luaIsGmInvisible);

	//doPlayerToogleGmInvisible(cid)
	lua_register(m_luaState, "doPlayerToogleGmInvisible", LuaScriptInterface::luaDoPlayerToogleGmInvisible);

	//doPlayerAddPremiumDays(cid, days)
	lua_register(m_luaState, "doPlayerAddPremiumDays", LuaScriptInterface::luaDoPlayerAddPremiumDays);

	//doPlayerRemovePremiumDays(cid, days)
	lua_register(m_luaState, "doPlayerRemovePremiumDays", LuaScriptInterface::luaDoPlayerRemovePremiumDays);

	//getFirstItemFromInventory(cid, id/name, <optional> reportError)
	lua_register(m_luaState, "getFirstItemFromInventory", LuaScriptInterface::luaGetFirstItemFromInventory);

	//getCreatureConditionInfo(cid, conditionType, <optional: default: 0> subId, <optional: default: CONDITIONID_DEFAULT> conditionId)
	lua_register(m_luaState, "getCreatureConditionInfo", LuaScriptInterface::luaGetCreatureConditionInfo);

	//getCreatureCondition(cid, conditionType, <optional: default: 0> subId, <optional: default: CONDITIONID_DEFAULT> conditionId)
	lua_register(m_luaState, "getCreatureCondition", LuaScriptInterface::luaGetCreatureCondition);

	//getPlayerModes(cid)
	lua_register(m_luaState, "getPlayerModes", LuaScriptInterface::luaGetPlayerModes);

	//doSavePlayer(cid)
	lua_register(m_luaState, "doSavePlayer", LuaScriptInterface::luaDoSavePlayer);
}

int LuaScriptInterface::internalGetPlayerInfo(lua_State *L, PlayerInfo_t info)
{
	uint32_t cid = popNumber(L);
	ScriptEnviroment* env = getScriptEnv();

	const Player* player = env->getPlayerByUID(cid);
	if(!player){
		std::stringstream error_str;
		error_str << "Player not found. info = " << info;
		reportErrorFunc(error_str.str());
		lua_pushboolean(L, false);
		return 1;
	}

	switch(info){
		case PlayerInfoAccess:
		{
			lua_pushnumber(L, player->getAccessLevel());
			return 1;
		}
		case PlayerInfoLevel:
		{
			lua_pushnumber(L, player->getLevel());
			return 1;
		}
		case PlayerInfoMagLevel:
		{
			lua_pushnumber(L, player->getMagicLevel());
			return 1;
		}
		case PlayerInfoMana:
		{
			lua_pushnumber(L, player->getMana());
			return 1;
		}
		case PlayerInfoMaxMana:
		{
			lua_pushnumber(L, player->getMaxMana());
			return 1;
		}
		case PlayerInfoMasterPos:
		{
			Position pos;
			pos = player->masterPos;
			pushPosition(L, pos, 0);
			return 1;
		}
		case PlayerInfoFood:
		{
			int32_t value = 0;

			Condition* condition = player->getCondition(CONDITION_REGENERATION, CONDITIONID_DEFAULT, 0);
			if(condition){
				value = condition->getTicks() / 1000;
			}

			lua_pushnumber(L, value);
			return 1;
		}
		case PlayerInfoVocation:
		{
			lua_pushnumber(L, player->getVocationId());
			return 1;
		}
		case PlayerInfoSoul:
		{
			lua_pushnumber(L, player->getPlayerInfo(PLAYERINFO_SOUL));
			return 1;
		}
		case PlayerInfoFreeCap:
		{
			lua_pushnumber(L, (int)player->getFreeCapacity());
			return 1;
		}
		case PlayerInfoGuildId:
		{
			lua_pushnumber(L, player->getGuildId());
			return 1;
		}
		case PlayerInfoGuildName:
		{
			if(player->getGuild()){
				lua_pushstring(L, player->getGuild()->getName().c_str());
			}
			else{
				lua_pushstring(L, "");
			}
			return 1;
			break;
		}
		case PlayerInfoGuildRank:
		{
			lua_pushstring(L, player->guildRank.c_str());
			return 1;
		}
		case PlayerInfoGuildNick:
		{
			lua_pushstring(L, player->guildNick.c_str());
			return 1;
		}
		case PlayerInfoGuildLevel:
		{
			lua_pushnumber(L, player->guildLevel);
			return 1;
		}
		case PlayerInfoSex:
		{
			lua_pushnumber(L, player->getSex());
			return 1;
		}
		case PlayerInfoTown:
		{
			lua_pushnumber(L, player->getTown());
			return 1;
		}
		case PlayerInfoGUID:
		{
			lua_pushnumber(L, player->getGUID());
			return 1;
		}
		case PlayerInfoPremiumDays:
		{
			lua_pushnumber(L, player->getPremiumDays());
			return 1;
		}
		case PlayerInfoSkullEndTime:
		{
			int32_t value = 0;
			#ifdef __SKULLSYSTEM__
			switch(player->getSkull()){
				case SKULL_RED:
					value = player->lastSkullTime + g_config.getNumber(ConfigManager::RED_SKULL_DURATION);
					break;

				default: break;
			}
			#endif

			lua_pushnumber(L, value);
			return 1;
		}

		case PlayerInfoUnjustKills:
		{
			lua_newtable(L);
			setField(L, "day", IOPlayer::instance()->getPlayerUnjustKillCount(player, UNJUST_KILL_PERIOD_DAY) );
			setField(L, "dayRedSkull", g_config.getNumber(ConfigManager::KILLS_PER_DAY_RED_SKULL) );

			setField(L, "week", IOPlayer::instance()->getPlayerUnjustKillCount(player, UNJUST_KILL_PERIOD_WEEK));
			setField(L, "weekRedSkull", g_config.getNumber(ConfigManager::KILLS_PER_WEEK_RED_SKULL) );

			setField(L, "month", IOPlayer::instance()->getPlayerUnjustKillCount(player, UNJUST_KILL_PERIOD_MONTH));
			setField(L, "monthRedSkull", g_config.getNumber(ConfigManager::KILLS_PER_MONTH_RED_SKULL) );
			return 1;
		}

		case PlayerInfoBalance:
		{
			lua_pushnumber(L, player->balance);
			return 1;
		}
		case PlayerInfoPzLock:
		{
			lua_pushboolean(L, player->isPzLocked());
			return 1;
		}
		case PlayerInfoPremium:
		{
			lua_pushboolean(L, player->isPremium());
			return 1;
		}
		case PlayerInfoLastLogin:
		{
			lua_pushnumber(L, player->getLastLoginSaved());
			return 1;
		}
		case PlayerInfoIp:
		{
			lua_pushnumber(L, player->getIP());
			return 1;
		}
		case PlayerInfoGmInvisible:
		{
			lua_pushboolean(L, player->isGmInvisible());
			return 1;
		}
		case PlayerInfoAccountId:
		{
			lua_pushnumber(L, player->getAccountId());
			return 1;
		}
		default:
		{
			std::string error_str = "Unknown player info. info = " + info;
			reportErrorFunc(error_str);
			lua_pushnumber(L, 0);
			return 1;
		}
	}
}

//getPlayer[Info](uid)
int LuaScriptInterface::luaGetPlayerFood(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoFood);
}

int LuaScriptInterface::luaGetPlayerAccess(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoAccess);
}

int LuaScriptInterface::luaGetPlayerLevel(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoLevel);
}

int LuaScriptInterface::luaGetPlayerMagLevel(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoMagLevel);
}

int LuaScriptInterface::luaGetPlayerMana(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoMana);
}

int LuaScriptInterface::luaGetPlayerMaxMana(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoMaxMana);
}

int LuaScriptInterface::luaGetPlayerVocation(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoVocation);
}

int LuaScriptInterface::luaGetPlayerMasterPos(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoMasterPos);
}

int LuaScriptInterface::luaGetPlayerSoul(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoSoul);
}

int LuaScriptInterface::luaGetPlayerFreeCap(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoFreeCap);
}

int LuaScriptInterface::luaGetPlayerGuildId(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoGuildId);
}

int LuaScriptInterface::luaGetPlayerGuildName(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoGuildName);
}

int LuaScriptInterface::luaGetPlayerGuildRank(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoGuildRank);
}

int LuaScriptInterface::luaGetPlayerGuildNick(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoGuildNick);
}

int LuaScriptInterface::luaGetPlayerGuildLevel(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoGuildLevel);
}

int LuaScriptInterface::luaGetPlayerSex(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoSex);
}

int LuaScriptInterface::luaGetPlayerTown(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoTown);}

int LuaScriptInterface::luaGetPlayerGUID(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoGUID);
}

int LuaScriptInterface::luaGetPlayerPremiumDays(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoPremiumDays);
}

int LuaScriptInterface::luaGetPlayerSkullEndTime(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoSkullEndTime);
}

int LuaScriptInterface::luaGetPlayerUnjustKills(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoUnjustKills);
}

int LuaScriptInterface::luaGetPlayerAccountBalance(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoBalance);
}

int LuaScriptInterface::luaIsPzLocked(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoPzLock);
}

int LuaScriptInterface::luaIsPremium(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoPremium);
}

int LuaScriptInterface::luaGetPlayerLastLogin(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoLastLogin);
}

int LuaScriptInterface::luaGetPlayerIp(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoIp);
}

int LuaScriptInterface::luaIsGmInvisible(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoGmInvisible);
}

int LuaScriptInterface::luaGetPlayerAccountId(lua_State *L)
{
	return internalGetPlayerInfo(L, PlayerInfoAccountId);
}
//

int LuaScriptInterface::luaGetPlayerSkullType(lua_State *L)
{
	//getPlayerSkullType(cid, <optional> viewer)
	#ifdef __SKULLSYSTEM__
	uint32_t viewer = 0;
	if(lua_gettop(L) > 1){
		viewer = popNumber(L);
	}

	uint32_t cid = popNumber(L);
	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(!player){
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	if(viewer == 0){
		lua_pushnumber(L, player->getSkull());
		return 1;
	}

	Player* viewingPlayer = env->getPlayerByUID(viewer);
	if(!viewingPlayer){
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushnumber(L, viewingPlayer->getSkullClient(player));
	#else
	lua_pushnumber(L, SKULL_NONE);
	#endif
	return 1;
}

int LuaScriptInterface::luaSetPlayerSkullType(lua_State *L)
{
	//setPlayerSkullType(cid, skull_type)
	#ifdef __SKULLSYSTEM__
	uint32_t skull = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);

	if(player){
		if(skull < SKULL_LAST){
			player->setSkull((Skulls_t)skull);
			g_game.updateCreatureSkull(player);
			lua_pushboolean(L, true);
		}
		else{
			reportErrorFunc("No valid skull type.");
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	#else
	reportErrorFunc("Skull system is not enabled.");
	lua_pushboolean(L, false);
	#endif
	return 1;
}

int LuaScriptInterface::luaGetPlayerFlagValue(lua_State *L)
{
	//getPlayerFlagValue(cid, flag)
	uint32_t flagindex = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		if(flagindex < PlayerFlag_LastFlag){
			lua_pushboolean(L, player->hasFlag((PlayerFlags)flagindex) ? true : false);
		}
		else{
			reportErrorFunc("No valid flag index.");
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}


int LuaScriptInterface::luaGetPlayerByNameWildcard(lua_State *L)
{
	//getPlayerByNameWildcard(name)
	std::string name = popString(L);
	Player* player = NULL;

	ScriptEnviroment* env = getScriptEnv();
	if(g_game.getPlayerByNameWildcard(name, player) == RET_NOERROR){
		uint32_t cid = env->addThing(player);
		lua_pushnumber(L, cid);
	}
	else{
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaPlayerLearnInstantSpell(lua_State *L)
{
	//playerLearnInstantSpell(cid, name)
	std::string spellName = popString(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(!player){
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	InstantSpell* spell = g_spells->getInstantSpellByName(spellName);
	if(!spell){
		std::string error_str = (std::string)"Spell \"" + spellName +  (std::string)"\" not found";
		reportErrorFunc(error_str);
		lua_pushboolean(L, false);
		return 1;
	}

	player->learnInstantSpell(spell->getName());
	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaCanPlayerLearnInstantSpell(lua_State *L)
{
	//canPlayerLearnInstantSpell(cid, name)
	std::string spellName = popString(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(!player){
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	InstantSpell* spell = g_spells->getInstantSpellByName(spellName);
	if(!spell){
		std::string error_str = (std::string)"Spell \"" + spellName +  (std::string)"\" not found";
		reportErrorFunc(error_str);
		lua_pushboolean(L, false);
		return 1;
	}

	if(!player->hasFlag(PlayerFlag_IgnoreSpellCheck)){
		if(player->getLevel() < spell->getLevel()){
			lua_pushboolean(L, false);
			return 1;
		}

		if(player->getMagicLevel() < spell->getMagicLevel()){
			lua_pushboolean(L, false);
			return 1;
		}
	}

	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaGetPlayerLearnedInstantSpell(lua_State *L)
{
	//getPlayerLearnedInstantSpell(cid, name)
	std::string spellName = popString(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(!player){
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	InstantSpell* spell = g_spells->getInstantSpellByName(spellName);
	if(!spell){
		std::string error_str = (std::string)"Spell \"" + spellName +  (std::string)"\" not found";
		reportErrorFunc(error_str);
		lua_pushboolean(L, false);
		return 1;
	}

	if(!player->hasLearnedInstantSpell(spellName)){
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaGetPlayerInstantSpellCount(lua_State *L)
{
	//getPlayerInstantSpellCount(cid)
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		lua_pushnumber(L, g_spells->getInstantSpellCount(player));
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetPlayerInstantSpellInfo(lua_State *L)
{
	//getPlayerInstantSpellInfo(cid, index)
	uint32_t index = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(!player){
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	InstantSpell* spell = g_spells->getInstantSpellByIndex(player, index);
	if(!spell){
		reportErrorFunc(getErrorDesc(LUA_ERROR_SPELL_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	lua_newtable(L);
	setField(L, "name", spell->getName());
	setField(L, "words", spell->getWords());
	setField(L, "level", spell->getLevel());
	setField(L, "mlevel", spell->getMagicLevel());
	setField(L, "mana", spell->getManaCost(player));
	setField(L, "manapercent", spell->getManaPercent());
	return 1;
}

int LuaScriptInterface::luaGetInstantSpellInfoByName(lua_State *L)
{
	//getInstantSpellInfoByName(cid, name)
	std::string spellName = popString(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(!player && cid != 0){
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	InstantSpell* spell = g_spells->getInstantSpellByName(spellName);
	if(!spell){
		reportErrorFunc(getErrorDesc(LUA_ERROR_SPELL_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	lua_newtable(L);
	setField(L, "name", spell->getName());
	setField(L, "words", spell->getWords());
	setField(L, "level", spell->getLevel());
	setField(L, "mlevel", spell->getMagicLevel());
	setField(L, "mana", (player != NULL ? spell->getManaCost(player) : 0));
	setField(L, "manapercent", spell->getManaPercent());
	return 1;
}

int LuaScriptInterface::luaGetInstantSpellWords(lua_State *L)
{
	//getInstantSpellWords(name)
	std::string spellName = popString(L);
	InstantSpell* spell = g_spells->getInstantSpellByName(spellName);
	if(!spell){
		reportErrorFunc(getErrorDesc(LUA_ERROR_SPELL_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushstring(L, spell->getWords().c_str());
	return 1;
}


int LuaScriptInterface::luaDoRemoveItem(lua_State *L)
{
	//doRemoveItem(uid, <optional> count)
	int32_t parameters = lua_gettop(L);

	int32_t count = -1;
	if(parameters > 1){
		count = popNumber(L);
	}

	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Item* item = env->getItemByUID(uid);
	if(!item){
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	ReturnValue ret = g_game.internalRemoveItem(item, count);
	if(ret != RET_NOERROR){
		lua_pushboolean(L, false);
		return 1;
	}

	env->removeItemByUID(uid);
	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaDoPlayerRemoveItem(lua_State *L)
{
	//doPlayerRemoveItem(cid, itemid, count, <optional> subtype, <optional> ignoreEquipped)
	int32_t parameters = lua_gettop(L);

	bool ignoreEquipped = false;
	if(parameters > 4){
		ignoreEquipped = popBoolean(L);
	}

	int32_t subType = -1;
	if(parameters > 3){
		subType = popNumber(L);
	}

	uint32_t count = popNumber(L);
	uint16_t itemId = (uint16_t)popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		if(g_game.removeItemOfType(player, itemId, count, subType, ignoreEquipped)){
			lua_pushboolean(L, true);
		}
		else{
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoFeedPlayer(lua_State *L)
{
	//doFeedPlayer(uid, food)
	uint32_t food = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		player->addDefaultRegeneration(food * 1000);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoSendCancel(lua_State *L)
{
	//doSendCancel(uid, text)
	std::string text = popString(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	const Player* player = env->getPlayerByUID(cid);
	if(player){
		player->sendCancel(text);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoSendDefaultCancel(lua_State *L)
{
	//doPlayerSendDefaultCancel(cid, ReturnValue)
	ReturnValue ret = (ReturnValue)popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	const Player* player = env->getPlayerByUID(cid);
	if(player){
		player->sendCancelMessage(ret);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}

	return 1;
}

int LuaScriptInterface::luaDoPlayerSetIdleTime(lua_State *L)
{
	//doPlayerSetIdleTime(cid, time, warned)
	bool warned = (int32_t(popNumber(L, true)) > 0);
	uint32_t time = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		player->setIdleTime(time, warned);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}

	return 1;
}

int LuaScriptInterface::luaDoTeleportThing(lua_State *L)
{
	//doTeleportThing(uid, newpos)
	PositionEx pos;
	popPosition(L, pos);
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Thing* tmp = env->getThingByUID(uid);
	if(tmp){
		if(g_game.internalTeleport(tmp, pos) == RET_NOERROR){
			lua_pushboolean(L, true);
		}
		else{
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_THING_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoTransformItem(lua_State *L)
{
	//doTransformItem(uid, toitemid, <optional> count/subtype)
	int32_t parameters = lua_gettop(L);

	int32_t count = -1;
	if(parameters > 2){
		count = popNumber(L);
	}

	uint16_t toId = (uint16_t)popNumber(L);
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Item* item = env->getItemByUID(uid);
	if(!item){
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	const ItemType& it = Item::items[toId];
	if(it.stackable && count > 100){
		reportErrorFunc("Stack count cannot be higher than 100.");
		count = 100;
	}

	Item* newItem = g_game.transformItem(item, toId, count);

	if(item->isRemoved()){
		env->removeItemByUID(uid);
	}

	if(newItem && newItem != item){
		env->insertThing(uid, newItem);
	}

	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaDoCreatureSay(lua_State *L)
{
	//doCreatureSay(cid, text, type)
	uint32_t type = popNumber(L);
	std::string text = popString(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(creature){
		g_game.internalCreatureSay(creature, (SpeakClasses)type, text);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoSendMagicEffect(lua_State *L)
{
	//doSendMagicEffect(pos, type[, player])
	ScriptEnviroment* env = getScriptEnv();

	uint32_t parameters = lua_gettop(L);
	SpectatorVec list;
	if(parameters > 2){
		uint32_t cid = popNumber(L);
		Player* player = env->getPlayerByUID(cid);
		if(player){
			list.push_back(player);
		}
	}

	uint32_t type = popNumber(L);
	PositionEx pos;
	popPosition(L, pos);

	if(pos.x == 0xFFFF){
		pos = env->getRealPos();
	}

	if(!list.empty()){
		g_game.addMagicEffect(list, pos, type);
	}
	else{
		g_game.addMagicEffect(pos, type);
	}
	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaDoChangeTypeItem(lua_State *L)
{
	//doChangeTypeItem(uid, newtype)
	int32_t subtype = (int32_t)popNumber(L);
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Item* item = env->getItemByUID(uid);
	if(!item){
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	Item* newItem = g_game.transformItem(item, item->getID(), subtype);

	if(item->isRemoved()){
		env->removeItemByUID(uid);
	}

	if(newItem && newItem != item){
		env->insertThing(uid, newItem);
	}

	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaDoPlayerAddInFightTicks(lua_State *L)
{
	//doPlayerAddInFightTicks(cid, ticks, <optional: default: false> pzLock)
	int32_t parameters = lua_gettop(L);

	bool doPzLock = false;
	if(parameters > 2){
		doPzLock = popBoolean(L);
	}

	uint32_t ticks = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	Player* player = env->getPlayerByUID(cid);
	if(player){
		player->addInFightTicks(ticks, doPzLock);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}

	return 1;
}

int LuaScriptInterface::luaDoPlayerAddSkillTry(lua_State *L)
{
	//doPlayerAddSkillTry(cid, skillid, n, <optional: default: 0> useMultiplier)
	int32_t parameters = lua_gettop(L);

	bool useMultiplier = false;
	if(parameters > 3){
		useMultiplier = (popNumber(L, true) >= 1);
	}

	uint32_t n = popNumber(L);
	uint32_t skillid = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		player->addSkillAdvance((skills_t)skillid, n, useMultiplier);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoPlayerAddManaSpent(lua_State *L)
{
	//doPlayerAddManaSpent(cid, mana, <optional: default: 0> useMultiplier)
	int32_t parameters = lua_gettop(L);

	bool useMultiplier = false;
	if(parameters > 2){
		useMultiplier = (popNumber(L, true) >= 1);
	}

	uint64_t mana = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		player->addManaSpent(mana, useMultiplier);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoCreatureAddHealth(lua_State *L)
{
	//doCreatureAddHealth(cid, health, <optional: default: true> filter)
	bool filter = true;
	if(lua_gettop(L) >= 3){
		filter = popBoolean(L);
	}

	int32_t healthChange = (int32_t)popNumber(L);
	uint32_t cid = popNumber(L);
	ScriptEnviroment* env = getScriptEnv();
	if(Creature* creature = env->getCreatureByUID(cid)){
		if(healthChange >= 0){
			creature->changeHealth(healthChange);
		}
		else{
			if(filter){
				creature->drainHealth(NULL, COMBAT_LIFEDRAIN, -healthChange);
			}
			else{
				g_game.combatChangeHealth(COMBAT_UNDEFINEDDAMAGE, NULL, creature, healthChange);
			}
		}
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoPlayerAddMana(lua_State *L)
{
	//doPlayerAddMana(cid, mana, <optional: default: true> withoutAnimation)
	//if you choose UtamoEffect and mana < 0 then it will show the animation
	bool withoutAnimation = true;
	if(lua_gettop(L) >= 3)
		withoutAnimation = popBoolean(L);

	int32_t manaChange = (int32_t)popNumber(L);
	ScriptEnviroment* env = getScriptEnv();
	if(Player* player = env->getPlayerByUID(popNumber(L))){
		if(manaChange >= 0 || withoutAnimation){
			player->changeMana(manaChange);
		}
		else{
			g_game.combatChangeMana(NULL, player, manaChange);
		}
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoPlayerAddItem(lua_State *L)
{
	//doPlayerAddItem(cid, itemid, <optional: default: 1> count/subtype, <optional: default: true> canDropOnMap)
	//doPlayerAddItem(cid, itemid, <optional: default: 1> count, <optional: default: true> canDropOnMap, <optional: default: 1>subtype)
	int32_t parameters = lua_gettop(L);

	int32_t subType = 1;
	if(parameters > 4){
		subType = popNumber(L);
	}

	bool canDropOnMap = true;
	if(parameters > 3){
		canDropOnMap = popBoolean(L);
	}

	uint32_t count = 1;
	if(parameters > 2){
		count = popNumber(L);
	}

	uint32_t itemId = (uint32_t)popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	Player* player = env->getPlayerByUID(cid);
	if(!player){
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	const ItemType& it = Item::items[itemId];

	int32_t itemCount = 1;
	if(parameters > 4){
		//subtype already supplied, count then is the amount
		itemCount = count;
	}
	
	else if(it.hasSubType())
		{
			if(it.stackable)
				itemCount = (int32_t)std::ceil((float)count / 100);
			

			subType = count;
		}
		else{
			itemCount = std::max((int32_t)1, (int32_t)count);
		}
	

	while(itemCount > 0){
		int32_t stackCount = std::min((int32_t)100, (int32_t)subType);
		Item* newItem = Item::CreateItem(itemId, stackCount);

		if(!newItem){
			reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
			lua_pushboolean(L, false);
			return 1;
		}

		if(it.stackable){
			subType -= stackCount;
		}
		ReturnValue ret = g_game.internalPlayerAddItem(player, newItem, canDropOnMap);

		if(ret != RET_NOERROR){
			delete newItem;
			lua_pushboolean(L, false);
			return 1;
		}

		--itemCount;
		if(itemCount == 0){
			if(newItem->getParent()){
				uint32_t uid = env->addThing(newItem);
				lua_pushnumber(L, uid);
				return 1;
			}
			else{
				//stackable item stacked with existing object, newItem will be released
				lua_pushnumber(L, 0);
				return 1;
			}
		}
	}

	lua_pushnumber(L, 0);
	return 1;
}

int LuaScriptInterface::luaDoPlayerAddItemEx(lua_State *L)
{
	//doPlayerAddItemEx(cid, uid, <optional: default: false> canDropOnMap, <optional> slot)
	int32_t parameters = lua_gettop(L);

	uint32_t slot = SLOT_WHEREEVER;
	if(parameters > 3){
		slot = popNumber(L);
		if(slot >= SLOT_LAST){ //depot is not a real slot, it IS >=
			reportErrorFunc("Invalid slot.");
			lua_pushboolean(L, false);
			return 1;
		}
	}

	bool canDropOnMap = false;
	if(parameters > 2){
		canDropOnMap = popBoolean(L);
	}

	uint32_t uid = (uint32_t)popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	Player* player = env->getPlayerByUID(cid);
	if(!player){
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	Item* item = env->getItemByUID(uid);
	if(!item){
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	if(item->getParent() != VirtualCylinder::virtualCylinder){
		reportErrorFunc("Item already has a parent");
		lua_pushboolean(L, false);
		return 1;
	}

	ReturnValue ret = g_game.internalPlayerAddItem(player, item, canDropOnMap, (slots_t)slot);
	lua_pushnumber(L, ret);
	return 1;
}

int LuaScriptInterface::luaDoTileAddItemEx(lua_State *L)
{
	//doTileAddItemEx(pos, uid)
	uint32_t uid = (uint32_t)popNumber(L);

	PositionEx pos;
	popPosition(L, pos);

	ScriptEnviroment* env = getScriptEnv();
	Tile* tile = g_game.getTile(pos.x, pos.y, pos.z);
	if(!tile){
		std::stringstream ss;
		ss << pos << " " << getErrorDesc(LUA_ERROR_TILE_NOT_FOUND);
		reportErrorFunc(ss.str().c_str());
		lua_pushboolean(L, false);
		return 1;
	}

	Item* item = env->getItemByUID(uid);

	if(!item){
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	if(item->getParent() != VirtualCylinder::virtualCylinder){
		reportErrorFunc("Item already has a parent");
		lua_pushboolean(L, false);
		return 1;
	}

	ReturnValue ret = g_game.internalAddItem(tile, item);
	lua_pushnumber(L, ret);
	return 1;
}

int LuaScriptInterface::luaAddContainerItemEx(lua_State *L)
{
	//doAddContainerItemEx(uid, virtuid)
	uint32_t virtuid = (uint32_t)popNumber(L);
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	Container* container = env->getContainerByUID(uid);
	if(container){
		Item* item = env->getItemByUID(virtuid);

		if(!item){
			reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
			lua_pushboolean(L, false);
			return 1;
		}

		if(item->getParent() != VirtualCylinder::virtualCylinder){
			reportErrorFunc("Item already has a parent");
			lua_pushboolean(L, false);
			return 1;
		}

		ReturnValue ret = RET_NOERROR;
		ret = g_game.internalAddItem(container, item);

		if(ret == RET_NOERROR){
			env->removeTempItem(item);
		}

		lua_pushnumber(L, ret);
		return 1;
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CONTAINER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}
}

int LuaScriptInterface::luaDoRelocate(lua_State *L)
{
	//doRelocate(pos, posTo, <optional: default: false> moveUnmoveable, <optional: default: 0> maxAmount)
	/*Moves all objects from pos to posTo (limited by the value of maxAmount - so only the first maxAmount items are
	  relocated even if there are too many items at the tile - it is important to prevent lag if there are too many items)
	  It also move ALL creatures from pos to posTo (it doesn't matter how many creatures there are in the tile). */
	int32_t parameters = lua_gettop(L);

	int32_t maxAmount = 0;
	bool moveUnmoveable = false;
	if(parameters > 3){
		maxAmount = popNumber(L);
	}
	if(parameters > 2){
		moveUnmoveable = popBoolean(L);
	}

	PositionEx toPos;
	popPosition(L, toPos);

	PositionEx fromPos;
	popPosition(L, fromPos);

	Tile* fromTile = g_game.getTile(fromPos.x, fromPos.y, fromPos.z);
	if(!fromTile){
		reportErrorFunc(getErrorDesc(LUA_ERROR_TILE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	Tile* toTile = g_game.getTile(toPos.x, toPos.y, toPos.z);
	if(!toTile){
		std::stringstream ss;
		ss << toPos << " " << getErrorDesc(LUA_ERROR_TILE_NOT_FOUND);
		reportErrorFunc(ss.str().c_str());
		lua_pushboolean(L, false);
		return 1;
	}

	if(fromTile != toTile){
		int32_t thingCount = fromTile->getThingCount();
		if (maxAmount > 0){
			thingCount = std::min(thingCount, maxAmount);
		}
		for(int32_t i = thingCount - 1; i >= 0; --i){
			Thing* thing = fromTile->__getThing(i);
			if(thing){
				if(Item* item = thing->getItem()){
					const ItemType& it = Item::items[item->getID()];
					if(!it.isGroundTile() && !it.alwaysOnTop && !it.isMagicField()){
						uint32_t flags = moveUnmoveable ? FLAG_IGNORENOTMOVEABLE : 0;
						g_game.internalTeleport(item, toPos, flags);
					}
				}
				else if(Creature* creature = thing->getCreature()){
					if(Position::areInRange<1,1>(fromPos, toPos)){
						g_game.internalMoveCreature(creature, fromTile, toTile, FLAG_NOLIMIT);
					}
					else{
						g_game.internalTeleport(creature, toPos);
					}
				}
			}
		}
		//makes sure that every creature at the tile has been moved even if maxAmount was reached
		if (thingCount > maxAmount){
			if (CreatureVector* creatures = (fromTile->getCreatures())){
				for(CreatureVector::iterator cit = creatures->begin(); cit != creatures->end(); ++cit){
					if(Position::areInRange<1,1>(fromPos, toPos)){
						g_game.internalMoveCreature(*cit, fromTile, toTile, FLAG_NOLIMIT);
					}
					else{
						g_game.internalTeleport(*cit, toPos);
					}
				}
			}
		}
	}

	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaDoPlayerSendTextMessage(lua_State *L)
{
	//doPlayerSendTextMessage(cid, MessageClasses, message)
	std::string text = popString(L);
	uint32_t messageClass = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	const Player* player = env->getPlayerByUID(cid);
	if(!player){
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	player->sendTextMessage((MessageClasses)messageClass, text);
	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaDoSendAnimatedText(lua_State *L)
{
	//doSendAnimatedText(pos, text, color)
	uint32_t color = popNumber(L);
	std::string text = popString(L);
	PositionEx pos;
	popPosition(L, pos);

	ScriptEnviroment* env = getScriptEnv();

	SpectatorVec list;
	SpectatorVec::iterator it;

	if(pos.x == 0xFFFF){
		pos = env->getRealPos();
	}

	g_game.addAnimatedText(pos, color, text);
	return 1;
}

int LuaScriptInterface::luaGetPlayerSkill(lua_State *L)
{
	//getPlayerSkill(cid, skillid)
	uint32_t skillid = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	const Player* player = env->getPlayerByUID(cid);
	if(player){
		if(skillid <= SKILL_LAST){
			uint32_t value = player->skills[skillid][SKILL_LEVEL];
			lua_pushnumber(L, value);
		}
		else{
			reportErrorFunc("No valid skillId");
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetPlayerLossPercent(lua_State *L)
{
	//getPlayerLossPercent(cid, lossType)
	uint8_t lossType = (uint8_t)popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	const Player* player = env->getPlayerByUID(cid);
	if(player){
		if(lossType <= LOSS_LAST){
			uint32_t value = player->getLossPercent((lossTypes_t)lossType);
			lua_pushnumber(L, value);
		}
		else{
			reportErrorFunc("No valid lossType");
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoPlayerSetLossPercent(lua_State *L)
{
	//doPlayerSetLossPercent(cid, lossType, newPercent)
	uint32_t newPercent = popNumber(L);
	uint8_t lossType = (uint8_t)popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		if(lossType <= LOSS_LAST){
			if(newPercent <= 100){
				player->setLossPercent((lossTypes_t)lossType, newPercent);
				lua_pushboolean(L, true);
			}
			else{
				reportErrorFunc("lossPercent value higher than 100");
				lua_pushboolean(L, false);
			}
		}
		else{
			reportErrorFunc("No valid lossType");
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoSetCreatureDropLoot(lua_State *L)
{
	//doSetCreatureDropLoot(cid, doDrop)
	bool doDrop = popBoolean(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(creature){
		creature->setDropLoot(doDrop);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}

	return 1;
}

int LuaScriptInterface::luaSendOutfitWindow(lua_State* L)
{
	// sendOutfitWindow()
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if (player) {
		player->sendOutfitWindow();
		lua_pushboolean(L, true);
	} else {
		lua_pushnil(L);
	}
	
	return 1;
}

int LuaScriptInterface::luaDoShowTextWindow(lua_State *L)
{
	//doShowTextWindow(uid, maxlen, canWrite)
	popNumber(L);
	popNumber(L);
	popNumber(L);
	reportErrorFunc("Deprecated function.");
	lua_pushboolean(L, false);
	return 1;
}

int LuaScriptInterface::luaDoShowTextDialog(lua_State *L)
{
	//doShowTextDialog(cid, itemid, text)
	std::string text = popString(L);
	uint32_t itemId = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		player->setWriteItem(NULL, 0);
		player->sendTextWindow(itemId, text);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetTownIdByName(lua_State *L)
{
	//getTownIdByName(townName)
	std::string townName = popString(L);

	Town* town = Towns::getInstance().getTown(townName);
	if(town){
		lua_pushnumber(L, town->getTownID());
	}
	else{
		lua_pushnumber(L, 0);
	}
	return 1;
}

int LuaScriptInterface::luaGetTownNameById(lua_State *L)
{
	//getTownNameById(townId)
	uint32_t townId = popNumber(L);

	Town* town = Towns::getInstance().getTown(townId);
	if(town){
		std::string townName = town->getName();
		lua_pushstring(L, townName.c_str());
	}
	else{
		lua_pushnumber(L, 0);
	}
	return 1;
}

int LuaScriptInterface::luaGetTownTemplePosition(lua_State *L)
{
	//getTownTemplePosition(townId)
	uint32_t townId = popNumber(L);

	Town* town = Towns::getInstance().getTown(townId);
	if(town){
		pushPosition(L, town->getTemplePosition());
	}
	else{
		reportErrorFunc("Could not find the town.");
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetItemRWInfo(lua_State *L)
{
	//getItemRWInfo(uid)
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	const Item* item = env->getItemByUID(uid);
	if(item){
		uint32_t rwflags = 0;
		if(item->isReadable())
			rwflags |= 1;

		if(item->canWriteText()){
			rwflags |= 2;
		}

		lua_pushnumber(L, rwflags);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}


int	LuaScriptInterface::luaGetThingDefaultDescription(lua_State *L)
{
	//getThingDefaultDescription(uid, lookDistance, <optional: default: false> canSeeXRay>)
	int32_t parameters = lua_gettop(L);

	bool canSeeXRay = false;
	if(parameters > 2){
		canSeeXRay = popBoolean(L);
	}

	uint32_t lookDistance = popNumber(L);
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Thing* thing = env->getThingByUID(uid);
	if(thing){
		std::stringstream ret;
		ret << thing->getDescription(lookDistance);
		if (canSeeXRay)
			ret << thing->getXRayDescription();
		lua_pushstring(L, ret.str().c_str());
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_THING_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}


int LuaScriptInterface::luaGetItemTypeDefaultDescription(lua_State *L)
{
	//getThingDefaultDescription(itemid, <optional> count)
	std::stringstream ret;

	int32_t parameters = lua_gettop(L);
	uint32_t count = 1;
	if(parameters > 1){
		count = std::max(popNumber(L), uint32_t(1));
	}

	uint32_t itemid = popNumber(L);
	const ItemType& it = Item::items[itemid];
	ret << it.getDescription((uint8_t) count);
	lua_pushstring(L, ret.str().c_str());
	return 1;
}

int LuaScriptInterface::luaGetItemSpecialDescription(lua_State *L)
{
	//getItemSpecialDescription(uid)
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	const Item* item = env->getItemByUID(uid);
	if(item){
		lua_pushstring(L, item->getSpecialDescription().c_str());
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoDecayItem(lua_State *L)
{
	//doDecayItem(uid)
	//Note: to stop decay set decayTo = 0 in items.otb
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Item* item = env->getItemByUID(uid);
	if(!item){
		lua_pushboolean(L, false);
		return 1;
	}

	g_game.startDecay(item);
	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaGetThingFromPos(lua_State *L)
{
	//Consider using getTileItemById/getTileItemByType/getTileThingByPos/getTopCreature instead.

	//getThingFromPos(pos)
	//Note:
	//	stackpos = 255. Get the top thing(item moveable or creature)
	//	stackpos = 254. Get MagicFieldtItem
	//	stackpos = 253. Get Creature

	PositionEx pos;
	popPosition(L, pos);

	ScriptEnviroment* env = getScriptEnv();

	Tile* tile = g_game.getMap()->getTile(pos);
	Thing* thing = NULL;

	if(tile){
		if(pos.stackpos == 255){
			thing = tile->getTopCreature();

			if(thing == NULL){
				Item* item = tile->getTopDownItem();
				if(item && !item->isNotMoveable())
					thing = item;
			}
		}
		else if(pos.stackpos == 254){
			thing = tile->getFieldItem();
		}
		else if(pos.stackpos == 253){
			thing = tile->getTopCreature();
		}
		else{
			thing = tile->__getThing(pos.stackpos);
		}

		if(thing){
			uint32_t thingid = env->addThing(thing);
			pushThing(L, thing, thingid);
		}
		else{
			pushThing(L, NULL, 0);
		}

		return 1;
	}
	else{
		std::stringstream ss;
		ss << pos << " " << getErrorDesc(LUA_ERROR_TILE_NOT_FOUND);
		reportErrorFunc(ss.str().c_str());
		pushThing(L, NULL, 0);
		return 1;
	}
}

int LuaScriptInterface::luaGetTileItemById(lua_State *L)
{
	//getTileItemById(pos, itemId, <optional> subType)
	ScriptEnviroment* env = getScriptEnv();

	uint32_t parameters = lua_gettop(L);

	int32_t subType = -1;
	if(parameters > 2){
		subType = (int32_t)popNumber(L);
	}

	int32_t itemId = (int32_t)popNumber(L);

	PositionEx pos;
	popPosition(L, pos);

	Tile* tile = g_game.getTile(pos.x, pos.y, pos.z);
	if(!tile){
		pushThing(L, NULL, 0);
		return 1;
	}

	Item* item = g_game.findItemOfType(tile, itemId, false, subType);
	if(!item){
		pushThing(L, NULL, 0);
		return 1;
	}

	uint32_t uid = env->addThing(item);
	pushThing(L, item, uid);

	return 1;
}

int LuaScriptInterface::luaGetTileItemByType(lua_State *L)
{
	//getTileItemByType(pos, type)

	ScriptEnviroment* env = getScriptEnv();

	uint32_t rType = (uint32_t)popNumber(L);

	if(rType >= ITEM_TYPE_LAST){
		reportErrorFunc("Not a valid item type");
		pushThing(L, NULL, 0);
		return 1;
	}

	PositionEx pos;
	popPosition(L, pos);

	Tile* tile = g_game.getTile(pos.x, pos.y, pos.z);
	if(!tile){
		pushThing(L, NULL, 0);
		return 1;
	}

	bool notFound = false;
	switch((ItemTypes_t) rType){
		case ITEM_TYPE_TELEPORT:
			if(!tile->hasFlag(TILESTATE_TELEPORT)){
				notFound = true;
			}
			break;
		case ITEM_TYPE_MAGICFIELD:
			if(!tile->hasFlag(TILESTATE_MAGICFIELD)){
				notFound = true;
			}
			break;
		case ITEM_TYPE_MAILBOX:
			if(!tile->hasFlag(TILESTATE_MAILBOX)){
				notFound = false;
			}
			break;
		case ITEM_TYPE_TRASHHOLDER:
			if(!tile->hasFlag(TILESTATE_TRASHHOLDER)){
				notFound = false;
			}
			break;
		case ITEM_TYPE_BED:
			if(!tile->hasFlag(TILESTATE_BED)){
				notFound = true;
			}
			break;

		default:
			break;
	}

	if(!notFound){
		for(uint32_t i = 0; i < tile->getThingCount(); ++i){
			if(Item* item = tile->__getThing(i)->getItem()){
				const ItemType& it = Item::items[item->getID()];
				if(it.type == (ItemTypes_t) rType){
					uint32_t uid = env->addThing(item);
					pushThing(L, item, uid);
					return 1;
				}
			}
		}
	}

	pushThing(L, NULL, 0);
	return 1;
}

int LuaScriptInterface::luaGetTileThingByPos(lua_State *L)
{
	//getTileThingByPos(pos)

	PositionEx pos;
	popPosition(L, pos);

	ScriptEnviroment* env = getScriptEnv();

	Tile* tile = g_game.getTile(pos.x, pos.y, pos.z);
	if(!tile){
		if(pos.stackpos == -1){
			lua_pushnumber(L, -1);
			return 1;
		}
		else{
			pushThing(L, NULL, 0);
			return 1;
		}
	}

	if(pos.stackpos == -1){
		lua_pushnumber(L, tile->getThingCount());
		return 1;
	}

	Thing* thing = tile->__getThing(pos.stackpos);
	if(!thing){
		pushThing(L, NULL, 0);
		return 1;
	}

	uint32_t uid = env->addThing(thing);
	pushThing(L, thing, uid);
	return 1;
}

int LuaScriptInterface::luaGetTileThingByTopOrder(lua_State *L)
{
	//getTileThingByTopOrder(pos, topOrder)

	uint32_t topOrder = popNumber(L);
	PositionEx pos;
	popPosition(L, pos);

	ScriptEnviroment* env = getScriptEnv();

	Tile* tile = g_game.getTile(pos.x, pos.y, pos.z);
	if(!tile){
		pushThing(L, NULL, 0);
		return 1;
	}

	Thing* thing = tile->getItemByTopOrder(topOrder);
	if(!thing){
		pushThing(L, NULL, 0);
		return 1;
	}

	uint32_t uid = env->addThing(thing);
	pushThing(L, thing, uid);
	return 1;
}

int LuaScriptInterface::luaGetTileStackItemsSize(lua_State *L)
{
	//getTileStackItemsSize(pos)
	PositionEx pos;
	popPosition(L, pos);

	Tile* tile = g_game.getTile(pos.x, pos.y, pos.z);
	if(!tile){
		reportErrorFunc(getErrorDesc(LUA_ERROR_TILE_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const TileItemVector* items = tile->getItemList();
	if (items){
		lua_pushnumber(L, items->size());
	}
	else{
		lua_pushnumber(L, 0);
	}

	return 1;
}

int LuaScriptInterface::luaGetAllCreatures(lua_State *L)
{
	//getAllCreatures(pos, <optional> flag)
	//bits for flag:
	//32 = GET NPCS
	//16 = GET INVISIBLE MONSTERS
	//8 = GET VISIBLE MONSTERS
	//4 = GET GMs AT INVISIBLE STATE
	//2 = GET PLAYERS WITH INVISIBILITY FLAG
	//1 = GET REGULAR PLAYERS (THOSE WITHOUT ANY KIND OF INVISIBILITY FLAG)

	uint32_t flag = 9; //by default shows visible monsters and players without any kind of invisibility flag

	uint32_t parameters = lua_gettop(L);
	if (parameters >= 2)
		flag = popNumber(L);

	PositionEx pos;
	popPosition(L, pos);

	ScriptEnviroment* env = getScriptEnv();

	Tile* tile = g_game.getTile(pos.x, pos.y, pos.z);
	if(!tile){
		reportErrorFunc(getErrorDesc(LUA_ERROR_TILE_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	lua_newtable(L);
	uint32_t index = 1;
	CreatureVector* creatures = tile->getCreatures();
	if (creatures){
	for(CreatureVector::const_iterator cit = creatures->begin(); cit != creatures->end(); ++cit){
		bool add = false;
		if(Player *p = (*cit)->getPlayer()) {
			if(((flag & 1) == 1 && !p->hasFlag(PlayerFlag_CannotBeSeen) && !p->isGmInvisible())||
				((flag & 2) == 2 && p->hasFlag(PlayerFlag_CannotBeSeen))||
				((flag & 4) == 4 && p->isGmInvisible())){
				add = true;
			}
		}
		else if (Monster *m = (*cit)->getMonster()){
			if (((flag & 8) == 8 && !m->hasCondition(CONDITION_INVISIBLE)) || (flag & 16) == 16){
				add = true;
			}
		}
		else if ((*cit)->getNpc() && (flag & 32) == 32){
			add = true;
		}

		if(add){
			lua_pushnumber(L, index);
			env->addThing(*cit);
			lua_pushnumber(L, (*cit)->getID());
			lua_settable(L, -3);
			index++;
		}
	}
	}
	return 1;
}

int LuaScriptInterface::luaGetTopCreature(lua_State *L)
{
	//getTopCreature(pos)

	PositionEx pos;
	popPosition(L, pos);

	ScriptEnviroment* env = getScriptEnv();

	Tile* tile = g_game.getTile(pos.x, pos.y, pos.z);
	if(!tile){
		pushThing(L, NULL, 0);
		return 1;
	}

	Thing* thing = tile->getTopCreature();
	if(!thing || !thing->getCreature()){
		pushThing(L, NULL, 0);
		return 1;
	}

	uint32_t uid = env->addThing(thing);
	pushThing(L, thing, uid);
	return 1;
}

int LuaScriptInterface::luaGetWaypointPositionByName(lua_State *L)
{
	//getWaypointPositionByName(name)

	std::string name = popString(L);

	if(Waypoint_ptr waypoint = g_game.getMap()->waypoints.getWaypointByName(name))
		pushPosition(L, waypoint->pos);
	else
		lua_pushboolean(L, false);

	return 1;
}

int LuaScriptInterface::luaDoCreateItem(lua_State *L)
{
	//doCreateItem(itemid, <optional> type/count, pos)
	//Returns uid of the created item, only works on tiles.

	uint32_t parameters = lua_gettop(L);

	PositionEx pos;
	popPosition(L, pos);

	uint32_t count = 1;
	if(parameters > 2){
		count = popNumber(L);
	}

	uint32_t itemId = (uint32_t)popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Tile* tile = g_game.getTile(pos.x, pos.y, pos.z);
	if(!tile){
		std::stringstream ss;
		ss << pos << " " << getErrorDesc(LUA_ERROR_TILE_NOT_FOUND);
		reportErrorFunc(ss.str().c_str());
		lua_pushboolean(L, false);
		return 1;
	}

	const ItemType& it = Item::items[itemId];

	int32_t itemCount = 1;
	int32_t subType = 1;

	if(it.hasSubType()){

		if(it.stackable){
			itemCount = (int32_t)std::ceil((float)count / 100);
		}

		subType = count;
	}
	else{
		itemCount = std::max((int32_t)1, (int32_t)count);
	}

	while(itemCount > 0){
		int32_t stackCount = std::min((int32_t)100, (int32_t)subType);
		Item* newItem = Item::CreateItem(itemId, stackCount);

		if(!newItem){
			reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
			lua_pushboolean(L, false);
			return 1;
		}

		if(it.stackable){
			subType -= stackCount;
		}
		ReturnValue ret = g_game.internalAddItem(tile, newItem, INDEX_WHEREEVER, FLAG_NOLIMIT);

		if(ret != RET_NOERROR){
			delete newItem;
			lua_pushboolean(L, false);
			return 1;
		}

		--itemCount;
		if(itemCount == 0){
			if(newItem->getParent()){
				uint32_t uid = env->addThing(newItem);
				lua_pushnumber(L, uid);
				return 1;
			}
			else{
				//stackable item stacked with existing object, newItem will be released
				lua_pushnumber(L, 0);
				return 1;
			}
		}
	}

	lua_pushnumber(L, 0);
	return 1;
}

int LuaScriptInterface::luaDoCreateItemEx(lua_State *L)
{
	//doCreateItemEx(itemid, <optional> count/subtype)
	//Returns uid of the created item

	int32_t parameters = lua_gettop(L);

	uint32_t count = 1;
	if(parameters > 1){
		count = popNumber(L);
	}

	uint32_t itemId = (uint32_t)popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	const ItemType& it = Item::items[itemId];
	if(it.stackable && count > 100){
		reportErrorFunc("Stack count cannot be higher than 100.");
		count = 100;
	}

	Item* newItem = Item::CreateItem(itemId, count);
	if(!newItem){
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	newItem->setParent(VirtualCylinder::virtualCylinder);
	env->addTempItem(env, newItem);

	uint32_t uid = env->addThing(newItem);
	lua_pushnumber(L, uid);
	return 1;
}

int LuaScriptInterface::luaDoCreateTeleport(lua_State *L)
{
	//doCreateTeleport(teleportID, positionToGo, createPosition)
	PositionEx createPos;
	popPosition(L, createPos);
	PositionEx toPos;
	popPosition(L, toPos);
	uint32_t itemId = (uint32_t)popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Tile* tile = g_game.getMap()->getTile(createPos);
	if(!tile){
		std::stringstream ss;
		ss << createPos << " " << getErrorDesc(LUA_ERROR_TILE_NOT_FOUND);
		reportErrorFunc(ss.str().c_str());
		lua_pushboolean(L, false);
		return 1;
	}

	Item* newItem = Item::CreateItem(itemId);
	if(!newItem){
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	Teleport* newTp = newItem->getTeleport();
	if(!newTp){
		delete newItem;
		reportErrorFunc("Wrong teleport id");
		lua_pushboolean(L, false);
		return 1;
	}

	newTp->setDestPos(toPos);

	ReturnValue ret = g_game.internalAddItem(tile, newTp, INDEX_WHEREEVER, FLAG_NOLIMIT);
	if(ret != RET_NOERROR){
		delete newItem;
		lua_pushboolean(L, false);
		return 1;
	}

	if(newItem->getParent()){
		uint32_t uid = env->addThing(newItem);
		lua_pushnumber(L, uid);
	}
	else{
		//stackable item stacked with existing object, newItem will be released
		lua_pushnumber(L, 0);
	}
	return 1;
}

int LuaScriptInterface::luaGetPlayerStorageValue(lua_State *L)
{
	//getPlayerStorageValue(cid, valueid)
	uint32_t key = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	const Player* player = env->getPlayerByUID(cid);
	if(player){
		int32_t value;
		if(player->getStorageValue(key, value)){
			lua_pushnumber(L, value);
			lua_pushboolean(L, true);
		}
		else{
			lua_pushnumber(L, -1);
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushnumber(L, -1);
		lua_pushboolean(L, false);
	}
	return 2;
}

int LuaScriptInterface::luaSetPlayerStorageValue(lua_State *L)
{
	//setPlayerStorageValue(cid, valueid, newvalue)
	int32_t value = (int32_t)popNumber(L);
	uint32_t key = popNumber(L);
	uint32_t cid = popNumber(L);
	if(IS_IN_KEYRANGE(key, RESERVED_RANGE)){
		std::stringstream error_str;
		error_str << "Accessing to reserved range: "  << key;
		reportErrorFunc(error_str.str());
		lua_pushboolean(L, false);
		return 1;
	}

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		player->addStorageValue(key,value);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoSetItemActionId(lua_State *L)
{
	//doSetItemActionId(uid, actionid)
	uint32_t actionid = popNumber(L);
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Item* item = env->getItemByUID(uid);
	if(item){
		if(item->getActionId() != 0){
			g_moveEvents->onRemoveTileItem(item->getTile(), item);
		}
		item->setActionId(actionid);
		g_moveEvents->onAddTileItem(item->getTile(), item);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoSetItemText(lua_State *L)
{
	//doSetItemText(uid, text)
	std::string text = popString(L);
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Item* item = env->getItemByUID(uid);
	if(item){
		std::string str(text);
		item->setText(str);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoSetItemSpecialDescription(lua_State *L)
{
	//doSetItemSpecialDescription(uid, desc)
	std::string desc = popString(L);
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Item* item = env->getItemByUID(uid);
	if(item){
		std::string str(desc);
		if(str != ""){
			item->setSpecialDescription(str);
		}
		else{
			item->resetSpecialDescription();
		}
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetTilePzInfo(lua_State *L)
{
	//getTilePzInfo(pos)
	PositionEx pos;
	popPosition(L, pos);

	Tile *tile = g_game.getMap()->getTile(pos);

	if(tile){
		if(tile->hasFlag(TILESTATE_PROTECTIONZONE)){
			lua_pushboolean(L, true);
		}
		else{
			lua_pushboolean(L, false);
		}
	}
	else{
		std::stringstream ss;
		ss << pos << " " << getErrorDesc(LUA_ERROR_TILE_NOT_FOUND);
		reportErrorFunc(ss.str().c_str());
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetTileHouseInfo(lua_State *L)
{
	//getTileHouseInfo(pos)
	PositionEx pos;
	popPosition(L, pos);

	Tile *tile = g_game.getMap()->getTile(pos);
	if(tile){
		HouseTile* houseTile = tile->getHouseTile();
		if(houseTile){
			House* house = houseTile->getHouse();
			if(house){
				lua_pushnumber(L, house->getId());
			}
			else{
				lua_pushnumber(L, 0);
			}
		}
		else{
			lua_pushnumber(L, 0);
		}
	}
	else{
		std::stringstream ss;
		ss << pos << " " << getErrorDesc(LUA_ERROR_TILE_NOT_FOUND);
		reportErrorFunc(ss.str().c_str());
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoSummonCreature(lua_State *L)
{
	//doSummonCreature(name, pos, <optional> extendedPosition, <optional: default: false> forceSpawn)
	bool extentedPosition = false;
	bool forceSpawn = false;
	int32_t parameters = lua_gettop(L);
	if(parameters > 3){
		forceSpawn = popBoolean(L);
	}
	if(parameters > 2){
		extentedPosition = popBoolean(L);
	}

	PositionEx pos;
	popPosition(L, pos);
	std::string name = popString(L);

	ScriptEnviroment* env = getScriptEnv();

	Monster* monster = Monster::createMonster(name);

	if(!monster){
		std::string error_str = (std::string)"Monster name(" + name + (std::string)") not found";
		reportErrorFunc(error_str);
		lua_pushboolean(L, false);
		return 1;
	}

	if(!g_game.placeCreature(monster, pos, extentedPosition, forceSpawn)){
		delete monster;
		std::string error_str = (std::string)"Can not summon monster: " + name;
		reportErrorFunc(error_str);
		lua_pushboolean(L, false);
		return 1;
	}

	uint32_t cid = env->addThing(monster);

	lua_pushnumber(L, cid);
	return 1;
}

int LuaScriptInterface::luaDoPlayerSummonCreature(lua_State *L)
{
	//doPlayerSummonCreature(cid, name, pos, <optional: default: false> extendedPosition, <optional: default: false> forceSpawn)
	bool extentedPosition = false;
	bool forceSpawn = false;
	int32_t parameters = lua_gettop(L);
	if(parameters > 4){
		forceSpawn = popBoolean(L);
	}
	if(parameters > 3){
		extentedPosition = popBoolean(L);
	}

	PositionEx pos;
	popPosition(L, pos);
	std::string name = popString(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	Player* player = env->getPlayerByUID(cid);
	if(player){
		Monster* monster = Monster::createMonster(name);
		if(!monster){
			std::string error_str = (std::string)"Monster name(" + name + (std::string)") not found";
			reportErrorFunc(error_str);
			lua_pushboolean(L, false);
			return 1;
		}

		player->addSummon(monster);
		if(!g_game.placeCreature(monster, pos, extentedPosition, forceSpawn)){
			player->removeSummon(monster);
			lua_pushboolean(L, false);
			return 1;
		}

		uint32_t uid = env->addThing(monster);
		lua_pushnumber(L, uid);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoRemoveCreature(lua_State *L)
{
	//doRemoveCreature(cid)
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(creature){
		//Players will get kicked without restrictions
		Player* player = creature->getPlayer();
		if(player){
			player->kickPlayer();
		}
		//Monsters/NPCs will get removed
		else{
			g_game.removeCreature(creature);
		}
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoPlayerRemoveMoney(lua_State *L)
{
	//doPlayerRemoveMoney(cid, money)
	int32_t money = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		if(g_game.removeMoney(player, money))
			lua_pushboolean(L, true);
		else
			lua_pushboolean(L, false);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoPlayerAddMoney(lua_State *L)
{
	//doPlayerAddMoney(cid, money)
	uint32_t money = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		if(g_game.addMoney(player, money)){
			lua_pushboolean(L, true);
		}
		else{
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoPlayerWithdrawMoney(lua_State *L)
{
	//doPlayerWithdrawMoney(cid, money)
	uint32_t money = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		if(player->withdrawMoney(money)){
			lua_pushboolean(L, true);
		}
		else{
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoPlayerDepositMoney(lua_State *L)
{
	//doPlayerDepositMoney(cid, money)
	uint32_t money = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		if(player->depositMoney(money)){
			lua_pushboolean(L, true);
		}
		else{
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoPlayerTransferMoneyTo(lua_State *L)
{
	//doPlayerTransferMoneyTo(cid, target, money)
	uint32_t money = popNumber(L);
	std::string target = popString(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		if(player->transferMoneyTo(target, money)){
			lua_pushboolean(L, true);
		}
		else{
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoPlayerSetMasterPos(lua_State *L)
{
	//doPlayerSetMasterPos(cid, pos)
	PositionEx pos;
	popPosition(L, pos);
	popNumber(L);

	reportErrorFunc("Deprecated function. Use doPlayerSetTown");
	lua_pushboolean(L, false);
	return 1;
}

int LuaScriptInterface::luaDoPlayerSetTown(lua_State *L)
{
	//doPlayerSetTown(cid, towind)
	uint32_t townid = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		Town* town = Towns::getInstance().getTown(townid);
		if(town){
			player->masterPos = town->getTemplePosition();
			player->setTown(townid);
			lua_pushboolean(L, true);
		}
		else{
			reportErrorFunc("Not found townid");
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoPlayerSetVocation(lua_State *L)
{
	//doPlayerSetVocation(cid, voc)
	uint32_t voc = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	bool result = false;
	if(player){
		result = player->setVocation(voc);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
	}
	lua_pushboolean(L, result);

	return 1;
}

int LuaScriptInterface::luaDoPlayerSetSex(lua_State *L)
{
	//doPlayerSetSex(cid, sex)
	uint32_t sex = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		player->setSex((PlayerSex_t)sex);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDebugPrint(lua_State *L)
{
	//debugPrint(text)
	std::string text = popString(L);
	reportErrorFunc(text);
	return 0;
}

int LuaScriptInterface::luaDoPlayerAddSoul(lua_State *L)
{
	//doPlayerAddSoul(cid, soul)
	int32_t addsoul = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		player->changeSoul(addsoul);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetPlayerItemCount(lua_State *L)
{
	//getPlayerItemCount(cid, itemid, subtype)

	int32_t subtype = -1;

	if(lua_gettop(L) > 2)
		subtype = popNumber(L);

	uint32_t itemId = (uint32_t)popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	const Player* player = env->getPlayerByUID(cid);
	if(player){
		uint32_t n = player->__getItemTypeCount(itemId, subtype);
		lua_pushnumber(L, n);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetHouseOwner(lua_State *L)
{
	//getHouseOwner(houseid)
	uint32_t houseid = popNumber(L);

	House* house = Houses::getInstance().getHouse(houseid);
	if(house){
		uint32_t owner = house->getOwner();
		lua_pushnumber(L, owner);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_HOUSE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetHouseName(lua_State *L)
{
	//getHouseName(houseid)
	uint32_t houseid = popNumber(L);

	House* house = Houses::getInstance().getHouse(houseid);
	if(house){
		lua_pushstring(L, house->getName().c_str());
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_HOUSE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

int LuaScriptInterface::luaGetHouseEntry(lua_State *L)
{
	//getHouseEntry(houseid)
	uint32_t houseid = popNumber(L);

	House* house = Houses::getInstance().getHouse(houseid);
	if(house){
		pushPosition(L, house->getEntryPosition(), 0);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_HOUSE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

int LuaScriptInterface::luaGetHouseRent(lua_State *L)
{
	//getHouseRent(houseid)
	uint32_t houseid = popNumber(L);

	House* house = Houses::getInstance().getHouse(houseid);
	if(house){
		lua_pushnumber(L, house->getRent());
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_HOUSE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

int LuaScriptInterface::luaGetHouseTown(lua_State *L)
{
	//getHouseTown(houseid)
	uint32_t houseid = popNumber(L);

	House* house = Houses::getInstance().getHouse(houseid);
	if(house){
		lua_pushnumber(L, house->getTownId());
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_HOUSE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

int LuaScriptInterface::luaGetHouseAccessList(lua_State *L)
{
	//getHouseAccessList(houseid, listid)
	uint32_t listid = popNumber(L);
	uint32_t houseid = popNumber(L);

	House* house = Houses::getInstance().getHouse(houseid);
	if(house){
		std::string list;
		if(house->getAccessList(listid, list)){
			lua_pushstring(L, list.c_str());
		}
		else{
			reportErrorFunc("No valid listid.");
			lua_pushnil(L);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_HOUSE_NOT_FOUND));
		lua_pushnil(L);
	}
	return 1;
}

int LuaScriptInterface::luaGetHouseByPlayerGUID(lua_State *L)
{
	//getHouseByPlayerGUID(playerGUID)
	uint32_t guid = popNumber(L);

	House* house = Houses::getInstance().getHouseByPlayerId(guid);
	if(house){
		lua_pushnumber(L, house->getId());
	}
	else{
		lua_pushnil(L);
	}
	return 1;
}

int LuaScriptInterface::luaGetHouseTilesSize(lua_State *L)
{
	//getHouseTilesSize(houseid)
	uint32_t houseid = popNumber(L);

	House* house = Houses::getInstance().getHouse(houseid);
	if(house){
		lua_pushnumber(L, house->getTileCount());
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_HOUSE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetHouseDoorCount(lua_State *L)
{
	//getHouseDoorCount(houseid)
	uint32_t houseid = popNumber(L);

	House* house = Houses::getInstance().getHouse(houseid);
	if(house){
		lua_pushnumber(L, house->getDoorCount());
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_HOUSE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetHouseBedCount(lua_State *L)
{
	//getHouseBedCount(houseid)
	uint32_t houseid = popNumber(L);

	House* house = Houses::getInstance().getHouse(houseid);
	if(house){
		lua_pushnumber(L, house->getBedCount());
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_HOUSE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaIsHouseGuildHall(lua_State *L)
{
	//isHouseGuildHall(houseid)
	uint32_t houseid = popNumber(L);

	House* house = Houses::getInstance().getHouse(houseid);
	if(house){
		lua_pushboolean(L, (house->isGuildHall() ? true : false));
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_HOUSE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaSetHouseAccessList(lua_State *L)
{
	//setHouseAccessList(houseid, listid, listtext)
	std::string list = popString(L);
	uint32_t listid = popNumber(L);
	uint32_t houseid = popNumber(L);

	House* house = Houses::getInstance().getHouse(houseid);
	if(house){
		house->setAccessList(listid, list);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_HOUSE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaSetHouseOwner(lua_State *L)
{
	//setHouseOwner(houseid, owner)
	uint32_t owner = popNumber(L);
	uint32_t houseid = popNumber(L);

	House* house = Houses::getInstance().getHouse(houseid);
	if(house){
		house->setOwner(owner);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_HOUSE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetHouseList(lua_State *L)
{
	//getHouseList(townid = 0)
	uint32_t townid = 0;

	if(lua_gettop(L) > 0)
		townid = popNumber(L);

	lua_newtable(L);
	uint32_t n = 1;
	for(HouseMap::iterator it = Houses::getInstance().getHouseBegin(); it != Houses::getInstance().getHouseEnd(); ++n, ++it){
		House* house = it->second;

		if(townid != 0 && house->getTownId() != townid)
			continue;
		lua_pushnumber(L, n);
		lua_pushnumber(L, house->getId());
		lua_settable(L, -3);
	}

	return 1;
}

int LuaScriptInterface::luaCleanHouse(lua_State *L)
{
	//cleanHouse(houseid)
	uint32_t houseid = popNumber(L);

	House* house = Houses::getInstance().getHouse(houseid);
	if(house){
		house->clean();
		lua_pushboolean(L, true);
	}
	else{
		lua_pushboolean(L, false);
	}

	return 1;
}

int LuaScriptInterface::luaGetWorldType(lua_State *L)
{
	//getWorldType()
	lua_pushnumber(L, g_game.getWorldType());
	return 1;
}

int LuaScriptInterface::luaGetWorldTime(lua_State *L)
{
	//getWorldTime()
	lua_pushnumber(L, g_game.getLightHour());
	return 1;
}

int LuaScriptInterface::luaGetWorldLight(lua_State *L)
{
	//getWorldLight()
	LightInfo lightInfo;
	g_game.getWorldLightInfo(lightInfo);

	lua_pushnumber(L, lightInfo.level);
	lua_pushnumber(L, lightInfo.color);
	return 2;
}

int LuaScriptInterface::luaGetWorldCreatures(lua_State *L)
{
	//getWorldCreatures(type)
	//0 players, 1 monsters, 2 npcs, 3 all
	uint32_t type = popNumber(L);
	uint32_t value;
	switch(type){
	case 0:
		value = g_game.getPlayersOnline();
		break;
	case 1:
		value = g_game.getMonstersOnline();
		break;
	case 2:
		value = g_game.getNpcsOnline();
		break;
	case 3:
		value = g_game.getCreaturesOnline();
		break;
	default:
		reportErrorFunc("Wrong creature type.");
		lua_pushboolean(L, false);
		return 1;
		break;
	}
	lua_pushnumber(L, value);
	return 1;
}

int LuaScriptInterface::luaGetWorldUpTime(lua_State *L)
{
	//getWorldUpTime()
	uint32_t uptime = 0;

	Status* status = Status::instance();
	if(status){
		uptime = status->getUptime();
	}

	lua_pushnumber(L, uptime);
	return 1;
}

int LuaScriptInterface::luaGetPlayersOnlineList(lua_State *L)
{
	//getPlayersOnlineList()
	ScriptEnviroment* env = getScriptEnv();

	AutoList<Player>::listiterator it = Player::listPlayer.list.begin();
	lua_newtable(L);
	for(uint32_t i = 1; it != Player::listPlayer.list.end(); ++it, ++i){
		uint32_t cid = env->addThing(it->second);
		lua_pushnumber(L, i);
		lua_pushnumber(L, cid);
		lua_settable(L, -3);
	}

	return 1;
}

int LuaScriptInterface::luaDoPlayerBroadcastMessage(lua_State* L)
{
	//doPlayerBroadcastMessage(cid, message)
	std::string message = popString(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player)
	{
		if(g_game.internalBroadcastMessage(player, message))
			lua_pushboolean(L, true);
		else
		{
			reportErrorFunc("Player with no access trying to broadcast message.");
			lua_pushboolean(L, false);
		}
	}
	else
	{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetCreatureLight(lua_State *L)
{
	//getCreatureLight(cid)
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	const Creature* creature = env->getCreatureByUID(cid);
	if(creature){
		LightInfo lightInfo;
		creature->getCreatureLight(lightInfo);
		lua_pushnumber(L, lightInfo.level);
		lua_pushnumber(L, lightInfo.color);//color
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}
	return 1;
}

int LuaScriptInterface::luaGetCreatureLookDir(lua_State *L)
{
	//getCreatureLookDir(cid)
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	const Creature* creature = env->getCreatureByUID(cid);
	if(creature){
		Direction dir = creature->getDirection();
		lua_pushnumber(L, dir);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}
	return 1;
}

int LuaScriptInterface::luaDoPlayerAddExp(lua_State *L)
{
	//doPlayerAddExp(cid, exp, <optional: default: 0> useRate, <optional: default: 0> useMultiplier)
	int32_t parameters = lua_gettop(L);

	bool useMultiplier = false;
	bool useRate = false;
	if(parameters > 3){
		useMultiplier = (popNumber(L, true) >= 1);
	}
	if(parameters > 2){
		useRate = (popNumber(L, true) >= 1);
	}

	uint64_t exp = (uint64_t)popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	Player* player = env->getPlayerByUID(cid);
	if(player){
		if(exp > 0){
			exp = uint64_t(exp * (useMultiplier? player->getRateValue(LEVEL_EXPERIENCE) : 1.0) * (useRate? g_config.getNumber(ConfigManager::RATE_EXPERIENCE) : 1.0));
			player->addExperience(exp);
			lua_pushboolean(L, true);
		}
		else{
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoPlayerRemoveExp(lua_State *L)
{
	//doPlayerRemoveExp(cid, exp, <optional: default: 0> useRate, <optional: default: 0> useMultiplier)
	int32_t parameters = lua_gettop(L);

	bool useMultiplier = false;
	bool useRate = false;
	if(parameters > 3){
		useMultiplier = (popNumber(L, true) >= 1);
	}
	if(parameters > 2){
		useRate = (popNumber(L, true) >= 1);
	}

	uint64_t exp = (uint64_t)popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	Player* player = env->getPlayerByUID(cid);
	if(player){
		if(exp > 0){
			exp = uint64_t(exp * (useMultiplier? player->getRateValue(LEVEL_EXPERIENCE) : 1.0) * (useRate? g_config.getNumber(ConfigManager::RATE_EXPERIENCE) : 1.0));
			player->removeExperience(exp);
			lua_pushboolean(L, true);
		}
		else{
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetPlayerExperience(lua_State *L)
{
	//getPlayerExperience(cid)
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	Player* player = env->getPlayerByUID(cid);
	if(player){
		lua_pushnumber(L, player->getExperience());
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetPlayerSlotItem(lua_State *L)
{
	//getPlayerSlotItem(cid, slot)
	uint32_t slot = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	const Player* player = env->getPlayerByUID(cid);
	if(player){
		Thing* thing = player->__getThing(slot);
		if(thing){
			uint32_t uid = env->addThing(thing);
			pushThing(L, thing, uid);
		}
		else{
			pushThing(L, NULL, 0);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		pushThing(L, NULL, 0);
	}
	return 1;
}

int LuaScriptInterface::luaGetPlayerItemById(lua_State *L)
{
	//getPlayerItemById(cid, deepSearch, itemId, <optional> subType)
	ScriptEnviroment* env = getScriptEnv();

	uint32_t parameters = lua_gettop(L);

	int32_t subType = -1;
	if(parameters > 3){
		subType = (int32_t)popNumber(L);
	}

	int32_t itemId = (int32_t)popNumber(L);
	bool deepSearch = popBoolean(L);
	uint32_t cid = popNumber(L);

	Player* player = env->getPlayerByUID(cid);
	if(!player){
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		pushThing(L, NULL, 0);
		return 1;
	}

	Item* item = g_game.findItemOfType(player, itemId, deepSearch, subType);
	if(!item){
		pushThing(L, NULL, 0);
		return 1;
	}

	uint32_t uid = env->addThing(item);
	pushThing(L, item, uid);
	return 1;
}

int LuaScriptInterface::luaGetThing(lua_State *L)
{
	//getThing(uid)
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Thing* thing = env->getThingByUID(uid);
	if(thing){
		pushThing(L, thing, uid);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_THING_NOT_FOUND));
		pushThing(L, NULL, 0);
	}
	return 1;
}

int LuaScriptInterface::luaQueryTileAddThing(lua_State *L)
{
	//queryTileAddThing(uid, pos, <optional> flags)
	int32_t parameters = lua_gettop(L);

	uint32_t flags = 0;
	if(parameters > 2){
		flags = popNumber(L);
	}

	PositionEx pos;
	popPosition(L, pos);
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Tile* tile = g_game.getTile(pos.x, pos.y, pos.z);
	if(!tile){
		std::stringstream ss;
		ss << pos << " " << getErrorDesc(LUA_ERROR_TILE_NOT_FOUND);
		reportErrorFunc(ss.str().c_str());
		lua_pushnumber(L, (uint32_t)RET_NOTPOSSIBLE);
		return 1;
	}

	Thing* thing = env->getThingByUID(uid);
	if(!thing){
		reportErrorFunc(getErrorDesc(LUA_ERROR_THING_NOT_FOUND));
		lua_pushnumber(L, (uint32_t)RET_NOTPOSSIBLE);
		return 1;
	}

	ReturnValue ret = tile->__queryAdd(0, thing, 1, flags);
	lua_pushnumber(L, (uint32_t)ret);
	return 1;
}

int LuaScriptInterface::luaGetThingPos(lua_State *L)
{
	//getThingPos(uid)
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Thing* thing = env->getThingByUID(uid);
	Position pos(0, 0, 0);
	uint32_t stackpos = 0;
	if(thing){
		pos = thing->getPosition();
		if(Tile* tile = thing->getTile()){
			stackpos = tile->__getIndexOfThing(thing);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_THING_NOT_FOUND));
	}

	pushPosition(L, pos, stackpos);
	return 1;
}

int LuaScriptInterface::luaCreateCombatObject(lua_State *L)
{
	//createCombatObject()
	ScriptEnviroment* env = getScriptEnv();

	if(env->getScriptId() != EVENT_ID_LOADING){
		reportError(__FUNCTION__, "This function can only be used while loading the script.");
		lua_pushboolean(L, false);
		return 1;
	}

	Combat* combat = new Combat;

	if(!combat){
		reportErrorFunc(getErrorDesc(LUA_ERROR_COMBAT_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	uint32_t newCombatId = env->addCombatObject(combat);
	lua_pushnumber(L, newCombatId);
	return 1;
}

bool LuaScriptInterface::getArea(lua_State *L, std::list<uint32_t>& list, uint32_t& rows)
{
	if(lua_type(L, -1) != LUA_TTABLE){
		reportError(__FUNCTION__, "Variable passed to [LuaInterface::getArea] is not a table.");
		return false;
	}
	rows = 0;
	uint32_t i = 0, j = 0;

	lua_pushnil(L);  // first key //
	while(lua_next(L, -2) != 0){
		if(lua_istable(L, -1) == 0){
			return false;
		}

		lua_pushnil(L);
		while(lua_next(L, -2) != 0){
			if(lua_isnumber(L, -1) == 0){
				return false;
			}

			list.push_back((uint32_t)lua_tonumber(L, -1));

			lua_pop(L, 1);  // removes `value'; keeps `key' for next iteration //
			j++;
		}

		++rows;

		j = 0;
		lua_pop(L, 1);  // removes `value'; keeps `key' for next iteration //
		i++;
	}

	lua_pop(L, 1);
	return (rows != 0);
}

int LuaScriptInterface::luaCreateCombatArea(lua_State *L)
{
	//createCombatArea( {area}, <optional> {extArea} )
	ScriptEnviroment* env = getScriptEnv();

	if(env->getScriptId() != EVENT_ID_LOADING){
		reportError(__FUNCTION__, "This function can only be used while loading the script.");
		lua_pushboolean(L, false);
		return 1;
	}

	int32_t parameters = lua_gettop(L);

	AreaCombat* area = new AreaCombat;

	if(parameters > 1){
		//has extra parameter with diagonal area information

		uint32_t rowsExtArea;
		std::list<uint32_t> listExtArea;
		if(!getArea(L, listExtArea, rowsExtArea)){
			reportError(__FUNCTION__, "Invalid extended area table.");
			lua_pushboolean(L, false);
			return 1;
		}

		/*setup all possible rotations*/
		area->setupExtArea(listExtArea, rowsExtArea);
	}

	uint32_t rowsArea = 0;
	std::list<uint32_t> listArea;
	if(!getArea(L, listArea, rowsArea)){
		reportError(__FUNCTION__, "Invalid area table.");
		lua_pushboolean(L, false);
		return 1;
	}

	/*setup all possible rotations*/
	area->setupArea(listArea, rowsArea);
	uint32_t newAreaId = env->addCombatArea(area);

	lua_pushnumber(L, newAreaId);
	return 1;
}

int LuaScriptInterface::luaCreateConditionObject(lua_State *L)
{
	//createConditionObject(type)
	ScriptEnviroment* env = getScriptEnv();

	if(env->getScriptId() != EVENT_ID_LOADING){
		reportError(__FUNCTION__, "This function can only be used while loading the script.");
		lua_pushboolean(L, false);
		return 1;
	}

	ConditionType_t type = (ConditionType_t)popNumber(L);

	Condition* condition = Condition::createCondition(CONDITIONID_COMBAT, type, 0, 0);
	if(condition){
		uint32_t newConditionId = env->addConditionObject(condition);
		lua_pushnumber(L, newConditionId);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CONDITION_NOT_FOUND));
		lua_pushboolean(L, false);
	}

	return 1;
}

int LuaScriptInterface::luaSetCombatArea(lua_State *L)
{
	//setCombatArea(combat, area)
	uint32_t areaId = popNumber(L);
	uint32_t combatId = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	if(env->getScriptId() != EVENT_ID_LOADING){
		reportError(__FUNCTION__, "This function can only be used while loading the script.");
		lua_pushboolean(L, false);
		return 1;
	}

	Combat* combat = env->getCombatObject(combatId);

	if(!combat){
		reportErrorFunc(getErrorDesc(LUA_ERROR_COMBAT_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	const AreaCombat* area = env->getCombatArea(areaId);
	if(!area){
		reportErrorFunc(getErrorDesc(LUA_ERROR_AREA_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	combat->setArea(new AreaCombat(*area));

	if(env->getSpell()){
		env->getSpell()->setArea(true);
	}

	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaSetCombatCondition(lua_State *L)
{
	//setCombatCondition(combat, condition)
	uint32_t conditionId = popNumber(L);
	uint32_t combatId = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	if(env->getScriptId() != EVENT_ID_LOADING){
		reportError(__FUNCTION__, "This function can only be used while loading the script.");
		lua_pushboolean(L, false);
		return 1;
	}

	Combat* combat = env->getCombatObject(combatId);

	if(!combat){
		reportErrorFunc(getErrorDesc(LUA_ERROR_COMBAT_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	const Condition* condition = env->getConditionObject(conditionId);
	if(!condition){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CONDITION_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	combat->setCondition(condition->clone());

	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaSetCombatParam(lua_State *L)
{
	//setCombatParam(combat, key, value)
	uint32_t value = popNumber(L, true);
	CombatParam_t key = (CombatParam_t)popNumber(L);
	uint32_t combatId = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	if(env->getScriptId() != EVENT_ID_LOADING){
		reportError(__FUNCTION__, "This function can only be used while loading the script.");
		lua_pushboolean(L, false);
		return 1;
	}

	Combat* combat = env->getCombatObject(combatId);

	if(combat){
		combat->setParam(key, value);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_COMBAT_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaSetConditionParam(lua_State *L)
{
	//setConditionParam(condition, key, value)
	int32_t value = (int32_t)popNumber(L, true);
	ConditionParam_t key = (ConditionParam_t)popNumber(L);
	uint32_t conditionId = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	if(env->getScriptId() != EVENT_ID_LOADING){
		reportError(__FUNCTION__, "This function can only be used while loading the script.");
		lua_pushboolean(L, false);
		return 1;
	}

	Condition* condition = env->getConditionObject(conditionId);

	if(condition){
		condition->setParam(key, value);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CONDITION_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaAddDamageCondition(lua_State *L)
{
	//addDamageCondition(condition, rounds, time, value)
	int32_t value = (int32_t)popNumber(L);
	int32_t time = (int32_t)popNumber(L);
	int32_t rounds  = (int32_t)popNumber(L);
	uint32_t conditionId = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	if(env->getScriptId() != EVENT_ID_LOADING){
		reportError(__FUNCTION__, "This function can only be used while loading the script.");
		lua_pushboolean(L, false);
		return 1;
	}

	ConditionDamage* condition = dynamic_cast<ConditionDamage*>(env->getConditionObject(conditionId));

	if(condition){
		condition->addDamage(rounds, time, value);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CONDITION_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaAddOutfitCondition(lua_State *L)
{
	//addOutfitCondition(condition, lookTypeEx, lookType, lookHead, lookBody, lookLegs, lookFeet)
	Outfit_t outfit;
	outfit.lookFeet = popNumber(L);
	outfit.lookLegs = popNumber(L);
	outfit.lookBody = popNumber(L);
	outfit.lookHead = popNumber(L);
	outfit.lookType = popNumber(L);
	outfit.lookTypeEx = popNumber(L);
	uint32_t conditionId = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	if(env->getScriptId() != EVENT_ID_LOADING){
		reportError(__FUNCTION__, "This function can only be used while loading the script.");
		lua_pushboolean(L, false);
		return 1;
	}

	ConditionOutfit* condition = dynamic_cast<ConditionOutfit*>(env->getConditionObject(conditionId));

	if(condition){
		condition->addOutfit(outfit);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CONDITION_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaSetCombatCallBack(lua_State *L)
{
	//setCombatCallBack(combat, key, function_name)
	std::string function = popString(L);
	CallBackParam_t key = (CallBackParam_t)popNumber(L);
	uint32_t combatId = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	if(env->getScriptId() != EVENT_ID_LOADING){
		reportError(__FUNCTION__, "This function can only be used while loading the script.");
		lua_pushboolean(L, false);
		return 1;
	}

	Combat* combat = env->getCombatObject(combatId);

	if(!combat){
		reportErrorFunc(getErrorDesc(LUA_ERROR_COMBAT_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	LuaScriptInterface* scriptInterface = env->getScriptInterface();

	combat->setCallback(key);
	CallBack* callback = combat->getCallback(key);
	if(!callback){
		std::stringstream ss;
		ss << (uint32_t)key << " is not a valid callback key";
		reportError(__FUNCTION__, ss.str());
		lua_pushboolean(L, false);
		return 1;
	}

	if(!callback->loadCallBack(scriptInterface, function)){
		reportError(__FUNCTION__, "Can not load callback");
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaSetCombatFormula(lua_State *L)
{
	//setCombatFormula(combat, type, mina, minb, maxa, maxb)
	ScriptEnviroment* env = getScriptEnv();

	if(env->getScriptId() != EVENT_ID_LOADING){
		reportError(__FUNCTION__, "This function can only be used while loading the script.");
		lua_pushboolean(L, false);
		return 1;
	}

	double maxb = popFloatNumber(L);
	double maxa = popFloatNumber(L);
	double minb = popFloatNumber(L);
	double mina = popFloatNumber(L);

	formulaType_t type = (formulaType_t)popNumber(L);
	uint32_t combatId = popNumber(L);

	Combat* combat = env->getCombatObject(combatId);

	if(combat){
		combat->setPlayerCombatValues(type, mina, minb, maxa, maxb);

		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_COMBAT_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaSetConditionFormula(lua_State *L)
{
	//setConditionFormula(condition, mina, minb, maxa, maxb)
	ScriptEnviroment* env = getScriptEnv();

	if(env->getScriptId() != EVENT_ID_LOADING){
		reportError(__FUNCTION__, "This function can only be used while loading the script.");
		lua_pushboolean(L, false);
		return 1;
	}

	float maxb = (float)popFloatNumber(L);
	float maxa = (float)popFloatNumber(L);
	float minb = (float)popFloatNumber(L);
	float mina = (float)popFloatNumber(L);

	uint32_t conditionId = popNumber(L);

	ConditionSpeed* condition = dynamic_cast<ConditionSpeed*>(env->getConditionObject(conditionId));

	if(condition){
		condition->setFormulaVars(mina, minb, maxa, maxb);

		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CONDITION_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoCombat(lua_State *L)
{
	//doCombat(cid, combat, param)
	ScriptEnviroment* env = getScriptEnv();

	LuaVariant var = popVariant(L);
	uint32_t combatId = (uint32_t)popNumber(L);
	uint32_t cid = (uint32_t)popNumber(L);

	Creature* creature = NULL;

	if(cid != 0){
		creature = env->getCreatureByUID(cid);

		if(!creature){
			reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushboolean(L, false);
			return 1;
		}
	}

	const Combat* combat = env->getCombatObject(combatId);

	if(!combat){
		reportErrorFunc(getErrorDesc(LUA_ERROR_COMBAT_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	if(var.type == VARIANT_NONE){
		reportErrorFunc(getErrorDesc(LUA_ERROR_VARIANT_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	switch(var.type){
		case VARIANT_NUMBER:
		{
			Creature* target = g_game.getCreatureByID(var.number);

			if(!target){
				lua_pushboolean(L, false);
				return 1;
			}

			if(combat->hasArea()){
				combat->doCombat(creature, target->getPosition());
				//std::cout << "Combat->hasArea()" << std::endl;
			}
			else{
				combat->doCombat(creature, target);
			}
			break;
		}

		case VARIANT_POSITION:
		{
			combat->doCombat(creature, var.pos);
			break;
		}

		case VARIANT_TARGETPOSITION:
		{
			if(combat->hasArea()){
				combat->doCombat(creature, var.pos);
			}
			else{
				combat->postCombatEffects(creature, var.pos);
				g_game.addMagicEffect(var.pos, NM_ME_PUFF);
			}
			break;
		}

		case VARIANT_STRING:
		{
			Player* target = g_game.getPlayerByName(var.text);
			if(!target){
				lua_pushboolean(L, false);
				return 1;
			}

			combat->doCombat(creature, target);
			break;
		}

		default:
		{
			reportErrorFunc(getErrorDesc(LUA_ERROR_VARIANT_UNKNOWN));
			lua_pushboolean(L, false);
			return 1;
			break;
		}
	}

	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaDoAreaCombatHealth(lua_State *L)
{
	//doAreaCombatHealth(cid, type, pos, area, min, max, effect)
	uint8_t effect = (uint8_t)popNumber(L);
	int32_t maxChange = (int32_t)popNumber(L);
	int32_t minChange = (int32_t)popNumber(L);
	uint32_t areaId = popNumber(L);

	PositionEx pos;
	popPosition(L, pos);

	CombatType_t combatType = (CombatType_t)popNumber(L);
	uint32_t cid = (uint32_t)popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = NULL;

	if(cid != 0){
		creature = env->getCreatureByUID(cid);

		if(!creature){
			reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushboolean(L, false);
			return 1;
		}
	}

	const AreaCombat* area = env->getCombatArea(areaId);
	if(area || areaId == 0){
		CombatParams params;
		params.combatType = combatType;
		params.impactEffect = effect;
		Combat::doCombatHealth(creature, pos, area, minChange, maxChange, params);

		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_AREA_NOT_FOUND));
		lua_pushboolean(L, false);
	}

	return 1;
}

int LuaScriptInterface::luaDoTargetCombatHealth(lua_State *L)
{
	//doTargetCombatHealth(cid, target, type, min, max, effect)
	uint8_t effect = (uint8_t)popNumber(L);
	int32_t maxChange = (int32_t)popNumber(L);
	int32_t minChange = (int32_t)popNumber(L);
	CombatType_t combatType = (CombatType_t)popNumber(L);
	uint32_t targetCid = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = NULL;

	if(cid != 0){
		creature = env->getCreatureByUID(cid);

		if(!creature){
			reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushboolean(L, false);
			return 1;
		}
	}

	Creature* target = env->getCreatureByUID(targetCid);
	if(target){
		CombatParams params;
		params.combatType = combatType;
		params.impactEffect = effect;
		Combat::doCombatHealth(creature, target, minChange, maxChange, params);

		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoAreaCombatMana(lua_State *L)
{
	//doAreaCombatMana(cid, pos, area, min, max, effect)
	uint8_t effect = (uint8_t)popNumber(L);
	int32_t maxChange = (int32_t)popNumber(L);
	int32_t minChange = (int32_t)popNumber(L);
	uint32_t areaId = popNumber(L);

	PositionEx pos;
	popPosition(L, pos);

	uint32_t cid = (uint32_t)popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = NULL;

	if(cid != 0){
		creature = env->getCreatureByUID(cid);

		if(!creature){
			reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushboolean(L, false);
			return 1;
		}
	}

	const AreaCombat* area = env->getCombatArea(areaId);
	if(area || areaId == 0){
		CombatParams params;
		params.impactEffect = effect;
		Combat::doCombatMana(creature, pos, area, minChange, maxChange, params);

		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_AREA_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoTargetCombatMana(lua_State *L)
{
	//doTargetCombatMana(cid, target, min, max, effect)
	uint8_t effect = (uint8_t)popNumber(L);
	int32_t maxChange = (int32_t)popNumber(L);
	int32_t minChange = (int32_t)popNumber(L);
	uint32_t targetCid = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = NULL;

	if(cid != 0){
		creature = env->getCreatureByUID(cid);

		if(!creature){
			reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushboolean(L, false);
			return 1;
		}
	}

	Creature* target = env->getCreatureByUID(targetCid);
	if(target){
		CombatParams params;
		params.impactEffect = effect;
		Combat::doCombatMana(creature, target, minChange, maxChange, params);

		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoAreaCombatCondition(lua_State *L)
{
	//doAreaCombatCondition(cid, pos, area, condition, effect)
	uint8_t effect = (uint8_t)popNumber(L);
	uint32_t conditionId = popNumber(L);
	uint32_t areaId = popNumber(L);
	PositionEx pos;
	popPosition(L, pos);
	uint32_t cid = (uint32_t)popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = NULL;

	if(cid != 0){
		creature = env->getCreatureByUID(cid);
		if(!creature){
			reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushboolean(L, false);
			return 1;
		}
	}

	const Condition* condition = env->getConditionObject(conditionId);
	if(condition){

		const AreaCombat* area = env->getCombatArea(areaId);

		if(area || areaId == 0){
			CombatParams params;
			params.impactEffect = effect;
			params.conditionList.push_back(condition);
			Combat::doCombatCondition(creature, pos, area, params);

			lua_pushboolean(L, true);
		}
		else{
			reportErrorFunc(getErrorDesc(LUA_ERROR_AREA_NOT_FOUND));
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CONDITION_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoTargetCombatCondition(lua_State *L)
{
	//doTargetCombatCondition(cid, target, condition, effect)
	uint8_t effect = (uint8_t)popNumber(L);
	uint32_t conditionId = popNumber(L);
	uint32_t targetCid = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = NULL;

	if(cid != 0){
		creature = env->getCreatureByUID(cid);

		if(!creature){
			reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushboolean(L, false);
			return 1;
		}
	}

	Creature* target = env->getCreatureByUID(targetCid);
	if(target){
		const Condition* condition = env->getConditionObject(conditionId);
		if(condition){
			CombatParams params;
			params.impactEffect = effect;
			params.conditionList.push_back(condition);
			Combat::doCombatCondition(creature, target, params);

			lua_pushboolean(L, true);
		}
		else{
			reportErrorFunc(getErrorDesc(LUA_ERROR_CONDITION_NOT_FOUND));
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoAreaCombatDispel(lua_State *L)
{
	//doAreaCombatDispel(cid, pos, area, type, effect)
	uint8_t effect = (uint8_t)popNumber(L);
	ConditionType_t dispelType = (ConditionType_t)popNumber(L);
	uint32_t areaId = popNumber(L);
	PositionEx pos;
	popPosition(L, pos);
	uint32_t cid = (uint32_t)popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = NULL;

	if(cid != 0){
		creature = env->getCreatureByUID(cid);

		if(!creature){
			reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushboolean(L, false);
			return 1;
		}
	}

	const AreaCombat* area = env->getCombatArea(areaId);
	if(area || areaId == 0){
		CombatParams params;
		params.impactEffect = effect;
		params.dispelType = dispelType;
		Combat::doCombatDispel(creature, pos, area, params);

		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_AREA_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoTargetCombatDispel(lua_State *L)
{
	//doTargetCombatDispel(cid, target, type, effect)
	uint8_t effect = (uint8_t)popNumber(L);
	ConditionType_t dispelType = (ConditionType_t)popNumber(L);
	uint32_t targetCid = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = NULL;

	if(cid != 0){
		creature = env->getCreatureByUID(cid);

		if(!creature){
			reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushboolean(L, false);
			return 1;
		}
	}

	Creature* target = env->getCreatureByUID(targetCid);
	if(target){
		CombatParams params;
		params.impactEffect = effect;
		params.dispelType = dispelType;
		Combat::doCombatDispel(creature, target, params);

		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}

	return 1;
}

int LuaScriptInterface::luaDoChallengeCreature(lua_State *L)
{
	//doChallengeCreature(cid, target)
	uint32_t targetCid = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);

	if(!creature){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	Creature* target = env->getCreatureByUID(targetCid);
	if(!target){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	target->challengeCreature(creature);
	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaDoConvinceCreature(lua_State *L)
{
	//doConvinceCreature(cid, target)
	uint32_t targetCid = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);

	if(!creature){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	Creature* target = env->getCreatureByUID(targetCid);
	if(!target){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	target->convinceCreature(creature);
	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaGetMonsterTargetList(lua_State *L)
{
	//getMonsterTargetList(cid)

	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(!creature){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	Monster* monster = creature->getMonster();
	if(!monster){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	lua_newtable(L);
	uint32_t i = 1;
	const CreatureList& targetList = monster->getTargetList();
	for(CreatureList::const_iterator it = targetList.begin(); it != targetList.end(); ++it){
		if(monster->isTarget(*it)){
			uint32_t targetCid = env->addThing(*it);
			lua_pushnumber(L, i);
			lua_pushnumber(L, targetCid);
			lua_settable(L, -3);
			++i;
		}
	}

	return 1;
}

int LuaScriptInterface::luaGetMonsterFriendList(lua_State *L)
{
	//getMonsterFriendList(cid)

	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(!creature){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	Monster* monster = creature->getMonster();
	if(!monster){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	lua_newtable(L);
	uint32_t i = 1;
	Creature* friendCreature;
	const CreatureList& friendList = monster->getFriendList();
	for(CreatureList::const_iterator it = friendList.begin(); it != friendList.end(); ++it){
		friendCreature = *it;
		if(!friendCreature->isRemoved() && friendCreature->getPosition().z == monster->getPosition().z){
			uint32_t friendCid = env->addThing(*it);
			lua_pushnumber(L, i);
			lua_pushnumber(L, friendCid);
			lua_settable(L, -3);
			++i;
		}
	}

	return 1;
}

int LuaScriptInterface::luaDoSetMonsterTarget(lua_State *L)
{
	//doSetMonsterTarget(cid, target)
	uint32_t targetCid = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(!creature){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	Monster* monster = creature->getMonster();
	if(!monster){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	Creature* target = env->getCreatureByUID(targetCid);
	if(!target){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	if(!monster->isSummon()){
		monster->selectTarget(target);
	}

	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaDoMonsterChangeTarget(lua_State *L)
{
	//doMonsterChangeTarget(cid)
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(!creature){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	Monster* monster = creature->getMonster();
	if(!monster){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	if(!monster->isSummon()){
		monster->searchTarget(TARGETSEARCH_RANDOM);
	}

	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaDoAddCondition(lua_State *L)
{
	//doAddCondition(cid, condition)

	uint32_t conditionId = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(!creature){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	Condition* condition = env->getConditionObject(conditionId);
	if(!condition){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CONDITION_NOT_FOUND));

		return 1;
	}

	if(creature->addCondition(condition->clone())){
		lua_pushboolean(L, true);
		return 1;
	}

	lua_pushboolean(L, false);
	return 1;
}

int LuaScriptInterface::luaDoRemoveCondition(lua_State *L)
{
	//doRemoveCondition(cid, type, <optional> subId)
	uint32_t subId = 0;
	int32_t parameters = lua_gettop(L);
	if(parameters > 2){
		subId = popNumber(L);
	}

	ConditionType_t conditionType = (ConditionType_t)popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(!creature){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	Condition* condition;
	while((condition = creature->getCondition(conditionType, CONDITIONID_COMBAT, subId))) {
		  creature->removeCondition(condition);
		  }
	while((condition = creature->getCondition(conditionType, CONDITIONID_DEFAULT, subId))) {
		  creature->removeCondition(condition);
		  }

	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaNumberToVariant(lua_State *L)
{
	//numberToVariant(number)
	LuaVariant var;
	var.type = VARIANT_NUMBER;
	var.number = popNumber(L);

	LuaScriptInterface::pushVariant(L, var);
	return 1;
}

int LuaScriptInterface::luaStringToVariant(lua_State *L)
{
	//stringToVariant(string)
	LuaVariant var;
	var.type = VARIANT_STRING;
	var.text = popString(L);

	LuaScriptInterface::pushVariant(L, var);
	return 1;
}

int LuaScriptInterface::luaPositionToVariant(lua_State *L)
{
	//positionToVariant(pos)
	LuaVariant var;
	var.type = VARIANT_POSITION;
	popPosition(L, var.pos);

	LuaScriptInterface::pushVariant(L, var);
	return 1;
}

int LuaScriptInterface::luaTargetPositionToVariant(lua_State *L)
{
	//targetPositionToVariant(pos)
	LuaVariant var;
	var.type = VARIANT_TARGETPOSITION;
	popPosition(L, var.pos);

	LuaScriptInterface::pushVariant(L, var);
	return 1;
}

int LuaScriptInterface::luaVariantToNumber(lua_State *L)
{
	//variantToNumber(var, <optional> vieweruid, <optional: default: true> checkVisibility)
	/*vieweruid is an optional parameter that is used to determine if invisible GMs or players should be considered when trying to convert
	from a position to a uid. If viewerUid is ommited, it uses the point of view of a generic player (no special flags).
	The checkVisibility parameter is about the ConditionInvisible. */

	ScriptEnviroment* env = getScriptEnv();

	Creature* viewer = NULL;
	bool checkVisibility = true;
	uint16_t parameters = lua_gettop(L);
	if(parameters >= 3){
		checkVisibility = popBoolean(L);
	}
	if (parameters >= 2){
		uint32_t viewerUid = popNumber(L);
		viewer = env->getCreatureByUID(viewerUid);
		if(!viewer){
			reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushnil(L);
			return 1;
		}
	}
	LuaVariant var = popVariant(L);

	uint32_t number = 0;
	switch (var.type) {
		case VARIANT_NUMBER:
			number = var.number;
			break;
		case VARIANT_TARGETPOSITION:
		case VARIANT_POSITION:
			if (Tile* tile = g_game.getMap()->getTile(var.pos)) {
				if (Creature *c = tile->getTopVisibleCreature(viewer, checkVisibility)){
					number = env->addThing(c);
				}
			}
			break;
		case VARIANT_STRING:
			Player *player;
			if (g_game.getPlayerByNameWildcard(var.text, player) == RET_NOERROR){
				number = env->addThing(player);
			}
			break;
		default:
			break;
	}

	lua_pushnumber(L, number);
	return 1;
}

int LuaScriptInterface::luaVariantToString(lua_State *L)
{
	//variantToString(var, <optional> vieweruid, <optional: default: true> checkVisibility)
	/*vieweruid is an optional parameter that is used to determine if invisible GMs or players should be considered when trying to convert
	from a position to a uid. If viewerUid is ommited, it uses the point of view of a generic player (no special flags).
	The checkVisibility parameter is about the ConditionInvisible. */
	LuaVariant var = popVariant(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* viewer = NULL;
	bool checkVisibility = true;
	uint16_t parameters = lua_gettop(L);
	if(parameters >= 3){
		checkVisibility = popBoolean(L);
	}
	if (parameters >= 2){
		uint32_t viewerUid = popNumber(L);
		viewer = env->getCreatureByUID(viewerUid);
		if(!viewer){
			reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
			lua_pushnil(L);
			return 1;
		}
	}

	std::string text = "";

	switch (var.type) {
		case VARIANT_STRING:
			text = var.text;
			break;
		case VARIANT_NUMBER:
			if (Player* player = env->getPlayerByUID(var.number))
				text = player->getName();
			break;
		case VARIANT_TARGETPOSITION:
		case VARIANT_POSITION:
			if (Tile* tile = g_game.getMap()->getTile(var.pos)){
				if (Thing* thing = tile->getTopVisibleThing(viewer, checkVisibility)){
					if (thing->getCreature() && thing->getCreature()->getPlayer()){
						text = thing->getCreature()->getPlayer()->getName();
					}
				}
			}
			break;
		default:
			break;
	}

	lua_pushstring(L, text.c_str());
	return 1;
}

int LuaScriptInterface::luaVariantToPosition(lua_State *L)
{
	//variantToPosition(var)
	LuaVariant var = popVariant(L);

	ScriptEnviroment* env = getScriptEnv();

	PositionEx pos(0, 0, 0, 0);

	switch (var.type) {
		case VARIANT_TARGETPOSITION:
		case VARIANT_POSITION:
			pos = var.pos;
			break;
		case VARIANT_STRING:
			Player *player;
			if (g_game.getPlayerByNameWildcard(var.text, player) == RET_NOERROR){
				pos = player->getPosition();
			}
			break;
		case VARIANT_NUMBER:
			if (Creature* creature = env->getCreatureByUID(var.number)){
				pos = creature->getPosition();
			}
			break;
		default:
			break;
	}

	pushPosition(L, pos, pos.stackpos);
	return 1;
}

int LuaScriptInterface::luaDoChangeSpeed(lua_State *L)
{
	//doChangeSpeed(cid, delta)
	int32_t delta = (int32_t)popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);

	if(creature){
		g_game.changeSpeed(creature, delta);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaSetCreatureOutfit(lua_State *L)
{
	//doSetCreatureOutfit(cid, outfit, time)
	int32_t time = (int32_t)popNumber(L);
	Outfit_t outfit;
	outfit.lookType = getField(L, "lookType");
	outfit.lookHead = getField(L, "lookHead");
	outfit.lookBody = getField(L, "lookBody");
	outfit.lookLegs = getField(L, "lookLegs");
	outfit.lookFeet = getField(L, "lookFeet");
	lua_pop(L, 1);

	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);

	if(creature){
		ReturnValue ret = Spell::CreateIllusion(creature, outfit, time);
		if(ret == RET_NOERROR){
			lua_pushboolean(L, true);
		}
		else{
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetCreatureOutfit(lua_State *L)
{
	//getCreatureOutfit(cid)
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(creature){
		const Outfit_t outfit = creature->getCurrentOutfit();

		lua_newtable(L);
		setField(L, "lookType", outfit.lookType);
		setField(L, "lookHead", outfit.lookHead);
		setField(L, "lookBody", outfit.lookBody);
		setField(L, "lookLegs", outfit.lookLegs);
		setField(L, "lookFeet", outfit.lookFeet);

		return 1;
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}

	return 1;
}

int LuaScriptInterface::luaSetMonsterOutfit(lua_State *L)
{
	//doSetMonsterOutfit(cid, name, time)
	int32_t time = (int32_t)popNumber(L);
	std::string name = popString(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);

	if(creature){
		ReturnValue ret = Spell::CreateIllusion(creature, name, time);
		if(ret == RET_NOERROR){
			lua_pushboolean(L, true);
		}
		else{
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaSetItemOutfit(lua_State *L)
{
	//doSetItemOutfit(cid, item, time)
	int32_t time = (int32_t)popNumber(L);
	uint32_t item = (uint32_t)popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);

	if(creature){
		ReturnValue ret = Spell::CreateIllusion(creature, item, time);
		if(ret == RET_NOERROR){
			lua_pushboolean(L, true);
		}
		else{
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetGlobalStorageValue(lua_State *L)
{
	//getGlobalStorageValue(valueid)
	uint32_t key = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	int32_t value;
	if(env->getGlobalStorageValue(key, value)){
		lua_pushnumber(L, value);
		lua_pushboolean(L, true);
	}
	else{
		lua_pushnumber(L, -1);
		lua_pushboolean(L, false);
	}
	return 2;
}

int LuaScriptInterface::luaSetGlobalStorageValue(lua_State *L)
{
	//setGlobalStorageValue(valueid, newvalue)
	int32_t value = (int32_t)popNumber(L);
	uint32_t key = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	env->addGlobalStorageValue(key,value);
	lua_pushnumber(L,0);
	return 1;
}

int LuaScriptInterface::luaGetPlayerDepotItems(lua_State *L)
{
	//getPlayerDepotItems(cid, depotid)
	uint32_t depotid = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	Player* player = env->getPlayerByUID(cid);
	if(player){
		const Depot* depot = player->getDepot(depotid, true);
		if(depot){
			lua_pushnumber(L, depot->getItemHoldingCount());
		}
		else{
			reportErrorFunc("Depot not found");
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoPlayerSetGuildRank(lua_State *L)
{
	//doPlayerSetGuildRank(cid, rank)
	std::string rank = popString(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	Player* player = env->getPlayerByUID(cid);
	if(player){
		player->guildRank = rank;
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoPlayerSetGuildNick(lua_State *L)
{
	//doPlayerSetGuildNick(cid, nick)
	std::string nick = popString(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	Player* player = env->getPlayerByUID(cid);
	if(player){
		player->guildNick = nick;
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetGuildId(lua_State *L)
{
	//getGuildId(guild_name)
	std::string guildName = popString(L);

	uint32_t guildId;
	if(g_guilds.getGuildIdByName(guildId, guildName))
		lua_pushnumber(L, guildId);
	else
		lua_pushboolean(L, false);

	return 1;
}

int LuaScriptInterface::luaDoMoveCreature(lua_State *L)
{
	//doMoveCreature(cid, direction, <optional>flags)
	uint32_t flags = FLAG_NOLIMIT; //default value, to keep compatibility with old scripts
	int32_t parameters = lua_gettop(L);
	if (parameters >= 3)
		flags = popNumber(L);
	uint32_t direction = popNumber(L);
	uint32_t cid = popNumber(L);

	switch(direction){
		case NORTH:
		case SOUTH:
		case WEST:
		case EAST:
		case SOUTHWEST:
		case NORTHWEST:
		case NORTHEAST:
		case SOUTHEAST:
			break;
		default:
			reportErrorFunc("No valid direction");
			lua_pushboolean(L, false);
			return 1;
	}

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(creature){
		ReturnValue ret = g_game.internalMoveCreature(creature, (Direction)direction, flags);
		lua_pushnumber(L, ret);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoSetCreatureDirection(lua_State *L)
{
	//doSetCreatureDirection(cid, direction)
	uint32_t direction = popNumber(L);
	uint32_t cid = popNumber(L);

	switch(direction){
		case NORTH:
		case SOUTH:
		case WEST:
		case EAST:
			break;
		default:
			reportErrorFunc("No valid direction");
			lua_pushboolean(L, false);
			return 1;
	}

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(creature){
		g_game.internalCreatureTurn(creature, (Direction)direction);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaIsValidUID(lua_State *L)
{
	//isValidUID(uid)
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	if(env->getThingByUID(uid)){
		lua_pushboolean(L, true);
	}
	else{
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaIsCreatureImmuneToCondition(lua_State *L)
{
    //isCreatureImmuneToCondition(cid, conditionType, <optional> aggressive)
    bool aggressive;
    int32_t parameters = lua_gettop(L);
    if (parameters >= 3)
		aggressive = popBoolean(L);
    ConditionType_t conditionType = (ConditionType_t) popNumber(L);
    if (parameters < 3)
		aggressive = Condition::canBeAggressive(conditionType);
    uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(creature){
		if (creature->isImmune(conditionType, aggressive))
			lua_pushboolean(L, true);
		else
			lua_pushboolean(L, false);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}


int LuaScriptInterface::luaIsCreature(lua_State *L)
{
	//isCreature(cid)
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	if(env->getCreatureByUID(cid)){
		lua_pushboolean(L, true);
	}
	else{
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaIsContainer(lua_State *L)
{
	//isContainer(uid)
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	if(env->getContainerByUID(uid)){
		lua_pushboolean(L, true);
	}
	else{
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaIsCorpse(lua_State *L)
{
	//isCorpse(uid)
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Item* item = env->getItemByUID(uid);
	if(item){
		const ItemType& it = Item::items[item->getID()];
		lua_pushboolean(L, (it.corpseType != RACE_NONE ? true : false));
	}
	else{
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaIsMoveable(lua_State *L)
{
	//isMoveable(uid)
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Thing* thing = env->getThingByUID(uid);

	if(thing && thing->isPushable()){
		lua_pushboolean(L, true);
	}
	else{
		lua_pushboolean(L, false);
	}

	return 1;
}

int LuaScriptInterface::luaGetPlayerByAccountNumber(lua_State *L)
{
	//GetPlayerByAccountNumber(accountNumber)
	uint32_t accno = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	lua_newtable(L);
	PlayerVector players = g_game.getPlayersByAccount(accno);
	int index = 0;
	PlayerVector::iterator iter = players.begin();
	while(iter != players.end()) {
		uint32_t cid = env->addThing(*iter);

		lua_pushnumber(L, index);
		lua_pushnumber(L, cid);
		lua_settable(L, -3);

		++iter, ++index;
	}
	return 1;
}

int LuaScriptInterface::luaGetIPByPlayerName(lua_State *L)
{
	//getIPByPlayerName(playerName)
	std::string name = popString(L);

	if(Player* player = g_game.getPlayerByNameEx(name)){
		if(player->isOffline()){
			uint32_t ip;
			IOPlayer::instance()->getLastIP(ip, player->getGUID());
			lua_pushnumber(L, ip);
		}
		else{
			lua_pushnumber(L, player->getIP());
		}
	}
	else{
		lua_pushnumber(L, 0);
	}
	return 1;
}

int LuaScriptInterface::luaGetPlayersByIPAddress(lua_State *L)
{
	//getPlayersByIPAddress(ip)
	uint32_t ip = (uint32_t)popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	lua_newtable(L);
	PlayerVector players = g_game.getPlayersByIP(ip);
	int index = 0;
	PlayerVector::iterator iter = players.begin();
	while(iter != players.end()) {
		uint32_t cid = env->addThing(*iter);

		lua_pushnumber(L, index);
		lua_pushnumber(L, cid);
		lua_settable(L, -3);

		++iter, ++index;
	}
	return 1;
}

int LuaScriptInterface::luaGetAccountNumberByPlayerName(lua_State *L)
{
	//getAccountNumberByPlayerName(name)
	std::string name = popString(L);

	Player* player = g_game.getPlayerByName(name);

	if(player){
		lua_pushnumber(L, player->getAccountId());
	}
	else{
		lua_pushnumber(L, -1);
	}

	return 1;
}

int LuaScriptInterface::luaGetPlayerGUIDByName(lua_State *L)
{
	//getPlayerGUIDByName(name)
	std::string name = popString(L);

	Player* player = g_game.getPlayerByName(name);
	uint32_t guid;

	if(player){
		lua_pushnumber(L, player->getGUID());
	}
	else if (IOPlayer::instance()->getGuidByName(guid, name)){
		lua_pushnumber(L, guid);
	}
	else {
		lua_pushnumber(L, 0);
	}

	return 1;
}

int LuaScriptInterface::luaRegisterCreatureEvent(lua_State *L)
{
	//registerCreatureEvent(cid, name)
	std::string name = popString(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(creature){
		creature->registerCreatureEvent(name);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}

	return 1;
}

int LuaScriptInterface::luaGetContainerSize(lua_State *L)
{
	//getContainerSize(uid)
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	if(Container* container = env->getContainerByUID(uid)){
		lua_pushnumber(L, container->size());
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CONTAINER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetContainerCap(lua_State *L)
{
	//getContainerCap(uid)
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	if(Container* container = env->getContainerByUID(uid)){
		lua_pushnumber(L, container->capacity());
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CONTAINER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetContainerItem(lua_State *L)
{
	//getContainerItem(uid, slot)
	uint32_t slot = popNumber(L);
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	if(Container* container = env->getContainerByUID(uid)){
		Item* item = container->getItem(slot);
		if(item){
			uint32_t uid = env->addThing(item);
			pushThing(L, item, uid);
		}
		else{
			pushThing(L, NULL, 0);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CONTAINER_NOT_FOUND));
		pushThing(L, NULL, 0);
	}
	return 1;

}

int LuaScriptInterface::luaDoAddContainerItem(lua_State *L)
{
	//doAddContainerItem(uid, itemid, <optional> count/subtype)
	int32_t parameters = lua_gettop(L);

	uint32_t count = 1;
	if(parameters > 2){
		count = popNumber(L);
	}

	uint16_t itemId = (uint16_t)popNumber(L);
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	Container* container = env->getContainerByUID(uid);
	if(!container){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CONTAINER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	const ItemType& it = Item::items[itemId];

	int32_t itemCount = 1;
	int32_t subType = 1;

	if(it.hasSubType()){

		if(it.stackable){
			itemCount = (int32_t)std::ceil((float)count / 100);
		}

		subType = count;
	}
	else{
		itemCount = std::max((int32_t)1, (int32_t)count);
	}

	while(itemCount > 0){
		int32_t stackCount = std::min((int32_t)100, (int32_t)subType);
		Item* newItem = Item::CreateItem(itemId, stackCount);

		if(!newItem){
			reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
			lua_pushboolean(L, false);
			return 1;
		}

		if(it.stackable){
			subType -= stackCount;
		}
		ReturnValue ret = g_game.internalAddItem(container, newItem);

		if(ret != RET_NOERROR){
			delete newItem;
			lua_pushboolean(L, false);
			return 1;
		}

		--itemCount;
		if(itemCount == 0){
			if(newItem->getParent()){
				uint32_t uid = env->addThing(newItem);
				lua_pushnumber(L, uid);
				return 1;
			}
			else{
				//stackable item stacked with existing object, newItem will be released
				lua_pushnumber(L, 0);
				return 1;
			}
		}
	}

	lua_pushnumber(L, 0);
	return 1;
}

int LuaScriptInterface::luaGetDepotId(lua_State *L)
{
	//getDepotId(uid)
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	Container* container = env->getContainerByUID(uid);
	if(container){
		Depot* depot = container->getDepot();
		if(depot){
			lua_pushnumber(L, depot->getDepotId());
		}
		else{
			reportErrorFunc("Depot not found.");
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CONTAINER_NOT_FOUND));
		lua_pushboolean(L, false);
	}

	return 1;
}

int LuaScriptInterface::luaGetFluidSourceType(lua_State *L)
{
	//getFluidSourceType(type)
	uint32_t type = popNumber(L);

	const ItemType& it = Item::items[type];
	if(it.id != 0){
		lua_pushnumber(L, it.fluidSource);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaIsIntegerInArray(lua_State *L)
{
	//isIntegerInArray(array, value)
	int32_t value = (int32_t)popNumber(L);
	if(lua_istable(L, -1) == 0){
		lua_pop(L, 1);
		lua_pushboolean(L, false);
		return 1;
	}

	int i = 1;
	while(1){
		lua_pushnumber(L, i);
		lua_gettable(L, -2);
		if(lua_isnil(L, -1) == 1){
			lua_pop(L, 2);
			lua_pushboolean(L, false);
			return 1;
		}
		else if(lua_isnumber(L, -1) == 1){
			int32_t array_value = (int32_t)popNumber(L);
			if(array_value == value){
				lua_pop(L, 1);
				lua_pushboolean(L, true);
				return 1;
			}
		}
		else{
			lua_pop(L, 2);
			lua_pushboolean(L, false);
			return 1;
		}
		++i;
	}
}

int LuaScriptInterface::luaDoCreatureChangeOutfit(lua_State *L)
{
	//doCreatureChangeOutfit(cid, outfit)
	Outfit_t outfit;
	outfit.lookType = getField(L, "lookType");
	outfit.lookHead = getField(L, "lookHead");
	outfit.lookBody = getField(L, "lookBody");
	outfit.lookLegs = getField(L, "lookLegs");
	outfit.lookFeet = getField(L, "lookFeet");
	lua_pop(L, 1);

	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	Creature* creature = env->getCreatureByUID(cid);
	if(creature){
		creature->defaultOutfit = outfit;
		g_game.internalCreatureChangeOutfit(creature, outfit);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoSetCreatureLight(lua_State *L)
{
	//doSetCreatureLight(cid, lightLevel, lightColor, time)
	uint32_t time = popNumber(L);
	uint8_t color = (uint8_t)popNumber(L);
	uint8_t level = (uint8_t)popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	Creature* creature = env->getCreatureByUID(cid);
	if(creature){
		Condition* condition = Condition::createCondition(CONDITIONID_COMBAT, CONDITION_LIGHT, time, level | (color << 8));

		creature->addCondition(condition);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetCreaturePosition(lua_State *L)
{
	//getCreaturePosition(cid)
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(creature){
		Position pos = creature->getPosition();
		uint32_t stackpos = 0;
		if(Tile* tile = creature->getTile()){
			stackpos = tile->__getIndexOfThing(creature);
		}
		pushPosition(L, pos, stackpos);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetCreatureName(lua_State *L)
{
	//getCreatureName(cid)
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(creature){
		lua_pushstring(L, creature->getName().c_str());
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaIsItemStackable(lua_State *L)
{
	//isItemStackable(itemid)
	uint32_t itemid = popNumber(L);
	const ItemType& it = Item::items[itemid];
	if(it.stackable){
		lua_pushboolean(L, true);
	}
	else{
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaIsItemRune(lua_State *L)
{
	//isItemRune(itemid)
	uint32_t itemid = popNumber(L);
	const ItemType& it = Item::items[itemid];
	if(it.isRune()){
		lua_pushboolean(L, true);
	}
	else{
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaIsItemDoor(lua_State *L)
{
	//isItemDoor(itemid)
	uint32_t itemid = popNumber(L);
	const ItemType& it = Item::items[itemid];
	if(it.isDoor()){
		lua_pushboolean(L, true);
	}
	else{
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaIsItemContainer(lua_State *L)
{
	//isItemContainer(itemid)
	uint32_t itemid = popNumber(L);
	const ItemType& it = Item::items[itemid];
	if(it.isContainer()){
		lua_pushboolean(L, true);
	}
	else{
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaIsItemFluidContainer(lua_State *L)
{
	//isItemFluidContainer(itemid)
	uint32_t itemid = popNumber(L);
	const ItemType& it = Item::items[itemid];
	if(it.isFluidContainer()){
		lua_pushboolean(L, true);
	}
	else{
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaIsItemMoveable(lua_State *L)
{
	uint32_t itemid = popNumber(L);
	const ItemType& it = Item::items[itemid];
	if(it.moveable){
		lua_pushboolean(L, true);
	}
	else{
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetItemName(lua_State *L)
{
	//getItemName(itemid)
	uint32_t itemid = popNumber(L);
	const ItemType& it = Item::items[itemid];
	lua_pushstring(L, it.name.c_str());
	return 1;
}

int LuaScriptInterface::luaGetItemDescriptions(lua_State *L)
{
	//getItemDescriptions(itemid)
	//returns the name, the article and the plural name of the item
	uint32_t itemid = popNumber(L);
	const ItemType& it = Item::items[itemid];

	lua_newtable(L);
	setField(L, "name", it.name.c_str());
	setField(L, "article", it.article.c_str());
	setField(L, "plural", it.pluralName.c_str());
	setField(L, "stackable", it.stackable);
	setField(L, "showCount", it.showCount);
	return 1;
}

int LuaScriptInterface::luaGetItemWeight(lua_State *L)
{
	//getItemWeight(uid)
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Item* item = env->getItemByUID(uid);
	if(!item){
		lua_pushboolean(L, false);
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		return 1;
	}

	lua_pushnumber(L, item->getWeight());
	return 1;
}

int LuaScriptInterface::luaAddEvent(lua_State *L)
{
	//addEvent(callback, delay, ...)
	ScriptEnviroment* env = getScriptEnv();

	LuaScriptInterface* script_interface = env->getScriptInterface();
	if(!script_interface){
		reportError(__FUNCTION__, "No valid script interface!");
		lua_pushboolean(L, false);
		return 1;
	}

	int32_t parameters = lua_gettop(L);
	if(lua_isfunction(L, -parameters) == 0){ //-parameters means the first parameter from left to right
		reportError(__FUNCTION__, "callback parameter should be a function.");
		lua_pushboolean(L, false);
		return 1;
	}

	LuaTimerEventDesc eventDesc;
	std::list<int> params;
	for(int32_t i = 0; i < parameters-2; ++i){ //-2 because addEvent needs at least two parameters
		params.push_back(luaL_ref(L, LUA_REGISTRYINDEX));
	}
	eventDesc.parameters = params;

	uint32_t delay = std::max((uint32_t)100, popNumber(L));
	eventDesc.function = luaL_ref(L, LUA_REGISTRYINDEX);

	eventDesc.scriptId = env->getScriptId();

	script_interface->m_lastEventTimerId++;

	eventDesc.eventId = g_scheduler.addEvent(createSchedulerTask(delay, boost::bind(&LuaScriptInterface::executeTimerEvent,
		script_interface, script_interface->m_lastEventTimerId)));

	script_interface->m_timerEvents[script_interface->m_lastEventTimerId] = eventDesc;

	lua_pushnumber(L, script_interface->m_lastEventTimerId);
	return 1;
}

int LuaScriptInterface::luaStopEvent(lua_State *L)
{
	//stopEvent(eventid)
	uint32_t eventId = popNumber(L);
	ScriptEnviroment* env = getScriptEnv();

	LuaScriptInterface* script_interface = env->getScriptInterface();
	if(!script_interface){
		reportError(__FUNCTION__, "No valid script interface!");
		lua_pushboolean(L, false);
		return 1;
	}

	LuaTimerEvents::iterator it = script_interface->m_timerEvents.find(eventId);
	if(it != script_interface->m_timerEvents.end()){
		g_scheduler.stopEvent(it->second.eventId);
		for(std::list<int>::iterator lt = it->second.parameters.begin(); lt != it->second.parameters.end(); ++lt){
			luaL_unref(script_interface->m_luaState, LUA_REGISTRYINDEX, *lt);
		}
		it->second.parameters.clear();
		luaL_unref(script_interface->m_luaState, LUA_REGISTRYINDEX, it->second.function);
		script_interface->m_timerEvents.erase(it);
		lua_pushboolean(L, true);
	}
	else{
		lua_pushboolean(L, false);
	}

	return 1;
}

int LuaScriptInterface::luaGetDataDirectory(lua_State *L)
{
	//getDataDir()
	std::string datadir = g_config.getString(ConfigManager::DATA_DIRECTORY);
	lua_pushstring(L, datadir.c_str());
	return 1;
}

int LuaScriptInterface::luaDoPlayerSetRate(lua_State *L)
{
	//doPlayerSetRate(cid, type, value)
	double value = popFloatNumber(L);
	uint32_t rateType = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		if(rateType <= LEVEL_LAST){
			player->setRateValue((levelTypes_t)rateType, value);
			lua_pushboolean(L, true);
		}
		else{
			reportErrorFunc("No valid rate type.");
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}

	return 1;
}

int LuaScriptInterface::luaDoPlayerSetVipLimit(lua_State *L)
{
	//doPlayerSetVipLimit(cid, quantity)
	uint32_t quantity = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	Player* player = env->getPlayerByUID(cid);
	if(player){
		player->maxVipLimit = quantity;
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}

	return 1;
}

int LuaScriptInterface::luaDoPlayerSetDepotLimit(lua_State *L)
{
	//doPlayerSetDepotLimit(cid, quantity)
	uint32_t quantity = popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	Player* player = env->getPlayerByUID(cid);
	if(player){
		player->maxDepotLimit = quantity;
		for(DepotMap::iterator it = player->depots.begin(); it != player->depots.end(); ++it){
			it->second->setMaxDepotLimit(player->maxDepotLimit);
		}
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}

	return 1;
}

int LuaScriptInterface::luaDoSaveServer(lua_State *L)
{
	//doSaveServer(payHouses)
	bool payHouses = (int32_t(popNumber(L, true)) > 0);
	if(!g_game.saveServer(payHouses)){
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaDoSetGameState(lua_State *L)
{
	//doSetGameState(gameState)
	uint32_t gameState = popNumber(L);
	if(gameState > GAME_STATE_LAST){
		reportErrorFunc("Invalid game state.");
		lua_pushboolean(L, false);
	}
	else{
		g_game.setGameState((GameState_t)gameState);
		lua_pushboolean(L, true);
	}
	return 1;
}

int LuaScriptInterface::luaDoReloadInfo(lua_State *L)
{
	//doReloadInfo(info)
	uint32_t info = popNumber(L);
	if(info > RELOAD_TYPE_LAST){
		reportErrorFunc("Invalid reloadable info.");
		lua_pushboolean(L, false);
	}
	else{
		g_dispatcher.addTask(
			createTask(boost::bind(&Game::reloadInfo, &g_game, (reloadTypes_t)info)));
		lua_pushboolean(L, true);
	}
	return 1;
}

int LuaScriptInterface::luaDoRefreshMap(lua_State *L)
{
	g_game.proceduralRefresh();
	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaGetPlayerNameByGUID(lua_State *L)
{
	//getPlayerNameByGUID(guid)
	uint32_t guid = popNumber(L);
	std::string playername;

	if(!IOPlayer::instance()->getNameByGuid(guid, playername)){
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushstring(L, playername.c_str());
	return 1;
}

int LuaScriptInterface::luaGetCreatureSpeed(lua_State *L)
{
	//getCreatureSpeed(cid)
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(creature){
		lua_pushnumber(L, creature->getSpeed());
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetCreatureBaseSpeed(lua_State *L)
{
	//getCreatureBaseSpeed(cid)
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(creature){
		lua_pushnumber(L, creature->getBaseSpeed());
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetCreatureTarget(lua_State *L)
{
	//getCreatureTarget(cid)
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(creature){
		Creature* target = creature->getAttackedCreature();
		if(target){
			uint32_t targetCid = env->addThing(target);
			lua_pushnumber(L, targetCid);
		}
		else{
			lua_pushnumber(L, 0);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaGetCreatureHealth(lua_State *L)
{
	//getCreatureHealth(cid)
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(creature){
		lua_pushnumber(L, creature->getHealth());
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}

	return 1;
}

int LuaScriptInterface::luaGetCreatureMaxHealth(lua_State *L)
{
	//getCreatureMaxHealth(cid)
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(creature){
		lua_pushnumber(L, creature->getMaxHealth());
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}

	return 1;
}

int LuaScriptInterface::luaGetCreatureByName(lua_State *L)
{
	//getCreatureByName(name)
	std::string name = popString(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = g_game.getCreatureByName(name);
	if(creature){
		uint32_t cid = env->addThing(creature);
		lua_pushnumber(L, cid);
	}
	else{
		lua_pushnumber(L, 0);
	}
	return 1;
}

int LuaScriptInterface::luaHasCondition(lua_State *L)
{
	//hasCondition(cid, conditionid)
	ConditionType_t conditionType = (ConditionType_t)popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(creature){
		if(creature->hasCondition(conditionType)){
			lua_pushboolean(L, true);
		}
		else{
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
	}

	return 1;
}

int LuaScriptInterface::luaHasProperty(lua_State *L)
{
	//hasProperty(uid, prop)
	uint32_t prop = popNumber(L);
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Item* item = env->getItemByUID(uid);
	if(!item){
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	//Check if the item is a tile, so we can get more accurate properties
	bool hasProp = item->hasProperty((ITEMPROPERTY)prop);
	if(item->getTile() && item->getTile()->ground == item){
		hasProp = item->getTile()->hasProperty((ITEMPROPERTY)prop);
	}

	lua_pushboolean(L, hasProp? true : false);
	return 1;
}

int LuaScriptInterface::luaDoSendDistanceShoot(lua_State *L)
{
	//doSendDistanceShoot(frompos, topos, type)
	uint8_t type = (uint8_t)popNumber(L);
	PositionEx toPos;
	popPosition(L, toPos);
	PositionEx fromPos;
	popPosition(L, fromPos);

	ScriptEnviroment* env = getScriptEnv();

	if(fromPos.x == 0xFFFF){
		fromPos = env->getRealPos();
	}
	if(toPos.x == 0xFFFF){
		toPos = env->getRealPos();
	}

	g_game.addDistanceEffect(fromPos, toPos, type);
	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaGetCreatureMaster(lua_State *L)
{
	//getCreatureMaster(cid)
	//returns the creature's master or itself if the creature isn't a summon
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(!creature){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	Creature* master = creature->getMaster();
	if(!master){
		lua_pushnumber(L, cid);
		return 1;
	}

	uint32_t masterId = env->addThing(master);
	lua_pushnumber(L, masterId);
	return 1;
}

int LuaScriptInterface::luaGetCreatureSummons(lua_State *L)
{
	//getCreatureSummons(cid)
	//returns a table with all the summons of the creature
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(!creature){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	lua_newtable(L);
	const std::list<Creature*>& summons = creature->getSummons();
	std::list<Creature*>::const_iterator it = summons.begin();
	for(uint32_t i = 1; it != summons.end(); ++it, ++i){
		uint32_t summonCid = env->addThing(*it);
		lua_pushnumber(L, i);
		lua_pushnumber(L, summonCid);
		lua_settable(L, -3);
	}

	return 1;
}

int LuaScriptInterface::luaGetSpectators(lua_State *L)
{
	//getSpectators(centerPos, rangex, rangey, multifloor)

	bool multifloor = popBoolean(L);
	uint32_t rangey = popNumber(L);
	uint32_t rangex = popNumber(L);

	PositionEx centerPos;
	popPosition(L, centerPos);

	SpectatorVec list;
	g_game.getSpectators(list, centerPos, false, multifloor, rangex, rangex, rangey, rangey);
	if(list.empty()){
		lua_pushnil(L);
		return 1;
	}

	lua_newtable(L);
	SpectatorVec::const_iterator it = list.begin();
	for(uint32_t i = 1; it != list.end(); ++it, ++i){
		lua_pushnumber(L, i);
		lua_pushnumber(L, (*it)->getID());
		lua_settable(L, -3);
	}

	return 1;
}

int LuaScriptInterface::luaGetPartyMembers(lua_State *L)
{
	//getPartyMembers(cid)
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Creature* creature = env->getCreatureByUID(cid);
	if(!creature){
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	Player* player = creature->getPlayer();
	if(!player){
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	if(!player->getParty()){
		lua_pushnil(L);
		return 1;
	}

	PlayerVector list = player->getParty()->getMemberList();
	list.push_back(player->getParty()->getLeader());

	lua_newtable(L);
	PlayerVector::const_iterator it = list.begin();
	for(uint32_t i = 1; it != list.end(); ++it, ++i){
		lua_pushnumber(L, i);
		lua_pushnumber(L, (*it)->getID());
		lua_settable(L, -3);
	}

	return 1;
}

int LuaScriptInterface::luaGetItemIdByName(lua_State *L)
{
	//getItemIdByName(name)
	std::string name = popString(L);

	int32_t itemid = Item::items.getItemIdByName(name);
	if(itemid == -1){
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushnumber(L, itemid);
	return 1;
}

int LuaScriptInterface::luaIsSightClear(lua_State *L)
{
	//isSightClear(fromPos, toPos, floorCheck)
	PositionEx fromPos, toPos;
	bool floorCheck = popBoolean(L);
	popPosition(L, toPos);
	popPosition(L, fromPos);

	bool result = g_game.isSightClear(fromPos, toPos, floorCheck);
	lua_pushboolean(L, (result ? true : false));
	return 1;
}

int LuaScriptInterface::luaIsMonsterName(lua_State *L)
{
	//isMonsterName(name)
	std::string name = popString(L);
	MonsterType *mType = g_monsters.getMonsterType(name);
	if (!mType)
		lua_pushboolean(L, false);
	else
		lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaIsValidItemId(lua_State *L)
{
	//isValidItemId(itemid)
	uint32_t itemId = popNumber(L);
	const ItemType& it = Item::items[itemId];
	if (it.id == 0)
		lua_pushboolean(L, false);
	else
		lua_pushboolean(L, true);
	return 1;
}

// Bans
int LuaScriptInterface::luaAddPlayerBan(lua_State *L)
{
	//addPlayerBan(playerName[, length[, admin[, comment]]])
	int32_t parameters = lua_gettop(L);

	uint32_t reason = 0;
	violationAction_t action = ACTION_NOTATION;
	std::string statement;
	std::string comment = "No comment";
	uint32_t admin = 0;
	int32_t length = 0;

	if(parameters > 6)
		statement = popString(L);
	if(parameters > 5)
		reason = popNumber(L);
	if(parameters > 4)
		action = (violationAction_t)popNumber(L);
	if(parameters > 3)
		comment = popString(L);
	if(parameters > 2)
		admin = popNumber(L);
	if(parameters > 1)
		length = popNumber(L);

	std::string name = popString(L);
	g_bans.addPlayerBan(name, (length > 0 ? std::time(NULL) + length : length), admin, comment, statement, reason, action);

	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaAddIPBan(lua_State *L)
{
	//addIPBan(accno[, mask[, length[, admin[, comment]]]])
	int32_t parameters = lua_gettop(L);

	std::string comment = "No comment";
	uint32_t admin = 0;
	int32_t length = 0;
	uint32_t mask = 0xFFFFFFFF;

	if(parameters > 4)
		comment = popString(L);
	if(parameters > 3)
		admin = popNumber(L);
	if(parameters > 2)
		length = popNumber(L);
	if(parameters > 1)
		mask = popNumber(L);

	uint32_t ip = popNumber(L);
	g_bans.addIpBan(ip, mask, (length > 0 ? std::time(NULL) + length : length), admin, comment);

	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaAddAccountBan(lua_State *L)
{
	//addAccountBan(accno[, length[, admin[, comment]]])
	int32_t parameters = lua_gettop(L);

	uint32_t reason = 0;
	violationAction_t action = ACTION_NOTATION;
	std::string statement;
	std::string comment = "No comment";
	uint32_t admin = 0;
	int32_t length = 0;

	if(parameters > 6)
		statement = popString(L);
	if(parameters > 5)
		reason = popNumber(L);
	if(parameters > 4)
		action = (violationAction_t)popNumber(L);
	if(parameters > 3)
		comment = popString(L);
	if(parameters > 2)
		admin = popNumber(L);
	if(parameters > 1)
		length = popNumber(L);

	uint32_t accno = popNumber(L);
	g_bans.addAccountBan(accno, (length > 0 ? std::time(NULL) + length : length), admin, comment, statement, reason, action);

	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaRemovePlayerBan(lua_State *L)
{
	//removePlayerBan(name)
	std::string name = popString(L);

	bool b = g_bans.removePlayerBans(name);

	lua_pushboolean(L, b? true : false);
	return 1;
}

int LuaScriptInterface::luaRemoveAccountBan(lua_State *L)
{
	//removeAccountBan(accno)
	uint32_t accno = popNumber(L);

	bool b = g_bans.removeAccountBans(accno);

	lua_pushboolean(L, b? true : false);
	return 1;
}

int LuaScriptInterface::luaRemoveIPBan(lua_State *L)
{
	//removeIPBan(accno)
	int32_t parameters = lua_gettop(L);

	uint32_t mask = 0xFFFFFFFF;
	if(parameters > 1)
		mask = popNumber(L);
	uint32_t ip = popNumber(L);
	bool b = g_bans.removeIpBans(ip, mask);

	lua_pushboolean(L, b? true : false);
	return 1;
}

int LuaScriptInterface::luaGetPlayerBanList(lua_State *L)
{
	//getPlayerBanList()
	std::vector<Ban> bans = g_bans.getBans(BAN_PLAYER);
	lua_createtable(L, bans.size(), 0);

	int index = 1;
	for(std::vector<Ban>::const_iterator iter = bans.begin(); iter != bans.end(); ++iter, ++index) {
		const Ban& ban = *iter;
		lua_pushnumber(L, index);

		lua_createtable(L, 9, 0);
		setField(L, "playerId", ban.value);
		std::string playername;
		IOPlayer::instance()->getNameByGuid(atoi(ban.value.c_str()), playername);
		setField(L, "playerName", playername);
		setField(L, "added", ban.added);
		setField(L, "expires", ban.expires);
		setField(L, "admin", ban.adminId);
		setField(L, "reason", ban.reason);
		setField(L, "action", ban.action);
		setField(L, "comment", ban.comment);
		setField(L, "statement", ban.statement);
		lua_settable(L, -3);
	}
	return 1;
}

int LuaScriptInterface::luaGetAccountBanList(lua_State *L)
{
	//getAccountBanList()
	std::vector<Ban> bans = g_bans.getBans(BAN_ACCOUNT);
	lua_createtable(L, bans.size(), 0);

	int index = 1;
	for(std::vector<Ban>::const_iterator iter = bans.begin(); iter != bans.end(); ++iter, ++index) {
		const Ban& ban = *iter;
		lua_pushnumber(L, index);

		lua_createtable(L, 9, 0);
		setField(L, "accountId", ban.value);

		std::string accountName;
		setField(L, "accountName", accountName);
		setField(L, "added", ban.added);
		setField(L, "expires", ban.expires);
		setField(L, "admin", ban.adminId);
		setField(L, "reason", ban.reason);
		setField(L, "action", ban.action);
		setField(L, "comment", ban.comment);
		setField(L, "statement", ban.statement);
		lua_settable(L, -3);
	}
	return 1;
}

int LuaScriptInterface::luaGetIPBanList(lua_State *L)
{
	//getIPBanList()
	std::vector<Ban> bans = g_bans.getBans(BAN_IPADDRESS);
	lua_createtable(L, bans.size(), 0);

	int index = 1;
	for(std::vector<Ban>::const_iterator iter = bans.begin(); iter != bans.end(); ++iter, ++index) {
		const Ban& ban = *iter;
		lua_pushnumber(L, index);

		lua_createtable(L, 9, 0);
		setField(L, "ip", ban.value);
		setField(L, "mask", ban.param);
		setField(L, "added", ban.added);
		setField(L, "expires", ban.expires);
		setField(L, "admin", ban.adminId);
		setField(L, "reason", ban.reason);
		setField(L, "action", ban.action);
		setField(L, "comment", ban.comment);
		setField(L, "statement", ban.statement);
		lua_settable(L, -3);
	}
	return 1;
}

int LuaScriptInterface::luaIsNpcName(lua_State *L)
{
	//isNpcName(name)
	std::string name = popString(L);

	for(AutoList<Npc>::listiterator it = Npc::listNpc.list.begin(); it != Npc::listNpc.list.end(); ++it){
		if (it->second->getName() == name) {
			lua_pushboolean(L, true);
			return 1;
		}
	}

	lua_pushboolean(L, false);
	return 1;
}

int LuaScriptInterface::luaGetMonsterParameter(lua_State *L)
{
	//getMonsterParameter(name, key)
	std::string key = popString(L);
	std::string name = popString(L);

	MonsterType* mType = g_monsters.getMonsterType(name);
	if(mType){
		std::string value;
		if(mType->getParameter(key, value))
			lua_pushstring(L, value.c_str());
		else
			lua_pushnil(L);
	}
	else{
		reportErrorFunc("Could not find monster by name given.");
		lua_pushnil(L);
	}

	return 1;
}

int LuaScriptInterface::luaGetNpcParameterByName(lua_State *L)
{
	//getNpcParameterByName(name, key)
	Npc* npc = NULL;
	std::string key = popString(L);
	std::string name = popString(L);

	for(AutoList<Npc>::listiterator it = Npc::listNpc.list.begin(); it != Npc::listNpc.list.end(); ++it){
		if(it->second->getName() == name){
			npc = it->second;
			break;
		}
	}

	if(npc){
		std::string value;
		if(npc->getParameter(key, value))
			lua_pushstring(L, value.c_str());
		else
			lua_pushnil(L);
	}
	else{
		reportErrorFunc("Could not find npc by name given.");
		lua_pushnil(L);
	}

	return 1;
}

int LuaScriptInterface::luaGetConfigValue(lua_State *L)
{
	//getConfigValue(key)
	g_config.getConfigValue(popString(L), L);
	return 1;
}

std::string LuaScriptInterface::escapeString(const std::string& string)
{
	std::string s = string;
	replaceString(s, "\\", "\\\\");
	replaceString(s, "\"", "\\\"");
	replaceString(s, "'", "\\'");
	replaceString(s, "[[", "\\[[");
	return s;
}

// Bit lib

const luaL_Reg LuaScriptInterface::luaBitReg[] =
{
	//{"cast", LuaScriptInterface::luaBitCast},
	{"bnot", LuaScriptInterface::luaBitNot},
	{"band", LuaScriptInterface::luaBitAnd},
	{"bor", LuaScriptInterface::luaBitOr},
	{"bxor", LuaScriptInterface::luaBitXor},
	{"lshift", LuaScriptInterface::luaBitLeftShift},
	{"rshift", LuaScriptInterface::luaBitRightShift},
	// Unsigned
	{"ubnot", LuaScriptInterface::luaBitUNot},
	{"uband", LuaScriptInterface::luaBitUAnd},
	{"ubor", LuaScriptInterface::luaBitUOr},
	{"ubxor", LuaScriptInterface::luaBitUXor},
	{"ulshift", LuaScriptInterface::luaBitULeftShift},
	{"urshift", LuaScriptInterface::luaBitURightShift},
	//{"arshift", LuaScriptInterface::luaBitArithmeticalRightShift},
	{NULL,NULL}
};

int LuaScriptInterface::luaBitNot(lua_State *L)
{
	int32_t number = (int32_t)popNumber(L);
	lua_pushnumber(L, ~number);
	return 1;
}

int LuaScriptInterface::luaBitUNot(lua_State *L)
{
	uint32_t number = (uint32_t)popNumber(L);
	lua_pushnumber(L, ~number);
	return 1;
}

#define MULTIOP(type, name, op) \
	int LuaScriptInterface::luaBit##name(lua_State *L) { \
		int n = lua_gettop(L); \
		type w = (type)popNumber(L); \
		for(int i = 2; i <= n; ++i){ \
			w op popNumber(L); \
		} \
		lua_pushnumber(L, w); \
		return 1; \
	}

MULTIOP(int32_t, And, &=)
MULTIOP(int32_t, Or, |=)
MULTIOP(int32_t, Xor, ^=)
MULTIOP(uint32_t, UAnd, &=)
MULTIOP(uint32_t, UOr, |=)
MULTIOP(uint32_t, UXor, ^=)

#define SHIFTOP(type, name, op) \
	int LuaScriptInterface::luaBit##name(lua_State *L) { \
		type n2 = (type)popNumber(L), n1 = (type)popNumber(L); \
		lua_pushnumber(L, (n1 op n2)); \
		return 1; \
	}

SHIFTOP(int32_t, LeftShift, <<)
SHIFTOP(int32_t, RightShift, >>)
SHIFTOP(uint32_t, ULeftShift, <<)
SHIFTOP(uint32_t, URightShift, >>)


const luaL_Reg LuaScriptInterface::luaDatabaseTable[] =
{
	{"query", LuaScriptInterface::luaDatabaseExecute},
	{"storeQuery", LuaScriptInterface::luaDatabaseStoreQuery},
	{"escapeString", LuaScriptInterface::luaDatabaseEscapeString},
	{"escapeBlob", LuaScriptInterface::luaDatabaseEscapeBlob},
	{"lastInsertId", LuaScriptInterface::luaDatabaseLastInsertId},
	{"stringComparer", LuaScriptInterface::luaDatabaseStringComparer},
	{"updateLimiter", LuaScriptInterface::luaDatabaseUpdateLimiter},
	{"connected", LuaScriptInterface::luaDatabaseConnected},
	{"tableExists", LuaScriptInterface::luaDatabaseTableExists},
	{NULL,NULL}
};

int32_t LuaScriptInterface::luaDatabaseExecute(lua_State* L)
{
	DBQuery query;
	lua_pushboolean(L, Database::instance()->executeQuery(popString(L)));
	return 1;
}

int32_t LuaScriptInterface::luaDatabaseStoreQuery(lua_State* L)
{
	ScriptEnviroment* env = getScriptEnv();

	DBQuery query;
	if(DBResult* res = Database::instance()->storeQuery(popString(L)))
		lua_pushnumber(L, env->addResult(res));
	else
		lua_pushboolean(L, false);

	return 1;
}

int32_t LuaScriptInterface::luaDatabaseEscapeString(lua_State* L)
{
	DBQuery query;
	lua_pushstring(L, Database::instance()->escapeString(popString(L)).c_str());
	return 1;
}

int32_t LuaScriptInterface::luaDatabaseEscapeBlob(lua_State* L)
{
	uint32_t length = popNumber(L);
	DBQuery query;

	lua_pushstring(L, Database::instance()->escapeBlob(popString(L).c_str(), length).c_str());
	return 1;
}

int32_t LuaScriptInterface::luaDatabaseLastInsertId(lua_State* L)
{
	DBQuery query;
	lua_pushnumber(L, Database::instance()->getLastInsertId());
	return 1;
}

int32_t LuaScriptInterface::luaDatabaseStringComparer(lua_State* L)
{
	lua_pushstring(L, Database::instance()->getStringComparer().c_str());
	return 1;
}

int32_t LuaScriptInterface::luaDatabaseUpdateLimiter(lua_State* L)
{
	lua_pushstring(L, Database::instance()->getUpdateLimiter().c_str());
	return 1;
}

int32_t LuaScriptInterface::luaDatabaseConnected(lua_State* L)
{
	lua_pushboolean(L, Database::instance()->isConnected());
	return 1;
}

int32_t LuaScriptInterface::luaDatabaseTableExists(lua_State* L)
{
	//lua_pushboolean(L, DatabaseManager::getInstance()->tableExists(popString(L)));
	lua_pushboolean(L, true);
	return 1;
}

const luaL_Reg LuaScriptInterface::luaResultTable[] =
{
	{"getDataInt", LuaScriptInterface::luaResultGetDataInt},
	{"getDataLong", LuaScriptInterface::luaResultGetDataLong},
	{"getDataString", LuaScriptInterface::luaResultGetDataString},
	{"getDataStream", LuaScriptInterface::luaResultGetDataStream},
	{"getAllData", LuaScriptInterface::luaResultGetAllData},
	{"next", LuaScriptInterface::luaResultNext},
	{"free", LuaScriptInterface::luaResultFree},
	{NULL, NULL}
};

int32_t LuaScriptInterface::luaResultGetDataInt(lua_State* L)
{
	const std::string& s = popString(L);
	ScriptEnviroment* env = getScriptEnv();

	DBResult* res = env->getResultByID(popNumber(L));
	if(!res)
	{
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushnumber(L, res->getDataInt(s));
	return 1;
}

int32_t LuaScriptInterface::luaResultGetDataLong(lua_State* L)
{
	const std::string& s = popString(L);
	ScriptEnviroment* env = getScriptEnv();

	DBResult* res = env->getResultByID(popNumber(L));
	if(!res)
	{
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushnumber(L, res->getDataLong(s));
	return 1;
}

int32_t LuaScriptInterface::luaResultGetDataString(lua_State* L)
{
	const std::string& s = popString(L);
	ScriptEnviroment* env = getScriptEnv();

	DBResult* res = env->getResultByID(popNumber(L));
	if(!res)
	{
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushstring(L, res->getDataString(s).c_str());
	return 1;
}

int32_t LuaScriptInterface::luaResultGetDataStream(lua_State* L)
{
	const std::string& s = popString(L);
	ScriptEnviroment* env = getScriptEnv();

	DBResult* res = env->getResultByID(popNumber(L));
	if(!res)
	{
		lua_pushboolean(L, false);
		return 1;
	}

	unsigned long length = 0;
	lua_pushstring(L, res->getDataStream(s, length));
	lua_pushnumber(L, length);
	return 2;
}

int32_t LuaScriptInterface::luaResultGetAllData(lua_State* L)
{
	ScriptEnviroment* env = getScriptEnv();

	DBResult* res = env->getResultByID(popNumber(L));
	if(!res)
	{
		lua_pushboolean(L, false);
		return 1;
	}

	lua_newtable(L);
	listNames_t listNames = res->getListNames();
	for(listNames_t::iterator it = listNames.begin(); it != listNames.end(); ++it)
		setField(L, it->first.c_str(), res->getDataString(it->first));

	return 1;
}

int32_t LuaScriptInterface::luaResultNext(lua_State* L)
{
	ScriptEnviroment* env = getScriptEnv();

	DBResult* res = env->getResultByID(popNumber(L));
	if(!res)
	{
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushboolean(L, res->next());
	return 1;
}

int32_t LuaScriptInterface::luaResultFree(lua_State* L)
{
	ScriptEnviroment* env = getScriptEnv();
	lua_pushboolean(L, env->removeResult(popNumber(L)));
	return 1;
}


int LuaScriptInterface::luaGetItemWeaponType(lua_State *L)
{
	//getItemWeaponType(itemid)
	uint32_t itemid = popNumber(L);
	const ItemType& it = Item::items[itemid];
	if(it.id != 0)
		lua_pushnumber(L, (int32_t) it.weaponType);
	else{
		reportErrorFunc("Invalid itemid number.");
		lua_pushnil(L);
	}

	return 1;
}

int LuaScriptInterface::luaGetItemAttack(lua_State *L)
{
	//getItemAttack(itemid)
	uint32_t itemid = popNumber(L);
	const ItemType& it = Item::items[itemid];
	if(it.id != 0)
		lua_pushnumber(L, it.attack);
	else{
		reportErrorFunc("Invalid itemid number.");
		lua_pushnil(L);
	}

	return 1;
}

int LuaScriptInterface::luaGetItemDefense(lua_State *L)
{
	//getItemDefense(itemid)
	uint32_t itemid = popNumber(L);
	const ItemType& it = Item::items[itemid];
	if(it.id != 0)
		lua_pushnumber(L, it.defense);
	else{
		reportErrorFunc("Invalid itemid number.");
		lua_pushnil(L);
	}

	return 1;
}

int LuaScriptInterface::luaGetItemExtraDef(lua_State *L)
{
	//getItemExtraDef(itemid)
	uint32_t itemid = popNumber(L);
	const ItemType& it = Item::items[itemid];
	if(it.id != 0)
		lua_pushnumber(L, it.extraDef);
	else{
		reportErrorFunc("Invalid itemid number.");
		lua_pushnil(L);
	}

	return 1;
}

int LuaScriptInterface::luaGetItemArmor(lua_State *L)
{
	//getItemArmor(itemid)
	uint32_t itemid = popNumber(L);
	const ItemType& it = Item::items[itemid];
	if(it.id != 0)
		lua_pushnumber(L, it.armor);
	else{
		reportErrorFunc("Invalid itemid number.");
		lua_pushnil(L);
	}

	return 1;
}

int LuaScriptInterface::luaGetItemWeaponTypeByUID(lua_State *L)
{
	//getItemWeaponTypeByUID(uid)
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Item* item = env->getItemByUID(uid);
	if(!item){
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	const ItemType& it = Item::items[item->getID()];
	lua_pushnumber(L, (int32_t) it.weaponType);

	return 1;
}

int LuaScriptInterface::luaGetItemAttackByUID(lua_State *L)
{
	//getItemAttackByUID(uid)
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Item* item = env->getItemByUID(uid);
	if(!item){
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	lua_pushnumber(L, item->getAttack());
	return 1;
}

int LuaScriptInterface::luaGetItemDefenseByUID(lua_State *L)
{
	//getItemDefenseByUID(uid)
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Item* item = env->getItemByUID(uid);
	if(!item){
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	lua_pushnumber(L, item->getDefense());
	return 1;
}

int LuaScriptInterface::luaGetItemExtraDefByUID(lua_State *L)
{
	//getItemExtraDefByUID(uid)
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Item* item = env->getItemByUID(uid);
	if(!item){
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	lua_pushnumber(L, item->getExtraDef());
	return 1;
}

int LuaScriptInterface::luaGetItemArmorByUID(lua_State *L)
{
	//getItemArmorByUID(uid)
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Item* item = env->getItemByUID(uid);
	if(!item){
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	lua_pushnumber(L, item->getArmor());
	return 1;
}

int LuaScriptInterface::luaIsItemTwoHandedByUID(lua_State *L)
{
	//isItemTwoHandedByUID(uid)
	uint32_t uid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Item* item = env->getItemByUID(uid);
	if(!item){
		reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));
		lua_pushnil(L);
		return 1;
	}

	if (item->getSlotPosition() & SLOTP_TWO_HAND){
		lua_pushboolean(L, true);
	}
	else{
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaIsItemTwoHanded(lua_State *L)
{
	//isItemTwoHanded(itemid)
	uint32_t itemid = popNumber(L);
	if(itemid > 0){
		const ItemType& it = Item::items[itemid];
		if (it.slot_position & SLOTP_TWO_HAND){
			lua_pushboolean(L, true);
		}
		else{
			lua_pushboolean(L, false);
		}
	}
	else{
		reportErrorFunc("Invalid itemid number.");
		lua_pushnil(L);
	}

	return 1;
}

int LuaScriptInterface::luaGetOTSYSTime(lua_State *L)
{
	//getOTSYSTime()
	int64_t ret = OTSYS_TIME();
	lua_pushnumber(L, ret);
	return 1;
}

int LuaScriptInterface::luaDoPlayerToogleGmInvisible(lua_State *L)
{
	//doPlayerToogleGmInvisible(cid)
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		player->toogleGmInvisible();
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}

	return 1;
}

int LuaScriptInterface::luaDoPlayerAddPremiumDays(lua_State *L)
{
	//doPlayerAddPremiumDays(cid, days)
	uint32_t days = popNumber(L);
	uint32_t cid = popNumber(L);

	if(days < 1){
		lua_pushboolean(L, false);
		return 1;
	}
	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);

	if(!player){
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	uint32_t accountnumber = player->getAccountId();
	Account account =  IOAccount::instance()->loadAccount(accountnumber, true);

	if(account.premEnd < std::time(NULL)){
		account.premEnd = std::time(NULL) + (days * 24 * 60 * 60);
	}
	else{
		account.premEnd += (days * 24 * 60 * 60);
	}
	IOAccount::instance()->saveAccount(account);
	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaDoPlayerRemovePremiumDays(lua_State *L)
{
	//doPlayerRemovePremiumDays(cid, days)
	uint32_t days = popNumber(L);
	uint32_t cid = popNumber(L);

	if(days < 1){
		lua_pushboolean(L, false);
		return 1;
	}
	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);

	if(!player){
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	uint32_t accountnumber = player->getAccountId();
	Account account = IOAccount::instance()->loadAccount(accountnumber, true);

	account.premEnd -= (days * 24 * 60 * 60);
	if(account.premEnd <= std::time(NULL)){
		account.premEnd = 0;
	}

	IOAccount::instance()->saveAccount(account);
	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaGetFirstItemFromInventory(lua_State *L)
{
	//getFirstItemFromInventory(cid, id/name, <optional> reportError)
	bool reporterror = false;
	if(lua_gettop(L) > 2)
		reporterror = popBoolean(L);

	bool isname = lua_type(L, -1) == LUA_TSTRING;
	std::string value = popString(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	Player* player = env->getPlayerByUID(cid);
	if(!player){
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	int32_t id = isname ? Item::items.getItemIdByName(value) : atoi(value.c_str());
	Item* item = player->getFirstItemById(id);
	if(!item){
		if(reporterror)
			reportErrorFunc(getErrorDesc(LUA_ERROR_ITEM_NOT_FOUND));

		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushnumber(L, env->addThing(item));
	return 1;
}

int LuaScriptInterface::luaGetCreatureConditionInfo(lua_State *L)
{
	//getCreatureConditionInfo(cid, conditionType, <optional: default: 0> subId, <optional: default: CONDITIONID_DEFAULT> conditionId)
	ConditionId_t conditionId = CONDITIONID_DEFAULT;
	if(lua_gettop(L) > 3)
		conditionId = (ConditionId_t)popNumber(L);

	int subId = 0;
	if(lua_gettop(L) > 2)
		subId = popNumber(L);

	ConditionType_t conditionType = (ConditionType_t)popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	Creature* creature = env->getCreatureByUID(cid);
	if(!creature)
	{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	Condition* condition = creature->getCondition(conditionType, conditionId, subId);
	if(!condition)
	{
		lua_pushboolean(L, false);
		return 1;
	}

	lua_newtable(L);
	setField(L, "icons", condition->getIcons());
	setField(L, "endTime", condition->getEndTime());
	setField(L, "ticks", condition->getTicks());
	setFieldBool(L, "persistent", condition->isPersistent());
	return 1;
}

int LuaScriptInterface::luaGetCreatureCondition(lua_State *L)
{
	//getCreatureCondition(cid, conditionType, <optional: default: 0> subId, <optional: default: CONDITIONID_DEFAULT> conditionId)
	ConditionId_t conditionId = CONDITIONID_DEFAULT;
	if(lua_gettop(L) > 3)
		conditionId = (ConditionId_t)popNumber(L);

	int subId = 0;
	if(lua_gettop(L) > 2)
		subId = popNumber(L);

	ConditionType_t conditionType = (ConditionType_t)popNumber(L);
	uint32_t cid = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();
	Creature* creature = env->getCreatureByUID(cid);
	if(!creature)
	{
		reportErrorFunc(getErrorDesc(LUA_ERROR_CREATURE_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	Condition* condition = creature->getCondition(conditionType, conditionId, subId);
	if(!condition)
	{
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushboolean(L, true);
	return 1;
}

int LuaScriptInterface::luaGetPlayerModes(lua_State* L)
{
	//getPlayerModes(cid)
	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(popNumber(L));
	if(!player)
	{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
		return 1;
	}

	lua_newtable(L);
	setField(L, "chase", player->getChaseMode());
	setField(L, "fight", player->getFightMode());
	setField(L, "safe", player->hasSafeMode());
	return 1;
}

int LuaScriptInterface::luaGetPlayerStorageValueByName(lua_State *L)
{
	//getPlayerStorageValueByName(name, key)
	uint32_t key = popNumber(L);
	std::string name = popString(L);
	int32_t ret;
	if (Player::getStorageValueByName(name, key, ret))
		lua_pushnumber(L,ret);
	else
		lua_pushnil(L);
	return 1;
}

int LuaScriptInterface::luaSetPlayerStorageValueByName(lua_State *L)
{
	//setPlayerStorageValueByName(name, key, val)
	int32_t value = popNumber(L);
	uint32_t key = popNumber(L);
	std::string name = popString(L);

	lua_pushboolean(L, Player::setStorageValueByName(name, key, value));

	return 1;
}

int LuaScriptInterface::luaDoErasePlayerStorageValueByName(lua_State *L)
{
	//doErasePlayerStorageValueByName(name, key)
	uint32_t key = popNumber(L);
	std::string name = popString(L);

	lua_pushboolean(L, Player::eraseStorageValueByName(name, key));

	return 1;
}

int LuaScriptInterface::luaDoErasePlayerStorageValue(lua_State *L)
{
	//erasePlayerStorageValue(cid, key)
	uint32_t key = popNumber(L);
	uint32_t cid = popNumber(L);
	if(IS_IN_KEYRANGE(key, RESERVED_RANGE)){
		std::stringstream error_str;
		error_str << "Accessing to reserved range: "  << key;
		reportErrorFunc(error_str.str());
		lua_pushboolean(L, false);
		return 1;
	}

	ScriptEnviroment* env = getScriptEnv();

	Player* player = env->getPlayerByUID(cid);
	if(player){
		player->eraseStorageValue(key);
		lua_pushboolean(L, true);
	}
	else{
		reportErrorFunc(getErrorDesc(LUA_ERROR_PLAYER_NOT_FOUND));
		lua_pushboolean(L, false);
	}
	return 1;
}

int LuaScriptInterface::luaDoEraseGlobalStorageValue(lua_State *L)
{
	//eraseGlobalStorageValue(key)
	uint32_t key = popNumber(L);

	ScriptEnviroment* env = getScriptEnv();

	lua_pushboolean(L, env->eraseGlobalStorageValue(key));
	return 1;
}

//doSavePlayer(cid)
int LuaScriptInterface::luaDoSavePlayer(lua_State *L)
{
	uint32_t cid = popNumber(L);
	ScriptEnviroment* env = getScriptEnv();
 
	Player* player = env->getPlayerByUID(cid);
 
	if (player){
		if (IOPlayer::instance()->savePlayer(player)){
			lua_pushboolean(L, true);
			return 1;
		} else {
			lua_pushboolean(L, false);
		}
	} else {
		lua_pushboolean(L, false);
	}
 
	return 0;
}
