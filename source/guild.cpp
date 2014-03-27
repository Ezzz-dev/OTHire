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

#include "guild.h"
#include "game.h"
#include "database.h"
#include "chat.h"
#include "player.h"
#include "configmanager.h"
#include "ioplayer.h"
#include <boost/algorithm/string/predicate.hpp>

extern Game g_game;
extern Chat g_chat;
extern ConfigManager g_config;
extern Guilds g_guilds;

Guild* Guilds::getGuildById(uint32_t guildId)
{
	GuildsMap::iterator it = loadedGuilds.find(guildId);
	if(it != loadedGuilds.end())
		return it->second;
	else{
		Database* db = Database::instance();
		DBResult* result;
		DBQuery query;

		query << "SELECT `id`, `name` FROM `guilds` WHERE `id` = " << guildId;
		if((result = db->storeQuery(query.str()))){
			Guild* guild = new Guild();
			guild->setId(result->getDataInt("id"));
			guild->setName(result->getDataString("name"));
			loadedGuilds[guild->getId()] = guild;
			db->freeResult(result);
			return guild;
		}
	}

	return NULL;
}

bool Guilds::getGuildIdByName(uint32_t& guildId, const std::string& guildName)
{
	//Check cache
	for(GuildsMap::iterator it = loadedGuilds.begin(); it != loadedGuilds.end(); ++it){
		if(boost::algorithm::iequals(it->second->getName(), guildName)){
			guildId = it->first;
			return true;
		}
	}

	//Not in cache, let's try database (also add in cache if found)
	Database* db = Database::instance();
	DBResult* result;
	DBQuery query;

	query << "SELECT `id`, `name` FROM `guilds` WHERE `name` = " << db->escapeString(guildName);
	if((result = db->storeQuery(query.str()))){
		Guild* guild = new Guild();
		guild->setId(result->getDataInt("id"));
		guild->setName(result->getDataString("name"));
		loadedGuilds[guild->getId()] = guild;
		db->freeResult(result);
		return true;
	}

	return false;
}

Guild::Guild()
{
	id = 0;
	name = "";
}

Guild::~Guild()
{
}

void Guild::broadcastMessage(SpeakClasses type, const std::string& msg) const
{
	ChatChannel* channel = g_chat.getGuildChannel(getId());
	if(channel) //Channel doesn't necessarily exists
		channel->sendInfo(type, msg);
}