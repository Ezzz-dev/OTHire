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

#ifndef __OTSERV_GUILD_H__
#define __OTSERV_GUILD_H__

#include "definitions.h"
#include "const.h"
#include "creature.h"
#include <string>
#include <map>

class Player;

class Guild
{
public:
	Guild();
	~Guild();

	void setId(uint32_t _id){ id = _id; }
	void setName(std::string _name){ name = _name; }

	uint32_t getId() const { return id; }
	std::string getName() const { return name; }

	void broadcastMessage(SpeakClasses type, const std::string& msg) const;
protected:
	uint32_t id;
	std::string name;
};

class Guilds
{
public:

	Guild* getGuildById(uint32_t guildId);
	bool getGuildIdByName(uint32_t& guildId, const std::string& guildName);

protected:
	typedef std::map<uint32_t, Guild*> GuildsMap; //guild id, guild class
	GuildsMap loadedGuilds;
};

#endif
