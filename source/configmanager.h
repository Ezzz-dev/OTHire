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

#ifndef _CONFIG_MANAGER_H
#define _CONFIG_MANAGER_H

#include "definitions.h"
#include <string>

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
}


class ConfigManager {
public:
	ConfigManager();
	~ConfigManager();

	enum string_config_t {
		DUMMY_STR = 0,
		CONFIG_FILE,
		DATA_DIRECTORY,
		MAP_FILE,
		MAP_STORE_FILE,
		HOUSE_STORE_FILE,
		HOUSE_RENT_PERIOD,
		MAP_KIND,
		LOGIN_MSG,
		SERVER_NAME,
		WORLD_NAME,
		OWNER_NAME,
		OWNER_EMAIL,
		URL,
		LOCATION,
		IP,
		MOTD,
		PASSWORD_TYPE_STR,
		PASSWORD_SALT,
		WORLD_TYPE,
		SQL_HOST,
		SQL_USER,
		SQL_PASS,
		SQL_DB,
		SQL_TYPE,
		MAP_STORAGE_TYPE,
		DEATH_MSG,
		LAST_STRING_CONFIG /* this must be the last one */
	};

	enum integer_config_t {
		LOGIN_TRIES = 0,
		GAME_PORT,
		ADMIN_PORT,
		LOGIN_PORT,
		STATUS_PORT,
		RETRY_TIMEOUT,
		LOGIN_TIMEOUT,
		MOTD_NUM,
		MAX_PLAYERS,
		EXHAUSTED,
		EXHAUSTED_ADD,
		COMBAT_EXHAUSTED,
		HEAL_EXHAUSTED,
		HUNTING_KILL_DURATION,
		IN_FIGHT_DURATION,
		FIELD_OWNERSHIP_DURATION,
		MIN_ACTIONTIME,
		MIN_ACTIONEXTIME,
		DEFAULT_DESPAWNRANGE,
		DEFAULT_DESPAWNRADIUS,
		ALLOW_CLONES,
		PARTY_MEMBER_EXP_BONUS,
		RATE_EXPERIENCE,
		RATE_SKILL,
		RATE_LOOT,
		RATE_MAGIC,
		RATE_SPAWN,
		MAX_MESSAGEBUFFER,
		SAVE_CLIENT_DEBUG_ASSERTIONS,
		CHECK_ACCOUNTS,
		PASSWORD_TYPE,
		SQL_PORT,
		STATUSQUERY_TIMEOUT,
		PREMIUM_ONLY_BEDS,
		UNJUST_SKULL_DURATION,
		KILLS_PER_DAY_RED_SKULL,
		KILLS_PER_WEEK_RED_SKULL,
		KILLS_PER_MONTH_RED_SKULL,
		KILLS_TO_BAN,
		RED_SKULL_DURATION,
		REMOVE_AMMUNITION,
		REMOVE_RUNE_CHARGES,
		REMOVE_WEAPON_CHARGES,
		USE_BALANCE_HOUSE_PAYING,
		LOGIN_ATTACK_DELAY,
		STAIRHOP_EXHAUSTED,
		IDLE_TIME,
		IDLE_TIME_WARNING,
		HOUSE_ONLY_PREMIUM,
		HOUSE_LEVEL,
		HOUSE_TILE_PRICE,
		SHOW_HOUSE_PRICES,
		NOTATIONS_TO_BAN,
		WARNINGS_TO_FINALBAN,
		WARNINGS_TO_DELETION,
		BAN_LENGTH,
		FINALBAN_LENGTH,
		IPBANISHMENT_LENGTH,
		BROADCAST_BANISHMENTS,
		ALLOW_GAMEMASTER_MULTICLIENT,
		DISTANCE_WEAPON_INTERRUPT_SWING,
		DEATH_ASSIST_COUNT,
		LAST_HIT_PZBLOCK_ONLY,
		DEFENSIVE_PZ_LOCK,
		RATES_FOR_PLAYER_KILLING,
		RATE_EXPERIENCE_PVP,
		FIST_STRENGTH,
		CAN_ROPE_CREATURES,
		CAN_ATTACK_INVISIBLE,
		MIN_PVP_LEVEL,
		#ifdef __MIN_PVP_LEVEL_APPLIES_TO_SUMMONS__
		MIN_PVP_LEVEL_APPLIES_TO_SUMMONS,
		#endif
		HEIGHT_MINIMUM_FOR_IDLE,
		EXPERIENCE_STAGES,
		PUSH_INTERVAL,
		WANDS_INTERRUPT_SWING,
		MOVEITEM_TIME,
		MAX_STACK_SIZE,
		PVP_DAMAGE,
		PLAYER_QUERYDESTINATION_DEEPNESS,
		TILE_LIMIT,
		PROTECTION_TILE_LIMIT,
		HOUSE_TILE_LIMIT,
		LUA_EXCEPTED_TYPE_ERRORS_ENABLED,
		MAX_AMOUNT_ITEMS_INSIDE_CONTAINERS,
		MAX_DEEPNESS_OF_CHAIN_OF_CONTAINERS,
		BIND_ONLY_GLOBAL_ADDRESS,
		MAX_CONTAINERS_INSIDE_PLAYER_INVENTORY,
		USE_RUNE_LEVEL_REQUIREMENTS,
		CONTAINER_ITEMS_AUTO_STACK,
		LAST_INTEGER_CONFIG /* this must be the last one */
	};

	bool loadFile(const std::string& _filename);
	bool reload();

	void getConfigValue(const std::string& key, lua_State* _L);
	const std::string& getString(uint32_t _what) const;
	int64_t getNumber(uint32_t _what) const;
	bool getBoolean(uint32_t _what) const { return getNumber(_what) != 0; }
	bool setNumber(uint32_t _what, int64_t _value);
	bool setString(uint32_t _what, const std::string& _value);

private:
	static void moveValue(lua_State* fromL, lua_State* toL);
	std::string getGlobalString(lua_State* _L, const std::string& _identifier, const std::string& _default="");
	int64_t getGlobalNumber(lua_State* _L, const std::string& _identifier, int64_t _default=0);
	double getGlobalFloat(lua_State* _L, const std::string& _identifier, double _default=0.0);
	bool getGlobalBoolean(lua_State* _L, const std::string& _identifier, bool _default=false);

	lua_State* L;
	bool m_isLoaded;
	std::string m_confString[LAST_STRING_CONFIG];
	int64_t m_confInteger[LAST_INTEGER_CONFIG];
};


#endif /* _CONFIG_MANAGER_H */
