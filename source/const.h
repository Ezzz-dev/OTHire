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

#ifndef __OTSERV_CONST_H__
#define __OTSERV_CONST_H__

#include "definitions.h"

#define NETWORKMESSAGE_MAXSIZE 15340

//Ranges for ID Creatures
#define PLAYER_ID_RANGE 0x10000000
#define MONSTER_ID_RANGE 0x40000000
#define NPC_ID_RANGE 0x80000000

enum MagicEffectClasses {
	NM_ME_DRAW_BLOOD       = 0x00,
	NM_ME_LOSE_ENERGY      = 0x01,
	NM_ME_PUFF             = 0x02,
	NM_ME_BLOCKHIT         = 0x03,
	NM_ME_EXPLOSION_AREA   = 0x04,
	NM_ME_EXPLOSION_DAMAGE = 0x05,
	NM_ME_FIRE_AREA        = 0x06,
	NM_ME_YELLOW_RINGS     = 0x07,
	NM_ME_POISON_RINGS     = 0x08,
	NM_ME_HIT_AREA         = 0x09,
	NM_ME_TELEPORT         = 0x0A, //10
	NM_ME_ENERGY_DAMAGE    = 0x0B, //11
	NM_ME_MAGIC_ENERGY     = 0x0C, //12
	NM_ME_MAGIC_BLOOD      = 0x0D, //13
	NM_ME_MAGIC_POISON     = 0x0E, //14
	NM_ME_HITBY_FIRE       = 0x0F, //15
	NM_ME_POISON           = 0x10, //16
	NM_ME_MORT_AREA        = 0x11, //17
	NM_ME_SOUND_GREEN      = 0x12, //18
	NM_ME_SOUND_RED        = 0x13, //19
	NM_ME_POISON_AREA      = 0x14, //20
	NM_ME_SOUND_YELLOW     = 0x15, //21
	NM_ME_SOUND_PURPLE     = 0x16, //22
	NM_ME_SOUND_BLUE       = 0x17, //23
	NM_ME_SOUND_WHITE      = 0x18, //24

	//for internal use, dont send to client
	NM_ME_NONE             = 0xFF,
	NM_ME_UNK              = 0xFFFF
};

enum ShootType_t {
	NM_SHOOT_SPEAR          = 0x00,
	NM_SHOOT_BOLT           = 0x01,
	NM_SHOOT_ARROW          = 0x02,
	NM_SHOOT_FIRE           = 0x03,
	NM_SHOOT_ENERGY         = 0x04,
	NM_SHOOT_POISONARROW    = 0x05,
	NM_SHOOT_BURSTARROW     = 0x06,
	NM_SHOOT_THROWINGSTAR   = 0x07,
	NM_SHOOT_THROWINGKNIFE  = 0x08,
	NM_SHOOT_SMALLSTONE     = 0x09,
	NM_SHOOT_DEATH          = 0x0A, //10
	NM_SHOOT_LARGEROCK      = 0x0B, //11
	NM_SHOOT_SNOWBALL       = 0x0C, //12
	NM_SHOOT_POWERBOLT      = 0x0D, //13
	NM_SHOOT_POISONFIELD    = 0x0E, //14

	//for internal use, dont send to client
	NM_SHOOT_WEAPONTYPE     = 0xFE, //254
	NM_SHOOT_NONE           = 0xFF,
	NM_SHOOT_UNK            = 0xFFFF
};

enum SpeakClasses {
	SPEAK_SAY				= 0x01,
	SPEAK_WHISPER			= 0x02,
	SPEAK_YELL				= 0x03,
	SPEAK_PRIVATE			= 0x04,
	SPEAK_CHANNEL_Y			= 0x05,	// Yellow
	SPEAK_RVR_CHANNEL		= 0x06,
	SPEAK_RVR_ANSWER		= 0x07,
	SPEAK_RVR_CONTINUE		= 0x08,
	SPEAK_BROADCAST			= 0x09,
	SPEAK_CHANNEL_R1		= 0x0A,	// Red - #c text
	SPEAK_PRIVATE_RED		= 0x0B,	// @name@text
	SPEAK_CHANNEL_O			= 0x0C,	// orange
	SPEAK_CHANNEL_R2		= 0x0E,	//red anonymous - #d text
	SPEAK_MONSTER_YELL		= 0x11,
	SPEAK_MONSTER_SAY		= 0x10,
};

enum MessageClasses {
	MSG_STATUS_CONSOLE_YELLOW	= 0x01, //Yellow message in the console
	MSG_STATUS_CONSOLE_LBLUE	= 0x04, //Light blue message in the console
	MSG_STATUS_CONSOLE_ORANGE	= 0x11, //Orange message in the console
	MSG_STATUS_WARNING			= 0x12, //Red message in game window and in the console
	MSG_EVENT_ADVANCE			= 0x13, //White message in game window and in the console
	MSG_EVENT_DEFAULT			= 0x14, //White message at the bottom of the game window and in the console
	MSG_STATUS_DEFAULT			= 0x15, //White message at the bottom of the game window and in the console
	MSG_INFO_DESCR				= 0x16, //Green message in game window and in the console
	MSG_STATUS_SMALL			= 0x17, //White message at the bottom of the game window"
	MSG_STATUS_CONSOLE_BLUE		= 0x18, //Blue message in the console
	MSG_STATUS_CONSOLE_RED		= 0x19, //Red message in the console
};

enum ClientFluidTypes_t {
	CFLUID_EMPTY,
	CFLUID_WATER,
	CFLUID_MANA,
	CFLUID_BEER,
	CFLUID_OIL,
	CFLUID_BLOOD,
	CFLUID_SLIME,
	CFLUID_MUD,
	CFLUID_LEMONADE,
	CFLUID_MILK,
	CFLUID_WINE,
	CFLUID_LIFE,
	CFLUID_URINE,
	CFLUID_RUM,
	CFLUID_FRUITJUICE,
	CFLUID_COCONUTMILK,
	CFLUID_TEA,
	CFLUID_MEAD,
	CFLUID_CUSTOM
};

enum FluidTypes_t {
	//notice that FluidType aren't supposed to be negative, or you will get into troubles!
	FLUID_EMPTY = 0,
	FLUID_WATER = 1,
	FLUID_BLOOD = 2,
	FLUID_BEER = 3,
	FLUID_SLIME = 4,
	FLUID_LEMONADE = 5,
	FLUID_MILK = 6,
	FLUID_MANA = 7,
	FLUID_LIFE = 10,
	FLUID_OIL = 11,
	FLUID_URINE = 13,
	FLUID_COCONUTMILK = 14,
	FLUID_WINE = 15,
	FLUID_MUD = 19,
	FLUID_FRUITJUICE = 21,
	FLUID_RUM = 27,
	FLUID_TEA = 35,
	FLUID_MEAD = 36,
};

enum SquareColor_t {
	SQ_COLOR_NONE   = 256,
	SQ_COLOR_BLACK  = 0
};

enum TextColor_t {
	TEXTCOLOR_BLUE        = 5,
	TEXTCOLOR_GREEN       = 18,
	TEXTCOLOR_LIGHTGREEN  = 30,
	TEXTCOLOR_LIGHTBLUE   = 35,
	TEXTCOLOR_TEAL	      = 65,
	TEXTCOLOR_DARKRED     = 108,
	TEXTCOLOR_LIGHTGREY   = 129,
	TEXTCOLOR_SKYBLUE	  = 143,
	TEXTCOLOR_PURPLE      = 154,
	TEXTCOLOR_RED         = 180,
	TEXTCOLOR_ORANGE      = 198,
	TEXTCOLOR_YELLOW      = 210,
	TEXTCOLOR_WHITE_EXP   = 215,
	TEXTCOLOR_NONE        = 255,
	TEXTCOLOR_UNK         = 256
};

enum Icons_t{
	ICON_NONE       = 0, // unused, 7.7 client debugs if sent
	ICON_POISON     = 1,
	ICON_BURN       = 2,
	ICON_ENERGY     = 4,
	ICON_DRUNK      = 8,
	ICON_MANASHIELD = 16,
	ICON_PARALYZE   = 32,
	ICON_HASTE      = 64,
	ICON_SWORDS     = 128
};

enum WeaponType_t {
	WEAPON_NONE     = 0,
	WEAPON_SWORD    = 1,
	WEAPON_CLUB     = 2,
	WEAPON_AXE      = 3,
	WEAPON_SHIELD   = 4,
	WEAPON_DIST     = 5,
	WEAPON_WAND     = 6,
	WEAPON_AMMO     = 7
};

enum Ammo_t {
	AMMO_NONE           = 0,
	AMMO_BOLT           = 1,
	AMMO_ARROW          = 2,
	AMMO_SPEAR          = 3,
	AMMO_THROWINGSTAR   = 4,
	AMMO_THROWINGKNIFE  = 5,
	AMMO_STONE          = 6,
	AMMO_SNOWBALL       = 7
};

enum AmmoAction_t{
	AMMOACTION_NONE,
	AMMOACTION_REMOVECOUNT,
	AMMOACTION_REMOVECHARGE,
	AMMOACTION_MOVE,
	AMMOACTION_MOVEBACK
};

enum WieldInfo_t{
	WIELDINFO_LEVEL     = 1,
	WIELDINFO_MAGLV     = 2,
	WIELDINFO_VOCREQ    = 4,
	WIELDINFO_PREMIUM   = 8
};

enum Skulls_t{
	SKULL_NONE      = 0,
	SKULL_YELLOW    = 1,
	SKULL_GREEN     = 2,
	SKULL_WHITE     = 3,
	SKULL_RED       = 4,
	SKULL_LAST
};

enum PartyShields_t{
	SHIELD_NONE = 0,
	SHIELD_WHITEYELLOW = 1,
	SHIELD_WHITEBLUE = 2,
	SHIELD_BLUE = 3,
	SHIELD_YELLOW = 4
};

enum item_t {
	ITEM_FIREFIELD        = 1492,
	ITEM_FIREFIELD_SAFE   = 1500,
	ITEM_FIREFIELD_NOT_DECAYING = 1487,

	ITEM_POISONFIELD      = 1496,
	ITEM_POISONFIELD_SAFE = 1503,
	ITEM_POISONFIELD_NOT_DECAYING = 1490,

	ITEM_ENERGYFIELD      = 1495,
	ITEM_ENERGYFIELD_SAFE = 1504,
	ITEM_ENERGYFIELD_NOT_DECAYING = 1491,

	ITEM_MAGICWALL        = 1497,
	ITEM_MAGICWALL_SAFE = 1497,

	ITEM_WILDGROWTH       = 1499,
	ITEM_WILDGROWTH_SAFE = 1499,

	ITEM_COINS_GOLD       = 2148,
	ITEM_COINS_PLATINUM   = 2152,
	ITEM_COINS_CRYSTAL    = 2160,

	ITEM_DEPOT            = 2594,
	ITEM_LOCKER1          = 2589,
	ITEM_GLOWING_SWITCH   = 11063,

	ITEM_MALE_CORPSE	  = 3058,
	ITEM_FEMALE_CORPSE	  = 3065,

	ITEM_MEAT             = 2666,
	ITEM_HAM              = 2671,
	ITEM_GRAPE            = 2681,
	ITEM_APPLE            = 2674,
	ITEM_BREAD            = 2689,
	ITEM_ROLL             = 2690,
	ITEM_CHEESE           = 2696,

	ITEM_FULLSPLASH       = 2016,
	ITEM_SMALLSPLASH      = 2019,

	ITEM_PARCEL           = 2595,
	ITEM_PARCEL_STAMPED   = 2596,
	ITEM_LETTER           = 2597,
	ITEM_LETTER_STAMPED   = 2598,
	ITEM_LABEL            = 2599,

	ITEM_DOCUMENT_RO      = 1968, //read-only

	ITEM_GATE_EXPERTISE_1	= 1227,
	ITEM_GATE_EXPERTISE_2	= 1229,
	ITEM_GATE_EXPERTISE_3	= 1245,
	ITEM_GATE_EXPERTISE_4	= 1247,
	ITEM_GATE_EXPERTISE_5	= 1259,
	ITEM_GATE_EXPERTISE_6	= 1261,
	ITEM_GATE_EXPERTISE_7	= 3540,
	ITEM_GATE_EXPERTISE_8	= 3549
};

enum PlayerFlags{
	//Add the flag's numbers to get the groupFlags number you need
	PlayerFlag_CannotUseCombat = 0,         //2^0 = 1
	PlayerFlag_CannotAttackPlayer,          //2^1 = 2
	PlayerFlag_CannotAttackMonster,         //2^2 = 4
	PlayerFlag_CannotBeAttacked,            //2^3 = 8
	PlayerFlag_CanConvinceAll,              //2^4 = 16
	PlayerFlag_CanSummonAll,                //2^5 = 32
	PlayerFlag_CanIllusionAll,              //2^6 = 64
	PlayerFlag_CanSenseInvisibility,        //2^7 = 128
	PlayerFlag_IgnoredByMonsters,           //2^8 = 256
	PlayerFlag_NotGainInFight,              //2^9 = 512
	PlayerFlag_HasInfiniteMana,             //2^10 = 1024
	PlayerFlag_HasInfiniteSoul,             //2^11 = 2048
	PlayerFlag_HasNoExhaustion,             //2^12 = 4096
	PlayerFlag_CannotUseSpells,             //2^13 = 8192
	PlayerFlag_CannotPickupItem,            //2^14 = 16384
	PlayerFlag_CanAlwaysLogin,              //2^15 = 32768
	PlayerFlag_CanBroadcast,                //2^16 = 65536
	PlayerFlag_CanEditHouses,               //2^17 = 131072
	PlayerFlag_CannotBeBanned,              //2^18 = 262144
	PlayerFlag_CannotBePushed,              //2^19 = 524288
	PlayerFlag_HasInfiniteCapacity,         //2^20 = 1048576
	PlayerFlag_CanPushAllCreatures,         //2^21 = 2097152
	PlayerFlag_CanTalkRedPrivate,           //2^22 = 4194304
	PlayerFlag_CanTalkRedChannel,           //2^23 = 8388608
	PlayerFlag_TalkOrangeHelpChannel,       //2^24 = 16777216
	PlayerFlag_NotGainExperience,           //2^25 = 33554432
	PlayerFlag_NotGainMana,                 //2^26 = 67108864
	PlayerFlag_NotGainHealth,               //2^27 = 134217728
	PlayerFlag_NotGainSkill,                //2^28 = 268435456
	PlayerFlag_SetMaxSpeed,                 //2^29 = 536870912
	PlayerFlag_SpecialVIP,                  //2^30 = 1073741824
	PlayerFlag_NotGenerateLoot,             //2^31 = 2147483648
	PlayerFlag_CanTalkRedChannelAnonymous,  //2^32 = 4294967296
	PlayerFlag_IgnoreProtectionZone,        //2^33 = 8589934592
	PlayerFlag_IgnoreSpellCheck,            //2^34 = 17179869184
	PlayerFlag_IgnoreWeaponCheck,           //2^35 = 34359738368
	PlayerFlag_CannotBeMuted,               //2^36 = 68719476736
	PlayerFlag_IsAlwaysPremium,             //2^37 = 137438953472
	PlayerFlag_CanAnswerRuleViolations,     //2^38 = 274877906944
	PlayerFlag_CanReloadContent,            //2^39 = 549755813888
	PlayerFlag_ShowGroupInsteadOfVocation,  //2^40 = 1099511627776
	PlayerFlag_CannotMoveItems,				//2^41 = 2199023255552
	PlayerFlag_CannotMoveCreatures,         //2^42 = 4398046511104
	PlayerFlag_CanReportBugs,				//2^43 = 8796093022208
	PlayerFlag_CanSeeSpecialDescription,    //2^44 = 17592186044416
	PlayerFlag_CannotBeSeen,				//2^45 = 35184372088832
	//add new flags here
	PlayerFlag_LastFlag
};

enum ViolationActions_t
{
	Action_None                     = 0,
	Action_Notation                 = 1 << 0,
	Action_Namelock                 = 1 << 1,
	Action_Banishment               = 1 << 2,
	Action_NamelockBan              = 1 << 3,
	Action_BanFinalWarning          = 1 << 4,
	Action_NamelockBanFinalWarning  = 1 << 5,
	Action_StatementReport          = 1 << 6,
	Action_IpBan                    = 1 << 7
};

const int maxViolationLevel = 18;

const int32_t violationReasons[maxViolationLevel + 1] =
{
	0,	//ignore this
	3,	//all name reasons
	9,	//all name & statement reasons
	18,	//all name, statement & cheating reasons
};

const int32_t violationNames[maxViolationLevel + 1] =
{
	Action_None,
	Action_Namelock,
	Action_Namelock,
	Action_Namelock | Action_NamelockBan,
};

const int32_t violationStatements[maxViolationLevel + 1] =
{
	Action_None,
	Action_None,
	Action_StatementReport,
	Action_StatementReport | Action_Notation | Action_Banishment,
};

//Reserved player storage key ranges
//[10000000 - 20000000]
#define PSTRG_RESERVED_RANGE_START  10000000
#define PSTRG_RESERVED_RANGE_SIZE   10000000

#define IS_IN_KEYRANGE(key, range) (key >= PSTRG_##range##_START && ((key - PSTRG_##range##_START) < PSTRG_##range##_SIZE))

#endif
