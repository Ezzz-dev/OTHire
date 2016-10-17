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

#ifndef __OTSERV_NPC_H__
#define __OTSERV_NPC_H__

#include "definitions.h"
#include "creature.h"
#include "luascript.h"
#include "templates.h"

//////////////////////////////////////////////////////////////////////
// Defines an NPC...
class Npc;
class Player;
class NpcResponse;
struct NpcState;

typedef std::list<Npc*> NpcList;
class Npcs{
public:
	Npcs() {};
	~Npcs() {};
	void reload();
};

class NpcScriptInterface : public LuaScriptInterface
{
public:
	NpcScriptInterface();
	virtual ~NpcScriptInterface();

	bool loadNpcLib(std::string file);

protected:

	virtual void registerFunctions();

	static int luaActionSay(lua_State *L);
	static int luaActionMove(lua_State *L);
	static int luaActionMoveTo(lua_State *L);
	static int luaActionTurn(lua_State* L);
	static int luaActionFollow(lua_State* L);
	static int luaSelfGetPos(lua_State *L);
	static int luagetDistanceTo(lua_State *L);
	static int luaSetNpcFocus(lua_State *L);
	static int luaGetNpcCid(lua_State *L);
	static int luaGetNpcPos(lua_State *L);
	static int luaGetNpcName(lua_State *L);
	static int luaGetNpcParameter(lua_State *L);

private:
	virtual bool initState();
	virtual bool closeState();

	bool m_libLoaded;
};

class NpcEventsHandler
{
public:
	NpcEventsHandler(Npc* npc);
	virtual ~NpcEventsHandler();

	virtual void onCreatureAppear(const Creature* creature){};
	virtual void onCreatureDisappear(const Creature* creature){};
	virtual void onCreatureMove(const Creature* creature, const Position& oldPos, const Position& newPos){};
	virtual void onCreatureSay(const Creature* creature, SpeakClasses, const std::string& text){};
	virtual void onThink(){};

	bool isLoaded();

protected:
	Npc* m_npc;
	bool m_loaded;
};

class NpcScript : public NpcEventsHandler
{
public:
	NpcScript(std::string file, Npc* npc);
	NpcScript(Npc* npc);
	virtual ~NpcScript();

	virtual void onCreatureAppear(const Creature* creature);
	virtual void onCreatureDisappear(const Creature* creature);
	virtual void onCreatureMove(const Creature* creature, const Position& oldPos, const Position& newPos);
	virtual void onCreatureSay(const Creature* creature, SpeakClasses, const std::string& text);
	virtual void onThink();

private:
	NpcScriptInterface* m_scriptInterface;

	int32_t m_onCreatureAppear;
	int32_t m_onCreatureDisappear;
	int32_t m_onCreatureMove;
	int32_t m_onCreatureSay;
	int32_t m_onThink;
};

#define MAX_RAND_RANGE 10000000
class Npc : public Creature
{
public:
#ifdef __ENABLE_SERVER_DIAGNOSTIC__
	static uint32_t npcCount;
#endif

	virtual ~Npc();

	virtual Npc* getNpc() { return this; };
	virtual const Npc* getNpc() const { return this; };

	virtual bool isPushable() const { return true; };

	virtual uint32_t idRange(){ return NPC_ID_RANGE; }
	static AutoList<Npc> listNpc;
	void removeList() { listNpc.removeList(getID()); }
	void addList() { listNpc.addList(this); }

	static Npc* createNpc(const std::string& name);

	virtual bool canSee(const Position& pos) const;

	bool load();
	void reload();

	virtual const std::string& getName() const { return name; };
	virtual const std::string& getNameDescription() const { return name; };

	void doSay(const std::string& text, SpeakClasses type, Player* player);

	void doMove(Direction dir);
	void doTurn(Direction dir);
	void doMoveTo(Position pos);
	bool isLoaded(){ return loaded; }
	virtual void setMasterPos(const Position& pos, uint32_t radius = 1) {
		masterPos = pos;
		if (masterRadius == -1)
			masterRadius = radius;
	}

	void turnToCreature(Creature* creature);
	void setCreatureFocus(Creature* creature);
	void setInitialLookDirection(Direction dir) { initialLookDir = dir; };
	void turnToInitialLookDirection();
	bool getParameter(const std::string key, std::string& value);
	NpcScriptInterface* getScriptInterface();

protected:
	Npc(const std::string& _name);

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
	virtual void onPlacedCreature();
	virtual void onCreatureTurn(const Creature* creature);
	virtual void onCreatureSay(const Creature* creature, SpeakClasses type, const std::string& text);
	virtual void onCreatureChangeOutfit(const Creature* creature, const Outfit_t& outfit);
	virtual void onThink(uint32_t interval);
	virtual std::string getDescription(int32_t lookDistance) const;

	bool isImmune(CombatType_t type) const { return true; }
	bool isImmune(ConditionType_t type, bool aggressive = true) const { return true; }
	virtual bool isAttackable() const { return attackable; }
	virtual bool getNextStep(Direction& dir, uint32_t& flags);

	bool canWalkTo(const Position& fromPos, Direction dir);
	bool getRandomStep(Direction& dir);

	void reset();
	bool loadFromXml(const std::string& name);

	std::string name;
	std::string m_datadir;
	std::string m_scriptdir;
	std::string m_filename;
	uint32_t walkTicks;
	bool floorChange;
	Direction initialLookDir;
	bool attackable;
	bool isIdle;
	bool hasScriptedFocus;
	int32_t focusCreature;
	ParametersMap m_parameters;

	NpcEventsHandler* m_npcEventHandler;

	bool loaded;

	static NpcScriptInterface* m_scriptInterface;

	friend class Npcs;
	friend class NpcScriptInterface;
};

#endif
