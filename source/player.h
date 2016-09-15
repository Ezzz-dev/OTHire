//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
//
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

#ifndef __OTSERV_PLAYER_H__
#define __OTSERV_PLAYER_H__

#include "definitions.h"
#include "otsystem.h"
#include "creature.h"
#include "container.h"
#include "depot.h"
#include "cylinder.h"
#include "enums.h"
#include "vocation.h"
#include "protocolgame.h"
#include <vector>
#include <ctime>
#include <algorithm>

class House;
class Weapon;
class ProtocolGame;
class Npc;
class Party;
class SchedulerTask;
class Guild;

enum skillsid_t {
	SKILL_LEVEL=0,
	SKILL_TRIES=1,
	SKILL_PERCENT=2
};

enum playerinfo_t {
	PLAYERINFO_LEVEL,
	PLAYERINFO_LEVELPERCENT,
	PLAYERINFO_HEALTH,
	PLAYERINFO_MAXHEALTH,
	PLAYERINFO_MANA,
	PLAYERINFO_MAXMANA,
	PLAYERINFO_MAGICLEVEL,
	PLAYERINFO_MAGICLEVELPERCENT,
	PLAYERINFO_SOUL
};

enum freeslot_t {
	SLOT_TYPE_NONE,
	SLOT_TYPE_INVENTORY,
	SLOT_TYPE_CONTAINER
};

enum chaseMode_t {
	CHASEMODE_STANDSTILL,
	CHASEMODE_FOLLOW
};

enum fightMode_t {
	FIGHTMODE_ATTACK,
	FIGHTMODE_BALANCED,
	FIGHTMODE_DEFENSE
};

enum tradestate_t {
	TRADE_NONE,
	TRADE_INITIATED,
	TRADE_ACCEPT,
	TRADE_ACKNOWLEDGE,
	TRADE_TRANSFER
};

typedef std::pair<uint32_t, Container*> containervector_pair;
typedef std::vector<containervector_pair> ContainerVector;
typedef std::map<uint32_t, Depot*> DepotMap;
typedef std::map<uint32_t, int32_t> StorageMap;
typedef std::set<uint32_t> VIPListSet;
typedef std::map<uint32_t, uint32_t> MuteCountMap;
typedef std::map<uint32_t, std::string> ChannelStatementMap;
typedef std::list<std::string> LearnedInstantSpellList;
typedef std::list<Party*> PartyList;

#define PLAYER_MAX_SPEED 1500
#define PLAYER_MIN_SPEED 10

//////////////////////////////////////////////////////////////////////
// Defines a player...

class Player : public Creature, public Cylinder
{
public:
#ifdef __ENABLE_SERVER_DIAGNOSTIC__
	static uint32_t playerCount;
#endif

	Player(const std::string& name, ProtocolGame* p);
	virtual ~Player();

	virtual Player* getPlayer() {return this;}
	virtual const Player* getPlayer() const {return this;}

	static MuteCountMap muteCountMap;
	static ChannelStatementMap channelStatementMap;
	static uint32_t channelStatementGuid;

	virtual const std::string& getName() const {return name;}
	virtual const std::string& getNameDescription() const {return name;}
	virtual std::string getDescription(int32_t lookDistance) const;

	void setGUID(uint32_t _guid) {guid = _guid;};
	uint32_t getGUID() const { return guid;};
	virtual uint32_t idRange(){ return PLAYER_ID_RANGE;}
	static AutoList<Player> listPlayer;
	void removeList();
	void addList();
	void kickPlayer();

	static uint64_t getExpForLevel(int32_t level)
    {
      
        level--;
        //"original"
            return ((50ULL * level * level * level) - (150ULL * level * level) + (400ULL * level))/3ULL;       
        
      
        //"tibianic"
        //   return (level * 35ULL) * (level * level * level) / 500ULL + 10ULL * level;
         
      
        //"mastercores"
        //    return ((level * 50ULL) * (level * level * level) / 500ULL)+10ULL;
        
      }

	//[ guild settings
	void setGuild(Guild* _guild) { guild = _guild; }
	Guild* getGuild() const { return guild; }
	uint32_t getGuildId() const;

	std::string guildRank, guildNick;
	uint32_t guildLevel;
	//]
	
	//[ update inventory public
	void updateInventoryWeight();
	//]

	//[ gm invisible
	void toogleGmInvisible();
	bool isGmInvisible() const {return gmInvisible;}
	bool canSeeGmInvisible(const Player* player) const { return (player->getAccessLevel() <= getAccessLevel()); }
	bool hasSomeInvisibilityFlag() const { return hasFlag(PlayerFlag_CannotBeSeen) || isGmInvisible(); }
	//]
	
	void setFlags(uint64_t flags){ groupFlags = flags;}
	bool hasFlag(PlayerFlags value) const { return (0 != (groupFlags & ((uint64_t)1 << value)));}

	uint16_t getPremiumDays() const {return premiumDays;}
	bool isPremium() const {return (premiumDays > 0 || hasFlag(PlayerFlag_IsAlwaysPremium));}

	bool isOffline() const {return (getID() == 0);}
	bool isOnline() const {return !isOffline();}
	void disconnect() {if(client) client->disconnect();}
	uint32_t getIP() const;

	void addContainer(uint32_t cid, Container* container);
	void closeContainer(uint32_t cid);
	int32_t getContainerID(const Container* container) const;
	Container* getContainer(uint32_t cid);

	void addStorageValue(const uint32_t key, const int32_t value);
	bool getStorageValue(const uint32_t key, int32_t& value) const;
	bool eraseStorageValue(const uint32_t key);
	static bool getStorageValueByName(const std::string name, const uint32_t key, int32_t& value);
	static bool setStorageValueByName(const std::string name, const uint32_t key, const int32_t value);
	static bool eraseStorageValueByName(const std::string name, const uint32_t key);

	bool withdrawMoney(uint32_t amount);
	bool depositMoney(uint32_t amount);
	bool transferMoneyTo(const std::string& name, uint32_t amount);
	uint32_t balance;

	inline StorageMap::const_iterator getStorageIteratorBegin() const {return storageMap.begin();}
	inline StorageMap::const_iterator getStorageIteratorEnd() const {return storageMap.end();}

	uint32_t getAccountId() const {return accountId;}
	uint32_t getLevel() const {return level;}
	uint32_t getMagicLevel() const {return getPlayerInfo(PLAYERINFO_MAGICLEVEL);}
	int16_t getAccessLevel() const {return accessLevel;}
	int16_t getViolationLevel() const {return violationLevel;}
	std::string getGroupName() const {return groupName;}

	bool setVocation(uint32_t vocId);
	uint32_t getVocationId() const;
	Vocation* getVocation() const {return vocation;};

	PlayerSex_t getSex() const {return sex;}
	bool isMale() const {return getSex() % 2 != 0;}
	bool isFemale() const {return getSex() % 2 == 0;}

	void setSex(PlayerSex_t player_sex);
	int32_t getPlayerInfo(playerinfo_t playerinfo) const;
	int64_t getExperience() const {return experience;}

	time_t getLastLoginSaved() const {return lastLoginSaved;}
	const Position& getLoginPosition() const {return loginPosition;}
	const Position& getTemplePosition() const {return masterPos;}
	uint32_t getTown() const {return town;}
	void setTown(uint32_t _town) {town = _town;}

	bool isLoginAttackLocked(uint32_t attackerId) const;

	virtual bool isPushable() const;
	virtual bool canBePushedBy(const Player *player) const;
	virtual int getThrowRange() const {return 1;};
	virtual bool canSeeInvisibility() const;
	uint32_t getMuteTime();
	void addMessageBuffer();
	void removeMessageBuffer();

	double getCapacity() const {
		if(hasFlag(PlayerFlag_CannotPickupItem)){
			return 0.00;
		}
		else if(hasFlag(PlayerFlag_HasInfiniteCapacity)){
			return 9999.9;
		}
		else if(capacity < 0){
			return 0.00;
		}
		return capacity;
	}

	double getFreeCapacity() const {
		if(hasFlag(PlayerFlag_CannotPickupItem)){
			return 0.00;
		}
		else if(hasFlag(PlayerFlag_HasInfiniteCapacity)){
			return 9999.9;
		}
		else{
			return std::max(0.00, capacity - inventoryWeight);
		}
	}

	virtual int32_t getMaxHealth() const {return getPlayerInfo(PLAYERINFO_MAXHEALTH);}
	virtual int32_t getMaxMana() const {return getPlayerInfo(PLAYERINFO_MAXMANA);}

	// Returns the inventory item in the slot position
	Item* getInventoryItem(slots_t slot) const;
	// As above, but returns NULL if the item can not be weared in that slot (armor in hand for example)
	Item* getEquippedItem(slots_t slot) const;

    // Returns the first found item with chosen itemid
    Item* getFirstItemById(uint32_t id) const;

	bool isItemAbilityEnabled(slots_t slot) const {return inventoryAbilities[slot];}
	void setItemAbility(slots_t slot, bool enabled) {inventoryAbilities[slot] = enabled;}

	int32_t getVarSkill(skills_t skill) const {return varSkills[skill];}
	void setVarSkill(skills_t skill, int32_t modifier) {varSkills[skill] += modifier;}

	int32_t getVarStats(stats_t stat) const {return varStats[stat];}
	void setVarStats(stats_t stat, int32_t modifier);
	int32_t getDefaultStats(stats_t stat);

	void setConditionSuppressions(uint32_t conditions, bool remove);

	double getRateValue(levelTypes_t rateType) const {return rateValue[rateType];}
	void setRateValue(levelTypes_t rateType, double value){rateValue[rateType] = value;}

	uint32_t getLossPercent(lossTypes_t lossType) const {return lossPercent[lossType];}
	void setLossPercent(lossTypes_t lossType, uint32_t newPercent)
	{
		lossPercent[lossType] = newPercent;
	}

	Depot* getDepot(uint32_t depotId, bool autoCreateDepot);
	bool addDepot(Depot* depot, uint32_t depotId);
	bool isNearDepotBox(uint32_t depotId);

	virtual bool canSee(const Position& pos) const;
	virtual bool canSeeCreature(const Creature* creature) const;
	virtual bool canWalkthrough(const Creature* creature) const;

	virtual RaceType_t getRace() const {return RACE_BLOOD;}

	//safe-trade functions
	void setTradeState(tradestate_t state) {tradeState = state;};
	tradestate_t getTradeState() {return tradeState;};
	Item* getTradeItem() {return tradeItem;};

	//V.I.P. functions
	void notifyLogIn(Player* player);
	void notifyLogOut(Player* player);
	bool removeVIP(uint32_t guid);
	bool addVIP(uint32_t guid, std::string& name, bool isOnline, bool interal = false);

	//follow functions
	virtual bool setFollowCreature(Creature* creature, bool fullPathSearch = false);
	virtual void goToFollowCreature();

	//follow events
	virtual void onFollowCreature(const Creature* creature);

	//walk events
	virtual void onWalk(Direction& dir);
	virtual void onWalkAborted();
	virtual void onWalkComplete();

	void checkIdleTime(uint32_t ticks);
	void resetIdle() {idleTime = 0; idleWarned = false;};
	void setIdleTime(uint32_t value, bool warned){idleTime = value; idleWarned = warned;}

	void setChaseMode(chaseMode_t mode);
	chaseMode_t getChaseMode() const {return chaseMode;}
	void setFightMode(fightMode_t mode);
	fightMode_t getFightMode() const {return fightMode;}
	void setSafeMode(bool _safeMode) {safeMode = _safeMode;}
	bool hasSafeMode() const {return safeMode;}
	uint16_t getIcons() const;

	//combat functions
	virtual bool setAttackedCreature(Creature* creature);
	bool isImmune(CombatType_t type) const;
	bool isImmune(ConditionType_t type, bool aggressive = true) const;
	bool hasShield() const;
	virtual bool isAttackable() const;

	virtual void changeHealth(int32_t healthChange);
	virtual void changeMana(int32_t manaChange);
	void changeSoul(int32_t soulChange);

	bool isPzLocked() const { return pzLocked; }
	virtual BlockType_t blockHit(Creature* attacker, CombatType_t combatType, int32_t& damage,
		bool checkDefense = false, bool checkArmor = false);
	virtual void doAttacking(uint32_t interval);
	virtual bool hasExtraSwing() {return lastAttack > 0 && ((OTSYS_TIME() - lastAttack) >= getAttackSpeed());}
	int32_t getShootRange() const {return shootRange;}

	int32_t getSkill(skills_t skilltype, skillsid_t skillinfo) const;
	static std::string getSkillName(int skillid);

	bool getAddAttackSkill() const {return addAttackSkillPoint;}
	BlockType_t getLastAttackBlockType() const {return lastAttackBlockType;}

	Item* getWeapon(bool ignoreAmmu = false);
	virtual WeaponType_t getWeaponType();
	int32_t getWeaponSkill(const Item* item) const;
	void getShieldAndWeapon(const Item* &shield, const Item* &weapon) const;

	virtual void drainHealth(Creature* attacker, CombatType_t combatType, int32_t damage);
	virtual void drainMana(Creature* attacker, int32_t points);
	void addManaSpent(uint64_t amount, bool useMultiplier = true);
	void addSkillAdvance(skills_t skill, uint32_t count, bool useMultiplier = true);

	virtual int32_t getArmor() const;
	virtual int32_t getDefense() const;
	virtual float getAttackFactor() const;
	virtual float getDefenseFactor() const;

	void addCombatExhaust(uint32_t ticks);
	void addHealExhaust(uint32_t ticks);
	void addInFightTicks(uint32_t ticks, bool pzlock = false);
	void addDefaultRegeneration(uint32_t addTicks);

	virtual uint64_t getGainedExperience(Creature* attacker) const;
	void getGainExperience(uint64_t& gainExp, bool fromMonster);

	//combat event functions
	virtual void onAddCondition(ConditionType_t type, bool hadCondition);
	virtual void onAddCombatCondition(ConditionType_t type, bool hadCondition);
	virtual void onEndCondition(ConditionType_t type, bool lastCondition);
	virtual void onCombatRemoveCondition(const Creature* attacker, Condition* condition);
	virtual void onTickCondition(ConditionType_t type, int32_t interval, bool& bRemove);
	virtual void onAttackedCreature(Creature* target);
	virtual void onSummonAttackedCreature(Creature* summon, Creature* target);
	virtual void onAttacked();
	virtual void onAttackedCreatureDrainHealth(Creature* target, int32_t points);
	virtual void onSummonAttackedCreatureDrainHealth(Creature* summon, Creature* target, int32_t points);
	virtual void onAttackedCreatureDrainMana(Creature* target, int32_t points);
	virtual void onSummonAttackedCreatureDrainMana(Creature* summon, Creature* target, int32_t points);
	virtual void onTargetCreatureGainHealth(Creature* target, int32_t points);
	virtual void onKilledCreature(Creature* target, bool lastHit);
	virtual void onGainExperience(uint64_t gainExp, bool fromMonster);
	virtual void onAttackedCreatureBlockHit(Creature* target, BlockType_t blockType);
	virtual void onBlockHit(BlockType_t blockType);
	virtual void onChangeZone(ZoneType_t zone);
	virtual void onAttackedCreatureChangeZone(ZoneType_t zone);
	virtual void onIdleStatus();
	virtual void onPlacedCreature();
	virtual void getCreatureLight(LightInfo& light) const;

	void setParty(Party* _party) {party = _party;}
	Party* getParty() const {return party;}
	PartyShields_t getPartyShield(const Player* player) const;
	bool isInviting(const Player* player) const;
	bool isPartner(const Player* player) const;
	void sendPlayerPartyIcons(Player* player);
	bool addPartyInvitation(Party* party);
	bool removePartyInvitation(Party* party);
	void clearPartyInvitations();

#ifdef __SKULLSYSTEM__
	Skulls_t getSkull() const;
	Skulls_t getSkullClient(const Player* player) const;
	bool hasAttacked(const Player* attacked) const;
	void addAttacked(const Player* attacked);
	void clearAttacked();
	void addUnjustifiedDead(const Player* attacked);
	void setSkull(Skulls_t newSkull) {skullType = newSkull;}
	void sendCreatureSkull(const Creature* creature) const
		{if(client) client->sendCreatureSkull(creature);}
	void checkSkullTicks(int32_t ticks);
#endif

	void checkRecentlyGainedExperience(uint32_t interval);
	bool canLogout();
	bool checkPzBlock(Player* targetPlayer);
	bool canAddContainer(const Container* container) const;
	
	//creature events
	void onAdvanceEvent(levelTypes_t type, uint32_t oldLevel, uint32_t newLevel);

	//tile
	//send methods
	void sendAddTileItem(const Tile* tile, const Position& pos, const Item* item)
	{if(client) client->sendAddTileItem(tile, pos, tile->getClientIndexOfThing(this, item), item);}
	void sendUpdateTileItem(const Tile* tile, const Position& pos, const Item* olditem, const Item* newitem)
	{if(client) client->sendUpdateTileItem(tile, pos, tile->getClientIndexOfThing(this, newitem), newitem);}
	void sendRemoveTileItem(const Tile* tile, const Position& pos, uint32_t stackpos, const Item* item)
		{if(client) client->sendRemoveTileItem(tile, pos, stackpos);}
	void sendUpdateTile(const Tile* tile, const Position& pos)
		{if(client) client->sendUpdateTile(tile, pos);}

	void sendCreatureAppear(const Creature* creature, const Position& pos, bool isLoggingIn)
		{if(client) client->sendAddCreature(creature, pos, creature->getTile()->getClientIndexOfThing(this, creature), isLoggingIn);}
	void sendCreatureDisappear(const Creature* creature, uint32_t stackpos, bool isLogout)
		{if(client) client->sendRemoveCreature(creature, creature->getPosition(),
			stackpos, isLogout);}
	void sendCreatureMove(const Creature* creature, const Tile* newTile, const Position& newPos,
		const Tile* oldTile, const Position& oldPos, uint32_t oldStackPos, bool teleport)
		{if(client) client->sendMoveCreature(creature, newTile, newPos, newTile->getClientIndexOfThing(this, creature),
		oldTile, oldPos, oldStackPos, teleport);}

	void sendCreatureTurn(const Creature* creature)
		{if(client) client->sendCreatureTurn(creature, creature->getTile()->getClientIndexOfThing(this, creature));}
	void sendCreatureSay(const Creature* creature, SpeakClasses type, const std::string& text)
		{if(client) client->sendCreatureSay(creature, type, text);}
	void sendCreatureSquare(const Creature* creature, SquareColor_t color)
		{if(client) client->sendCreatureSquare(creature, color);}
	void sendCreatureChangeOutfit(const Creature* creature, const Outfit_t& outfit)
		{if(client) client->sendCreatureOutfit(creature, outfit);}
	void sendCreatureChangeVisible(const Creature* creature, bool visible)
		{
			if(client){
				if(creature->getPlayer()){
					if(creature == this || !creature->getPlayer()->hasFlag(PlayerFlag_CannotBeSeen)){
						if(visible){
							client->sendCreatureOutfit(creature, creature->getCurrentOutfit());
						}
						else{
							static Outfit_t outfit;
							client->sendCreatureOutfit(creature, outfit);
						}
					}
				}
				else{
					if(canSeeInvisibility()){
						client->sendCreatureOutfit(creature, creature->getCurrentOutfit());
					}
					else{
						if(visible){
							client->sendAddCreature(creature, creature->getPosition(), creature->getTile()->getClientIndexOfThing(this, creature), true);
						}
						else{
							client->sendRemoveCreature(creature, creature->getPosition(), creature->getTile()->getClientIndexOfThing(this, creature), false);
						}
					}
				}
			}
		}
	#ifdef __MIN_PVP_LEVEL_APPLIES_TO_SUMMONS__
	void forceClientToReloadCreature(const Creature* creature) {
		if (client && creature) {
			client->sendRemoveCreature(creature, creature->getPosition(), creature->getTile()->getClientIndexOfThing(this, creature), false);
			client->sendAddCreature(creature, creature->getPosition(), creature->getTile()->getClientIndexOfThing(this, creature), false);
			}
		}
	#endif
	void sendCreatureLight(const Creature* creature)
		{if(client) client->sendCreatureLight(creature);}
	void sendCreatureShield(const Creature* creature)
		{if(client) client->sendCreatureShield(creature);}

	//container
	void sendAddContainerItem(const Container* container, const Item* item);
	void sendUpdateContainerItem(const Container* container, uint8_t slot, const Item* oldItem, const Item* newItem);
	void sendRemoveContainerItem(const Container* container, uint8_t slot, const Item* item);
	void sendContainer(uint32_t cid, const Container* container, bool hasParent)
		{if(client) client->sendContainer(cid, container, hasParent); }

	//inventory
	void sendAddInventoryItem(slots_t slot, const Item* item)
		{if(client) client->sendAddInventoryItem(slot, item);}
	void sendUpdateInventoryItem(slots_t slot, const Item* oldItem, const Item* newItem)
		{if(client) client->sendUpdateInventoryItem(slot, newItem);}
	void sendRemoveInventoryItem(slots_t slot, const Item* item)
		{if(client) client->sendRemoveInventoryItem(slot);}

	//event methods
	virtual void onAddTileItem(const Tile* tile, const Position& pos, const Item* item);
	virtual void onUpdateTileItem(const Tile* tile, const Position& pos,
		const Item* oldItem, const ItemType& oldType, const Item* newItem, const ItemType& newType);
	virtual void onRemoveTileItem(const Tile* tile, const Position& pos,
		const ItemType& iType, const Item* item);
	virtual void onUpdateTile(const Tile* tile, const Position& pos);

	virtual void onCreatureAppear(const Creature* creature, bool isLogin);
	virtual void onCreatureDisappear(const Creature* creature, bool isLogout);
	virtual void onCreatureMove(const Creature* creature, const Tile* newTile, const Position& newPos,
		const Tile* oldTile, const Position& oldPos, bool teleport);

	virtual void onAttackedCreatureDissapear(bool isLogout);
	virtual void onFollowCreatureDissapear(bool isLogout);

	//container
	void onAddContainerItem(const Container* container, const Item* item);
	void onUpdateContainerItem(const Container* container, uint8_t slot,
		const Item* oldItem, const ItemType& oldType, const Item* newItem, const ItemType& newType);
	void onRemoveContainerItem(const Container* container, uint8_t slot, const Item* item);

	void onCloseContainer(const Container* container);
	void onSendContainer(const Container* container);
	void autoCloseContainers(const Container* container);

	//inventory
	void onAddInventoryItem(slots_t slot, Item* item);
	void onUpdateInventoryItem(slots_t slot, Item* oldItem, const ItemType& oldType,
		Item* newItem, const ItemType& newType);
	void onRemoveInventoryItem(slots_t slot, Item* item);

	//other send messages
	void sendAnimatedText(const Position& pos, unsigned char color, std::string text) const
		{if(client) client->sendAnimatedText(pos,color,text);}
	void sendCancel(const std::string& msg) const
		{if(client) client->sendCancel(msg);}
	void sendCancelMessage(ReturnValue message) const;
	void sendCancelTarget() const
		{if(client) client->sendCancelTarget();}
	void sendCancelWalk() const
		{if(client) client->sendCancelWalk();}
	void sendChangeSpeed(const Creature* creature, uint32_t newSpeed) const
		{if(client) client->sendChangeSpeed(creature, newSpeed);}
	void sendCreatureHealth(const Creature* creature) const
		{if(client) client->sendCreatureHealth(creature);}
	void sendDistanceShoot(const Position& from, const Position& to, unsigned char type) const
		{if(client) client->sendDistanceShoot(from, to, type);}
	void sendHouseWindow(House* house, uint32_t listId) const;
	void sendOutfitWindow() const;
	void sendCreatePrivateChannel(uint16_t channelId, const std::string& channelName)
		{if(client) client->sendCreatePrivateChannel(channelId, channelName);}
	void sendClosePrivate(uint16_t channelId) const
		{if(client) client->sendClosePrivate(channelId);}
	void sendIcons() const;
	void sendMagicEffect(const Position& pos, unsigned char type) const
		{if(client) client->sendMagicEffect(pos,type);}
	void sendStats();
	void sendSkills() const
		{if(client) client->sendSkills();}
	void sendTextMessage(MessageClasses mclass, const std::string& message) const
		{if(client) client->sendTextMessage(mclass, message);}
	void sendTextWindow(Item* item, uint16_t maxlen, bool canWrite) const
		{if(client) client->sendTextWindow(windowTextId, item, maxlen, canWrite);}
	void sendTextWindow(uint32_t itemId, const std::string& text) const
		{if(client) client->sendTextWindow(windowTextId, itemId, text);}
	void sendToChannel(Creature* creature, SpeakClasses type, const std::string& text, uint16_t channelId, uint32_t time = 0) const
		{if(client) client->sendToChannel(creature, type, text, channelId, time);}
	void sendTradeItemRequest(const Player* player, const Item* item, bool ack) const
		{if(client) client->sendTradeItemRequest(player, item, ack);}
	void sendTradeClose() const
		{if(client) client->sendCloseTrade();}
	void sendWorldLight(LightInfo& lightInfo)
		{if(client) client->sendWorldLight(lightInfo);}
	void sendChannelsDialog()
		{if(client) client->sendChannelsDialog();}
	void sendOpenPrivateChannel(const std::string& receiver)
		{if(client) client->sendOpenPrivateChannel(receiver);}
	void sendOutfitWindow()
		{if(client) client->sendOutfitWindow();}
	void sendCloseContainer(uint32_t cid)
		{if(client) client->sendCloseContainer(cid);}
	void sendChannel(uint16_t channelId, const std::string& channelName)
		{if(client) client->sendChannel(channelId, channelName);}
	void sendRuleViolationsChannel(uint16_t channelId)
		{if(client) client->sendRuleViolationsChannel(channelId);}
	void sendRemoveReport(const std::string& name)
		{if(client) client->sendRemoveReport(name);}
	void sendLockRuleViolation()
		{if(client) client->sendLockRuleViolation();}
	void sendRuleViolationCancel(const std::string& name)
		{if(client) client->sendRuleViolationCancel(name);}
	
	void receivePing() {last_pong = OTSYS_TIME();}

	virtual void onThink(uint32_t interval);
	virtual void onAttacking(uint32_t interval);

	virtual void postAddNotification(Thing* thing, const Cylinder* oldParent, int32_t index, cylinderlink_t link = LINK_OWNER, bool isNewItem = true);
	virtual void postRemoveNotification(Thing* thing, const Cylinder* newParent, int32_t index, bool isCompleteRemoval, cylinderlink_t link = LINK_OWNER);

	Item* getWriteItem(uint32_t& _windowTextId, uint16_t& _maxWriteLen);
	void setWriteItem(Item* item, uint16_t _maxWriteLen = 0);

	void registerMoveItemAsNow() { lastMoveItem = OTSYS_TIME(); }
	bool canMoveItem() const;

	House* getEditHouse(uint32_t& _windowTextId, uint32_t& _listId);
	void setEditHouse(House* house, uint32_t listId = 0);

	void setNextAction(int64_t time) {if(time > nextAction) {nextAction = time;}}
	bool canDoAction() const {return nextAction <= OTSYS_TIME();}
	uint32_t getNextActionTime() const;
	virtual uint32_t getAttackSpeed() const;
	void setLastAttackAsNow() { lastAttack = OTSYS_TIME(); }
	int64_t getLastTimeRequestOutfit() const { return lastTimeRequestOutfit; }
	void setLastTimeRequestOutfitAsNow() { lastTimeRequestOutfit = OTSYS_TIME(); }

	void learnInstantSpell(const std::string& name);
	bool hasLearnedInstantSpell(const std::string& name) const;
	void stopWalk();
	
	VIPListSet VIPList;
	uint32_t maxVipLimit;

	//items
	ContainerVector containerVec;
	void preSave();

	//depots
	DepotMap depots;
	uint32_t maxDepotLimit;

protected:
	void checkTradeState(const Item* item);
	bool hasCapacity(const Item* item, uint32_t count) const;

	void gainExperience(uint64_t& gainExp, bool fromMonster);
	void addExperience(uint64_t exp);
	void removeExperience(uint64_t exp, bool updateStats = true);
	
	void setNextWalkActionTask(SchedulerTask* task);
	void setNextActionTask(SchedulerTask* task);

	void onDie();
	void die();
	virtual Item* dropCorpse();
	virtual Item* createCorpse();

	//cylinder implementations
	virtual ReturnValue __queryAdd(int32_t index, const Thing* thing, uint32_t count,
		uint32_t flags) const;
	virtual ReturnValue __queryMaxCount(int32_t index, const Thing* thing, uint32_t count, uint32_t& maxQueryCount,
		uint32_t flags) const;
	virtual ReturnValue __queryRemove(const Thing* thing, uint32_t count, uint32_t flags) const;
	virtual Cylinder* __queryDestination(int32_t& index, const Thing* thing, Item** destItem,
		uint32_t& flags);

	virtual void __addThing(Thing* thing);
	virtual void __addThing(int32_t index, Thing* thing);

	virtual void __updateThing(Thing* thing, uint16_t itemId, uint32_t count);
	virtual void __replaceThing(uint32_t index, Thing* thing);

	virtual void __removeThing(Thing* thing, uint32_t count);

	virtual int32_t __getIndexOfThing(const Thing* thing) const;
	virtual int32_t __getFirstIndex() const;
	virtual int32_t __getLastIndex() const;
	virtual uint32_t __getItemTypeCount(uint16_t itemId, int32_t subType = -1) const;
	virtual std::map<uint32_t, uint32_t>& __getAllItemTypeCount(std::map<uint32_t, uint32_t>& countMap) const;
	virtual Thing* __getThing(uint32_t index) const;

	virtual void __internalAddThing(Thing* thing);
	virtual void __internalAddThing(uint32_t index, Thing* thing);

protected:
	ProtocolGame* client;

	uint32_t level;
	uint32_t levelPercent;
	uint32_t magLevel;
	uint32_t magLevelPercent;
	int16_t accessLevel;
	int16_t violationLevel;
	std::string groupName;
	uint64_t experience;
	uint32_t damageImmunities;
	uint32_t conditionImmunities;
	uint32_t conditionSuppressions;
	uint32_t condition;
	uint64_t manaSpent;
	Vocation_t vocation_id;
	Vocation* vocation;
	PlayerSex_t sex;
	int32_t soul, soulMax;
	uint64_t groupFlags;
	uint16_t premiumDays;
	uint32_t MessageBufferTicks;
	int32_t MessageBufferCount;
	uint32_t actionTaskEvent;
	uint32_t walkTaskEvent;
	SchedulerTask* walkTask;

	int32_t idleTime;
	bool idleWarned;
	int64_t lastTimeRequestOutfit;

	double inventoryWeight;
	double capacity;

	int64_t last_ping;
	int64_t last_pong;
	int64_t nextAction;
	int64_t lastMoveItem;

	bool pzLocked;
	bool isConnecting;
	int32_t bloodHitCount;
	int32_t shieldBlockCount;
	BlockType_t lastAttackBlockType;
	bool addAttackSkillPoint;
	uint64_t lastAttack;
	int32_t shootRange;

	chaseMode_t chaseMode;
	fightMode_t fightMode;
	bool safeMode;

	//
	bool gmInvisible;

	//account variables
	uint32_t accountId;
	std::string password;
	time_t lastLoginSaved;
	time_t lastLogout;
	int64_t lastLoginMs;
	Position loginPosition;
	uint32_t lastip;

	//inventory variables
	Item* inventory[11];
	bool inventoryAbilities[11];

	//player advances variables
	uint32_t skills[SKILL_LAST + 1][3];

	//extra skill modifiers
	int32_t varSkills[SKILL_LAST + 1];

	//extra stat modifiers
	int32_t varStats[STAT_LAST + 1];

	//loss percent variables
	uint32_t lossPercent[LOSS_LAST + 1];

	//rate value variables
	double rateValue[LEVEL_LAST + 1];

	LearnedInstantSpellList learnedInstantSpellList;

	ConditionList storedConditionList;

	//trade variables
	Player* tradePartner;
	tradestate_t tradeState;
	Item* tradeItem;

	//party variables
	Party* party;
	PartyList invitePartyList;

	std::string name;
	std::string nameDescription;
	uint32_t guid;

	uint32_t town;

	//Guild
	Guild* guild;

	StorageMap storageMap;
	LightInfo itemsLight;
	std::pair<Container*, int32_t> backpack;

	bool requestedOutfitWindow;

	//read/write storage data
	uint32_t windowTextId;
	Item* writeItem;
	uint16_t maxWriteLen;
	House* editHouse;
	uint32_t editListId;

#ifdef __SKULLSYSTEM__
	Skulls_t skullType;
	int64_t lastSkullTime;
	typedef std::set<uint32_t> AttackedSet;
	AttackedSet attackedSet;
#endif

	void updateItemsLight(bool internal = false);
	virtual int32_t getStepSpeed() const;
	void updateBaseSpeed();

	static uint32_t getPercentLevel(uint64_t count, uint32_t nextLevelCount);
	virtual uint64_t getLostExperience() const;

	virtual void dropLoot(Container* corpse);
	virtual uint32_t getDamageImmunities() const { return damageImmunities; }
	virtual uint32_t getConditionImmunities() const { return conditionImmunities; }
	virtual uint32_t getConditionSuppressions() const { return conditionSuppressions; }
	virtual uint16_t getLookCorpse() const;
	virtual void getPathSearchParams(const Creature* creature, FindPathParams& fpp) const;

	friend class Game;
	friend class Npc;
	friend class LuaScriptInterface;
	friend class Map;
	friend class Actions;
	friend class IOPlayer;
	friend class ProtocolGame;
};

#endif
