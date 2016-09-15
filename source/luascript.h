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


#ifndef __OTSERV_LUASCRIPT_H__
#define __OTSERV_LUASCRIPT_H__

#include "definitions.h"
#include "position.h"
#include "database.h"
#include <string>
#include <map>
#include <list>
#include <vector>
#include <cassert>

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

class Thing;
class Creature;
class Player;
class Item;
class Container;
class AreaCombat;
class Combat;
class Condition;
class Npc;
class Spell;

enum LuaVariantType_t{
	VARIANT_NONE = 0,
	VARIANT_NUMBER,
	VARIANT_POSITION,
	VARIANT_TARGETPOSITION,
	VARIANT_STRING
};

struct LuaVariant{
	LuaVariant()
	{
		type = VARIANT_NONE;
		text = "";
		pos.x = 0;
		pos.y = 0;
		pos.z = 0;
		pos.stackpos = 0;
		number = 0;
	}

	LuaVariantType_t type;
	std::string text;
	PositionEx pos;
	uint32_t number;
};

class LuaScriptInterface;
class Game;

class ScriptEnviroment
{
public:
	ScriptEnviroment();
	~ScriptEnviroment();

	void resetEnv();
	void resetCallback() {m_callbackId = 0;}

	void setScriptId(int32_t scriptId, LuaScriptInterface* scriptInterface)
		{m_scriptId = scriptId; m_interface = scriptInterface;}
	bool setCallbackId(int32_t callbackId, LuaScriptInterface* scriptInterface);
	void setEventDesc(const std::string& desc) {m_eventdesc = desc;}

	std::string getEventDesc() const {return m_eventdesc;}
	int32_t getScriptId() const {return m_scriptId;}
	int32_t getCallbackId() const {return m_callbackId;}
	LuaScriptInterface* getScriptInterface() {return m_interface;}

	void setTimerEvent() {m_timerEvent = true;}
	void resetTimerEvent() {m_timerEvent = false;}

	void getEventInfo(int32_t& scriptId, std::string& desc, LuaScriptInterface*& scriptInterface, int32_t& callbackId, bool& timerEvent);

	static void addTempItem(ScriptEnviroment* env, Item* item);
	static void removeTempItem(ScriptEnviroment* env, Item* item);
	static void removeTempItem(Item* item);
	static void addUniqueThing(Thing* thing);
	static void removeUniqueThing(Thing* thing);
	uint32_t addThing(Thing* thing);
	void insertThing(uint32_t uid, Thing* thing);

	DBResult* getResultByID(uint32_t id);
	uint32_t addResult(DBResult* res);
	bool removeResult(uint32_t id);
	
	void addGlobalStorageValue(const uint32_t key, const int32_t value);
	bool getGlobalStorageValue(const uint32_t key, int32_t& value) const;
	bool eraseGlobalStorageValue(const uint32_t key);

	void setRealPos(const Position& realPos) {m_realPos = realPos;}
	Position getRealPos() {return m_realPos;}

	void setNpc(Npc* npc) {m_curNpc = npc;}
	Npc* getNpc() const {return m_curNpc;}

	void setSpell(Spell* spell) {m_curSpell = spell;}
	Spell* getSpell() const {return m_curSpell;}

	Thing* getThingByUID(uint32_t uid);
	Item* getItemByUID(uint32_t uid);
	Container* getContainerByUID(uint32_t uid);
	Creature* getCreatureByUID(uint32_t uid);
	Player* getPlayerByUID(uint32_t uid);
	void removeItemByUID(uint32_t uid);

	static uint32_t addCombatArea(AreaCombat* area);
	static AreaCombat* getCombatArea(uint32_t areaId);

	static uint32_t addCombatObject(Combat* combat);
	static Combat* getCombatObject(uint32_t combatId);

	static uint32_t addConditionObject(Condition* condition);
	static Condition* getConditionObject(uint32_t conditionId);

	static bool loadGameState();
	static bool saveGameState();

	static uint32_t getLastCombatId() {return m_lastCombatId;}

private:
	typedef std::map<uint32_t, Thing*> ThingMap;
	typedef std::vector<const LuaVariant*> VariantVector;
	typedef std::map<uint32_t, int32_t> StorageMap;
	typedef std::map<uint32_t, AreaCombat*> AreaMap;
	typedef std::map<uint32_t, Combat*> CombatMap;
	typedef std::map<uint32_t, Condition*> ConditionMap;
	typedef std::list<Item*> ItemList;
	typedef std::map<uint32_t, DBResult*> DBResultMap;

	//script file id
	int32_t m_scriptId;
	int32_t m_callbackId;
	bool m_timerEvent;
	LuaScriptInterface* m_interface;
	//script event desc
	std::string m_eventdesc;

	static StorageMap m_globalStorageMap;
	//unique id map
	static ThingMap m_globalMap;

	Position m_realPos;

	//item/creature map
	int32_t m_lastUID;
	ThingMap m_localMap;

	//temporary item list
	typedef std::map<ScriptEnviroment*, ItemList> TempItemListMap;
	static TempItemListMap m_tempItems;

	//area map
	static uint32_t m_lastAreaId;
	static AreaMap m_areaMap;

	//combat map
	static uint32_t m_lastCombatId;
	static CombatMap m_combatMap;

	//condition map
	static uint32_t m_lastConditionId;
	static ConditionMap m_conditionMap;

	//result map
	static uint32_t m_lastResultId;
	static DBResultMap m_tempResults;	
	
	//for npc scripts
	Npc* m_curNpc;

	//for spell initialization
	Spell* m_curSpell;
};

class Position;

enum PlayerInfo_t{
	PlayerInfoFood,
	PlayerInfoAccess,
	PlayerInfoLevel,
	PlayerInfoMagLevel,
	PlayerInfoMana,
	PlayerInfoMaxMana,
	PlayerInfoVocation,
	PlayerInfoMasterPos,
	PlayerInfoSoul,
	PlayerInfoFreeCap,
	PlayerInfoGuildId,
	PlayerInfoGuildName,
	PlayerInfoGuildRank,
	PlayerInfoGuildNick,
	PlayerInfoGuildLevel,
	PlayerInfoSex,
	PlayerInfoTown,
	PlayerInfoGUID,
	PlayerInfoPremiumDays,
	PlayerInfoSkullEndTime,
	PlayerInfoUnjustKills,
	PlayerInfoBalance,
	PlayerInfoPzLock,
	PlayerInfoPremium,
	PlayerInfoLastLogin,
	PlayerInfoIp,
	PlayerInfoGmInvisible,
	PlayerInfoAccountId,
	PlayerInfoStamina
};

#define reportErrorFunc(a)  reportError(__FUNCTION__, a)

enum ErrorCode_t{
	LUA_ERROR_PLAYER_NOT_FOUND,
	LUA_ERROR_CREATURE_NOT_FOUND,
	LUA_ERROR_ITEM_NOT_FOUND,
	LUA_ERROR_THING_NOT_FOUND,
	LUA_ERROR_TILE_NOT_FOUND,
	LUA_ERROR_HOUSE_NOT_FOUND,
	LUA_ERROR_COMBAT_NOT_FOUND,
	LUA_ERROR_CONDITION_NOT_FOUND,
	LUA_ERROR_AREA_NOT_FOUND,
	LUA_ERROR_CONTAINER_NOT_FOUND,
	LUA_ERROR_VARIANT_NOT_FOUND,
	LUA_ERROR_VARIANT_UNKNOWN,
	LUA_ERROR_SPELL_NOT_FOUND
};


class LuaScriptInterface
{
public:
	LuaScriptInterface(std::string interfaceName);
	virtual ~LuaScriptInterface();

	virtual bool initState();
	bool reInitState();

	int32_t loadFile(const std::string& file, bool reserveEnviroment = true);
	int32_t loadBuffer(const std::string& text, bool reserveEnviroment = true);
	const std::string& getFileById(int32_t scriptId);

	int32_t getEvent(const std::string& eventName);

	static ScriptEnviroment* getScriptEnv(){
		assert(m_scriptEnvIndex >= 0 && m_scriptEnvIndex < 16);
		return &m_scriptEnv[m_scriptEnvIndex];
	}

	static bool reserveScriptEnv(){
		++m_scriptEnvIndex;
		if(m_scriptEnvIndex < 15){
			return true;
		}
		else{
			--m_scriptEnvIndex;
			return false;
		}
	}

	static void releaseScriptEnv(){
		if(m_scriptEnvIndex >= 0){
			m_scriptEnv[m_scriptEnvIndex].resetEnv();
			--m_scriptEnvIndex;
		}
	}

	static void reportError(const char* function, const std::string& error_desc);

	std::string getInterfaceName() {return m_interfaceName;}
	const std::string& getLastLuaError() const {return m_lastLuaError;}
	void dumpLuaStack();

	lua_State* getLuaState() {return m_luaState;}

	bool pushFunction(int32_t functionId);

	static int luaErrorHandler(lua_State* L);
	bool callFunction(uint32_t nParams, bool getReturnValue = true);

	//push/pop common structures
	static void pushThing(lua_State *L, Thing* thing, uint32_t thingid);
	static void pushVariant(lua_State *L, const LuaVariant& var);
	static void pushPosition(lua_State *L, const PositionEx& position);
	static void pushPosition(lua_State *L, const Position& position, uint32_t stackpos);
	static void pushCallback(lua_State *L, int32_t callback);

	static LuaVariant popVariant(lua_State *L);
	static void popPosition(lua_State *L, PositionEx& position);
	static void popPosition(lua_State *L, Position& position, uint32_t& stackpos);
	static bool popBoolean(lua_State *L, bool acceptIntegers = true);
	static uint32_t popNumber(lua_State *L, bool acceptBooleans = false);
	static double popFloatNumber(lua_State *L);
	static std::string popString(lua_State *L);
	static int32_t popCallback(lua_State *L);

	static int32_t getField(lua_State *L, const char *key);
	static uint32_t getFieldU32(lua_State *L, const char *key);
	static void setField(lua_State *L, const char* index, double val);
	static void setField(lua_State *L, const char* index, const std::string& val);
	static std::string getFieldString(lua_State *L, const char *key);
	static void setFieldBool(lua_State *L, const char* index, bool val);
	static bool getFieldBool(lua_State *L, const char *key);
	static std::string escapeString(const std::string& string);

protected:
	virtual bool closeState();

	virtual void registerFunctions();

	static std::string getErrorDesc(ErrorCode_t code);
	static bool getArea(lua_State *L, std::list<uint32_t>& list, uint32_t& rows);

	//lua functions
	static int luaGetOTSYSTime(lua_State *L);
	static int luaGetConfigValue(lua_State *L);
	static int luaDoRemoveItem(lua_State *L);
	static int luaDoFeedPlayer(lua_State *L);
	static int luaDoSendCancel(lua_State *L);
	static int luaDoSendDefaultCancel(lua_State *L);
	static int luaDoPlayerSetIdleTime(lua_State *L);
	static int luaDoTeleportThing(lua_State *L);
	static int luaDoTransformItem(lua_State *L);
	static int luaDoSendMagicEffect(lua_State *L);
	static int luaDoSendDistanceShoot(lua_State *L);
	static int luaDoChangeTypeItem(lua_State *L);
	static int luaDoSendAnimatedText(lua_State *L);
	static int luaSendOutfitWindow(lua_State *L);
	static int luaDoShowTextWindow(lua_State *L);
	static int luaDoShowTextDialog(lua_State *L);
	static int luaDoDecayItem(lua_State *L);
	static int luaDoCreateItem(lua_State *L);
	static int luaDoCreateItemEx(lua_State *L);
	static int luaDoCreateTeleport(lua_State *L);
	static int luaDoSummonCreature(lua_State *L);
	static int luaDoPlayerSummonCreature(lua_State *L);
	static int luaDoRemoveCreature(lua_State *L);
	static int luaDoMoveCreature(lua_State *L);
	static int luaDoSetCreatureDirection(lua_State *L);
	static int luaDoCreatureSay(lua_State *L);
	static int luaDoCreatureAddHealth(lua_State *L);
	static int luaGetCreatureLight(lua_State *L);
	static int luaGetCreatureLookDir(lua_State *L);

	static int luaDoPlayerAddInFightTicks(lua_State *L);
	static int luaDoPlayerAddSkillTry(lua_State *L);
	static int luaDoPlayerAddManaSpent(lua_State *L);
	static int luaDoPlayerAddMana(lua_State *L);
	static int luaDoPlayerAddItem(lua_State *L);
	static int luaDoPlayerAddItemEx(lua_State *L);
	static int luaDoTileAddItemEx(lua_State *L);
	static int luaAddContainerItemEx(lua_State *L);
	static int luaDoRelocate(lua_State *L);
	static int luaDoPlayerSendTextMessage(lua_State *L);
	static int luaDoPlayerRemoveMoney(lua_State *L);
	static int luaDoPlayerAddMoney(lua_State *L);
	static int luaDoPlayerWithdrawMoney(lua_State *L);
	static int luaDoPlayerDepositMoney(lua_State *L);
	static int luaDoPlayerTransferMoneyTo(lua_State *L);
	static int luaDoPlayerSetMasterPos(lua_State *L);
	static int luaDoPlayerSetTown(lua_State *L);
	static int luaDoPlayerSetVocation(lua_State *L);
	static int luaDoPlayerSetSex(lua_State *L);
	static int luaDoPlayerRemoveItem(lua_State *L);
	static int luaDoPlayerAddSoul(lua_State *L);
	static int luaDoPlayerAddExp(lua_State *L);
	static int luaDoPlayerRemoveExp(lua_State *L);
	static int luaDoPlayerSetGuildRank(lua_State *L);
	static int luaDoPlayerSetGuildNick(lua_State *L);
	static int luaDoSetCreatureLight(lua_State *L);
	static int luaDoPlayerSetLossPercent(lua_State *L);
	static int luaDoSetCreatureDropLoot(lua_State *L);
	static int luaGetPlayerSkullType(lua_State *L);
	static int luaSetPlayerSkullType(lua_State *L);
	static int luaGetPlayerSkullEndTime(lua_State *L);
	static int luaGetPlayerUnjustKills(lua_State *L);
	static int luaGetTownIdByName(lua_State *L);
	static int luaGetTownNameById(lua_State *L);
	static int luaGetTownTemplePosition(lua_State *L);
	static int luaGetWaypointPositionByName(lua_State *L);

	//queries
	static int luaGetPlayerGUIDByName(lua_State *L);
	static int luaGetPlayerByAccountNumber(lua_State *L);
	static int luaGetAccountNumberByPlayerName(lua_State *L);
	static int luaGetIPByPlayerName(lua_State *L);
	static int luaGetPlayersByIPAddress(lua_State *L);

	//get item info
	static int luaGetItemRWInfo(lua_State *L);
	static int luaGetThingFromPos(lua_State *L);
	static int luaGetThing(lua_State *L);
	static int luaGetThingPos(lua_State *L);
	static int luaGetTileStackItemsSize(lua_State *L);
	static int luaGetTileItemById(lua_State *L);
	static int luaGetTileItemByType(lua_State *L);
	static int luaGetTileThingByPos(lua_State *L);
	static int luaGetTileThingByTopOrder(lua_State *L);
	static int luaGetTopCreature(lua_State *L);
	static int luaGetAllCreatures(lua_State *L);
	static int luaGetThingDefaultDescription(lua_State *L);
	static int luaGetItemTypeDefaultDescription(lua_State *L);
	static int luaGetItemSpecialDescription(lua_State *L);

	//set item
	static int luaDoSetItemActionId(lua_State *L);
	static int luaDoSetItemText(lua_State *L);
	static int luaDoSetItemSpecialDescription(lua_State *L);

	//get tile info
	static int luaGetTilePzInfo(lua_State *L);
	static int luaGetTileHouseInfo(lua_State *L);
	static int luaQueryTileAddThing(lua_State *L);

	//houses
	static int luaGetHouseOwner(lua_State *L);
	static int luaGetHouseName(lua_State *L);
	static int luaGetHouseEntry(lua_State *L);
	static int luaGetHouseRent(lua_State *L);
	static int luaGetHouseTown(lua_State *L);
	static int luaGetHouseAccessList(lua_State *L);
	static int luaGetHouseByPlayerGUID(lua_State *L);
	static int luaGetHouseTilesSize(lua_State *L);
	static int luaGetHouseDoorCount(lua_State *L);
	static int luaGetHouseBedCount(lua_State *L);
	static int luaIsHouseGuildHall(lua_State *L);
	static int luaSetHouseOwner(lua_State *L);
	static int luaSetHouseAccessList(lua_State *L);
	static int luaGetHouseList(lua_State *L);
	static int luaCleanHouse(lua_State *L);

	//get player info functions
	static int luaGetPlayerFood(lua_State *L);
	static int luaGetPlayerAccess(lua_State *L);
	static int luaGetPlayerLevel(lua_State *L);
	static int luaGetPlayerMagLevel(lua_State *L);
	static int luaGetPlayerMana(lua_State *L);
	static int luaGetPlayerMaxMana(lua_State *L);
	static int luaGetPlayerSkill(lua_State *L);
	static int luaGetPlayerVocation(lua_State *L);
	static int luaGetPlayerMasterPos(lua_State *L);
	static int luaGetPlayerTown(lua_State *L);
	static int luaGetPlayerItemCount(lua_State *L);
	static int luaGetPlayerSoul(lua_State *L);
	static int luaGetPlayerFreeCap(lua_State *L);
	static int luaGetPlayerSlotItem(lua_State *L);
	static int luaGetPlayerItemById(lua_State *L);
	static int luaGetPlayerDepotItems(lua_State *L);
	static int luaGetPlayerGuildId(lua_State *L);
	static int luaGetPlayerGuildName(lua_State *L);
	static int luaGetPlayerGuildRank(lua_State *L);
	static int luaGetPlayerGuildNick(lua_State *L);
	static int luaGetPlayerGuildLevel(lua_State *L);
	static int luaGetPlayerSex(lua_State *L);
	static int luaGetPlayerGUID(lua_State *L);
	static int luaGetPlayerFlagValue(lua_State *L);
	static int luaGetPlayerLossPercent(lua_State *L);
	static int luaGetPlayerPremiumDays(lua_State *L);
	static int luaGetPlayerAccountBalance(lua_State *L);
	static int luaGetPlayerByNameWildcard(lua_State *L);
	static int luaIsPremium(lua_State *L);
	static int luaGetPlayerLastLogin(lua_State *L);
	static int luaGetPlayerExperience(lua_State *L);

	static int luaPlayerLearnInstantSpell(lua_State *L);
	static int luaCanPlayerLearnInstantSpell(lua_State *L);
	static int luaGetPlayerLearnedInstantSpell(lua_State *L);
	static int luaGetPlayerInstantSpellInfo(lua_State *L);
	static int luaGetPlayerInstantSpellCount(lua_State *L);
	static int luaGetInstantSpellInfoByName(lua_State *L);
	static int luaGetInstantSpellWords(lua_State *L);

	static int luaGetPlayerStorageValue(lua_State *L);
	static int luaSetPlayerStorageValue(lua_State *L);
	static int luaDoErasePlayerStorageValueByName(lua_State *L);
	static int luaSetPlayerStorageValueByName(lua_State *L);
	static int luaGetPlayerStorageValueByName(lua_State *L);
	static int luaDoErasePlayerStorageValue(lua_State *L);

	static int luaGetGlobalStorageValue(lua_State *L);
	static int luaSetGlobalStorageValue(lua_State *L);
	static int luaDoEraseGlobalStorageValue(lua_State *L);

	static int luaIsGmInvisible(lua_State *L);
	static int luaDoPlayerToogleGmInvisible(lua_State *L);
	static int luaGetPlayerAccountId(lua_State *L);
	static int luaDoPlayerAddPremiumDays(lua_State *L);
	static int luaDoPlayerRemovePremiumDays(lua_State *L);

	static int luaGetFirstItemFromInventory(lua_State *L);
	static int luaGetCreatureConditionInfo(lua_State *L);
	static int luaGetCreatureCondition(lua_State *L);
	static int luaGetPlayerModes(lua_State* L);
	static int luaDoSavePlayer(lua_State *L);
	static int luaDoPlayerOpenChannel(lua_State* L);

	static int luaGetWorldType(lua_State *L);
	static int luaGetWorldTime(lua_State *L);
	static int luaGetWorldLight(lua_State *L);
	static int luaGetWorldCreatures(lua_State *L);
	static int luaGetWorldUpTime(lua_State *L);
	static int luaGetPlayersOnlineList(lua_State *L);
	static int luaDoPlayerBroadcastMessage(lua_State *L);
	static int luaGetGuildId(lua_State *L);

	//type validation
	static int luaIsMonsterName(lua_State *L);
	static int luaIsValidItemId(lua_State *L);
	static int luaIsCreature(lua_State *L);
	static int luaIsContainer(lua_State *L);
	static int luaIsCorpse(lua_State *L);
	static int luaIsMoveable(lua_State *L);
	static int luaIsValidUID(lua_State *L);

	//container
	static int luaGetContainerSize(lua_State *L);
	static int luaGetContainerCap(lua_State *L);
	static int luaGetContainerItem(lua_State *L);
	static int luaDoAddContainerItem(lua_State *L);
	static int luaGetDepotId(lua_State *L);

	// bans
	static int luaAddPlayerBan(lua_State *L);
	static int luaAddAccountBan(lua_State *L);
	static int luaAddIPBan(lua_State *L);
	static int luaRemoveAccountBan(lua_State *L);
	static int luaRemovePlayerBan(lua_State *L);
	static int luaRemoveIPBan(lua_State *L);
	static int luaGetPlayerBanList(lua_State *L);
	static int luaGetAccountBanList(lua_State *L);
	static int luaGetIPBanList(lua_State *L);

	//
	static int luaCreateCombatObject(lua_State *L);
	static int luaCreateCombatArea(lua_State *L);
	static int luaSetCombatArea(lua_State *L);
	static int luaSetCombatCondition(lua_State *L);
	static int luaSetCombatParam(lua_State *L);
	static int luaCreateConditionObject(lua_State *L);
	static int luaSetConditionParam(lua_State *L);
	static int luaAddDamageCondition(lua_State *L);
	static int luaAddOutfitCondition(lua_State *L);

	static int luaSetCombatCallBack(lua_State *L);
	static int luaSetCombatFormula(lua_State *L);
	static int luaSetConditionFormula(lua_State *L);
	static int luaDoCombat(lua_State *L);

	static int luaDoAreaCombatHealth(lua_State *L);
	static int luaDoTargetCombatHealth(lua_State *L);

	//
	static int luaDoAreaCombatMana(lua_State *L);
	static int luaDoTargetCombatMana(lua_State *L);

	static int luaDoAreaCombatCondition(lua_State *L);
	static int luaDoTargetCombatCondition(lua_State *L);

	static int luaDoAreaCombatDispel(lua_State *L);
	static int luaDoTargetCombatDispel(lua_State *L);

	static int luaDoChallengeCreature(lua_State *L);
	static int luaDoConvinceCreature(lua_State *L);
	static int luaGetMonsterTargetList(lua_State *L);
	static int luaGetMonsterFriendList(lua_State *L);
	static int luaDoSetMonsterTarget(lua_State *L);
	static int luaDoMonsterChangeTarget(lua_State *L);
	static int luaDoAddCondition(lua_State *L);
	static int luaDoRemoveCondition(lua_State *L);

	static int luaNumberToVariant(lua_State *L);
	static int luaStringToVariant(lua_State *L);
	static int luaPositionToVariant(lua_State *L);
	static int luaTargetPositionToVariant(lua_State *L);

	static int luaVariantToNumber(lua_State *L);
	static int luaVariantToString(lua_State *L);
	static int luaVariantToPosition(lua_State *L);

	static int luaDoChangeSpeed(lua_State *L);

	static int luaDoCreatureChangeOutfit(lua_State *L);
	static int luaSetCreatureOutfit(lua_State *L);
	static int luaGetCreatureOutfit(lua_State *L);
	static int luaSetMonsterOutfit(lua_State *L);
	static int luaSetItemOutfit(lua_State *L);
	static int luaGetCreaturePosition(lua_State *L);
	static int luaGetCreatureName(lua_State *L);
	static int luaGetCreatureSpeed(lua_State *L);
	static int luaGetCreatureBaseSpeed(lua_State *L);
	static int luaGetCreatureTarget(lua_State *L);
	static int luaGetCreatureHealth(lua_State *L);
	static int luaGetCreatureMaxHealth(lua_State *L);
	static int luaGetCreatureByName(lua_State *L);
	static int luaGetCreatureMaster(lua_State *L);
	static int luaGetCreatureSummons(lua_State *L);
	static int luaGetSpectators(lua_State *L);
	static int luaGetPartyMembers(lua_State *L);
	static int luaHasCondition(lua_State *L);

	static int luaIsCreatureImmuneToCondition(lua_State *L);
	static int luaHasProperty(lua_State *L);
	static int luaIsItemTwoHandedByUID(lua_State *L);
	static int luaIsItemTwoHanded(lua_State *L);
	static int luaIsItemStackable(lua_State *L);
	static int luaIsItemRune(lua_State *L);
	static int luaIsItemDoor(lua_State *L);
	static int luaIsItemContainer(lua_State *L);
	static int luaIsItemFluidContainer(lua_State *L);
	static int luaIsItemMoveable(lua_State *L);
	static int luaGetItemName(lua_State *L);
	static int luaGetItemDescriptions(lua_State *L);
	static int luaGetItemWeight(lua_State *L);
	static int luaGetItemIdByName(lua_State *L);
	static int luaIsSightClear(lua_State *L);
	static int luaGetPlayerNameByGUID(lua_State *L);

	static int luaDebugPrint(lua_State *L);
	static int luaIsIntegerInArray(lua_State *L);
	static int luaGetFluidSourceType(lua_State *L);
	static int luaAddEvent(lua_State *L);
	static int luaStopEvent(lua_State *L);
	static int luaRegisterCreatureEvent(lua_State *L);
	static int luaGetDataDirectory(lua_State *L);
	static int luaDoPlayerSetRate(lua_State *L);
	static int luaDoPlayerSetVipLimit(lua_State *L);
	static int luaDoPlayerSetDepotLimit(lua_State *L);
	static int luaIsPzLocked(lua_State *L);
	static int luaDoSaveServer(lua_State *L);
	static int luaDoSetGameState(lua_State *L);
	static int luaDoReloadInfo(lua_State *L);
	static int luaDoRefreshMap(lua_State *L);
	//
	static int luaGetPlayerIp(lua_State *L);

	static int internalGetPlayerInfo(lua_State *L, PlayerInfo_t info);

	static int luaIsNpcName(lua_State *L);
	static int luaGetMonsterParameter(lua_State *L);
	static int luaGetNpcParameterByName(lua_State *L);

	static const luaL_Reg luaBitReg[13];
	static int luaBitNot(lua_State *L);
	static int luaBitAnd(lua_State *L);
	static int luaBitOr(lua_State *L);
	static int luaBitXor(lua_State *L);
	static int luaBitLeftShift(lua_State *L);
	static int luaBitRightShift(lua_State *L);
	static int luaBitUNot(lua_State *L);
	static int luaBitUAnd(lua_State *L);
	static int luaBitUOr(lua_State *L);
	static int luaBitUXor(lua_State *L);
	static int luaBitULeftShift(lua_State *L);
	static int luaBitURightShift(lua_State *L);

	static const luaL_Reg luaDatabaseTable[10];
	static int32_t luaDatabaseExecute(lua_State* L);
	static int32_t luaDatabaseStoreQuery(lua_State* L);
	static int32_t luaDatabaseEscapeString(lua_State* L);
	static int32_t luaDatabaseEscapeBlob(lua_State* L);
	static int32_t luaDatabaseLastInsertId(lua_State* L);
	static int32_t luaDatabaseStringComparer(lua_State* L);
	static int32_t luaDatabaseUpdateLimiter(lua_State* L);
	static int32_t luaDatabaseConnected(lua_State* L);
	static int32_t luaDatabaseTableExists(lua_State* L);

	static const luaL_Reg luaResultTable[8];
	static int32_t luaResultGetDataInt(lua_State* L);
	static int32_t luaResultGetDataLong(lua_State* L);
	static int32_t luaResultGetDataString(lua_State* L);
	static int32_t luaResultGetDataStream(lua_State* L);
	static int32_t luaResultGetAllData(lua_State* L);
	static int32_t luaResultNext(lua_State* L);
	static int32_t luaResultFree(lua_State* L);
	
	static int luaGetItemWeaponType(lua_State *L);
	static int luaGetItemAttack(lua_State *L);
	static int luaGetItemDefense(lua_State *L);
	static int luaGetItemExtraDef(lua_State *L);
	static int luaGetItemArmor(lua_State *L);
	static int luaGetItemWeaponTypeByUID(lua_State *L);
	static int luaGetItemAttackByUID(lua_State *L);
	static int luaGetItemDefenseByUID(lua_State *L);
	static int luaGetItemExtraDefByUID(lua_State *L);
	static int luaGetItemArmorByUID(lua_State *L);

	lua_State* m_luaState;
	std::string m_lastLuaError;

private:

	static ScriptEnviroment m_scriptEnv[16];
	static int32_t m_scriptEnvIndex;

	int32_t m_runningEventId;
	std::string m_loadingFile;

	//script file cache
	typedef std::map<int32_t , std::string> ScriptsCache;
	ScriptsCache m_cacheFiles;

	//events information
	struct LuaTimerEventDesc{
		int32_t scriptId;
		int function;
		std::list<int> parameters;
		uint32_t eventId;
	};
	uint32_t m_lastEventTimerId;

	typedef std::map<uint32_t , LuaTimerEventDesc > LuaTimerEvents;
	LuaTimerEvents m_timerEvents;

	void executeTimerEvent(uint32_t eventIndex);

	std::string m_interfaceName;
};

#endif
