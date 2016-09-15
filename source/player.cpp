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
#include "otpch.h"

#include "ban.h"
#include "player.h"
#include "ioplayer.h"
#include "game.h"
#include "configmanager.h"
#include "chat.h"
#include "house.h"
#include "combat.h"
#include "movement.h"
#include "weapons.h"
#include "creatureevent.h"
#include "status.h"
#include "beds.h"
#include "party.h"
#include "guild.h"
#include <boost/algorithm/string/predicate.hpp>
#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>

extern ConfigManager g_config;
extern Game g_game;
extern BanManager g_bans;
extern Chat g_chat;
extern Vocations g_vocations;
extern MoveEvents* g_moveEvents;
extern Weapons* g_weapons;
extern CreatureEvents* g_creatureEvents;
extern Guilds g_guilds;

AutoList<Player> Player::listPlayer;
MuteCountMap Player::muteCountMap;
ChannelStatementMap Player::channelStatementMap;
uint32_t Player::channelStatementGuid = 0;

#ifdef __ENABLE_SERVER_DIAGNOSTIC__
uint32_t Player::playerCount = 0;
#endif

Player::Player(const std::string& _name, ProtocolGame* p) :
Creature()
{
	client = p;
	isConnecting = false;
	if(client){
		client->setPlayer(this);
	}
	accountId   = 0;
	name        = _name;
	setVocation(VOCATION_NONE);
	capacity   = 300.00;
	mana       = 0;
	manaMax    = 0;
	manaSpent  = 0;
	soul       = 0;
	soulMax    = 100;

	level      = 1;
	levelPercent = 0;
	magLevelPercent = 0;
	magLevel   = 0;
	experience = 0;
	damageImmunities = 0;
	conditionImmunities = 0;
	conditionSuppressions = 0;
	accessLevel = 0;
	violationLevel = 0;
	lastip = 0;
	lastLoginSaved = 0;
	lastLogout = 0;
	lastLoginMs = 0;
	last_ping = OTSYS_TIME();
	last_pong = OTSYS_TIME();
	lastMoveItem = OTSYS_TIME();
	MessageBufferTicks = 0;
	MessageBufferCount = 0;
	nextAction = 0;

	pzLocked = false;
	bloodHitCount = 0;
	shieldBlockCount = 0;
	lastAttackBlockType = BLOCK_NONE;
	addAttackSkillPoint = false;
	lastAttack = 0;
	shootRange = 1;

	chaseMode = CHASEMODE_STANDSTILL;
	fightMode = FIGHTMODE_ATTACK;
	safeMode = true;

	gmInvisible = false;

	tradePartner = NULL;
	tradeState = TRADE_NONE;
	tradeItem = NULL;

	walkTask = NULL;
	walkTaskEvent = 0;
	actionTaskEvent = 0;

	idleTime = 0;
	idleWarned = false;

	lastTimeRequestOutfit = 0;

	for(int32_t i = 0; i < 11; ++i){
		inventory[i] = NULL;
		inventoryAbilities[i] = false;
	}

	for(int32_t i = SKILL_FIRST; i <= SKILL_LAST; ++i){
		skills[i][SKILL_LEVEL]= 10;
		skills[i][SKILL_TRIES]= 0;
		skills[i][SKILL_PERCENT] = 0;
	}

	for(int32_t i = SKILL_FIRST; i <= SKILL_LAST; ++i){
		varSkills[i] = 0;
	}

	for(int32_t i = STAT_FIRST; i <= STAT_LAST; ++i){
		varStats[i] = 0;
	}

	for(int32_t i = LOSS_FIRST; i <= LOSS_LAST; ++i){
		lossPercent[i] = 10;
	}

	for(int32_t i = LEVEL_FIRST; i <= LEVEL_LAST; ++i){
		rateValue[i] = 1.0f;
	}

	maxDepotLimit = 1000;
	maxVipLimit = 50;
	groupFlags = 0;
	premiumDays = 0;
	balance = 0;

	guildLevel = 0;
	guildRank = "";
	guildNick = "";

	sex = PLAYERSEX_LAST;
	vocation_id = (Vocation_t)0;

	town = 0;
	lastip = 0;

	windowTextId = 0;
	writeItem = NULL;
	maxWriteLen = 0;

	editHouse = NULL;
	editListId = 0;

	setParty(NULL);
	setGuild(NULL);

#ifdef __SKULLSYSTEM__
	lastSkullTime = 0;
	skullType = SKULL_NONE;
#endif

#ifdef __ENABLE_SERVER_DIAGNOSTIC__
	playerCount++;
#endif
}

Player::~Player()
{
	for(int i = 0; i < 11; ++i){
		if(inventory[i]){
			inventory[i]->setParent(NULL);
			inventory[i]->releaseThing2();
			inventory[i] = NULL;
			inventoryAbilities[i] = false;
		}
	}

	DepotMap::iterator it;
	for(it = depots.begin();it != depots.end(); ++it){
		it->second->releaseThing2();
	}

	//std::cout << "Player destructor " << this << std::endl;

	setWriteItem(NULL);
	setEditHouse(NULL);
	setNextWalkActionTask(NULL);

#ifdef __ENABLE_SERVER_DIAGNOSTIC__
	playerCount--;
#endif
}

bool Player::setVocation(uint32_t vocId)
{
	if(!g_vocations.getVocation(vocId, vocation)){
		return false;
	}
	vocation_id = (Vocation_t)vocId;

	//Update health/mana gain condition
	Condition* condition = getCondition(CONDITION_REGENERATION, CONDITIONID_DEFAULT, 0);
	if(condition){
		condition->setParam(CONDITIONPARAM_HEALTHGAIN, vocation->getHealthGainAmount());
		condition->setParam(CONDITIONPARAM_HEALTHTICKS, vocation->getHealthGainTicks() * 1000);
		condition->setParam(CONDITIONPARAM_MANAGAIN, vocation->getManaGainAmount());
		condition->setParam(CONDITIONPARAM_MANATICKS, vocation->getManaGainTicks() * 1000);
	}

	//Set the player's max soul according to their vocation
	soulMax = vocation->getSoulMax();

	return true;
}

uint32_t Player::getVocationId() const
{
	return vocation_id;
}

bool Player::isPushable() const
{
	bool ret = Creature::isPushable();

	if(hasFlag(PlayerFlag_CannotBePushed)){
		return false;
	}

	return ret;
}

bool Player::canMoveItem() const
{
	int32_t moveItemTime = g_config.getNumber(ConfigManager::MOVEITEM_TIME);
	return ( (moveItemTime <= 0) || (OTSYS_TIME() - lastMoveItem >= moveItemTime) );
}

std::string Player::getDescription(int32_t lookDistance) const
{
	std::stringstream s;
	std::string str;

	if(lookDistance == -1){
		s << "yourself.";

		if(hasFlag(PlayerFlag_ShowGroupInsteadOfVocation))
			s << " You are " << getGroupName() << ".";
		else if(getVocationId() != VOCATION_NONE)
			s << " You are " << vocation->getDescription() << ".";
		else
			s << " You have no vocation.";
	}
	else {
		s << name << " (Level " << level <<").";

		s << " " << playerSexSubjectString(getSex());

		if(hasFlag(PlayerFlag_ShowGroupInsteadOfVocation))
			s << " is " << getGroupName() << ".";
		else if(getVocationId() != VOCATION_NONE)
			s << " is " << vocation->getDescription() << ".";
		else
			s << " has no vocation.";
	}

	if(getGuild()){
		if(lookDistance == -1){
			s << " You are ";
		}
		else{
			s << " " << playerSexSubjectString(getSex()) << " is ";
		}

		if(guildRank.length()){
			s << guildRank;
		}
		else{
			s << "a member";
		}

		s << " of the " << getGuild()->getName();

		if(guildNick.length()){
			s << " (" << guildNick << ")";
		}

		s << ".";
	}

	str = s.str();
	return str;
}

Item* Player::getInventoryItem(slots_t slot) const
{
	if(slot > 0 && slot < SLOT_LAST)
		return inventory[slot];
	if(slot == SLOT_HAND)
		return inventory[SLOT_LEFT]? inventory[SLOT_LEFT] : inventory[SLOT_RIGHT];

	return NULL;
}

Item* Player::getEquippedItem(slots_t slot) const
{
	Item* item = getInventoryItem(slot);
	if(item){
		switch(slot){
			case SLOT_RIGHT:
			case SLOT_LEFT:
				return (item->getWieldPosition() == SLOT_HAND) ? item : NULL;
			default:
				return (slot == item->getWieldPosition()) ? item : NULL;
		}
	}

	return NULL;
}

Item* Player::getFirstItemById(uint32_t id) const
{
	Item* tmpItem = NULL;
	Container* tmpContainer = NULL;
	std::list<Container*> listContainer;
	for(int32_t slot = SLOT_FIRST; slot <= SLOT_LAST; slot++){
		if((tmpItem = getInventoryItem((slots_t)slot))){
			if(tmpItem->getID() == id){
				return tmpItem;
			}
			else if((tmpContainer = tmpItem->getContainer())){
				listContainer.push_back(tmpContainer);
			}
		}
	}

	ItemList::const_iterator it;
	while(!listContainer.empty()){
		Container* container = listContainer.front();
		listContainer.pop_front();
		for(it = container->getItems(); it != container->getEnd(); ++it){
			if((tmpItem = *it)){
				if(tmpItem->getID() == id){
					return tmpItem;
				}
				else if((tmpContainer = tmpItem->getContainer())){
					listContainer.push_back(tmpContainer);
				}
			}
		}
	}

	return NULL;
}

void Player::setConditionSuppressions(uint32_t conditions, bool remove)
{
	if(!remove){
		conditionSuppressions |= conditions;
	}
	else{
		conditionSuppressions &= ~conditions;
	}
}

Item* Player::getWeapon(bool ignoreAmmu /*= false*/)
{
	Item* item;

	for(uint32_t slot = SLOT_RIGHT; slot <= SLOT_LEFT; ++slot){
		item = getInventoryItem((slots_t)slot);
		if(!item){
			continue;
		}

		switch(item->getWeaponType()){
			case WEAPON_SWORD:
			case WEAPON_AXE:
			case WEAPON_CLUB:
			case WEAPON_WAND:
			{
				const Weapon* weapon = g_weapons->getWeapon(item);
				if(weapon){
					return item;
				}

				break;
			}

			case WEAPON_DIST:
			{
				if(!ignoreAmmu && item->getAmuType() != AMMO_NONE){
					Item* ammuItem = getInventoryItem(SLOT_AMMO);

					if(ammuItem && ammuItem->getAmuType() == item->getAmuType()){
						const Weapon* weapon = g_weapons->getWeapon(ammuItem);
						if(weapon){
							shootRange = item->getShootRange();
							return ammuItem;
						}
					}
				}
				else{
					const Weapon* weapon = g_weapons->getWeapon(item);
					if(weapon){
						shootRange = item->getShootRange();
						return item;
					}
				}
			}

			default:
				break;
		}
	}

	return NULL;
}

WeaponType_t Player::getWeaponType()
{
	Item* item = getWeapon();
	if(!item){
		return WEAPON_NONE;
	}

	return item->getWeaponType();
}

int32_t Player::getWeaponSkill(const Item* item) const
{
	if(!item){
		return getSkill(SKILL_FIST, SKILL_LEVEL);
	}

	WeaponType_t weaponType = item->getWeaponType();
	int32_t attackSkill;

	switch(weaponType){
		case WEAPON_SWORD:
			attackSkill = getSkill(SKILL_SWORD, SKILL_LEVEL);
			break;

		case WEAPON_CLUB:
		{
			attackSkill = getSkill(SKILL_CLUB, SKILL_LEVEL);
			break;
		}

		case WEAPON_AXE:
		{
			attackSkill = getSkill(SKILL_AXE, SKILL_LEVEL);
			break;
		}

		case WEAPON_DIST:
		{
			attackSkill = getSkill(SKILL_DIST, SKILL_LEVEL);
			break;
		}
		default:
		{
			attackSkill = 0;
			break;
		}
	}
	return attackSkill;
}

int32_t Player::getArmor() const
{
	int32_t armor = 0;

	if(getInventoryItem(SLOT_HEAD))
		armor += getInventoryItem(SLOT_HEAD)->getArmor();
	if(getInventoryItem(SLOT_NECKLACE))
		armor += getInventoryItem(SLOT_NECKLACE)->getArmor();
	if(getInventoryItem(SLOT_ARMOR))
		armor += getInventoryItem(SLOT_ARMOR)->getArmor();
	if(getInventoryItem(SLOT_LEGS))
		armor += getInventoryItem(SLOT_LEGS)->getArmor();
	if(getInventoryItem(SLOT_FEET))
		armor += getInventoryItem(SLOT_FEET)->getArmor();
	if(getInventoryItem(SLOT_RING))
		armor += getInventoryItem(SLOT_RING)->getArmor();

	return (vocation->getArmorDefense() != 1.0 ? int32_t(armor * vocation->getArmorDefense()) : armor);
}

void Player::getShieldAndWeapon(const Item* &shield, const Item* &weapon) const
{
	Item* item;
	shield = NULL;
	weapon = NULL;
	for(uint32_t slot = SLOT_RIGHT; slot <= SLOT_LEFT; ++slot){
		item = getInventoryItem((slots_t)slot);
		if(item){
			switch(item->getWeaponType()){
			case WEAPON_NONE:
				break;
			case WEAPON_SHIELD:
				shield = item;
				break;
			default: // weapons that are not shields
				weapon = item;
				break;
			}
		}
	}
	return;
}

int32_t Player::getDefense() const
{
	int32_t baseDefense = 5;
	int32_t defenseSkill = 0;
	int32_t defenseValue = 0;
	int32_t extraDef = 0;
	float defenseFactor = getDefenseFactor();
	const Item* weapon = NULL;
	const Item* shield = NULL;
	getShieldAndWeapon(shield, weapon);

	if(weapon){
		defenseValue = weapon->getDefense();
		defenseSkill = getWeaponSkill(weapon);
		extraDef = weapon->getExtraDef();
	}

	if(shield && shield->getDefense() >= defenseValue){
		defenseValue = shield->getDefense() + extraDef;
		defenseSkill = getSkill(SKILL_SHIELD, SKILL_LEVEL);
	}

	defenseValue += baseDefense;

	if(defenseSkill == 0)
		return 0;

	if(vocation && vocation->getBaseDefense() != 1.0){
		defenseValue = int32_t(defenseValue * vocation->getBaseDefense());
	}

	return ((int32_t)std::ceil(((float)(defenseSkill * (defenseValue * 0.015)) + (defenseValue * 0.1)) * defenseFactor));
}

float Player::getAttackFactor() const
{
	switch(fightMode){
		case FIGHTMODE_ATTACK:
		{
			return 1.0f;
			break;
		}

		case FIGHTMODE_BALANCED:
		{
			return 1.2f;
			break;
		}

		case FIGHTMODE_DEFENSE:
		{
			return 2.0f;
			break;
		}

		default:
			return 1.0f;
			break;
	}
}

float Player::getDefenseFactor() const
{
	switch(fightMode){
		case FIGHTMODE_ATTACK:
		{
			return 1.0f;
			break;
		}

		case FIGHTMODE_BALANCED:
		{
			return 1.2f;
			break;
		}

		case FIGHTMODE_DEFENSE:
		{
			return 2.0f;
			break;
		}

		default:
			return 1.0f;
			break;
	}
}

uint16_t Player::getIcons() const
{
	uint16_t icons = ICON_NONE;

	ConditionList::const_iterator it;
	for(it = conditions.begin(); it != conditions.end(); ++it){
		if(!isSuppress((*it)->getType())){
			icons |= (*it)->getIcons();
		}
	}

	return icons;
}

void Player::sendIcons() const
{
	if(client){
		client->sendIcons(getIcons());
	}
}

void Player::updateInventoryWeight()
{
	inventoryWeight = 0.00;

	if(!hasFlag(PlayerFlag_HasInfiniteCapacity)){
		for(int i = SLOT_FIRST; i < SLOT_LAST; ++i){
			Item* item = getInventoryItem((slots_t)i);
			if(item){
				inventoryWeight += item->getWeight();
			}
		}
	}
}

int32_t Player::getPlayerInfo(playerinfo_t playerinfo) const
{
	switch(playerinfo) {
		case PLAYERINFO_LEVEL: return level; break;
		case PLAYERINFO_LEVELPERCENT: return levelPercent; break;
		case PLAYERINFO_MAGICLEVEL: return std::max((int32_t)0, ((int32_t)magLevel + varStats[STAT_MAGICPOINTS])); break;
		case PLAYERINFO_MAGICLEVELPERCENT: return magLevelPercent; break;
		case PLAYERINFO_HEALTH: return std::max(0, health); break;
		case PLAYERINFO_MAXHEALTH: return std::max((int32_t)1, ((int32_t)healthMax + varStats[STAT_MAXHITPOINTS])); break;
		case PLAYERINFO_MANA: return std::max(0, mana); break;
		case PLAYERINFO_MAXMANA: return std::max((int32_t)0, ((int32_t)manaMax + varStats[STAT_MAXMANAPOINTS])); break;
		case PLAYERINFO_SOUL: return std::max((int32_t)0, ((int32_t)soul + varStats[STAT_SOULPOINTS])); break;
		default:
			return 0; break;
	}

	return 0;
}

uint64_t Player::getLostExperience() const
{
	if(!skillLoss)
		return 0;

	if(level < 25)
		return experience * lossPercent[LOSS_EXPERIENCE] / 1000;

	double levels_to_lose = (getLevel() + 50) / 100.;
	uint64_t xp_to_lose = 0;
	uint32_t clevel = getLevel();

	while(levels_to_lose >= 1.0){
		xp_to_lose += (getExpForLevel(clevel) - getExpForLevel(clevel - 1));
		clevel--;
		levels_to_lose -= 1.0;
	}
	if(levels_to_lose > 0.0)
		xp_to_lose += uint64_t((getExpForLevel(clevel) - getExpForLevel(clevel - 1)) * levels_to_lose);

	return xp_to_lose * lossPercent[LOSS_EXPERIENCE] / 100;
}

int32_t Player::getSkill(skills_t skilltype, skillsid_t skillinfo) const
{
	int32_t n = skills[skilltype][skillinfo];

	if(skillinfo == SKILL_LEVEL){
		n += varSkills[skilltype];
	}

	return std::max((int32_t)0, (int32_t)n);
}

std::string Player::getSkillName(int skillid)
{
	std::string skillname;
	switch(skillid){
	case SKILL_FIST:
		skillname = "fist fighting";
		break;
	case SKILL_CLUB:
		skillname = "club fighting";
		break;
	case SKILL_SWORD:
		skillname = "sword fighting";
		break;
	case SKILL_AXE:
		skillname = "axe fighting";
		break;
	case SKILL_DIST:
		skillname = "distance fighting";
		break;
	case SKILL_SHIELD:
		skillname = "shielding";
		break;
	case SKILL_FISH:
		skillname = "fishing";
		break;
	default:
		skillname = "unknown";
		break;
	}
	return skillname;
}

void Player::addSkillAdvance(skills_t skill, uint32_t count, bool useMultiplier /*= true*/)
{
	if(useMultiplier){
		count = uint32_t(count * getRateValue((levelTypes_t)skill));
	}
	skills[skill][SKILL_TRIES] += count * g_config.getNumber(ConfigManager::RATE_SKILL);

#ifdef __DEBUG__
	std::cout << getName() << ", has the vocation: " << (int)getVocationId() << " and is training his " << Player::getSkillName(skill) << "(" << skill << "). Tries: " << skills[skill][SKILL_TRIES] << "(" << vocation->getReqSkillTries(skill, skills[skill][SKILL_LEVEL] + 1) << ")" << std::endl;
	std::cout << "Current skill: " << skills[skill][SKILL_LEVEL] << std::endl;
#endif

	//Need skill up?
	if(skills[skill][SKILL_TRIES] >= vocation->getReqSkillTries(skill, skills[skill][SKILL_LEVEL] + 1)){
		skills[skill][SKILL_LEVEL]++;
		skills[skill][SKILL_TRIES] = 0;
		skills[skill][SKILL_PERCENT] = 0;
		std::stringstream advMsg;
		advMsg << "You advanced in " << Player::getSkillName(skill) << ".";
		
		sendTextMessage(MSG_EVENT_ADVANCE, advMsg.str());

		//scripting event - onAdvance
		onAdvanceEvent((levelTypes_t)skill, (skills[skill][SKILL_LEVEL] - 1), skills[skill][SKILL_LEVEL]);

		sendSkills();
	}
	else{
		//update percent
		uint32_t newPercent = Player::getPercentLevel(skills[skill][SKILL_TRIES], vocation->getReqSkillTries(skill, skills[skill][SKILL_LEVEL] + 1));
		if(skills[skill][SKILL_PERCENT] != newPercent){
			skills[skill][SKILL_PERCENT] = newPercent;
			sendSkills();
		}
	}
}

void Player::setVarStats(stats_t stat, int32_t modifier)
{
	varStats[stat] += modifier;

	switch(stat){
		case STAT_MAXHITPOINTS:
		{
			if(getHealth() > getMaxHealth()){
				//Creature::changeHealth is called  to avoid sendStats()
				Creature::changeHealth(getMaxHealth() - getHealth());
			}
			else{
				g_game.addCreatureHealth(this);
			}
			break;
		}

		case STAT_MAXMANAPOINTS:
		{
			if(getMana() > getMaxMana()){
				//Creature::changeMana is called  to avoid sendStats()
				Creature::changeMana(getMaxMana() - getMana());
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

int32_t Player::getDefaultStats(stats_t stat)
{
	switch(stat){
		case STAT_MAXHITPOINTS:
		{
			return getMaxHealth() - getVarStats(STAT_MAXHITPOINTS);
			break;
		}

		case STAT_MAXMANAPOINTS:
			return getMaxMana() - getVarStats(STAT_MAXMANAPOINTS);
			break;

		case STAT_SOULPOINTS:
			return getPlayerInfo(PLAYERINFO_SOUL) - getVarStats(STAT_SOULPOINTS);
			break;

		case STAT_MAGICPOINTS:
			return getMagicLevel() - getVarStats(STAT_MAGICPOINTS);
			break;

		default:
			return 0;
			break;
	}
}

int32_t Player::getStepSpeed() const
{
	if(getSpeed() > PLAYER_MAX_SPEED){
		return PLAYER_MAX_SPEED;
	}
	else if(getSpeed() < PLAYER_MIN_SPEED){
		return PLAYER_MIN_SPEED;
	}

	return getSpeed();
}

void Player::updateBaseSpeed()
{
	if(!hasFlag(PlayerFlag_SetMaxSpeed)){
		baseSpeed = 220 + (2* (level - 1));
	}
	else{
		baseSpeed = 900;
	};
}

Container* Player::getContainer(uint32_t cid)
{
  for(ContainerVector::iterator it = containerVec.begin(); it != containerVec.end(); ++it){
		if(it->first == cid)
			return it->second;
	}

	return NULL;
}

int32_t Player::getContainerID(const Container* container) const
{
  for(ContainerVector::const_iterator cl = containerVec.begin(); cl != containerVec.end(); ++cl){
	  if(cl->second == container)
			return cl->first;
	}

	return -1;
}

void Player::addContainer(uint32_t cid, Container* container)
{
#ifdef __DEBUG__
	std::cout << getName() << ", addContainer: " << (int)cid << std::endl;
#endif
	if(cid > 0xF)
		return;

	for(ContainerVector::iterator cl = containerVec.begin(); cl != containerVec.end(); ++cl) {
		if(cl->first == cid) {
			cl->second = container;
			return;
		}
	}

	//id doesnt exist, create it
	containervector_pair cv;
	cv.first = cid;
	cv.second = container;

	containerVec.push_back(cv);
}

void Player::closeContainer(uint32_t cid)
{
  for(ContainerVector::iterator cl = containerVec.begin(); cl != containerVec.end(); ++cl){
	  if(cl->first == cid){
		  containerVec.erase(cl);
			break;
		}
	}

#ifdef __DEBUG__
	std::cout << getName() << ", closeContainer: " << (int)cid << std::endl;
#endif
}

uint16_t Player::getLookCorpse() const
{
	if(sex != 0)
		return ITEM_MALE_CORPSE;
	else
		return ITEM_FEMALE_CORPSE;
}

void Player::dropLoot(Container* corpse)
{
	if(!corpse){
		return;
	}

	uint32_t itemLoss = lossPercent[LOSS_ITEMS];
	uint32_t backpackLoss = lossPercent[LOSS_CONTAINERS];
#ifdef __SKULLSYSTEM__
	if(getSkull() == SKULL_RED){
		itemLoss = 100;
		backpackLoss = 100;
	}
#endif
	if(!lootDrop){
		itemLoss = 0;
		backpackLoss = 0;
	}

	if(itemLoss > 0 || backpackLoss > 0){
		for(int i = SLOT_FIRST; i < SLOT_LAST; ++i){
			Item* item = inventory[i];
			if(item){
				if((item->getContainer() && (uint32_t)random_range(1, 100) <= backpackLoss) || (!item->getContainer() && (uint32_t)random_range(1, 100) <= itemLoss)){
					g_game.internalMoveItem(this, corpse, INDEX_WHEREEVER, item, item->getItemCount(), 0);
				}
			}
		}
	}
}

void Player::addStorageValue(const uint32_t key, const int32_t value)
{

	storageMap[key] = value;
}

bool Player::getStorageValue(const uint32_t key, int32_t& value) const
{
	StorageMap::const_iterator it;
	it = storageMap.find(key);
	if(it != storageMap.end()){
		value = it->second;
		return true;
	}
	else{
		value = 0;
		return false;
	}
}

bool Player::eraseStorageValue(const uint32_t key)
{
 	if(IS_IN_KEYRANGE(key, RESERVED_RANGE)) {
		std::cout << "You can't erase the key number "<< key << "as it is reserved.\n";
		return(false);
		}
	StorageMap::iterator it= storageMap.find(key);
	if(it != storageMap.end()) {
		storageMap.erase(it);
		return(true);
		}
	return(false); //key not found
}

//static
bool Player::getStorageValueByName(const std::string name, const uint32_t key, int32_t& value)
{
	Player* target = g_game.getPlayerByName(name);
	if(!target){
		target = new Player(name, NULL);
		if(!IOPlayer::instance()->loadPlayer(target, name)){
			delete target;
			return false;
		}
	}
	bool ret = target->getStorageValue(key, value);
	if(!target->isOnline()){
		IOPlayer::instance()->savePlayer(target);
		delete target;
	}
	return (ret);
}

//static
bool Player::setStorageValueByName(const std::string name, const uint32_t key, const int32_t value)
{
	Player* target = g_game.getPlayerByName(name);
	if(!target){
		target = new Player(name, NULL);
		if(!IOPlayer::instance()->loadPlayer(target, name)){
			delete target;
			return false;
		}
	}
	target->addStorageValue(key, value);
	if(!target->isOnline()){
		IOPlayer::instance()->savePlayer(target);
		delete target;
	}
	return (true);
}

//static
bool Player::eraseStorageValueByName(const std::string name, const uint32_t key)
{
	Player* target = g_game.getPlayerByName(name);
	if(!target){
		target = new Player(name, NULL);
		if(!IOPlayer::instance()->loadPlayer(target, name)){
			delete target;
			return false;
		}
	}
	bool ret = target->eraseStorageValue(key);
	if(!target->isOnline()){
		IOPlayer::instance()->savePlayer(target);
		delete target;
	}
	return (ret);
}

bool Player::canSee(const Position& pos) const
{
	if(client){
		return client->canSee(pos);
	}

	return false;
}

bool Player::canSeeInvisibility() const
{
	return hasFlag(PlayerFlag_CanSenseInvisibility);
}

bool Player::canSeeCreature(const Creature* creature) const
{
	if(creature->getPlayer() && creature->getPlayer()->isGmInvisible() &&
		!canSeeGmInvisible((Player*)creature->getPlayer()))
		return false;

	if(canSeeInvisibility() || creature == this){
		return true;
	}

	if(creature->getPlayer()){
		/*if(creature->getPlayer()->hasFlag(PlayerFlag_CannotBeSeen)){
			return false;
		}
		*/
		return true;
	}
	else if(creature->isInvisible()){
		return false;
	}

	return true;
}

bool Player::canWalkthrough(const Creature* creature) const
{
	if(!creature->getPlayer()){
    return false;
  }

	if(creature->getPlayer()->hasSomeInvisibilityFlag()){
		return true;
	}

  return false;
}

bool Player::canBePushedBy(const Player *player) const
{
	if (player->hasFlag(PlayerFlag_CanPushAllCreatures))
		return true;

	return isPushable();
}

bool Player::isNearDepotBox(uint32_t depotId)
{
	Position pos = getPosition();

	for(int32_t cx = -1; cx <= 1; ++cx){
		for(int32_t cy = -1; cy <= 1; ++cy){
			Tile* tile = g_game.getTile(pos.x + cx, pos.y + cy, pos.z);
			if(!tile){
				return false;
			}

			if(!tile->hasFlag(TILESTATE_DEPOT)){
				continue;
			}

			for(uint32_t i = 0; i < tile->getThingCount(); ++i){
				if(Item* item = tile->__getThing(i)->getItem()){
					const ItemType& it = Item::items[item->getID()];
					if(it.type == ITEM_TYPE_DEPOT){
						Depot* depot = NULL;
						if(item->getContainer() && (depot = item->getContainer()->getDepot())){
							if(depot->getDepotId() == depotId){
								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}

Depot* Player::getDepot(uint32_t depotId, bool autoCreateDepot)
{
	DepotMap::iterator it = depots.find(depotId);
	if(it != depots.end()){
		return it->second;
	}

	//depot does not yet exist

	//create a new depot?
	if(autoCreateDepot){
		Depot* depot = NULL;
		Item* tmpDepot = Item::CreateItem(ITEM_LOCKER1);
		if(tmpDepot->getContainer() && (depot = tmpDepot->getContainer()->getDepot())){
			Item* depotChest = Item::CreateItem(ITEM_DEPOT);
			depot->__internalAddThing(depotChest);

			addDepot(depot, depotId);
			return depot;
		}
		else{
			g_game.FreeThing(tmpDepot);
			std::cout << "Failure: Creating a new depot with id: "<< depotId <<
				", for player: " << getName() << std::endl;
		}
	}

	return NULL;
}

bool Player::addDepot(Depot* depot, uint32_t depotId)
{
	Depot* depot2 = getDepot(depotId, false);
	if(depot2){
		return false;
	}

	depots[depotId] = depot;
	depot->setMaxDepotLimit(maxDepotLimit);
	return true;
}

void Player::sendCancelMessage(ReturnValue message) const
{
	switch(message){
	case RET_CONTAINERHASTOMANYCONTAINERS:
		sendCancel("It would exceed the maximum size for a chain of containers.");
		break;

	case RET_CONTAINERHASTOMANYITEMS:
		sendCancel("It would exceed the maximum amount of items inside of some container.");
		break;

	case RET_DESTINATIONOUTOFREACH:
		sendCancel("Destination is out of reach.");
		break;

	case RET_NOTMOVEABLE:
		sendCancel("You cannot move this object.");
		break;

	case RET_DROPTWOHANDEDITEM:
		sendCancel("Drop the double-handed object first.");
		break;

	case RET_BOTHHANDSNEEDTOBEFREE:
		sendCancel("Both hands needs to be free.");
		break;

	case RET_CANNOTBEDRESSED:
		sendCancel("You cannot dress this object there.");
		break;

	case RET_PUTTHISOBJECTINYOURHAND:
		sendCancel("Put this object in your hand.");
		break;

	case RET_PUTTHISOBJECTINBOTHHANDS:
		sendCancel("Put this object in both hands.");
		break;

	case RET_CANONLYUSEONEWEAPON:
		sendCancel("You may only use one weapon.");
		break;

	case RET_CANONLYUSEONESHIELD:
		sendCancel("You may only use one shield.");
		break;

	case RET_TOOFARAWAY:
		sendCancel("Too far away.");
		break;

	case RET_FIRSTGODOWNSTAIRS:
		sendCancel("First go downstairs.");
		break;

	case RET_FIRSTGOUPSTAIRS:
		sendCancel("First go upstairs.");
		break;

	case RET_NOTENOUGHCAPACITY:
		sendCancel("This object is too heavy.");
		break;

	case RET_CONTAINERNOTENOUGHROOM:
		sendCancel("You cannot put more objects in this container.");
		break;

	case RET_NEEDEXCHANGE:
	case RET_NOTENOUGHROOM:
		sendCancel("There is not enough room.");
		break;

	case RET_CANNOTPICKUP:
		sendCancel("You cannot pickup this object.");
		break;

	case RET_CANNOTTHROW:
		sendCancel("You cannot throw there.");
		break;

	case RET_THEREISNOWAY:
		sendCancel("There is no way.");
		break;

	case RET_THISISIMPOSSIBLE:
		sendCancel("This is impossible.");
		break;

	case RET_PLAYERISPZLOCKED:
		sendCancel("You can not enter a protection zone after attacking another player.");
		break;

	case RET_PLAYERISNOTINVITED:
		sendCancel("You are not invited.");
		break;

	case RET_CREATUREDOESNOTEXIST:
		sendCancel("Creature does not exist.");
		break;

	case RET_DEPOTISFULL:
		sendCancel("Your depot is full. Remove surplus items before storing new ones.");
		break;

	case RET_CANNOTUSETHISOBJECT:
		sendCancel("You cannot use this object.");
		break;

	case RET_PLAYERWITHTHISNAMEISNOTONLINE:
		sendCancel("A player with this name is not online.");
		break;

	case RET_NOTREQUIREDLEVELTOUSERUNE:
		sendCancel("You do not have the required magic level to use this rune.");
		break;

	case RET_YOUAREALREADYTRADING:
		sendCancel("You are already trading.");
		break;

	case RET_THISPLAYERISALREADYTRADING:
		sendCancel("This player is already trading.");
		break;

	case RET_YOUMAYNOTLOGOUTDURINGAFIGHT:
		sendCancel("You may not logout during or immediately after a fight!");
		break;

	case RET_DIRECTPLAYERSHOOT:
		sendCancel("You are not allowed to shoot directly on players.");
		break;

	case RET_NOTENOUGHLEVEL:
		sendCancel("You do not have enough level.");
		break;

	case RET_NOTENOUGHMAGICLEVEL:
		sendCancel("You do not have enough magic level.");
		break;

	case RET_NOTENOUGHMANA:
		sendCancel("You do not have enough mana.");
		break;

	case RET_NOTENOUGHSOUL:
		sendCancel("You do not have enough soul.");
		break;

	case RET_YOUAREEXHAUSTED:
		sendCancel("You are exhausted.");
		break;

	case RET_CANONLYUSETHISRUNEONCREATURES:
		sendCancel("You can only use this rune on creatures.");
		break;

	case RET_PLAYERISNOTREACHABLE:
		sendCancel("Player is not reachable.");
		break;

	case RET_CREATUREISNOTREACHABLE:
		sendCancel("Creature is not reachable.");
		break;

	case RET_ACTIONNOTPERMITTEDINPROTECTIONZONE:
		sendCancel("This action is not permitted in a protection zone.");
		break;

	case RET_YOUMAYNOTATTACKTHISPERSON:
		sendCancel("You may not attack this person.");
		break;

	case RET_YOUMAYNOTATTACKTHISCREATURE:
		sendCancel("You may not attack this creature.");
		break;

	case RET_YOUMAYNOTATTACKAPERSONINPROTECTIONZONE:
		sendCancel("You may not attack a person in a protection zone.");
		break;

	case RET_YOUMAYNOTATTACKAPERSONWHILEINPROTECTIONZONE:
		sendCancel("You may not attack a person while you are in a protection zone.");
		break;

	case RET_YOUCANONLYUSEITONCREATURES:
		sendCancel("You can only use it on creatures.");
		break;

	case RET_TURNSECUREMODETOATTACKUNMARKEDPLAYERS:
		sendCancel("Turn secure mode off if you really want to attack unmarked players.");
		break;

	case RET_YOUNEEDPREMIUMACCOUNT:
		sendCancel("You need a premium account to use this spell.");
		break;

	case RET_YOUNEEDTOLEARNTHISSPELL:
		sendCancel("You need to learn this spell first.");
		break;

	case RET_YOURVOCATIONCANNOTUSETHISSPELL:
		sendCancel("Your vocation cannot use this spell.");
		break;

	case RET_YOUNEEDAWEAPONTOUSETHISSPELL:
		sendCancel("You need to equip a weapon to use this spell.");
		break;

	case RET_PLAYERISPZLOCKEDLEAVEPVPZONE:
		sendCancel("You can not leave a pvp zone after attacking another player.");
		break;

	case RET_PLAYERISPZLOCKEDENTERPVPZONE:
		sendCancel("You can not enter a pvp zone after attacking another player.");
		break;

	case RET_ACTIONNOTPERMITTEDINANONPVPZONE:
		sendCancel("This action is not permitted in a non-pvp zone.");
		break;

	case RET_YOUCANNOTLOGOUTHERE:
		sendCancel("You can not logout here.");
		break;

	case RET_YOUNEEDAMAGICITEMTOCASTSPELL:
		sendCancel("You need a magic item to cast this spell.");
		break;

	case RET_CANNOTCONJUREITEMHERE:
		sendCancel("You cannot conjure items here.");
		break;

	case RET_YOUNEEDTOSPLITYOURSPEARS:
		sendCancel("You need to split your spears first.");
		break;

	case RET_NAMEISTOOAMBIGIOUS:
		sendCancel("Name is too ambigious.");
		break;

	case RET_YOUARENOTTHEOWNER:
		sendCancel("You are not the owner.");
		break;

	case RET_NOTREQUIREDPROFESSION:
		sendCancel("You don't have the required profession.");
		break;

	case RET_NOTREQUIREDLEVEL:
		sendCancel("You don't have the required level.");
		break;

	case RET_NEEDPREMIUMTOEQUIPITEM:
		sendCancel("You need a premium account to equip this item.");
		break;

	case RET_NOTPOSSIBLE:
	default:
		sendCancel("Sorry, not possible.");
		break;
	}
}

void Player::sendStats()
{
	if(client){
		client->sendStats();
	}
}

Item* Player::getWriteItem(uint32_t& _windowTextId, uint16_t& _maxWriteLen)
{
	_windowTextId = windowTextId;
	_maxWriteLen = maxWriteLen;
	return writeItem;
}

void Player::setWriteItem(Item* item, uint16_t _maxWriteLen /*= 0*/)
{
	windowTextId++;
	if(writeItem){
		writeItem->releaseThing2();
	}

	if(item){
		writeItem = item;
		maxWriteLen = _maxWriteLen;
		writeItem->useThing2();
	}
	else{
		writeItem = NULL;
		maxWriteLen = 0;
	}
}

House* Player::getEditHouse(uint32_t& _windowTextId, uint32_t& _listId)
{
	_windowTextId = windowTextId;
	_listId = editListId;
	return editHouse;
}

void Player::setEditHouse(House* house, uint32_t listId /*= 0*/)
{
	windowTextId++;
	editHouse = house;
	editListId = listId;
}

void Player::sendHouseWindow(House* house, uint32_t listId) const
{
	if(client){
		std::string text;
		if(house->getAccessList(listId, text)){
			client->sendHouseWindow(windowTextId, house, listId, text);
		}
	}
}

//container
void Player::sendAddContainerItem(const Container* container, const Item* item)
{
	if(client){
		for(ContainerVector::const_iterator cl = containerVec.begin(); cl != containerVec.end(); ++cl){
			if(cl->second == container){
				client->sendAddContainerItem(cl->first, item);
			}
		}
	}
}

void Player::sendUpdateContainerItem(const Container* container, uint8_t slot, const Item* oldItem, const Item* newItem)
{
	if(client){
		for(ContainerVector::const_iterator cl = containerVec.begin(); cl != containerVec.end(); ++cl){
			if(cl->second == container){
				client->sendUpdateContainerItem(cl->first, slot, newItem);
			}
		}
	}
}

void Player::sendRemoveContainerItem(const Container* container, uint8_t slot, const Item* item)
{
	if(client){
		for(ContainerVector::const_iterator cl = containerVec.begin(); cl != containerVec.end(); ++cl){
			if(cl->second == container){
				client->sendRemoveContainerItem(cl->first, slot);
			}
		}
	}
}

void Player::onAddTileItem(const Tile* tile, const Position& pos, const Item* item)
{
	Creature::onAddTileItem(tile, pos, item);
}

void Player::onUpdateTileItem(const Tile* tile, const Position& pos,
	const Item* oldItem, const ItemType& oldType, const Item* newItem, const ItemType& newType)
{
	Creature::onUpdateTileItem(tile, pos, oldItem, oldType, newItem, newType);

	if(oldItem != newItem){
		onRemoveTileItem(tile, pos, oldType, oldItem);
	}

	if(tradeState != TRADE_TRANSFER){
		if(tradeItem && oldItem == tradeItem){
			g_game.internalCloseTrade(this);
		}
	}
}

void Player::onRemoveTileItem(const Tile* tile, const Position& pos,
	const ItemType& iType, const Item* item)
{
	Creature::onRemoveTileItem(tile, pos, iType, item);

	if(tradeState != TRADE_TRANSFER){
		checkTradeState(item);

		if(tradeItem){
			const Container* container = item->getContainer();
			if(container && container->isHoldingItem(tradeItem)){
				g_game.internalCloseTrade(this);
			}
		}
	}
}

void Player::onUpdateTile(const Tile* tile, const Position& pos)
{
	Creature::onUpdateTile(tile, pos);
}

void Player::onCreatureAppear(const Creature* creature, bool isLogin)
{
	Creature::onCreatureAppear(creature, isLogin);

	if(isLogin && creature == this){
		Item* item;
		for(int slot = SLOT_FIRST; slot < SLOT_LAST; ++slot){
			if((item = getInventoryItem((slots_t)slot))){
				item->__startDecaying();
				g_moveEvents->onPlayerEquip(this, item, (slots_t)slot);
			}
		}

		if(!storedConditionList.empty()){
			for(ConditionList::const_iterator it = storedConditionList.begin(); it != storedConditionList.end(); ++it){
				if((*it)->getType() == CONDITION_REGENERATION && (*it)->getSubId() == 0){
					(*it)->setParam(CONDITIONPARAM_HEALTHGAIN, vocation->getHealthGainAmount());
					(*it)->setParam(CONDITIONPARAM_HEALTHTICKS, vocation->getHealthGainTicks() * 1000);
					(*it)->setParam(CONDITIONPARAM_MANAGAIN, vocation->getManaGainAmount());
					(*it)->setParam(CONDITIONPARAM_MANATICKS, vocation->getManaGainTicks() * 1000);
				}
				addCondition(*it);
			}

			storedConditionList.clear();
		}

		//[ added for beds system
		BedItem* bed = Beds::instance().getBedBySleeper(getGUID());
		if(bed){
			bed->wakeUp();
			#ifdef __DEBUG__
			std::cout << "Player " << getName() << " waking up." << std::endl;
			#endif
		}
		//]

		if(lastLogout > 0)
		{
			sendStats();
		}
	}
}

void Player::onAttackedCreatureDissapear(bool isLogout)
{
	sendCancelTarget();

	if(!isLogout){
		sendTextMessage(MSG_STATUS_SMALL, "Target lost.");
	}
}

void Player::onFollowCreatureDissapear(bool isLogout)
{
	sendCancelTarget();

	if(!isLogout){
		sendTextMessage(MSG_STATUS_SMALL, "Target lost.");
	}
}

void Player::onChangeZone(ZoneType_t zone)
{
	if(attackedCreature){
		if(zone == ZONE_PROTECTION){
			if(!hasFlag(PlayerFlag_IgnoreProtectionZone)){
				setAttackedCreature(NULL);
				onAttackedCreatureDissapear(false);
			}
		}
		else if(zone == ZONE_NOPVP){
			if( (attackedCreature->getPlayer() ||
					(attackedCreature->isPlayerSummon()) ) &&
					!hasFlag(PlayerFlag_IgnoreProtectionZone)){
				setAttackedCreature(NULL);
				onAttackedCreatureDissapear(false);
			}
		}
	}

	sendIcons();
}

void Player::onAttackedCreatureChangeZone(ZoneType_t zone)
{
	if(zone == ZONE_PROTECTION){
		if(!hasFlag(PlayerFlag_IgnoreProtectionZone)){
			setAttackedCreature(NULL);
			onAttackedCreatureDissapear(false);
		}
	}
	else if(zone == ZONE_NOPVP){
		if(attackedCreature->getPlayer() && !hasFlag(PlayerFlag_IgnoreProtectionZone)){
			setAttackedCreature(NULL);
			onAttackedCreatureDissapear(false);
		}
	}
	else if(zone == ZONE_NORMAL){
		//attackedCreature can leave a pvp zone if not pzlocked
		if (g_game.getWorldType() == WORLD_TYPE_NO_PVP){
			if(attackedCreature->getPlayer()){
				setAttackedCreature(NULL);
				onAttackedCreatureDissapear(false);
			}
		}
	}
}

void Player::onCreatureDisappear(const Creature* creature, bool isLogout)
{
	Creature::onCreatureDisappear(creature, isLogout);

	if(creature == this){
		if(isLogout){
			loginPosition = getPosition();
		}

		if(eventWalk != 0){
			setFollowCreature(NULL);
		}

		clearPartyInvitations();
		if(getParty()){
			getParty()->leaveParty(this);
		}

		if(tradePartner){
			g_game.internalCloseTrade(this);
		}

		g_game.cancelRuleViolation(this);

		if(hasFlag(PlayerFlag_CanAnswerRuleViolations)){
			std::list<Player*> closeReportList;
			for(RuleViolationsMap::const_iterator it = g_game.getRuleViolations().begin(); it != g_game.getRuleViolations().end(); ++it){
				if(it->second->gamemaster == this){
					closeReportList.push_back(it->second->reporter);
				}
			}

			for(std::list<Player*>::iterator it = closeReportList.begin(); it != closeReportList.end(); ++it){
				g_game.closeRuleViolation(*it);
			}
		}

		g_chat.removeUserFromAllChannels(this);

		lastLogout = std::time(NULL);
		IOPlayer::instance()->updateLogoutInfo(this);

		bool saved = false;
		for(uint32_t tries = 0; tries < 3; ++tries){
			if(IOPlayer::instance()->savePlayer(this)){
				saved = true;
				break;
			}
		}
		if(!saved){
			std::cout << "Error while saving player: " << getName() << std::endl;
		}

#ifdef __DEBUG_PLAYERS__
		std::cout << (uint32_t)g_game.getPlayersOnline() << " players online." << std::endl;
#endif
	}
}

void Player::onWalk(Direction& dir)
{
	Creature::onWalk(dir);
	setNextActionTask(NULL);
	setNextAction(OTSYS_TIME() + getStepDuration(dir));
}

void Player::onCreatureMove(const Creature* creature, const Tile* newTile, const Position& newPos,
	const Tile* oldTile, const Position& oldPos, bool teleport)
{
	Creature::onCreatureMove(creature, newTile, newPos, oldTile, oldPos, teleport);

	if(creature == this){
		if(tradeState != TRADE_TRANSFER){
			//check if we should close trade
			if(tradeItem){
				if(!Position::areInRange<1,1,0>(tradeItem->getPosition(), getPosition())){
					g_game.internalCloseTrade(this);
				}
			}

			if(tradePartner){
				if(!Position::areInRange<2,2,0>(tradePartner->getPosition(), getPosition())){
					g_game.internalCloseTrade(this);
				}
			}
		}

		if(getParty()){
			getParty()->updateSharedExperience();
		}

		if(teleport || (oldPos.z != newPos.z)){
			addCondition(Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_PACIFIED,
				g_config.getNumber(ConfigManager::STAIRHOP_EXHAUSTED)));
			addCondition(Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_EXHAUST_COMBAT,
				g_config.getNumber(ConfigManager::STAIRHOP_EXHAUSTED)));
		}
	}
}

//container
void Player::onAddContainerItem(const Container* container, const Item* item)
{
	checkTradeState(item);
	if(backpack.first && (const_cast<Container*>(container) != backpack.first || backpack.first->full())){
		backpack.first = NULL;
	}
}

void Player::onUpdateContainerItem(const Container* container, uint8_t slot,
	const Item* oldItem, const ItemType& oldType, const Item* newItem, const ItemType& newType)
{
	if(tradeState == TRADE_TRANSFER){
		return;
	}

	checkTradeState(oldItem);
	if(oldItem != newItem && tradeItem){
		if(tradeItem->getParent() != container && container->isHoldingItem(tradeItem)){
			g_game.internalCloseTrade(this);
		}
	}

	if(tradeState != TRADE_TRANSFER){
		checkTradeState(oldItem);
	}
}

void Player::onRemoveContainerItem(const Container* container, uint8_t slot, const Item* item)
{
	backpack.first = NULL;
	if(tradeState == TRADE_TRANSFER){
		return;
	}

	checkTradeState(item);
	if(tradeItem){
		if(tradeItem->getParent() != container && container->isHoldingItem(tradeItem)){
			g_game.internalCloseTrade(this);
		}
	}
}

void Player::onCloseContainer(const Container* container)
{
	if(client){
		for(ContainerVector::const_iterator cl = containerVec.begin(); cl != containerVec.end(); ++cl){
			if(cl->second == container){
				client->sendCloseContainer(cl->first);
			}
		}
	}
}

void Player::onSendContainer(const Container* container)
{
	if(client){
		bool hasParent = (dynamic_cast<const Container*>(container->getParent()) != NULL);

		for(ContainerVector::const_iterator cl = containerVec.begin(); cl != containerVec.end(); ++cl){
			if(cl->second == container){
				client->sendContainer(cl->first, container, hasParent);
			}
		}
	}
}

//inventory
void Player::onAddInventoryItem(slots_t slot, Item* item)
{
	//
}

void Player::onUpdateInventoryItem(slots_t slot, Item* oldItem, const ItemType& oldType,
	Item* newItem, const ItemType& newType)
{
	if(tradeState == TRADE_TRANSFER){
		return;
	}

	checkTradeState(oldItem);
	if(oldItem != newItem && tradeItem){
		const Container* container = oldItem->getContainer();
		if(container && container->isHoldingItem(tradeItem)){
			g_game.internalCloseTrade(this);
		}
	}

	if(tradeState != TRADE_TRANSFER){
		checkTradeState(oldItem);
	}
}

void Player::onRemoveInventoryItem(slots_t slot, Item* item)
{
	//setItemAbility(slot, false);

	backpack.first = NULL;
	if(tradeState == TRADE_TRANSFER){
		return;
	}

	checkTradeState(item);
	if(tradeItem){
		const Container* container = item->getContainer();
		if(container && container->isHoldingItem(tradeItem)){
			g_game.internalCloseTrade(this);
		}
	}
}

void Player::checkTradeState(const Item* item)
{
	if(tradeItem && tradeState != TRADE_TRANSFER){
		if(tradeItem == item){
			g_game.internalCloseTrade(this);
		}
		else{
			const Container* container = dynamic_cast<const Container*>(item->getParent());

			while(container != NULL){
				if(container == tradeItem){
					g_game.internalCloseTrade(this);
					break;
				}

				container = dynamic_cast<const Container*>(container->getParent());
			}
		}
	}
}

void Player::setNextWalkActionTask(SchedulerTask* task)
{
	if(walkTaskEvent != 0){
		g_scheduler.stopEvent(walkTaskEvent);
		walkTaskEvent = 0;
	}
	delete walkTask;
	walkTask = task;
	resetIdle();
}

void Player::setNextActionTask(SchedulerTask* task)
{
	if(actionTaskEvent != 0){
		g_scheduler.stopEvent(actionTaskEvent);
		actionTaskEvent = 0;
	}

	if(task){
		actionTaskEvent = g_scheduler.addEvent(task);
		resetIdle();
	}
}

uint32_t Player::getNextActionTime() const
{
	int64_t time = nextAction - OTSYS_TIME();
	if(time < SCHEDULER_MINTICKS){
		return SCHEDULER_MINTICKS;
	}

	return time;
}


void Player::onThink(uint32_t interval)
{
	Creature::onThink(interval);

	int64_t timeNow = OTSYS_TIME();
	if(timeNow - last_ping >= 5000){
		last_ping = timeNow;

		if(client){
			client->sendPing();
		}
	}

	if(OTSYS_TIME() - last_pong >= 60000){
		if(canLogout()){
			if(client){
				client->logout(true);
			}
			else{
				g_game.removeCreature(this, true);
			}
		}
	}

	if(canLogout() && !hasCondition(CONDITION_INFIGHT) && !client){
		g_game.removeCreature(this, true);
	}

	MessageBufferTicks += interval;
	if(MessageBufferTicks >= 1500){
		MessageBufferTicks = 0;
		addMessageBuffer();
	}

	checkIdleTime(interval);
#ifdef __SKULLSYSTEM__
	checkSkullTicks(interval);
#endif
}

uint32_t Player::getMuteTime()
{
	if(hasFlag(PlayerFlag_CannotBeMuted)){
		return 0;
	}

	int32_t muteTicks = 0;
	for(ConditionList::iterator it = conditions.begin(); it != conditions.end(); ++it){
		if((*it)->getType() == CONDITION_MUTED && (*it)->getTicks() > muteTicks){
			muteTicks = (*it)->getTicks();
		}
	}

	return ((uint32_t)muteTicks / 1000);
}

void Player::addMessageBuffer()
{
	if(MessageBufferCount > 0 && g_config.getNumber(ConfigManager::MAX_MESSAGEBUFFER) != 0 && !hasFlag(PlayerFlag_CannotBeMuted)){
		MessageBufferCount -= 1;
	}
}

void Player::removeMessageBuffer()
{
	if(!hasFlag(PlayerFlag_CannotBeMuted) && g_config.getNumber(ConfigManager::MAX_MESSAGEBUFFER) != 0
		&& MessageBufferCount <= g_config.getNumber(ConfigManager::MAX_MESSAGEBUFFER) + 1){
		MessageBufferCount += 1;

		if(MessageBufferCount > g_config.getNumber(ConfigManager::MAX_MESSAGEBUFFER)){
			uint32_t muteCount = 1;
			MuteCountMap::iterator it = muteCountMap.find(getGUID());
			if(it != muteCountMap.end()){
				muteCount = it->second;
			}

			uint32_t muteTime = 5 * muteCount * muteCount;
			muteCountMap[getGUID()] = muteCount + 1;
			Condition* condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_MUTED, muteTime * 1000, 0);
			addCondition(condition);

			std::stringstream ss;
			ss << "You are muted for " << muteTime << " seconds.";
			sendTextMessage(MSG_STATUS_SMALL, ss.str());
		}
	}
}

void Player::drainHealth(Creature* attacker, CombatType_t combatType, int32_t damage)
{
	Creature::drainHealth(attacker, combatType, damage);

	sendStats();

	std::stringstream ss;
	if(damage == 1) {
		ss << "You lose 1 hitpoint";
	}
	else
		ss << "You lose " << damage << " hitpoints";

	if(attacker){
		ss << " due to an attack by " << attacker->getNameDescription();
	}

	ss << ".";

	sendTextMessage(MSG_EVENT_DEFAULT, ss.str());
}

void Player::drainMana(Creature* attacker, int32_t points)
{
	Creature::drainMana(attacker, points);

	sendStats();

	std::stringstream ss;
	if(attacker){
		ss << "You lose " << points << " mana blocking an attack by " << attacker->getNameDescription() << ".";
	}
	else{
		ss << "You lose " << points << " mana.";
	}

	sendTextMessage(MSG_EVENT_DEFAULT, ss.str());
}

void Player::addManaSpent(uint64_t amount, bool useMultiplier /*= true*/)
{
	if(amount != 0 && !hasFlag(PlayerFlag_NotGainMana)){
		if(useMultiplier){
			amount = uint32_t(amount * getRateValue(LEVEL_MAGIC));
		}
		manaSpent += amount * g_config.getNumber(ConfigManager::RATE_MAGIC);

		uint32_t origLevel = magLevel;

		uint64_t reqMana = vocation->getReqMana(origLevel + 1);
		if(reqMana == 0)
			return;

		while(manaSpent >= reqMana){
			manaSpent -= reqMana;
			magLevel++;
			reqMana = vocation->getReqMana(magLevel + 1);
		}

		if (magLevel != origLevel){
			std::stringstream MaglvMsg;
			MaglvMsg << "You advanced to magic level " << magLevel << ".";
			sendTextMessage(MSG_EVENT_ADVANCE, MaglvMsg.str());

			//scripting event - onAdvance
			onAdvanceEvent(LEVEL_MAGIC, origLevel, magLevel);
		}

		magLevelPercent = Player::getPercentLevel(manaSpent, reqMana);
		sendStats();
	}
}

void Player::addExperience(uint64_t exp)
{
	experience += exp;
	uint32_t prevLevel = getLevel();
	uint32_t newLevel = getLevel();

	uint64_t currLevelExp = Player::getExpForLevel(newLevel);
	uint64_t nextLevelExp = Player::getExpForLevel(newLevel + 1);
	if(nextLevelExp < currLevelExp) {
		// Cannot gain more experience
		// Perhaps some sort of notice should be printed here?
		levelPercent = 0;
		sendStats();
		return;
	}
	while(experience >= nextLevelExp) {
		++newLevel;
		healthMax += vocation->getHPGain();
		health += vocation->getHPGain();
		manaMax += vocation->getManaGain();
		mana += vocation->getManaGain();
		capacity += vocation->getCapGain();
		nextLevelExp = Player::getExpForLevel(newLevel + 1);
	}

	if(prevLevel != newLevel){
		level = newLevel;
		updateBaseSpeed();

		int32_t newSpeed = getBaseSpeed();
		setBaseSpeed(newSpeed);

		g_game.changeSpeed(this, 0);
		g_game.addCreatureHealth(this);

		if(getParty()){
			getParty()->updateSharedExperience();
		}

		std::stringstream levelMsg;
		levelMsg << "You advanced from Level " << prevLevel << " to Level " << newLevel << ".";
		sendTextMessage(MSG_EVENT_ADVANCE, levelMsg.str());

		//scripting event - onAdvance
		onAdvanceEvent(LEVEL_EXPERIENCE, prevLevel, newLevel);
	}

	currLevelExp = Player::getExpForLevel(level);
	nextLevelExp = Player::getExpForLevel(level + 1);
	if(nextLevelExp > currLevelExp) {
		uint32_t newPercent = Player::getPercentLevel(getExperience() - currLevelExp, Player::getExpForLevel(level + 1) - currLevelExp);
		levelPercent = newPercent;
	}
	else {
		levelPercent = 0;
	}

	sendStats();
}

void Player::removeExperience(uint64_t exp, bool updateStats /*= true*/)
{
	experience -= std::min(exp, experience);
	uint32_t prevLevel = getLevel();
	uint32_t newLevel = getLevel();

	while(newLevel > 1 && experience < Player::getExpForLevel(newLevel)){
		newLevel--;
		healthMax -= vocation->getHPGain();
		manaMax -= vocation->getManaGain();
		capacity -= vocation->getCapGain();
	}

	if(prevLevel != newLevel){
		level = newLevel;
		std::stringstream levelMsg;
		levelMsg << "You were downgraded from Level " << prevLevel << " to Level " << newLevel << ".";
		sendTextMessage(MSG_EVENT_ADVANCE, levelMsg.str());
	}

	//Only if player is not going to be removed (usually when dying)
	if(updateStats){
		bool sentStats = false;

		uint64_t currLevelExp = Player::getExpForLevel(level);
		uint64_t nextLevelExp = Player::getExpForLevel(level + 1);
		if(nextLevelExp > currLevelExp){
			uint32_t newPercent = Player::getPercentLevel(getExperience() - currLevelExp, Player::getExpForLevel(level + 1) - currLevelExp);
			levelPercent = newPercent;
		}
		else{
			levelPercent = 0;
		}

		if(prevLevel != newLevel){
			int32_t healthChange = health > healthMax ? (health - healthMax) : 0;
			int32_t manaChange = mana > manaMax ? (mana - manaMax) : 0;
			changeMana(-manaChange);
			changeHealth(-healthChange);
			sentStats = true;
		}

		if(!sentStats){
			sendStats();
		}
	}
}

uint32_t Player::getPercentLevel(uint64_t count, uint32_t nextLevelCount)
{
	if(nextLevelCount > 0){
		uint32_t result = ((uint32_t)((double)count / nextLevelCount * 100));
		if(result < 0 || result > 100){
			return 0;
		}

		return result;
	}

	return 0;
}

void Player::onBlockHit(BlockType_t blockType)
{
	if(shieldBlockCount > 0){
		--shieldBlockCount;

		if(hasShield()){
			addSkillAdvance(SKILL_SHIELD, 1);
		}
	}
}

void Player::onAttackedCreatureBlockHit(Creature* target, BlockType_t blockType)
{
	Creature::onAttackedCreatureBlockHit(target, blockType);

	lastAttackBlockType = blockType;

	switch(blockType){
		case BLOCK_NONE:
		{
			addAttackSkillPoint = true;
			bloodHitCount = 30;
			shieldBlockCount = 30;

			break;
		}

		case BLOCK_DEFENSE:
		case BLOCK_ARMOR:
		{
			//need to draw blood every 30 hits
			if(bloodHitCount > 0){
				addAttackSkillPoint = true;
				--bloodHitCount;
			}
			else{
				addAttackSkillPoint = false;
			}

			break;
		}

		default:
		{
			addAttackSkillPoint = false;
			break;
		}
	}
}

bool Player::hasShield() const
{
	bool result = false;
	Item* item;

	item = getInventoryItem(SLOT_LEFT);
	if(item && item->getWeaponType() == WEAPON_SHIELD){
		result = true;
	}

	item = getInventoryItem(SLOT_RIGHT);
	if(item && item->getWeaponType() == WEAPON_SHIELD){
		result = true;
	}

	return result;
}

BlockType_t Player::blockHit(Creature* attacker, CombatType_t combatType, int32_t& damage,
							 bool checkDefense /* = false*/, bool checkArmor /* = false*/)
{
	BlockType_t blockType = Creature::blockHit(attacker, combatType, damage, checkDefense, checkArmor);

	if(attacker)
		sendCreatureSquare(attacker, SQ_COLOR_BLACK);

	if(blockType != BLOCK_NONE)
		return blockType;

	if(damage != 0){
		//reduce damage against inventory items
		Item* item = NULL;
		for(int32_t slot = SLOT_FIRST; slot < SLOT_LAST; ++slot){
			if(!(item = getEquippedItem((slots_t)slot)))
				continue;

			const ItemType& it = Item::items[item->getID()];

			if(it.abilities.absorb.reduce(combatType, damage)){
				int32_t charges = item->getCharges();
				if(charges != 0)
					g_game.transformItem(item, item->getID(), charges - 1);
			}
		}

		if(damage <= 0){
			damage = 0;
			blockType = BLOCK_DEFENSE;
		}
	}
	return blockType;
}

uint32_t Player::getIP() const
{
	if(client){
		return client->getIP();
	}
	return 0;
}

void Player::onDie()
{
	if(getZone() != ZONE_PVP){
		bool isLootPrevented = false;
		bool isSkillPrevented = false;

		Item* item = NULL;
		for(int32_t slot = SLOT_FIRST; slot < SLOT_LAST; ++slot){
			if(!(item = getEquippedItem((slots_t)slot))){
				continue;
			}
			const ItemType& it = Item::items[item->getID()];
			if((it.abilities.preventItemLoss && !isLootPrevented) || (it.abilities.preventSkillLoss && !isSkillPrevented)){
				if(it.abilities.preventItemLoss){
					isLootPrevented = true;
				}
				if(it.abilities.preventSkillLoss){
					isSkillPrevented = true;
				}
				int32_t newCharge = std::max((int32_t)0, ((int32_t)item->getCharges()) - 1);
				g_game.transformItem(item, item->getID(), newCharge);
			}
		}

		#ifdef __SKULLSYSTEM__
		if(isLootPrevented && getSkull() != SKULL_RED){
		#else
		if(isLootPrevented){
		#endif
			setDropLoot(false);
		}
		if(isSkillPrevented){
			setLossSkill(false);
		}

		DeathList killers = getKillers(g_config.getNumber(ConfigManager::DEATH_ASSIST_COUNT));
		IOPlayer::instance()->addPlayerDeath(this, killers);
		std::set<uint32_t> tmpList;

		for(DeathList::const_iterator it = killers.begin(); it != killers.end(); ++it){
			if(it->isCreatureKill()){
				Creature* attacker = it->getKillerCreature();
				Player* attackerPlayer = attacker->getPlayer();

				if(attacker->isPlayerSummon()){
					attackerPlayer = attacker->getPlayerMaster();
				}

				if(attackerPlayer){
					#ifdef __SKULLSYSTEM__
					if(it->isUnjustKill()){
						attackerPlayer->addUnjustifiedDead(this);
					}
					#endif
				}
			}
		}
	}

	Creature::onDie();
}

void Player::die()
{
	ConditionEnd_t conditionEndReason = CONDITIONEND_DIE;
	if(getZone() == ZONE_PVP){
		conditionEndReason = CONDITIONEND_ABORT;
	}

	for(ConditionList::iterator it = conditions.begin(); it != conditions.end();){
		if((*it)->isPersistent()){
			Condition* condition = *it;
			it = conditions.erase(it);

			condition->endCondition(this, conditionEndReason);

			bool lastCondition = !hasCondition(condition->getType(), false);
			onEndCondition(condition->getType(), lastCondition);
			delete condition;
		}
		else{
			++it;
		}
	}

	if(getZone() != ZONE_PVP){
		loginPosition = masterPos;

		if(skillLoss){
			uint64_t expLost = getLostExperience();

			//Level loss
			removeExperience(expLost, false);
			double lostPercent = 1. - (double(experience - expLost) / double(experience)); // 0.1 if 10% was lost

			//Magic level loss
			uint64_t sumMana = 0;
			uint64_t lostMana = 0;

			for(uint32_t i = 1; i <= magLevel; ++i){
				sumMana += vocation->getReqMana(i);
			}

			sumMana += manaSpent;

			double lostPercentMana = lostPercent * lossPercent[LOSS_MANASPENT] / 100;
			lostMana = (uint64_t)std::ceil(sumMana * lostPercentMana);

			while((uint64_t)lostMana > manaSpent && magLevel > 0){
				lostMana -= manaSpent;
				manaSpent = vocation->getReqMana(magLevel);
				magLevel--;
			}

			manaSpent = std::max((uint64_t)0, (uint64_t)manaSpent - lostMana);
			magLevelPercent = Player::getPercentLevel(manaSpent, vocation->getReqMana(magLevel + 1));

			//Skill loss
			uint32_t lostSkillTries;
			uint32_t sumSkillTries;
			for(uint32_t i = 0; i <= 6; ++i){
				lostSkillTries = 0;
				sumSkillTries = 0;

				for(uint32_t c = 11; c <= skills[i][SKILL_LEVEL]; ++c) {
					sumSkillTries += vocation->getReqSkillTries(i, c);
				}

				sumSkillTries += skills[i][SKILL_TRIES];
				double lossPercentSkill = lostPercent * lossPercent[LOSS_SKILLTRIES] / 100;
				lostSkillTries = (uint32_t)std::ceil(sumSkillTries * lossPercentSkill);

				while(lostSkillTries > skills[i][SKILL_TRIES]){
					lostSkillTries -= skills[i][SKILL_TRIES];
					skills[i][SKILL_TRIES] = vocation->getReqSkillTries(i, skills[i][SKILL_LEVEL]);
					if(skills[i][SKILL_LEVEL] > 10){
						skills[i][SKILL_LEVEL]--;
					}
					else{
						skills[i][SKILL_LEVEL] = 10;
						skills[i][SKILL_TRIES] = 0;
						lostSkillTries = 0;
						break;
					}
				}

				skills[i][SKILL_TRIES] = std::max((int32_t)0, (int32_t)(skills[i][SKILL_TRIES] - lostSkillTries));
			}
		}

		Creature::die();
		std::string death_msg = g_config.getString(ConfigManager::DEATH_MSG);
		if(death_msg.length() > 0)
			sendTextMessage(MSG_EVENT_DEFAULT, death_msg);
	}
	else{
		preSave();
		setDropLoot(true);
		setLossSkill(true);
		sendStats();
		g_game.internalTeleport(this, getTemplePosition());
		g_game.addCreatureHealth(this);
		onThink(EVENT_CREATURE_THINK_INTERVAL);
	}
}

Item* Player::dropCorpse()
{
	if(getZone() != ZONE_PVP){
		return Creature::dropCorpse();
	}

	return NULL;
}

Item* Player::createCorpse()
{
	Item* corpse = Creature::createCorpse();
	if(corpse && corpse->getContainer()){
		std::stringstream ss;

		ss << "You recognize " << getNameDescription() << ".";

		DeathList killers = getKillers(0);
		if(!killers.empty() && (*killers.begin()).isCreatureKill()){
			ss << " " << playerSexSubjectString(getSex()) << " was killed by "
				<< ((*killers.begin()).getKillerCreature())->getNameDescription() << ".";
		}

		corpse->setSpecialDescription(ss.str());
	}

	return corpse;
}

void Player::preSave()
{
	if(health <= 0){
		#ifdef __SKULLSYSTEM__
			health = healthMax;
			if(getZone() != ZONE_PVP){
				mana = manaMax;
			}
		#endif
	}
}

void Player::addCombatExhaust(uint32_t ticks)
{
	if(!hasFlag(PlayerFlag_HasNoExhaustion)){
		// Add exhaust condition
		Condition* condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_EXHAUST_COMBAT, ticks, 0);
		addCondition(condition);
	}
}

void Player::addHealExhaust(uint32_t ticks)
{
	if(!hasFlag(PlayerFlag_HasNoExhaustion)){
		Condition* condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_EXHAUST_HEAL, ticks, 0);
		addCondition(condition);
	}
}

void Player::addInFightTicks(uint32_t ticks, bool pzlock /*= false*/)
{
	if(!hasFlag(PlayerFlag_NotGainInFight)){
		if(pzlock){
			pzLocked = true;
			sendIcons();
		}

		if(ticks > 0){
			Condition* condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_INFIGHT, ticks, 0);
			addCondition(condition);
		}
	}
}

void Player::addDefaultRegeneration(uint32_t addTicks)
{
	Condition* condition = getCondition(CONDITION_REGENERATION, CONDITIONID_DEFAULT, 0);

	if(condition){
		condition->setTicks(condition->getTicks() + addTicks);
	}
	else{
		condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_REGENERATION, addTicks, 0);
		condition->setParam(CONDITIONPARAM_HEALTHGAIN, vocation->getHealthGainAmount());
		condition->setParam(CONDITIONPARAM_HEALTHTICKS, vocation->getHealthGainTicks() * 1000);
		condition->setParam(CONDITIONPARAM_MANAGAIN, vocation->getManaGainAmount());
		condition->setParam(CONDITIONPARAM_MANATICKS, vocation->getManaGainTicks() * 1000);

		addCondition(condition);
	}
}

void Player::removeList()
{
	listPlayer.removeList(getID());

	for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
	{
		(*it).second->notifyLogOut(this);
	}

	Status::instance()->removePlayer();
}

void Player::addList()
{
	for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
	{
		(*it).second->notifyLogIn(this);
	}

	listPlayer.addList(this);

	Status::instance()->addPlayer();
}

void Player::kickPlayer()
{
	g_creatureEvents->playerLogOut(this);

	if(client){
		client->logout(true);
	}
	else{
		g_game.removeCreature(this);
	}
}

uint32_t Player::getGuildId() const
{
	if(getGuild())
		return getGuild()->getId();
	else
		return 0;
}

void Player::notifyLogIn(Player* login_player)
{
	if(client){
		VIPListSet::iterator it = VIPList.find(login_player->getGUID());
		if(it != VIPList.end()){
			client->sendVIPLogIn(login_player->getGUID());
			client->sendTextMessage(MSG_STATUS_SMALL, (login_player->getName() + " has logged in."));
		}
	}
}

void Player::notifyLogOut(Player* logout_player)
{
	if(client){
		VIPListSet::iterator it = VIPList.find(logout_player->getGUID());
		if(it != VIPList.end()){
			client->sendVIPLogOut(logout_player->getGUID());
			client->sendTextMessage(MSG_STATUS_SMALL, (logout_player->getName() + " has logged out."));
		}
	}
}

bool Player::removeVIP(uint32_t _guid)
{
	VIPListSet::iterator it = VIPList.find(_guid);
	if(it != VIPList.end()){
		VIPList.erase(it);
		return true;
	}
	return false;
}

bool Player::addVIP(uint32_t _guid, std::string& name, bool isOnline, bool internal /*=false*/)
{
	if(guid == _guid){
		if(!internal)
			sendTextMessage(MSG_STATUS_SMALL, "You cannot add yourself.");
		return false;
	}

	if(VIPList.size() > maxVipLimit){
		if(!internal)
			sendTextMessage(MSG_STATUS_SMALL, "You cannot add more buddies.");
		return false;
	}

	VIPListSet::iterator it = VIPList.find(_guid);
	if(it != VIPList.end()){
		if(!internal)
			sendTextMessage(MSG_STATUS_SMALL, "This player is already in your list.");
		return false;
	}

	VIPList.insert(_guid);

	if(client && !internal){
		client->sendVIP(_guid, name, isOnline);
	}

	return true;
}

//close container and its child containers
void Player::autoCloseContainers(const Container* container)
{
	typedef std::vector<uint32_t> CloseList;
	CloseList closeList;

	for(ContainerVector::iterator it = containerVec.begin(); it != containerVec.end(); ++it){
		Container* tmpcontainer = it->second;
		while(tmpcontainer != NULL){
			if(tmpcontainer->isRemoved() || tmpcontainer == container){
				closeList.push_back(it->first);
				break;
			}

			tmpcontainer = dynamic_cast<Container*>(tmpcontainer->getParent());
		}
	}

	for(CloseList::iterator it = closeList.begin(); it != closeList.end(); ++it){
		closeContainer(*it);
		if(client){
			client->sendCloseContainer(*it);
		}
	}
}

bool Player::hasCapacity(const Item* item, uint32_t count) const
{
	if(hasFlag(PlayerFlag_CannotPickupItem)){
		return false;
	}

	if(!hasFlag(PlayerFlag_HasInfiniteCapacity) && item->getTopParent() != this){
		double itemWeight = 0;

		if(item->isStackable()){
			itemWeight = Item::items[item->getID()].weight * count;
		}
		else
			itemWeight = item->getWeight();

		return (itemWeight < getFreeCapacity());
	}

	return true;
}

ReturnValue Player::__queryAdd(int32_t index, const Thing* thing, uint32_t count,
	uint32_t flags) const
{
	Player* self = const_cast<Player*>(this);
	if(self == NULL){
		return RET_NOTPOSSIBLE;
	}

	const Item* item = thing->getItem();
	if(item == NULL){
		return RET_NOTPOSSIBLE;
	}

	bool childIsOwner = ((flags & FLAG_CHILDISOWNER) == FLAG_CHILDISOWNER);
	bool skipLimit = ((flags & FLAG_NOLIMIT) == FLAG_NOLIMIT);
	
	if(item->getContainer() && !canAddContainer(item->getContainer()))
		return RET_NOTPOSSIBLE;

	if(childIsOwner){
		//a child container is querying the player, just check if enough capacity
		if(skipLimit || hasCapacity(item, count))
			return RET_NOERROR;
		else
			return RET_NOTENOUGHCAPACITY;
	}

	if(!item->isPickupable()){
		return RET_CANNOTPICKUP;
	}

	ReturnValue ret = RET_NOERROR;

	if((item->getSlotPosition() & SLOTP_HEAD) ||
		(item->getSlotPosition() & SLOTP_NECKLACE) ||
		(item->getSlotPosition() & SLOTP_BACKPACK) ||
		(item->getSlotPosition() & SLOTP_ARMOR) ||
		(item->getSlotPosition() & SLOTP_LEGS) ||
		(item->getSlotPosition() & SLOTP_FEET) ||
		(item->getSlotPosition() & SLOTP_RING)){
		ret = RET_CANNOTBEDRESSED;
	}
	else if(item->getSlotPosition() & SLOTP_TWO_HAND){
		ret = RET_PUTTHISOBJECTINBOTHHANDS;
	}
	else if((item->getSlotPosition() & SLOTP_RIGHT) || (item->getSlotPosition() & SLOTP_LEFT)){
		ret = RET_PUTTHISOBJECTINYOURHAND;
	}

	//check if we can dress this object
	switch(index){
		case SLOT_HEAD:
			if(item->getSlotPosition() & SLOTP_HEAD)
				ret = RET_NOERROR;
			break;
		case SLOT_NECKLACE:
			if(item->getSlotPosition() & SLOTP_NECKLACE)
				ret = RET_NOERROR;
			break;
		case SLOT_BACKPACK:
			if(item->getSlotPosition() & SLOTP_BACKPACK)
				ret = RET_NOERROR;
			break;
		case SLOT_ARMOR:
			if(item->getSlotPosition() & SLOTP_ARMOR)
				ret = RET_NOERROR;
			break;
		case SLOT_RIGHT:
			if(item->getSlotPosition() & SLOTP_RIGHT){
				//check if we already carry an item in the other hand
				if(item->getSlotPosition() & SLOTP_TWO_HAND){
					if(inventory[SLOT_LEFT] && inventory[SLOT_LEFT] != item){
						ret = RET_BOTHHANDSNEEDTOBEFREE;
					}
					else
						ret = RET_NOERROR;
				}
				else{
					ret = RET_NOERROR;
					if(inventory[SLOT_LEFT]){
						const Item* leftItem = inventory[SLOT_LEFT];
						//check if we already carry a double-handed item
						if(leftItem->getSlotPosition() & SLOTP_TWO_HAND){
							ret = RET_DROPTWOHANDEDITEM;
						}
						else if(!(item == leftItem && count == item->getItemCount())){
							if(item->getWeaponType() != WEAPON_NONE && item->getWeaponType() != WEAPON_AMMO){
								//check so we only equip one shield
								if(item->getWeaponType() == WEAPON_SHIELD){
									if(leftItem->getWeaponType() == WEAPON_SHIELD){
										ret = RET_CANONLYUSEONESHIELD;
									}
								}
								else{
									//check so we can only equip one weapon
									if(	leftItem->getWeaponType() != WEAPON_NONE &&
										leftItem->getWeaponType() != WEAPON_SHIELD &&
										leftItem->getWeaponType() != WEAPON_AMMO){
										ret = RET_CANONLYUSEONEWEAPON;
									}
								}
							}
						}
					}
				if(item->getWeaponType() != WEAPON_NONE && ret == RET_NOERROR){
					self->setLastAttackAsNow();
				}
				}
			}
			break;
		case SLOT_LEFT:
			if(item->getSlotPosition() & SLOTP_LEFT){
				//check if we already carry an item in the other hand
				if(item->getSlotPosition() & SLOTP_TWO_HAND){
					if(inventory[SLOT_RIGHT] && inventory[SLOT_RIGHT] != item){
						ret = RET_BOTHHANDSNEEDTOBEFREE;
					}
					else
						ret = RET_NOERROR;
				}
				else{
					ret = RET_NOERROR;
					if(inventory[SLOT_RIGHT]){
						const Item* rightItem = inventory[SLOT_RIGHT];
						//check if we already carry a double-handed item
						if(rightItem->getSlotPosition() & SLOTP_TWO_HAND){
							ret = RET_DROPTWOHANDEDITEM;
						}
						else if(!(item == rightItem && count == item->getItemCount())){
							if(item->getWeaponType() != WEAPON_NONE && item->getWeaponType() != WEAPON_AMMO){
								if(item->getWeaponType() == WEAPON_SHIELD){
									//check so we only equip one shield
									if(rightItem->getWeaponType() == WEAPON_SHIELD){
										ret = RET_CANONLYUSEONESHIELD;
									}
								}
								else{
									//check so we can only equip one weapon
									if(	rightItem->getWeaponType() != WEAPON_NONE &&
										rightItem->getWeaponType() != WEAPON_SHIELD &&
										rightItem->getWeaponType() != WEAPON_AMMO){
										ret = RET_CANONLYUSEONEWEAPON;
									}
								}
							}
						}
					}
				if(item->getWeaponType() != WEAPON_NONE && ret == RET_NOERROR){
					self->setLastAttackAsNow();
					}
				}
			}
			break;
		case SLOT_LEGS:
			if(item->getSlotPosition() & SLOTP_LEGS)
				ret = RET_NOERROR;
			break;
		case SLOT_FEET:
			if(item->getSlotPosition() & SLOTP_FEET)
				ret = RET_NOERROR;
			break;
		case SLOT_RING:
			if(item->getSlotPosition() & SLOTP_RING)
				ret = RET_NOERROR;
			break;
		case SLOT_AMMO:
			if(item->getSlotPosition() & SLOTP_AMMO)
				ret = RET_NOERROR;
			break;
		case SLOT_WHEREEVER:
			ret = RET_NOTENOUGHROOM;
			break;
		case -1:
			ret = RET_NOTENOUGHROOM;
			break;

		default:
			ret = RET_NOTPOSSIBLE;
			break;
	}

	if(ret == RET_BOTHHANDSNEEDTOBEFREE && (index == SLOT_LEFT || index == SLOT_RIGHT)){
		Item* tmpItem = NULL;
		Container* tmpContainer = NULL;
		slots_t setSlot = (slots_t)index;
		if(inventory[setSlot]){
			for(int i = SLOT_FIRST; i <= SLOT_LAST; i++){
				tmpItem = getInventoryItem((slots_t)i);
				if(tmpItem){
					tmpContainer = tmpItem->getContainer();
					if(tmpContainer && (tmpContainer->getItemCount() != tmpContainer->getItemHoldingCount())){
						self->sendRemoveInventoryItem(setSlot, inventory[setSlot]);
						self->onRemoveInventoryItem(setSlot, inventory[setSlot]);
						g_game.internalAddItem(tmpContainer, inventory[setSlot], INDEX_WHEREEVER, FLAG_NOLIMIT);
						self->inventory[setSlot] = NULL;
						break;
					}
				}
			}
		}
	}

	if(ret == RET_NOERROR || ret == RET_NOTENOUGHROOM){
		//need an exchange with source?
		if(getInventoryItem((slots_t)index) != NULL){
			if(!getInventoryItem((slots_t)index)->isStackable() || getInventoryItem((slots_t)index)->getID() != item->getID()){
				return RET_NEEDEXCHANGE;
			}
		}

		//check moveEvent
		ReturnValue _ret = g_moveEvents->canPlayerWearEquip(const_cast<Player*>(this), const_cast<Item*>(item), (slots_t)index);
		if(_ret != RET_NOERROR){
			return _ret;
		}

		//check if enough capacity
		if(hasCapacity(item, count))
			return ret;
		else
			return RET_NOTENOUGHCAPACITY;
	}

	return ret;
}

ReturnValue Player::__queryMaxCount(int32_t index, const Thing* thing, uint32_t count, uint32_t& maxQueryCount,
	uint32_t flags) const
{
	const Item* item = thing->getItem();
	if(item == NULL){
		maxQueryCount = 0;
		return RET_NOTPOSSIBLE;
	}

	if(index == INDEX_WHEREEVER){
		uint32_t n = 0;
		for(int slotIndex = SLOT_FIRST; slotIndex < SLOT_LAST; ++slotIndex){
			Item* inventoryItem = inventory[slotIndex];

			if(inventoryItem){
				if(Container* subContainer = inventoryItem->getContainer()){
					uint32_t queryCount = 0;
					subContainer->__queryMaxCount(INDEX_WHEREEVER, item, item->getItemCount(), queryCount, flags);
					n += queryCount;

					//iterate through all items, including sub-containers (deep search)
					for(ContainerIterator cit = subContainer->begin(); cit != subContainer->end(); ++cit){
						if(Container* tmpContainer  = (*cit)->getContainer()){
							queryCount = 0;
							tmpContainer->__queryMaxCount(INDEX_WHEREEVER, item, item->getItemCount(), queryCount, flags);
							n += queryCount;
						}
					}
				}
				else if(inventoryItem->isStackable() && item->getID() == inventoryItem->getID() && inventoryItem->getItemCount() < 100){
					uint32_t remainder = (100 - inventoryItem->getItemCount());
					if(__queryAdd(slotIndex, item, remainder, flags) == RET_NOERROR){
						n += remainder;
					}
				}
			}
			//empty slot
			else if(__queryAdd(slotIndex, item, item->getItemCount(), flags) == RET_NOERROR){
				if(item->isStackable())
					n += 100;
				else
					n += 1;
			}
		}

		maxQueryCount = n;
	}
	else{
		const Thing* destThing = __getThing(index);
		const Item* destItem = NULL;
		if(destThing)
			destItem = destThing->getItem();

		if(destItem){
			if(destItem->isStackable() && item->getID() == destItem->getID() && destItem->getItemCount() < 100){
				maxQueryCount = 100 - destItem->getItemCount();
			}
			else
				maxQueryCount = 0;
		}
		//empty slot
		else if(__queryAdd(index, item, count, flags) == RET_NOERROR){
			if(item->isStackable())
				maxQueryCount = 100;
			else
				maxQueryCount = 1;

			return RET_NOERROR;
		}
	}

	if(maxQueryCount < count)
		return RET_NOTENOUGHROOM;
	else
		return RET_NOERROR;
}

ReturnValue Player::__queryRemove(const Thing* thing, uint32_t count, uint32_t flags) const
{
	int32_t index = __getIndexOfThing(thing);

	if(index == -1){
		return RET_NOTPOSSIBLE;
	}

	const Item* item = thing->getItem();
	if(item == NULL){
		return RET_NOTPOSSIBLE;
	}

	if(count == 0 || (item->isStackable() && count > item->getItemCount())){
		return RET_NOTPOSSIBLE;
	}

	if(item->isNotMoveable() && !hasBitSet(FLAG_IGNORENOTMOVEABLE, flags)){
		return RET_NOTMOVEABLE;
	}

	return RET_NOERROR;
}

Cylinder* Player::__queryDestination(int32_t& index, const Thing* thing, Item** destItem,
	uint32_t& flags)
{
	if(!index /*drop to capacity window*/ || index == INDEX_WHEREEVER){
		*destItem = NULL;
		const Item* item = thing->getItem();
		if(!item){
			return this;
		}

		bool autoStack = !((flags & FLAG_IGNOREAUTOSTACK) == FLAG_IGNOREAUTOSTACK);
		if((!autoStack || !item->isStackable()) && backpack.first &&
			backpack.first->__queryAdd(backpack.second, item, item->getItemCount(), flags)){
			index = backpack.second;
			if(backpack.second != INDEX_WHEREEVER){
				++backpack.second;
			}

			return backpack.first;
		}

		std::list<std::pair<Container*, int32_t> > containers;
		std::list<std::pair<Cylinder*, int32_t> > freeSlots;
		for(int32_t i = SLOT_FIRST; i < SLOT_LAST; ++i){
			if(Item* invItem = inventory[i]){
				if(invItem == item || invItem == tradeItem){
					continue;
				}

				if(autoStack && item->isStackable() && __queryAdd(i, item, item->getItemCount(), 0)
					== RET_NOERROR && invItem->getID() == item->getID() && invItem->getItemCount() < 100){
					*destItem = invItem;
					index = i;
					return this;
				}

				if(Container* container = invItem->getContainer()){
					if(!autoStack && !backpack.first && container->__queryAdd(
						INDEX_WHEREEVER, item, item->getItemCount(), flags) == RET_NOERROR){
						index = INDEX_WHEREEVER;
						backpack = std::make_pair(container, index);
						return container;
					}

					containers.push_back(std::make_pair(container, 0));
				}
			}
			else if(!autoStack){
				if(__queryAdd(i, item, item->getItemCount(), 0) == RET_NOERROR)	{
					index = i;
					return this;
				}
			}
			else{
				freeSlots.push_back(std::make_pair(this, i));
			}
		}

		int32_t deepness = g_config.getNumber(ConfigManager::PLAYER_QUERYDESTINATION_DEEPNESS);
		while(!containers.empty()){
			Container* tmpContainer = containers.front().first;
			int32_t level = containers.front().second;

			containers.pop_front();
			if(!tmpContainer){
				continue;
			}

			for(uint32_t n = 0; n < tmpContainer->capacity(); ++n){
				if(Item* tmpItem = tmpContainer->getItem(n)){
					if(tmpItem == item || tmpItem == tradeItem){
						continue;
					}

					if(autoStack && item->isStackable() && tmpContainer->__queryAdd(n, item, item->getItemCount(),
						0) == RET_NOERROR && tmpItem->getID() == item->getID() && tmpItem->getItemCount() < 100){
						index = n;
						*destItem = tmpItem;
						return tmpContainer;
					}

					if(Container* container = tmpItem->getContainer()){
						if(!autoStack && container->__queryAdd(INDEX_WHEREEVER,
							item, item->getItemCount(), flags) == RET_NOERROR){
							index = INDEX_WHEREEVER;
							backpack = std::make_pair(container, index);
							return container;
						}

						if(deepness < 0 || level < deepness){
							containers.push_back(std::make_pair(container, level + 1));
						}
					}
				}
				else{
					if(!autoStack){
						if(tmpContainer->__queryAdd(n, item, item->getItemCount(), 0) == RET_NOERROR){
							index = n;
							backpack = std::make_pair(tmpContainer, index + 1);
							return tmpContainer;
						}
					}
					else{
						freeSlots.push_back(std::make_pair(tmpContainer, n));
					}

					break; // one slot to check is definitely enough.
				}
			}
		}

		if(autoStack){
			while(!freeSlots.empty()){
				Cylinder* tmpCylinder = freeSlots.front().first;
				int32_t i = freeSlots.front().second;

				freeSlots.pop_front();
				if(!tmpCylinder){
					continue;
				}

				if(tmpCylinder->__queryAdd(i, item, item->getItemCount(), flags) == RET_NOERROR){
					index = i;
					return tmpCylinder;
				}
			}
		}

		return this;
	}

	Thing* destThing = __getThing(index);
	if(destThing){
		*destItem = destThing->getItem();
	}

	if(Cylinder* subCylinder = dynamic_cast<Cylinder*>(destThing)){
		index = INDEX_WHEREEVER;
		*destItem = NULL;
		return subCylinder;
	}

	return this;
}

void Player::__addThing(Thing* thing)
{
	__addThing(0, thing);
}

void Player::__addThing(int32_t index, Thing* thing)
{
	if(index < 0 || index > 11){
#ifdef __DEBUG__MOVESYS__
		std::cout << "Failure: [Player::__addThing], " << "player: " << getName() << ", index: " << index << ", index < 0 || index > 11" << std::endl;
		DEBUG_REPORT
#endif
		return /*RET_NOTPOSSIBLE*/;
	}

	if(index == 0){
#ifdef __DEBUG__MOVESYS__
		std::cout << "Failure: [Player::__addThing], " << "player: " << getName() << ", index == 0" << std::endl;
		DEBUG_REPORT
#endif
		return /*RET_NOTENOUGHROOM*/;
	}

	Item* item = thing->getItem();
	if(item == NULL){
#ifdef __DEBUG__MOVESYS__
		std::cout << "Failure: [Player::__addThing], " << "player: " << getName() << ", item == NULL" << std::endl;
		DEBUG_REPORT
#endif
		return /*RET_NOTPOSSIBLE*/;
	}

	item->setParent(this);
	inventory[index] = item;

	//send to client
	sendAddInventoryItem((slots_t)index, item);

	//event methods
	onAddInventoryItem((slots_t)index, item);
}

void Player::__updateThing(Thing* thing, uint16_t itemId, uint32_t count)
{
	int32_t index = __getIndexOfThing(thing);
	if(index == -1){
#ifdef __DEBUG__MOVESYS__
		std::cout << "Failure: [Player::__updateThing], " << "player: " << getName() << ", index == -1" << std::endl;
		DEBUG_REPORT
#endif
		return /*RET_NOTPOSSIBLE*/;
	}

	Item* item = thing->getItem();
	if(item == NULL){
#ifdef __DEBUG__MOVESYS__
		std::cout << "Failure: [Player::__updateThing], " << "player: " << getName() << ", item == NULL" << std::endl;
		DEBUG_REPORT
#endif
		return /*RET_NOTPOSSIBLE*/;
	}

	const ItemType& oldType = Item::items[item->getID()];
	const ItemType& newType = Item::items[itemId];

	item->setID(itemId);
	item->setSubType(count);

	//send to client
	sendUpdateInventoryItem((slots_t)index, item, item);

	//event methods
	onUpdateInventoryItem((slots_t)index, item, oldType, item, newType);
}

void Player::__replaceThing(uint32_t index, Thing* thing)
{
	if(index < 0 || index > 11){
#ifdef __DEBUG__MOVESYS__
		std::cout << "Failure: [Player::__replaceThing], " << "player: " << getName() << ", index: " << index << ",  index < 0 || index > 11" << std::endl;
		DEBUG_REPORT
#endif
		return /*RET_NOTPOSSIBLE*/;
	}

	Item* oldItem = getInventoryItem((slots_t)index);
	if(!oldItem){
#ifdef __DEBUG__MOVESYS__
		std::cout << "Failure: [Player::__updateThing], " << "player: " << getName() << ", oldItem == NULL" << std::endl;
		DEBUG_REPORT
#endif
		return /*RET_NOTPOSSIBLE*/;
	}

	Item* item = thing->getItem();
	if(item == NULL){
#ifdef __DEBUG__MOVESYS__
		std::cout << "Failure: [Player::__updateThing], " << "player: " << getName() << ", item == NULL" << std::endl;
		DEBUG_REPORT
#endif
		return /*RET_NOTPOSSIBLE*/;
	}

	const ItemType& oldType = Item::items[oldItem->getID()];
	const ItemType& newType = Item::items[item->getID()];

	//send to client
	sendUpdateInventoryItem((slots_t)index, oldItem, item);

	//event methods
	onUpdateInventoryItem((slots_t)index, oldItem, oldType, item, newType);

	item->setParent(this);
	inventory[index] = item;
}

void Player::__removeThing(Thing* thing, uint32_t count)
{
	Item* item = thing->getItem();
	if(item == NULL){
#ifdef __DEBUG__MOVESYS__
		std::cout << "Failure: [Player::__removeThing], " << "player: " << getName() << ", item == NULL" << std::endl;
		DEBUG_REPORT
#endif
		return /*RET_NOTPOSSIBLE*/;
	}

	int32_t index = __getIndexOfThing(thing);
	if(index == -1){
#ifdef __DEBUG__MOVESYS__
		std::cout << "Failure: [Player::__removeThing], " << "player: " << getName() << ", index == -1" << std::endl;
		DEBUG_REPORT
#endif
		return /*RET_NOTPOSSIBLE*/;
	}

	if(item->isStackable()){
		if(count == item->getItemCount()){
			//send change to client
			sendRemoveInventoryItem((slots_t)index, item);

			//event methods
			onRemoveInventoryItem((slots_t)index, item);

			item->setParent(NULL);
			inventory[index] = NULL;
		}
		else{
			uint8_t newCount = (uint8_t)std::max((int32_t)0, (int32_t)(item->getItemCount() - count));
			item->setItemCount(newCount);

			const ItemType& it = Item::items[item->getID()];

			//send change to client
			sendUpdateInventoryItem((slots_t)index, item, item);

			//event methods
			onUpdateInventoryItem((slots_t)index, item, it, item, it);
		}
	}
	else{
		//send change to client
		sendRemoveInventoryItem((slots_t)index, item);

		//event methods
		onRemoveInventoryItem((slots_t)index, item);

		item->setParent(NULL);
		inventory[index] = NULL;
	}
}

int32_t Player::__getIndexOfThing(const Thing* thing) const
{
	for(int i = SLOT_FIRST; i < SLOT_LAST; ++i){
		if(inventory[i] == thing)
			return i;
	}

	return -1;
}

int32_t Player::__getFirstIndex() const
{
	return SLOT_FIRST;
}

int32_t Player::__getLastIndex() const
{
	return SLOT_LAST;
}

uint32_t Player::__getItemTypeCount(uint16_t itemId, int32_t subType /*= -1*/) const
{
	uint32_t count = 0;

	for(int i = SLOT_FIRST; i < SLOT_LAST; ++i){
		Item* item = inventory[i];

		if(item){
			if(item->getID() == itemId){
				count += Item::countByType(item, subType);
			}
			else if(Container* container = item->getContainer()){
				for(ContainerIterator it = container->begin(), end = container->end(); it != end; ++it){
					if((*it)->getID() == itemId){
						count += Item::countByType(*it, subType);
					}
				}
			}
		}
	}

	return count;
}

std::map<uint32_t, uint32_t>& Player::__getAllItemTypeCount(std::map<uint32_t, uint32_t>& countMap) const
{
	for(int i = SLOT_FIRST; i < SLOT_LAST; ++i){
		Item* item = inventory[i];

		if(item){
			countMap[item->getID()] += Item::countByType(item, -1);

			if(Container* container = item->getContainer()){
				for(ContainerIterator it = container->begin(), end = container->end(); it != end; ++it){
					countMap[(*it)->getID()] += Item::countByType(*it, -1);
				}
			}
		}
	}

	return countMap;
}

Thing* Player::__getThing(uint32_t index) const
{
	if(index >= SLOT_FIRST && index < SLOT_LAST)
		return inventory[index];

	return NULL;
}

void Player::postAddNotification(Thing* thing, const Cylinder* oldParent, int32_t index, cylinderlink_t link /*= LINK_OWNER*/, bool isNewItem /*=true*/)
{
	if(link == LINK_OWNER && isNewItem){
		//calling movement scripts
		g_moveEvents->onPlayerEquip(this, thing->getItem(), (slots_t)index);
	}

	bool requireListUpdate = true;
	if(link == LINK_OWNER || link == LINK_TOPPARENT){
		const Item* i = (oldParent? oldParent->getItem() : NULL);

		// Check if we owned the old container too, so we don't need to do anything,
		// as the list was updated in postRemoveNotification
		assert(i? i->getContainer() != NULL : true);

		if(i)
			requireListUpdate = i->getContainer()->getHoldingPlayer() != this;
		else
			requireListUpdate = oldParent != this;

		updateInventoryWeight();
		updateItemsLight();
		sendStats();
	}

	if(const Item* item = thing->getItem()){
		if(const Container* container = item->getContainer()){
			onSendContainer(container);
		}
	}
	else if(const Creature* creature = thing->getCreature()){
		if(creature == this){
			//check containers
			std::vector<Container*> containers;
			for(ContainerVector::iterator it = containerVec.begin(); it != containerVec.end(); ++it){
				if(!Position::areInRange<1,1,0>(it->second->getPosition(), getPosition())){
					containers.push_back(it->second);
				}
			}

			for(std::vector<Container*>::const_iterator it = containers.begin(); it != containers.end(); ++it){
				autoCloseContainers(*it);
			}
		}
	}
}

void Player::postRemoveNotification(Thing* thing, const Cylinder* newParent, int32_t index, bool isCompleteRemoval, cylinderlink_t link /*= LINK_OWNER*/)
{
	if(link == LINK_OWNER && isCompleteRemoval){
		//calling movement scripts
		g_moveEvents->onPlayerDeEquip(this, thing->getItem(), (slots_t)index, isCompleteRemoval);
	}

	bool requireListUpdate = true;
	if(link == LINK_OWNER || link == LINK_TOPPARENT){
		const Item* i = (newParent? newParent->getItem() : NULL);

		// Check if we owned the old container too, so we don't need to do anything,
		// as the list was updated in postRemoveNotification
		assert(i? i->getContainer() != NULL : true);
		if(i)
			requireListUpdate = i->getContainer()->getHoldingPlayer() != this;
		else
			requireListUpdate = newParent != this;

		updateInventoryWeight();
		updateItemsLight();
		sendStats();
	}


	if(const Item* item = thing->getItem()){
		if(const Container* container = item->getContainer()){

			if(container->isRemoved() || !Position::areInRange<1,1,0>(getPosition(), container->getPosition())){
				//removed or out of range
				autoCloseContainers(container);
			}
			else if(container->getTopParent() == this){
				//the player equipped it
				onSendContainer(container);
			}
			else if(const Container* topContainer = dynamic_cast<const Container*>(container->getTopParent())){
				//moved the backpack
				if(const Depot* depot = dynamic_cast<const Depot*>(topContainer)){
					//moved into a depot
					bool isOwner = false;
					for(DepotMap::iterator it = depots.begin(); it != depots.end(); ++it){
						if(it->second == depot){
							//the player is the owner of the depot
							isOwner = true;
							onSendContainer(container);
						}
					}

					if(!isOwner){
						autoCloseContainers(container);
					}
				}
				else{
					onSendContainer(container);
				}
			}
			else{
				autoCloseContainers(container);
			}
		}
	}
}

void Player::__internalAddThing(Thing* thing)
{
	__internalAddThing(0, thing);
}

void Player::__internalAddThing(uint32_t index, Thing* thing)
{
#ifdef __DEBUG__MOVESYS__NOTICE
	std::cout << "[Player::__internalAddThing] index: " << index << std::endl;
#endif

	Item* item = thing->getItem();
	if(item == NULL){
#ifdef __DEBUG__MOVESYS__
		std::cout << "Failure: [Player::__internalAddThing] item == NULL" << std::endl;
#endif
		return;
	}

	//index == 0 means we should equip this item at the most appropiate slot
	if(index == 0){
#ifdef __DEBUG__MOVESYS__
		std::cout << "Failure: [Player::__internalAddThing] index == 0" << std::endl;
		DEBUG_REPORT
#endif
		return;
	}

	if(index > 0 && index < 11){
		if(inventory[index]){
#ifdef __DEBUG__MOVESYS__
			std::cout << "Warning: [Player::__internalAddThing], player: " << getName() << ", items[index] is not empty." << std::endl;
			//DEBUG_REPORT
#endif
			return;
		}

		inventory[index] = item;
		item->setParent(this);
	}
}

bool Player::setFollowCreature(Creature* creature, bool fullPathSearch /*= false*/)
{
	if(!Creature::setFollowCreature(creature, fullPathSearch)){
		setFollowCreature(NULL);
		setAttackedCreature(NULL);

		sendCancelMessage(RET_THEREISNOWAY);
		sendCancelTarget();
		stopWalk();
		return false;
	}

	return true;
}

bool Player::setAttackedCreature(Creature* creature)
{
	if(!Creature::setAttackedCreature(creature)){
		sendCancelTarget();
		return false;
	}

	if(chaseMode == CHASEMODE_FOLLOW && creature){
		if(followCreature != creature){
			//chase opponent
			setFollowCreature(creature);
		}
	}
	else{
		setFollowCreature(NULL);
	}

	if(creature){
		g_dispatcher.addTask(createTask(
			boost::bind(&Game::checkCreatureAttack, &g_game, getID())));
	}

	return true;
}

void Player::goToFollowCreature()
{
	if(!walkTask){
		Creature::goToFollowCreature();
	}
}

void Player::getPathSearchParams(const Creature* creature, FindPathParams& fpp) const
{
	Creature::getPathSearchParams(creature, fpp);
	fpp.fullPathSearch = true;
}

uint32_t Player::getAttackSpeed() const
{
	return vocation->getAttackSpeed();
}

void Player::onAttacking(uint32_t interval)
{
	Creature::onAttacking(interval);
}

void Player::doAttacking(uint32_t interval)
{
	if(lastAttack == 0){
		// - 1 to compensate for timer resolution etc.
		lastAttack = OTSYS_TIME() - getAttackSpeed() - 1;
	}

	// Can't attack while pacified
	if(hasCondition(CONDITION_PACIFIED))
	{
		return;
	}

	if((OTSYS_TIME() - lastAttack) >= getAttackSpeed() ){
		Item* tool = getWeapon();
		bool result = false;
		const Weapon* weapon = g_weapons->getWeapon(tool);
		if(weapon){
			if(!weapon->interruptSwing()){
				result = weapon->useWeapon(this, tool, attackedCreature);
			}
			else if(!canDoAction()){
				uint32_t delay = getNextActionTime();
				SchedulerTask* task = createSchedulerTask(delay, boost::bind(&Game::checkCreatureAttack,
					&g_game, getID()));
				setNextActionTask(task);
			}
			else {
				// If the player is not exhausted OR if the player's weapon
				// does not have hasExhaust, use the weapon.
				if(!hasCondition(CONDITION_EXHAUST_COMBAT))
				{
					result = weapon->useWeapon(this, tool, attackedCreature);
				}
			}
		}
		else{
			result = Weapon::useFist(this, attackedCreature);
		}

		if(result){
			lastAttack = OTSYS_TIME();
		}
	}
}

uint64_t Player::getGainedExperience(Creature* attacker) const
{
	if (g_game.getWorldType() == WORLD_TYPE_PVP_ENFORCED && g_config.getNumber(ConfigManager::RATE_EXPERIENCE_PVP) > 0){
		Player* attackerPlayer = attacker->getPlayer();
		if(attackerPlayer && attackerPlayer != this && skillLoss){
				/*Formula
				a = attackers level * 0.9
				b = victims level
				c = victims experience

				y = (1 - (a / b)) * 0.05 * c
				*/

				uint32_t a = (int32_t)std::floor(attackerPlayer->getLevel() * 0.9);
				uint32_t b = getLevel();
				uint64_t c = getExperience();

				uint64_t result = std::max((uint64_t)0, (uint64_t)std::floor(getDamageRatio(attacker) * std::max((double)0, ((double)(1 - (((double)a / b))))) * 0.05 * c ));
				return result * g_config.getNumber(ConfigManager::RATE_EXPERIENCE_PVP);
		}
	}

	return 0;
}

void Player::onFollowCreature(const Creature* creature)
{
	if(!creature){
		stopWalk();
	}
}

void Player::setChaseMode(chaseMode_t mode)
{
	chaseMode_t prevChaseMode = chaseMode;
	chaseMode = mode;

	if(prevChaseMode != chaseMode){
		if(chaseMode == CHASEMODE_FOLLOW){
			if(!followCreature && attackedCreature){
				//chase opponent
				setFollowCreature(attackedCreature);
			}
		}
		else if(attackedCreature){
			setFollowCreature(NULL);
			cancelNextWalk = true;
		}
	}
}

void Player::setFightMode(fightMode_t mode)
{
	fightMode = mode;
}

void Player::onWalkAborted()
{
	setNextWalkActionTask(NULL);
	sendCancelWalk();
}

void Player::onWalkComplete()
{
	if(walkTask){
		walkTaskEvent = g_scheduler.addEvent(walkTask);
		walkTask = NULL;
	}
}

void Player::stopWalk()
{
	cancelNextWalk = true;
}

void Player::getCreatureLight(LightInfo& light) const
{
	if(internalLight.level > itemsLight.level){
		light = internalLight;
	}
	else{
		light = itemsLight;
	}
}

void Player::updateItemsLight(bool internal /*=false*/)
{
	LightInfo maxLight;
	LightInfo curLight;
	for(int i = SLOT_FIRST; i < SLOT_LAST; ++i){
		Item* item = getInventoryItem((slots_t)i);
		if(item){
			item->getLight(curLight);
			if(curLight.level > maxLight.level){
				maxLight = curLight;
			}
		}
	}
	if(itemsLight.level != maxLight.level || itemsLight.color != maxLight.color){
		itemsLight = maxLight;
		if(!internal){
			g_game.changeLight(this);
		}
	}
}

void Player::onAddCondition(ConditionType_t type, bool hadCondition)
{
	Creature::onAddCondition(type, hadCondition);
	sendIcons();
}

void Player::onAddCombatCondition(ConditionType_t type, bool hadCondition)
{
	if(type == CONDITION_POISON){
		sendTextMessage(MSG_STATUS_DEFAULT, "You are poisoned.");
	}
	else if(type == CONDITION_PARALYZE){
		sendTextMessage(MSG_STATUS_DEFAULT, "You are paralyzed.");
	}
	else if(type == CONDITION_DRUNK){
		sendTextMessage(MSG_STATUS_DEFAULT, "You are drunk.");
	}
}

void Player::onEndCondition(ConditionType_t type, bool lastCondition)
{
	Creature::onEndCondition(type, lastCondition);

	if(type == CONDITION_INFIGHT){
		onIdleStatus();
		pzLocked = false;

#ifdef __SKULLSYSTEM__
		if(getSkull() != SKULL_RED){
			clearAttacked();
			setSkull(SKULL_NONE);
			g_game.updateCreatureSkull(this);
		}
#endif
	}

	sendIcons();
}

void Player::onCombatRemoveCondition(const Creature* attacker, Condition* condition)
{
	//Creature::onCombatRemoveCondition(attacker, condition);
	bool remove = true;

	if(condition->getId() > 0){
		remove = false;

		//Means the condition is from an item, id == slot
		if (g_game.getWorldType() == WORLD_TYPE_PVP_ENFORCED){
			Item* item = getInventoryItem((slots_t)condition->getId());
			if(item){
				//25% chance to destroy the item
				if(25 >= random_range(1, 100)){
					g_game.internalRemoveItem(item);
				}
			}
		}
	}

	if(remove){
		if(!canDoAction()){
			int32_t delay = getNextActionTime();
			delay -= (delay % EVENT_CREATURE_THINK_INTERVAL);
			if(delay < 0){
				removeCondition(condition);
			}
			else{
				condition->setTicks(delay);
			}
		}
		else{
			removeCondition(condition);
		}
	}
}

void Player::onTickCondition(ConditionType_t type, int32_t interval, bool& bRemove)
{
	Creature::onTickCondition(type, interval, bRemove);
}

void Player::onAttackedCreature(Creature* target)
{
	Creature::onAttackedCreature(target);

	if(!hasFlag(PlayerFlag_NotGainInFight)){
		bool doPzLock = false;
		if(target != this){
			if(Player* targetPlayer = target->getPlayer()){
				doPzLock = checkPzBlock(targetPlayer);

				#ifdef __SKULLSYSTEM__
				if( !isPartner(targetPlayer) && 
					!Combat::isInPvpZone(this, targetPlayer) &&
					!targetPlayer->hasAttacked(this)){

					addAttacked(targetPlayer);

					if(targetPlayer->getSkull() == SKULL_NONE && getSkull() == SKULL_NONE){
						//add a white skull
						setSkull(SKULL_WHITE);
						g_game.updateCreatureSkull(this);
					}

					if(getSkull() == SKULL_NONE){
						//yellow skull
						targetPlayer->sendCreatureSkull(this);
					}
				}
				#endif
			}
		}

		addInFightTicks(g_config.getNumber(ConfigManager::IN_FIGHT_DURATION), doPzLock);
	}
}

void Player::onSummonAttackedCreature(Creature* summon, Creature* target)
{
	Creature::onSummonAttackedCreature(summon, target);

	onAttackedCreature(target);
}

void Player::onAttacked()
{
	Creature::onAttacked();

	if(!hasFlag(PlayerFlag_NotGainInFight)){
		addInFightTicks(g_config.getNumber(ConfigManager::IN_FIGHT_DURATION));
	}
}

void Player::onIdleStatus()
{
	Creature::onIdleStatus();
}

void Player::onPlacedCreature()
{
	//scripting event - onLogIn
	if(!g_creatureEvents->playerLogIn(this)){
		kickPlayer(); //The script won't let the player be online for now.
	}
}

void Player::onAttackedCreatureDrainHealth(Creature* target, int32_t points)
{
	Creature::onAttackedCreatureDrainHealth(target, points);
}

void Player::onSummonAttackedCreatureDrainHealth(Creature* summon, Creature* target, int32_t points)
{
	// Nothing
}

void Player::onAttackedCreatureDrainMana(Creature* target, int32_t points)
{
	Creature::onAttackedCreatureDrainMana(target, points);
}

void Player::onSummonAttackedCreatureDrainMana(Creature* summon, Creature* target, int32_t points)
{
	// Nothing
}

void Player::onTargetCreatureGainHealth(Creature* target, int32_t points)
{
	Creature::onTargetCreatureGainHealth(target, points);
	if(target && getParty()){
		Player* tmpPlayer = NULL;
		if(target->getPlayer()){
			tmpPlayer = target->getPlayer();
		}
		else if(target->isPlayerSummon()){
			tmpPlayer = target->getPlayerMaster();
		}
	}
}

void Player::onKilledCreature(Creature* target, bool lastHit)
{
	if(hasFlag(PlayerFlag_NotGenerateLoot)){
		target->setDropLoot(false);
	}

	if(Player* targetPlayer = target->getPlayer()){
		if(targetPlayer->getZone() == ZONE_PVP){
			targetPlayer->setDropLoot(false);
			targetPlayer->setLossSkill(false);
		}
		else if (!isPartner(targetPlayer) || 
			(g_config.getNumber(ConfigManager::LAST_HIT_PZBLOCK_ONLY) &&
			lastHit)){
			if(checkPzBlock(targetPlayer))
				addInFightTicks(g_config.getNumber(ConfigManager::UNJUST_SKULL_DURATION), true);
		}
	}

	if(target->getMonster() && !target->isPlayerSummon()){
		Condition* condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_HUNTING, g_config.getNumber(ConfigManager::HUNTING_KILL_DURATION), 0);
		addCondition(condition);
	}

	Creature::onKilledCreature(target, lastHit);
}

void Player::gainExperience(uint64_t& gainExp, bool fromMonster)
{
	if(!hasFlag(PlayerFlag_NotGainExperience)){
		if(gainExp > 0){
			//soul regeneration
			if((uint32_t)gainExp >= getLevel()){
				Condition* condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_SOUL, 4 * 60 * 1000, 0);
				//Soul regeneration rate is defined by the vocation
				uint32_t vocSoulTicks = vocation->getSoulGainTicks();
				condition->setParam(CONDITIONPARAM_SOULGAIN, 1);
				condition->setParam(CONDITIONPARAM_SOULTICKS, vocSoulTicks * 1000);
				addCondition(condition);
			}

			//check stamina, player rate and other values
			getGainExperience(gainExp, fromMonster);

			//add experience
			addExperience(gainExp);
		}
	}
}

void Player::getGainExperience(uint64_t& gainExp, bool fromMonster)
{
	if(fromMonster || g_config.getNumber(ConfigManager::RATES_FOR_PLAYER_KILLING)){
		gainExp = (uint64_t)std::floor(gainExp * getRateValue(LEVEL_EXPERIENCE));
	}
}

void Player::onGainExperience(uint64_t gainExp, bool fromMonster)
{
	if(hasFlag(PlayerFlag_NotGainExperience)){
		gainExp = 0;
	}

	Party* party = getParty();
	if(party && party->isSharedExperienceActive() && party->isSharedExperienceEnabled()){
		party->shareExperience(gainExp, fromMonster);
		//We will get a share of the experience through the sharing mechanism
		gainExp = 0;
	}

	gainExperience(gainExp, fromMonster);
	Creature::onGainExperience(gainExp, fromMonster);
}

bool Player::isImmune(CombatType_t type) const
{
	if(hasFlag(PlayerFlag_CannotBeAttacked)){
		return true;
	}

	return Creature::isImmune(type);
}

bool Player::isImmune(ConditionType_t type, bool aggressive /* = true */) const
{
	if(hasFlag(PlayerFlag_CannotBeAttacked) && aggressive){
		return true;
	}

	return Creature::isImmune(type, aggressive);
}

bool Player::isAttackable() const
{
	if(hasFlag(PlayerFlag_CannotBeAttacked)){
		return false;
	}

	return true;
}

void Player::changeHealth(int32_t healthChange)
{
	Creature::changeHealth(healthChange);
	sendStats();
}

void Player::changeMana(int32_t manaChange)
{
	Creature::changeMana(manaChange);
	sendStats();
}

void Player::changeSoul(int32_t soulChange)
{
	if(soulChange > 0){
		soul += std::min(soulChange, soulMax - soul);
	}
	else{
		soul = std::max((int32_t)0, soul + soulChange);
	}

	sendStats();
}

PartyShields_t Player::getPartyShield(const Player* player) const
{
	if(!player){
		return SHIELD_NONE;
	}

	Party* party = getParty();
	if(party){
		if(party->getLeader() == player){
			return SHIELD_YELLOW;
		}

		if(party->isPlayerMember(player)){
			return SHIELD_BLUE;
		}

		if(isInviting(player)){
			return SHIELD_WHITEBLUE;
		}
	}

	if(player->isInviting(this)){
		return SHIELD_WHITEYELLOW;
	}

	return SHIELD_NONE;
}

bool Player::isInviting(const Player* player) const
{
	if(!player || !getParty() || getParty()->getLeader() != this){
		return false;
	}

	return getParty()->isPlayerInvited(player);
}

bool Player::isPartner(const Player* player) const
{
	if(!player || !getParty() || !player->getParty()){
		return false;
	}

	return (getParty() == player->getParty());
}

void Player::sendPlayerPartyIcons(Player* player)
{
	sendCreatureShield(player);

#ifdef __SKULLSYSTEM__
	sendCreatureSkull(player);
#endif
}

bool Player::addPartyInvitation(Party* party)
{
	if(!party){
		return false;
	}

	PartyList::iterator it = std::find(invitePartyList.begin(), invitePartyList.end(), party);
	if(it != invitePartyList.end()){
		return false;
	}

	invitePartyList.push_back(party);
	return true;
}

bool Player::removePartyInvitation(Party* party)
{
	if(!party){
		return false;
	}

	PartyList::iterator it = std::find(invitePartyList.begin(), invitePartyList.end(), party);
	if(it != invitePartyList.end()){
		invitePartyList.erase(it);
		return true;
	}

	return false;
}

void Player::clearPartyInvitations()
{
	if(!invitePartyList.empty()){
		PartyList list;
		for(PartyList::iterator it = invitePartyList.begin(); it != invitePartyList.end(); ++it){
			list.push_back(*it);
		}

		invitePartyList.clear();

		for(PartyList::iterator it = list.begin(); it != list.end(); ++it){
			(*it)->removeInvite(this);
		}
	}
}

#ifdef __SKULLSYSTEM__
Skulls_t Player::getSkull() const
{
	if(hasFlag(PlayerFlag_NotGainInFight)){
		return SKULL_NONE;
	}

	return skullType;
}

Skulls_t Player::getSkullClient(const Player* player) const
{
	if(!player){
		return SKULL_NONE;
	}

	if(getSkull() != SKULL_NONE && player->getSkull() != SKULL_RED){
		if(player->hasAttacked(this)){
			return SKULL_YELLOW;
		}
	}

	if (player->getSkull() == SKULL_NONE && isPartner(player) && g_game.getWorldType() != WORLD_TYPE_NO_PVP){
		return SKULL_GREEN;
	}

	return player->getSkull();
}

bool Player::hasAttacked(const Player* attacked) const
{
	if(hasFlag(PlayerFlag_NotGainInFight) || !attacked){
		return false;
	}

	AttackedSet::const_iterator it;
	uint32_t attackedId = attacked->getID();
	it = attackedSet.find(attackedId);
	if(it != attackedSet.end()){
		return true;
	}

	return false;
}

void Player::addAttacked(const Player* attacked)
{
	if(hasFlag(PlayerFlag_NotGainInFight) || !attacked || attacked == this){
		return;
	}

	AttackedSet::iterator it;
	uint32_t attackedId = attacked->getID();
	it = attackedSet.find(attackedId);
	if(it == attackedSet.end()){
		attackedSet.insert(attackedId);
	}
}

void Player::clearAttacked()
{
	attackedSet.clear();
}

void Player::addUnjustifiedDead(const Player* attacked)
{
	if(hasFlag(PlayerFlag_NotGainInFight) || attacked == this){
		return;
	}

	std::stringstream msg;
	msg << "Warning! The murder of " << attacked->getName() << " was not justified.";
	sendTextMessage(MSG_STATUS_WARNING, msg.str());

	Skulls_t oldSkull = getSkull();
	if(oldSkull == SKULL_RED){
		lastSkullTime = std::time(NULL);
	}

	//day
	int32_t unjustKills = IOPlayer::instance()->getPlayerUnjustKillCount(this, UNJUST_KILL_PERIOD_DAY);
	if(getSkull() != SKULL_RED &&
			g_config.getNumber(ConfigManager::KILLS_PER_DAY_RED_SKULL) > 0 &&
			g_config.getNumber(ConfigManager::KILLS_PER_DAY_RED_SKULL) <= unjustKills){
		setSkull(SKULL_RED);
	}

	//week
	unjustKills = IOPlayer::instance()->getPlayerUnjustKillCount(this, UNJUST_KILL_PERIOD_WEEK);
	if (getSkull() != SKULL_RED &&
			g_config.getNumber(ConfigManager::KILLS_PER_WEEK_RED_SKULL) > 0 &&
			g_config.getNumber(ConfigManager::KILLS_PER_WEEK_RED_SKULL) <= unjustKills){
		setSkull(SKULL_RED);
	}

	//month
	unjustKills = IOPlayer::instance()->getPlayerUnjustKillCount(this, UNJUST_KILL_PERIOD_MONTH);
	if (getSkull() != SKULL_RED &&
			g_config.getNumber(ConfigManager::KILLS_PER_MONTH_RED_SKULL) > 0 &&
			g_config.getNumber(ConfigManager::KILLS_PER_MONTH_RED_SKULL) <= unjustKills){
		setSkull(SKULL_RED);
	}
	
	uint16_t totalKills = IOPlayer::instance()->getPlayerUnjustKillCount(this, UNJUST_KILL_PERIOD_MONTH);

	if (g_config.getNumber(ConfigManager::KILLS_TO_BAN) != 0 && totalKills >= (g_config.getNumber(ConfigManager::KILLS_TO_BAN)))
	{
		if (g_bans.addPlayerBan(getName(), g_config.getNumber(ConfigManager::BAN_LENGTH), 1 /* Type here your gamemaster ID */, "Automatic", "", 28, ACTION_BANISHMENT))
		{
			g_game.addMagicEffect(getPosition(), NM_ME_MAGIC_POISON);
			g_scheduler.addEvent(createSchedulerTask(1000, boost::bind(&Player::kickPlayer, this)));
		}
	}

	if(oldSkull != getSkull()){
		lastSkullTime = std::time(NULL);
		g_game.updateCreatureSkull(this);
	}
}

void Player::checkSkullTicks(int32_t ticks)
{
	if(!hasCondition(CONDITION_INFIGHT) && getSkull() != SKULL_NONE){
		if( (skullType == SKULL_RED && std::time(NULL) >= lastSkullTime + g_config.getNumber(ConfigManager::RED_SKULL_DURATION)) ){
			lastSkullTime = 0;
			setSkull(SKULL_NONE);
			clearAttacked();
			g_game.updateCreatureSkull(this);
		}
	}
}
#endif

void Player::setSex(PlayerSex_t player_sex)
{
	if(sex != player_sex){
		sex = player_sex;
	}
}

bool Player::canLogout()
{
	if(isConnecting){
		return false;
	}

	if(isPzLocked()){
		return false;
	}

	if(getTile()->hasFlag(TILESTATE_NOLOGOUT)){
		return false;
	}

	if(!g_creatureEvents->playerLogOut(this)){
		return false;
	}

	return true;
}

void Player::learnInstantSpell(const std::string& name)
{
	if(!hasLearnedInstantSpell(name)){
		learnedInstantSpellList.push_back(name);
	}
}

bool Player::hasLearnedInstantSpell(const std::string& name) const
{
	if(hasFlag(PlayerFlag_CannotUseSpells)){
		return false;
	}

	if(hasFlag(PlayerFlag_IgnoreSpellCheck)){
		return true;
	}

	for(LearnedInstantSpellList::const_iterator it = learnedInstantSpellList.begin();
			it != learnedInstantSpellList.end(); ++it){
		if(boost::algorithm::iequals((*it), name)){
			return true;
		}
	}

	return false;
}

bool Player::withdrawMoney(uint32_t amount)
{
	if(balance < amount){
		return false;
	}

	bool ret = g_game.addMoney(this, amount);
	if(ret){
		balance -= amount;
	}
	return ret;
}

bool Player::depositMoney(uint32_t amount)
{
    if (safeIncrUInt32_t(balance, amount))
    {
		if (!g_game.removeMoney(this, amount))
        {
			balance -= amount; //undo the change in balance
			return false;
        }
    }
    else
        return false;
 
    return true;
 }

bool Player::transferMoneyTo(const std::string& name, uint32_t amount)
{
	Player* target = g_game.getPlayerByNameEx(name);
	if(!target){
		return false;
	}

	bool result = false;
	if (balance >= amount && safeIncrUInt32_t(target->balance, amount)){
		balance -= amount;
		result = true;
	}

	if(target->isOffline()){
		IOPlayer::instance()->savePlayer(target);
		delete target;
	}

	return result;
}

bool Player::isLoginAttackLocked(uint32_t attackerId) const
{
	if(OTSYS_TIME() <= lastLoginMs + g_config.getNumber(ConfigManager::LOGIN_ATTACK_DELAY))
		return !hasBeenAttacked(attackerId);
	return false;
}

void Player::checkIdleTime(uint32_t ticks)
{
	if(g_config.getNumber(ConfigManager::IDLE_TIME) > 0){
		if(!getTile()->hasFlag(TILESTATE_NOLOGOUT) && !hasFlag(PlayerFlag_CanAlwaysLogin)){
			idleTime += ticks;
			if(idleTime >= g_config.getNumber(ConfigManager::IDLE_TIME)){
				kickPlayer();
			}
			else if(idleTime >= g_config.getNumber(ConfigManager::IDLE_TIME_WARNING) && !idleWarned){
				int32_t alreadyIdleTime = g_config.getNumber(ConfigManager::IDLE_TIME_WARNING) / 60000;
				int32_t remainingTime = (g_config.getNumber(ConfigManager::IDLE_TIME) - g_config.getNumber(ConfigManager::IDLE_TIME_WARNING)) / 60000;
				std::stringstream message;
				message << "You have been idle for " << alreadyIdleTime << " " << (alreadyIdleTime > 1 ? "minutes" : "minute") << ", you will be disconnected in " << remainingTime << " " << (remainingTime > 1 ? "minutes" : "minute") << " if you are still idle then.";
				sendTextMessage(MSG_STATUS_WARNING, message.str());
				idleWarned = true;
			}
		}
	}
}

bool Player::checkPzBlock(Player* targetPlayer)
{
	if(hasFlag(PlayerFlag_NotGainInFight) || Combat::isInPvpZone(this, targetPlayer))
		return false;

	#ifdef __SKULLSYSTEM__
	if(targetPlayer->hasAttacked(this) && !g_config.getNumber(ConfigManager::DEFENSIVE_PZ_LOCK))
		return false;
	#endif

	return true;
}

void Player::onAdvanceEvent(levelTypes_t type, uint32_t oldLevel, uint32_t newLevel)
{
	CreatureEventList advanceEvents = getCreatureEvents(CREATURE_EVENT_ADVANCE);
	for(CreatureEventList::iterator it = advanceEvents.begin(); it != advanceEvents.end(); ++it){
		(*it)->executeOnAdvance(this, type, oldLevel, newLevel);
	}
}

void Player::toogleGmInvisible()
{
	//
	gmInvisible = !gmInvisible;

	//
	const SpectatorVec& list = g_game.getSpectators(getPosition());
	SpectatorVec::const_iterator it;

	//
	Player* tmpPlayer = NULL;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
		{
			if(tmpPlayer != this && !tmpPlayer->canSeeGmInvisible(this))
			{
				if(gmInvisible)
					tmpPlayer->sendCreatureDisappear(this, getTile()->getClientIndexOfThing(tmpPlayer, this), true);
				else
					tmpPlayer->sendCreatureAppear(this, getPosition(), true);
			}
			else
			{
				if(gmInvisible)
				{
					static Outfit_t Outfit;
					tmpPlayer->sendCreatureChangeOutfit(this, Outfit);
				}
				else
					tmpPlayer->sendCreatureChangeOutfit(this, getCurrentOutfit());
			}
		}
	}

	// VIPs
	for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
	{
		if(it->second != this && !it->second->canSeeGmInvisible(this))
		{
			if(gmInvisible)
				it->second->notifyLogOut(this);
			else
				it->second->notifyLogIn(this);
		}
	}

	//TODO: fire StepIn/StepOut events
}

bool Player::canAddContainer(const Container* container) const
{
	//We start searching for containers: if limit is reached, return false already, if not, keeps going
	//Let's start by the container that player is puting in his inventory
	uint32_t containers = 1; //the container itself counts
	for(ContainerIterator iter = container->begin(), end = container->end(); iter != end; ++iter)
	{
		if((*iter)->getContainer())
		{
			containers++;
			if(containers > g_config.getNumber(ConfigManager::MAX_CONTAINERS_INSIDE_PLAYER_INVENTORY)) //If already reached the limit, should stop searching now
				return false;
		}
	}

	//The container that the player is trying to add to his inventory did not exceed the limit of containers itself
	//But let's check if the contnier + player inventory containers won't either
	for(int i = SLOT_FIRST; i < SLOT_LAST; i++){
		Item* item = inventory[i];

		if(item)
		{
			Container* pcontainer = item->getContainer();
			if(pcontainer)
			{
				//We found a container in inventory
				containers++;
				if(containers > g_config.getNumber(ConfigManager::MAX_CONTAINERS_INSIDE_PLAYER_INVENTORY)) //If already reached the limit, should stop searching now
					return false;

				//Check inside the container
				for(ContainerIterator iter = pcontainer->begin(), end = pcontainer->end(); iter != end; ++iter)
				{
					if((*iter)->getContainer())
					{
						containers++;
						if(containers > g_config.getNumber(ConfigManager::MAX_CONTAINERS_INSIDE_PLAYER_INVENTORY)) //If already reached the limit, should stop searching now
							return false;
					}
				}
			}
		}
	}

	return true;
}