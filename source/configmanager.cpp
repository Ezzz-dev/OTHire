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

#include "configmanager.h"
#include <iostream>

ConfigManager::ConfigManager()
{
	L = NULL;

	m_isLoaded = false;

	m_confString[IP] = "";
	m_confInteger[ADMIN_PORT] = 0;
	m_confInteger[GAME_PORT] = 0;
	m_confInteger[LOGIN_PORT] = 0;
	m_confInteger[STATUS_PORT] = 0;
}

ConfigManager::~ConfigManager()
{
	//
}

bool ConfigManager::loadFile(const std::string& _filename)
{
	if(L)
		lua_close(L);

	L = lua_open();

	if(!L) return false;

	if(luaL_dofile(L, _filename.c_str()))
	{
		lua_close(L);
		L = NULL;
		return false;
	}

	// parse config
	if(!m_isLoaded) // info that must be loaded one time (unless we reset the modules involved)
	{
		m_confString[CONFIG_FILE] = _filename;

		// These settings might have been set from command line
		if(m_confString[IP] == "")
			m_confString[IP] = getGlobalString(L, "ip", "127.0.0.1");
		if(m_confInteger[GAME_PORT] == 0)
			m_confInteger[GAME_PORT] = getGlobalNumber(L, "game_port");
		if(m_confInteger[ADMIN_PORT] == 0)
			m_confInteger[ADMIN_PORT] = getGlobalNumber(L, "admin_port");
		if(m_confInteger[LOGIN_PORT] == 0)
			m_confInteger[LOGIN_PORT] = getGlobalNumber(L, "login_port");
		if(m_confInteger[STATUS_PORT] == 0)
			m_confInteger[STATUS_PORT] = getGlobalNumber(L, "status_port");

#if defined __CONFIG_V2__
		unsigned int pos = _filename.rfind("/");
		std::string configPath = "";
		if(pos != std::string::npos)
			configPath = _filename.substr(0, pos+1);

		m_confString[DATA_DIRECTORY] = configPath + getGlobalString(L, "datadir", "data/");
		m_confString[MAP_FILE] = m_confString[DATA_DIRECTORY] + getGlobalString(L, "map");
		m_confString[MAP_STORE_FILE] = m_confString[DATA_DIRECTORY] + getGlobalString(L, "mapstore");
		m_confString[HOUSE_STORE_FILE] = m_confString[DATA_DIRECTORY] + getGlobalString(L, "housestore");
#else
		m_confString[DATA_DIRECTORY] = getGlobalString(L, "datadir");
		m_confString[MAP_FILE] = getGlobalString(L, "map");
		m_confString[MAP_STORE_FILE] = getGlobalString(L, "mapstore");
		m_confString[HOUSE_STORE_FILE] = getGlobalString(L, "housestore");
#endif
		m_confString[HOUSE_RENT_PERIOD] = getGlobalString(L, "houserentperiod", "monthly");
		m_confString[MAP_KIND] = getGlobalString(L, "mapkind");
		if(getGlobalString(L, "md5passwords") != ""){
			std::cout << "Warning: [ConfigManager] md5passwords is deprecated. Use passwordtype instead." << std::endl;
		}
		m_confString[PASSWORD_TYPE_STR] = getGlobalString(L, "passwordtype");
		m_confString[PASSWORD_SALT] = getGlobalString(L, "passwordsalt", "");
		m_confString[WORLD_TYPE] = getGlobalString(L, "worldtype");
		m_confString[SQL_HOST] = getGlobalString(L, "sql_host");
		m_confString[SQL_USER] = getGlobalString(L, "sql_user");
		m_confString[SQL_PASS] = getGlobalString(L, "sql_pass");
		m_confString[SQL_DB] = getGlobalString(L, "sql_db");
		m_confString[SQL_TYPE] = getGlobalString(L, "sql_type");
		m_confInteger[SQL_PORT] = getGlobalNumber(L, "sql_port");
		m_confInteger[PASSWORD_TYPE] = PASSWORD_TYPE_PLAIN;
	}

	m_confString[LOGIN_MSG] = getGlobalString(L, "loginmsg", "Welcome.");
	m_confString[SERVER_NAME] = getGlobalString(L, "servername");
	m_confString[WORLD_NAME] = getGlobalString(L, "worldname", "Gameworld");
	m_confString[OWNER_NAME] = getGlobalString(L, "ownername");
	m_confString[OWNER_EMAIL] = getGlobalString(L, "owneremail");
	m_confString[URL] = getGlobalString(L, "url");
	m_confString[LOCATION] = getGlobalString(L, "location");
	m_confString[MAP_STORAGE_TYPE] = getGlobalString(L, "map_store_type", "relational");
	m_confInteger[LOGIN_TRIES] = getGlobalNumber(L, "logintries", 5);
	m_confInteger[RETRY_TIMEOUT] = getGlobalNumber(L, "retrytimeout", 30 * 1000);
	m_confInteger[LOGIN_TIMEOUT] = getGlobalNumber(L, "logintimeout", 5 * 1000);
	m_confString[MOTD] = getGlobalString(L, "motd");
	m_confInteger[MOTD_NUM] = getGlobalNumber(L, "motdnum");
	m_confInteger[MAX_PLAYERS] = getGlobalNumber(L, "maxplayers");
	m_confInteger[EXHAUSTED] = getGlobalNumber(L, "exhausted", 30000);
	m_confInteger[EXHAUSTED_ADD] = getGlobalNumber(L, "exhaustedadd", 5000);
	m_confInteger[COMBAT_EXHAUSTED] = getGlobalNumber(L, "fightexhausted", 2000);
	m_confInteger[HEAL_EXHAUSTED] = getGlobalNumber(L, "healexhausted", 1000);
	m_confInteger[STAIRHOP_EXHAUSTED] = getGlobalNumber(L, "stairhop_exhausted", 2*1000);
	m_confInteger[IN_FIGHT_DURATION] = getGlobalNumber(L, "in_fight_duration", 60 * 1000);
	m_confInteger[HUNTING_KILL_DURATION] = getGlobalNumber(L, "hunting_kill_duration", 60 * 1000);
	m_confInteger[FIELD_OWNERSHIP_DURATION] = getGlobalNumber(L, "field_ownership_duration", 5 * 1000);
	m_confInteger[MIN_ACTIONTIME] = getGlobalNumber(L, "minactioninterval", 200);
	m_confInteger[MIN_ACTIONEXTIME] = getGlobalNumber(L, "minactionexinterval", 1000);
	m_confInteger[DEFAULT_DESPAWNRANGE] = getGlobalNumber(L, "despawnrange", 2);
	m_confInteger[DEFAULT_DESPAWNRADIUS] = getGlobalNumber(L, "despawnradius", 50);
	m_confInteger[ALLOW_CLONES] = getGlobalBoolean(L, "allowclones", false);
	m_confInteger[PARTY_MEMBER_EXP_BONUS] = getGlobalNumber(L, "party_exp_mul", 5);
	m_confInteger[RATE_EXPERIENCE] = getGlobalNumber(L, "rate_exp", 1);
	m_confInteger[RATE_SKILL] = getGlobalNumber(L, "rate_skill", 1);
	m_confInteger[RATE_LOOT] = getGlobalNumber(L, "rate_loot", 1);
	m_confInteger[RATE_MAGIC] = getGlobalNumber(L, "rate_mag", 1);
	m_confInteger[RATE_SPAWN] = getGlobalNumber(L, "rate_spawn", 1);
	m_confInteger[LEVEL_TO_ROOK] = getGlobalNumber(L, "level_to_rook", 0);
	m_confInteger[ROOK_TEMPLE_ID] = getGlobalNumber(L, "rook_temple_id", 1);
	m_confInteger[STORAGE_SENDROOK] = getGlobalNumber(L, "storage_sendrook", 49786);
	m_confInteger[MAX_MESSAGEBUFFER] = getGlobalNumber(L, "maxmessagebuffer", 4);
	m_confInteger[SAVE_CLIENT_DEBUG_ASSERTIONS] = getGlobalBoolean(L, "saveclientdebug", true);
	m_confInteger[CHECK_ACCOUNTS] = getGlobalBoolean(L, "checkaccounts", true);
	m_confInteger[USE_BALANCE_HOUSE_PAYING] = getGlobalBoolean(L, "use_balance_house_paying", true);
	m_confInteger[PREMIUM_ONLY_BEDS] = getGlobalBoolean(L, "premonlybeds", true);
	m_confInteger[UNJUST_SKULL_DURATION] = getGlobalNumber(L, "unjust_skull_duration", 15*60*1000);
	m_confInteger[KILLS_PER_DAY_RED_SKULL] = getGlobalNumber(L, "kills_per_day_red_skull", 3);
	m_confInteger[KILLS_PER_WEEK_RED_SKULL] = getGlobalNumber(L, "kills_per_week_red_skull", 5);
	m_confInteger[KILLS_PER_MONTH_RED_SKULL] = getGlobalNumber(L, "kills_per_month_red_skull", 10);
	m_confInteger[RED_SKULL_DURATION] = getGlobalNumber(L, "red_skull_duration", 30*24*60*60);
	m_confInteger[REMOVE_AMMUNITION] = getGlobalBoolean(L, "remove_ammunition", true);
	m_confInteger[REMOVE_RUNE_CHARGES] = getGlobalBoolean(L, "remove_rune_charges", true);
	m_confInteger[REMOVE_WEAPON_CHARGES] = getGlobalBoolean(L, "remove_weapon_charges", true);
	m_confInteger[LOGIN_ATTACK_DELAY] = getGlobalNumber(L, "login_attack_delay", 10*1000);
	m_confInteger[IDLE_TIME] = getGlobalNumber(L, "max_idle_time", 16*60*1000);
	m_confInteger[IDLE_TIME_WARNING] = getGlobalNumber(L, "max_idle_time_warning", 15*60*1000);
	m_confInteger[HOUSE_ONLY_PREMIUM] = getGlobalBoolean(L, "house_only_premium", true);
	m_confInteger[HOUSE_LEVEL] = getGlobalNumber(L, "house_level", 1);
	m_confInteger[HOUSE_TILE_PRICE] = getGlobalNumber(L, "house_tile_price", 100);
	m_confInteger[SHOW_HOUSE_PRICES] = getGlobalBoolean(L, "show_house_prices", false);
	m_confInteger[BROADCAST_BANISHMENTS] = getGlobalBoolean(L, "broadcast_banishments", false);
	m_confInteger[NOTATIONS_TO_BAN] = getGlobalNumber(L, "notations_to_banishment", 3);
	m_confInteger[WARNINGS_TO_FINALBAN] = getGlobalNumber(L, "warnings_to_final_banishment", 4);
	m_confInteger[WARNINGS_TO_DELETION] = getGlobalNumber(L, "warnings_to_deletion", 5);
	m_confInteger[BAN_LENGTH] = getGlobalNumber(L, "banishment_length", 7 * 86400);
	m_confInteger[FINALBAN_LENGTH] = getGlobalNumber(L, "final_banishment_length", 30 * 86400);
	m_confInteger[IPBANISHMENT_LENGTH] = getGlobalNumber(L, "ip_banishment_length", 86400);
	m_confInteger[ALLOW_GAMEMASTER_MULTICLIENT] = getGlobalBoolean(L, "allow_gamemaster_multiclient", false);
	m_confInteger[DEATH_ASSIST_COUNT] = getGlobalNumber(L, "death_assist_count", 1);
	m_confInteger[LAST_HIT_PZBLOCK_ONLY] = getGlobalBoolean(L, "last_hit_pzblock_only", true);
	m_confInteger[DEFENSIVE_PZ_LOCK] = getGlobalBoolean(L, "defensive_pz_lock", false);
	m_confInteger[DISTANCE_WEAPON_INTERRUPT_SWING] = getGlobalBoolean(L, "distance_weapon_interrupt_swing", true);
	m_confInteger[RATES_FOR_PLAYER_KILLING] = getGlobalBoolean(L, "rates_for_player_killing", false);
	m_confInteger[RATE_EXPERIENCE_PVP] = getGlobalNumber(L, "rate_exp_pvp", 0);
	m_confInteger[FIST_STRENGTH] = getGlobalNumber(L, "fist_strength", 7);
	m_confInteger[STATUSQUERY_TIMEOUT] = getGlobalNumber(L, "statustimeout", 30 * 1000);
	m_confInteger[CAN_ROPE_CREATURES] = getGlobalBoolean(L, "can_rope_creatures", true);
	m_confString[DEATH_MSG] = getGlobalString(L, "death_msg", "You are dead.");
	m_confInteger[CAN_ATTACK_INVISIBLE] = getGlobalBoolean(L, "can_attack_invisible", false);
	m_confInteger[MIN_PVP_LEVEL] = getGlobalNumber(L, "min_pvp_level", 0);
	#ifdef __MIN_PVP_LEVEL_APPLIES_TO_SUMMONS__
	m_confInteger[MIN_PVP_LEVEL_APPLIES_TO_SUMMONS] = getGlobalBoolean(L, "min_pvp_level_applies_to_summons", true);
	#endif
	m_confInteger[HEIGHT_MINIMUM_FOR_IDLE] = getGlobalNumber(L, "height_minimum_for_idle", 3);
	m_confInteger[EXPERIENCE_STAGES] = getGlobalBoolean(L, "experience_stages", false);
	m_confInteger[PUSH_INTERVAL] = getGlobalNumber(L, "push_interval", 2000);
	m_confInteger[MOVEITEM_TIME] = getGlobalNumber(L, "move_item_interval", 500);
	m_confInteger[MAX_STACK_SIZE] = getGlobalNumber(L, "max_stack_size", 1000);
	m_confInteger[PVP_DAMAGE] = getGlobalNumber(L, "pvp_damage", 50);
	m_confInteger[WANDS_INTERRUPT_SWING] = getGlobalBoolean(L, "wands_interrupt_swing", true);
	m_confInteger[PLAYER_QUERYDESTINATION_DEEPNESS] = getGlobalNumber(L, "player_querydestination_deepness", -1);
	m_confInteger[TILE_LIMIT] = getGlobalNumber(L, "tile_limit", 0);
	m_confInteger[PROTECTION_TILE_LIMIT] = getGlobalNumber(L, "protection_tile_limit", 0);
	m_confInteger[HOUSE_TILE_LIMIT]	= getGlobalNumber(L, "house_tile_limit", 0);
	m_confInteger[LUA_EXCEPTED_TYPE_ERRORS_ENABLED]	= getGlobalBoolean(L, "lua_excepted_type_errors_enabled", false);
	m_confInteger[MAX_AMOUNT_ITEMS_INSIDE_CONTAINERS] = getGlobalNumber(L, "max_amount_items_inside_containers", 2000);
	m_confInteger[MAX_DEEPNESS_OF_CHAIN_OF_CONTAINERS] = getGlobalNumber(L, "max_deepness_of_chain_of_containers", 500);
	m_confInteger[BIND_ONLY_GLOBAL_ADDRESS]	= getGlobalBoolean(L, "bind_only_global_address", false);
	m_confInteger[MAX_CONTAINERS_INSIDE_PLAYER_INVENTORY] = getGlobalNumber(L, "max_containers_inside_player_inventory", 100);
	m_confInteger[USE_RUNE_LEVEL_REQUIREMENTS] = getGlobalBoolean(L, "use_rune_level_requirements", true);
	m_confInteger[CONTAINER_ITEMS_AUTO_STACK] = getGlobalBoolean(L, "container_items_auto_stack", false);
	m_confInteger[KILLS_TO_BAN] = getGlobalNumber(L, "kills_to_ban", 20);
	m_confInteger[MAX_PACKETS_PER_SECOND] = getGlobalNumber(L, "max_packets_per_second", 35);
	m_confInteger[PARCEL_BLOCK] = getGlobalBoolean(L, "parcel_block", false);
	
	// Guild War
	m_confInteger[GUILD_WAR_FEE] = getGlobalNumber(L, "guild_war_fee", 1000);
	m_confInteger[GUILD_WARS_END_ONLY_ON_STARTUP] = getGlobalBoolean(L, "guild_wars_end_only_on_startup", true);
	
	// Cast System
	m_confInteger[ENABLE_CAST] = getGlobalBoolean(L, "enableCast", false);
	m_confInteger[MAX_CAST_VIEWERS] = getGlobalNumber(L, "max_cast_viewers", 20);	
	
	m_isLoaded = true;
	return true;
}

bool ConfigManager::reload()
{
	if(!m_isLoaded)
		return false;

	return loadFile(m_confString[CONFIG_FILE]);
}

const std::string& ConfigManager::getString(uint32_t _what) const
{
	if(m_isLoaded && _what < LAST_STRING_CONFIG)
		return m_confString[_what];
	else
	{
		std::cout << "Warning: [ConfigManager::getString] " << _what << std::endl;
		return m_confString[DUMMY_STR];
	}
}

int64_t ConfigManager::getNumber(uint32_t _what) const
{
	if(m_isLoaded && _what < LAST_INTEGER_CONFIG)
		return m_confInteger[_what];
	else
	{
		std::cout << "Warning: [ConfigManager::getNumber] " << _what << std::endl;
		return 0;
	}
}

bool ConfigManager::setNumber(uint32_t _what, int64_t _value)
{
	if(_what < LAST_INTEGER_CONFIG)
	{
		m_confInteger[_what] = _value;
		return true;
	}
	else
	{
		std::cout << "Warning: [ConfigManager::setNumber] " << _what << std::endl;
		return false;
	}
}

bool ConfigManager::setString(uint32_t _what, const std::string& _value)
{
	if(_what < LAST_STRING_CONFIG)
	{
		m_confString[_what] = _value;
		return true;
	}
	else
	{
		std::cout << "Warning: [ConfigManager::setString] " << _what << std::endl;
		return false;
	}
}

std::string ConfigManager::getGlobalString(lua_State* _L, const std::string& _identifier, const std::string& _default)
{
	lua_getglobal(_L, _identifier.c_str());

	if(!lua_isstring(_L, -1)){
		lua_pop(_L, 1);
		return _default;
	}

	int len = (int)lua_strlen(_L, -1);
	std::string ret(lua_tostring(_L, -1), len);
	lua_pop(_L,1);

	return ret;
}

int64_t ConfigManager::getGlobalNumber(lua_State* _L, const std::string& _identifier, int64_t _default)
{
	lua_getglobal(_L, _identifier.c_str());

	if(!lua_isnumber(_L, -1)){
		lua_pop(_L, 1);
		return _default;
	}

	int64_t val = (int64_t)lua_tonumber(_L, -1);
	lua_pop(_L,1);

	return val;
}

double ConfigManager::getGlobalFloat(lua_State* _L, const std::string& _identifier, double _default)
{
	lua_getglobal(_L, _identifier.c_str());

	if(!lua_isnumber(_L, -1)){
		lua_pop(_L, 1);
		return _default;
	}

	double val = lua_tonumber(_L, -1);
	lua_pop(_L,1);

	return val;
}

bool ConfigManager::getGlobalBoolean(lua_State* _L, const std::string& _identifier, bool _default)
{
	lua_getglobal(_L, _identifier.c_str());

	if(lua_isnumber(_L, -1)){
		int val = (int)lua_tonumber(_L, -1);
		lua_pop(_L, 1);
		return val != 0;
	} else if(lua_isstring(_L, -1)){
		std::string val = lua_tostring(_L, -1);
		lua_pop(_L, 1);
		return val == "yes";
	} else if(lua_isboolean(_L, -1)){
		bool v = lua_toboolean(_L, -1) != 0;
		lua_pop(_L, 1);
		return v;
	}

	return _default;
}

void ConfigManager::getConfigValue(const std::string& key, lua_State* toL)
{
	lua_getglobal(L, key.c_str());
	moveValue(L, toL);
}

void ConfigManager::moveValue(lua_State* from, lua_State* to)
{
	switch(lua_type(from, -1)){
		case LUA_TNIL:
			lua_pushnil(to);
			break;
		case LUA_TBOOLEAN:
			lua_pushboolean(to, lua_toboolean(from, -1));
			break;
		case LUA_TNUMBER:
			lua_pushnumber(to, lua_tonumber(from, -1));
			break;
		case LUA_TSTRING:
		{
			size_t len;
			const char* str = lua_tolstring(from, -1, &len);
			lua_pushlstring(to, str, len);
		}
			break;
		case LUA_TTABLE:
			lua_newtable(to);

			lua_pushnil(from); // First key
			while(lua_next(from, -2)){
				// Move value to the other state
				moveValue(from, to);
				// Value is popped, key is left

				// Move key to the other state
				lua_pushvalue(from, -1); // Make a copy of the key to use for the next iteration
				moveValue(from, to);
				// Key is in other state.
				// We still have the key in the 'from' state ontop of the stack

				lua_insert(to, -2); // Move key above value
				lua_settable(to, -3); // Set the key
			}
		default:
			break;
	}
	// Pop the value we just read
	lua_pop(from, 1);
}

