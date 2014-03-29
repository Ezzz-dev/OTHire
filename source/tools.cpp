//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
// Various functions.
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

#include "tools.h"
#include "configmanager.h"
#include "md5.h"
#include "sha1.h"
#include <cmath>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <limits>
#include <boost/algorithm/string/predicate.hpp>

extern ConfigManager g_config;

void replaceString(std::string& str, const std::string sought, const std::string replacement)
{
	size_t pos = 0;
	size_t start = 0;
	size_t soughtLen = sought.length();
	size_t replaceLen = replacement.length();
	while((pos = str.find(sought, start)) != std::string::npos){
		str = str.substr(0, pos) + replacement + str.substr(pos + soughtLen);
		start = pos + replaceLen;
	}
}

void trim_right(std::string& source, const std::string& t)
{
	source.erase(source.find_last_not_of(t)+1);
}

void trim_left(std::string& source, const std::string& t)
{
	source.erase(0, source.find_first_not_of(t));
}

void trim(std::string& source, const std::string& t)
{
	trim_left(source, t);
	trim_right(source, t);
}

void toLowerCaseString(std::string& source)
{
	std::transform(source.begin(), source.end(), source.begin(), tolower);
}

void toUpperCaseString(std::string& source)
{
	std::transform(source.begin(), source.end(), source.begin(), upchar);
}

std::string asLowerCaseString(const std::string& source)
{
	std::string s = source;
	toLowerCaseString(s);
	return s;
}

std::string asUpperCaseString(const std::string& source)
{
	std::string s = source;
	toUpperCaseString(s);
	return s;
}

bool booleanString(std::string source)
{
	toLowerCaseString(source);
	return (source == "yes" || source == "true" || atoi(source.c_str()) > 0);
}

bool readXMLInteger(xmlNodePtr node, const char* tag, int32_t& value)
{
	char* nodeValue = (char*)xmlGetProp(node, (xmlChar*)tag);
	if(nodeValue){
		value = atoi(nodeValue);
		xmlFree(nodeValue);
		return true;
	}

	return false;
}

bool readXMLInteger64(xmlNodePtr node, const char* tag, uint64_t& value)
{
	char* nodeValue = (char*)xmlGetProp(node, (xmlChar*)tag);
	if(nodeValue){
		value = ATOI64(nodeValue);
		xmlFree(nodeValue);
		return true;
	}

	return false;
}

bool readXMLFloat(xmlNodePtr node, const char* tag, float& value)
{
	char* nodeValue = (char*)xmlGetProp(node, (xmlChar*)tag);
	if(nodeValue){
		value = atof(nodeValue);
		xmlFree(nodeValue);
		return true;
	}

	return false;
}

bool utf8ToLatin1(char* intext, std::string& outtext)
{
	outtext = "";

	if(intext == NULL){
		return false;
	}

	int inlen = strlen(intext);
	if(inlen == 0){
		return false;
	}

	int outlen = inlen*2;
	unsigned char* outbuf = new unsigned char[outlen];
	int res = UTF8Toisolat1(outbuf, &outlen, (unsigned char*)intext, &inlen);
	if(res < 0){
		delete[] outbuf;
		return false;
	}

	outbuf[outlen] = '\0';
	outtext = (char*)outbuf;
	delete[] outbuf;
	return true;
}

bool readXMLString(xmlNodePtr node, const char* tag, std::string& value)
{
	char* nodeValue = (char*)xmlGetProp(node, (xmlChar*)tag);
	if(nodeValue){
		if(!utf8ToLatin1(nodeValue, value)){
			value = nodeValue;
		}

		xmlFree(nodeValue);
		return true;
	}

	return false;
}

bool readXMLContentString(xmlNodePtr node, std::string& value)
{
	char* nodeValue = (char*)xmlNodeGetContent(node);
	if(nodeValue){
		if(!utf8ToLatin1(nodeValue, value)){
			value = nodeValue;
		}

		xmlFree(nodeValue);
		return true;
	}

	return false;
}

std::vector<std::string> explodeString(const std::string& inString, const std::string& separator)
{
	std::vector<std::string> returnVector;
	std::string::size_type start = 0;
	std::string::size_type end = 0;

	while((end=inString.find (separator, start)) != std::string::npos){
		returnVector.push_back (inString.substr (start, end-start));
		start = end+separator.size();
	}

	returnVector.push_back (inString.substr (start));
	return returnVector;
}

bool hasBitSet(uint32_t flag, uint32_t flags)
{
	return ((flags & flag) == flag);
}

 //safely adds incr to x, checking for overflow
bool safeIncrUInt32_t(uint32_t& x, uint32_t incr)
{
    const static uint32_t MAXUINT32 = std::numeric_limits<uint32_t>::max();
 
    if(MAXUINT32 - incr >= x)
    {
        x += incr;
        return true;
    }
 
    return false;
}

#define RAND_MAX24 16777216
uint32_t rand24b()
{
	return ((rand() << 12) ^ ((rand()) & (0xFFFFFF)) );
}

float box_muller(float m, float s)
{
	// normal random variate generator
	// mean m, standard deviation s

	float x1, x2, w, y1;
	static float y2;
	static int use_last = 0;

	if(use_last)			// use value from previous call
	{
		y1 = y2;
		use_last = 0;
	}
	else
	{
		do {
			double r1 = (((float)(rand()) / RAND_MAX));
			double r2 = (((float)(rand()) / RAND_MAX));

			x1 = 2.0 * r1 - 1.0;
			x2 = 2.0 * r2 - 1.0;
			w = x1 * x1 + x2 * x2;
		} while ( w >= 1.0 );

		w = sqrt( (-2.0 * log( w ) ) / w );
		y1 = x1 * w;
		y2 = x2 * w;
		use_last = 1;
	}

	return( m + y1 * s );
}

int random_range(int lowest_number, int highest_number, DistributionType_t type /*= DISTRO_UNIFORM*/, float deviation /*= 0.25*/)
{
	if(highest_number == lowest_number){
		return lowest_number;
	}

	if(lowest_number > highest_number){
		int nTmp = highest_number;
		highest_number = lowest_number;
		lowest_number = nTmp;
	}

	int range = highest_number - lowest_number;

	if(type == DISTRO_UNIFORM){
		int r = rand24b() % (range + 1);
		return lowest_number + r;
	}
	else if(type == DISTRO_NORMAL){
		float value = box_muller(0.5, deviation);

		if(value < 0){
			value = 0;
		}else if(value > 1){
			value = 1;
		}

		return lowest_number + (int)((float)range * value);
	}
	else{
		float r = 1.f - sqrt((1.f*rand24b())/RAND_MAX24);
		return lowest_number + (int)((float)range * r);
	}
}

// dump a part of the memory to stderr.
void hexdump(unsigned char *_data, int _len) {
	int i;
	for(; _len > 0; _data += 16, _len -= 16) {
		for (i = 0; i < 16 && i < _len; ++i)
			fprintf(stderr, "%02x ", _data[i]);
		for(; i < 16; ++i)
			fprintf(stderr, "   ");

		fprintf(stderr, " ");
		for(i = 0; i < 16 && i < _len; ++i)
			fprintf(stderr, "%c", (_data[i] & 0x70) < 32 ? '·' : _data[i]);

		fprintf(stderr, "\n");
	}
}

// Upcase a char.
char upchar(char c)
{
	if((c >= 97 && c <= 122) || (c <= -1 && c >= -32)){
		return c-32;
	}
	return c;
}

std::string urlEncode(const std::string& str)
{
	return urlEncode(str.c_str());
}

std::string urlEncode(const char* str)
{
	std::string out;
	const char* it;
	for(it = str; *it != 0; ++it){
		char ch = *it;
		if(!(ch >= '0' && ch <= '9') &&
			!(ch >= 'A' && ch <= 'Z') &&
			!(ch >= 'a' && ch <= 'z')){
				char tmp[4];
				sprintf(tmp, "%%%02X", ch);
				out = out + tmp;
			}
		else{
			out = out + *it;
		}
	}
	return out;
}

bool passwordTest(std::string plain, std::string &hash)
{
	// Salt it beforehand
	plain += g_config.getString(ConfigManager::PASSWORD_SALT);

	switch(g_config.getNumber(ConfigManager::PASSWORD_TYPE)){
	case PASSWORD_TYPE_PLAIN:
	{
		if(plain == hash){
			return true;
		}
		break;
	}
	case PASSWORD_TYPE_MD5:
	{
		MD5_CTX m_md5;
		std::stringstream hexStream;

		MD5Init(&m_md5, 0);
		MD5Update(&m_md5, (const unsigned char*)plain.c_str(), plain.length());
		MD5Final(&m_md5);

		hexStream.flags(std::ios::hex | std::ios::uppercase);
		for(uint32_t i = 0; i < 16; ++i){
			hexStream << std::setw(2) << std::setfill('0') << (uint32_t)m_md5.digest[i];
		}

		std::transform(hash.begin(), hash.end(), hash.begin(), upchar);
		if(hexStream.str() == hash){
			return true;
		}
		break;
	}
	case PASSWORD_TYPE_SHA1:
	{
		SHA1 sha1;
		unsigned sha1Hash[5];
		std::stringstream hexStream;

		sha1.Input((const unsigned char*)plain.c_str(), plain.length());
		sha1.Result(sha1Hash);

		hexStream.flags(std::ios::hex | std::ios::uppercase);
		for(uint32_t i = 0; i < 5; ++i){
			hexStream << std::setw(8) << std::setfill('0') << (uint32_t)sha1Hash[i];
		}

		std::transform(hash.begin(), hash.end(), hash.begin(), upchar);
		if(hexStream.str() == hash){
			return true;
		}

		break;
	}
	}
	return false;
}

std::string convertIPToString(uint32_t ip)
{
	char buffer[20];
	sprintf(buffer, "%d.%d.%d.%d", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24));
	return buffer;
}

//buffer should have at least 21 bytes
void formatDate(time_t time, char* buffer)
{
	const tm* tms = localtime(&time);
	if(tms){
		sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d", tms->tm_mday, tms->tm_mon + 1, tms->tm_year + 1900,
			tms->tm_hour, tms->tm_min, tms->tm_sec);
	}
	else{
		sprintf(buffer, "UNIX Time : %d", (int)time);
	}
}

//buffer should have at least 16 bytes
void formatDateShort(time_t time, char* buffer)
{
	const tm* tms = localtime(&time);
	if(tms){
		strftime(buffer, 12, "%d %b %Y", tms);
	}
	else{
		sprintf(buffer, "UNIX Time : %d", (int)time);
	}
}

struct MagicEffectNames{
	const char* name;
	MagicEffectClasses effect;
};

struct ShootTypeNames{
	const char* name;
	ShootType_t shoot;
};

struct AmmoTypeNames{
	const char* name;
	Ammo_t ammoType;
};

struct AmmoActionNames{
	const char* name;
	AmmoAction_t ammoAction;
};

MagicEffectNames magicEffectNames[] = {
	{"redspark",          NM_ME_DRAW_BLOOD},
	{"bluebubble",        NM_ME_LOSE_ENERGY},
	{"poff",              NM_ME_PUFF},
	{"yellowspark",       NM_ME_BLOCKHIT},
	{"explosionarea",     NM_ME_EXPLOSION_AREA},
	{"explosion",         NM_ME_EXPLOSION_DAMAGE},
	{"firearea",          NM_ME_FIRE_AREA},
	{"yellowbubble",      NM_ME_YELLOW_RINGS},
	{"greenbubble",       NM_ME_POISON_RINGS},
	{"blackspark",        NM_ME_HIT_AREA},
	{"teleport",          NM_ME_TELEPORT},
	{"energy",            NM_ME_ENERGY_DAMAGE},
	{"energyarea",		  NM_ME_ENERGY_DAMAGE },
	{"blueshimmer",       NM_ME_MAGIC_ENERGY},
	{"redshimmer",        NM_ME_MAGIC_BLOOD},
	{"greenshimmer",      NM_ME_MAGIC_POISON},
	{"fire",              NM_ME_HITBY_FIRE},
	{"earth",             NM_ME_POISON},
	{"greenspark",        NM_ME_POISON},
	{"mortarea",          NM_ME_MORT_AREA},
	{"suddendeath",		  NM_ME_MORT_AREA },
	{"greennote",         NM_ME_SOUND_GREEN},
	{"rednote",           NM_ME_SOUND_RED},
	{"poison",            NM_ME_POISON_AREA},
	{"yellownote",        NM_ME_SOUND_YELLOW},
	{"purplenote",        NM_ME_SOUND_PURPLE},
	{"bluenote",          NM_ME_SOUND_BLUE},
	{"whitenote",         NM_ME_SOUND_WHITE}
};

ShootTypeNames shootTypeNames[] = {
	{"spear",             NM_SHOOT_SPEAR},
	{"bolt",              NM_SHOOT_BOLT},
	{"arrow",             NM_SHOOT_ARROW},
	{"fire",              NM_SHOOT_FIRE},
	{"energy",            NM_SHOOT_ENERGY},
	{"poisonarrow",       NM_SHOOT_POISONARROW},
	{"burstarrow",        NM_SHOOT_BURSTARROW},
	{"throwingstar",      NM_SHOOT_THROWINGSTAR},
	{"throwingknife",     NM_SHOOT_THROWINGKNIFE},
	{"smallstone",        NM_SHOOT_SMALLSTONE},
	{"death",             NM_SHOOT_DEATH},
	{"suddendeath",		  NM_SHOOT_DEATH },
	{"largerock",         NM_SHOOT_LARGEROCK},
	{"snowball",          NM_SHOOT_SNOWBALL},
	{"powerbolt",         NM_SHOOT_POWERBOLT},
	{"poison",            NM_SHOOT_POISONFIELD}
};

AmmoTypeNames ammoTypeNames[] = {
	{"spear",          AMMO_SPEAR},
	{"bolt",           AMMO_BOLT},
	{"arrow",          AMMO_ARROW},
	{"poisonarrow",    AMMO_ARROW},
	{"burstarrow",     AMMO_ARROW},
	{"throwingstar",   AMMO_THROWINGSTAR},
	{"throwingknife",  AMMO_THROWINGKNIFE},
	{"smallstone",     AMMO_STONE},
	{"snowball",       AMMO_SNOWBALL},
	{"powerbolt",      AMMO_BOLT}
};

AmmoActionNames ammoActionNames[] = {
	{"move",          AMMOACTION_MOVE},
	{"moveback",      AMMOACTION_MOVEBACK},
	{"removecharge",  AMMOACTION_REMOVECHARGE},
	{"removecount",   AMMOACTION_REMOVECOUNT}
};

MagicEffectClasses getMagicEffect(const std::string& strValue)
{
	for(uint32_t i = 0; i < sizeof(magicEffectNames)/sizeof(MagicEffectNames); ++i){
		if(boost::algorithm::iequals(strValue.c_str(), magicEffectNames[i].name)){
			return magicEffectNames[i].effect;
		}
	}
	return NM_ME_UNK;
}

ShootType_t getShootType(const std::string& strValue)
{
	for(uint32_t i = 0; i < sizeof(shootTypeNames)/sizeof(ShootTypeNames); ++i){
		if(boost::algorithm::iequals(strValue.c_str(), shootTypeNames[i].name)){
			return shootTypeNames[i].shoot;
		}
	}
	return NM_SHOOT_UNK;
}

Ammo_t getAmmoType(const std::string& strValue)
{
	for(uint32_t i = 0; i < sizeof(ammoTypeNames)/sizeof(AmmoTypeNames); ++i){
		if(boost::algorithm::iequals(strValue.c_str(), ammoTypeNames[i].name)){
			return ammoTypeNames[i].ammoType;
		}
	}
	return AMMO_NONE;
}

AmmoAction_t getAmmoAction(const std::string& strValue)
{
	for(uint32_t i = 0; i < sizeof(ammoActionNames)/sizeof(AmmoActionNames); ++i){
		if(boost::algorithm::iequals(strValue.c_str(), ammoActionNames[i].name)){
			return ammoActionNames[i].ammoAction;
		}
	}
	return AMMOACTION_NONE;
}

std::string getViolationReasonString(int32_t reasonId)
{
	std::string banReason = "Unknown Ban Reason";

	switch(reasonId)
	{
	case 0:
		banReason = "offensive name";
		break;
	case 1:
		banReason = "name containing part of sentence";
		break;
	case 2:
		banReason = "name with nonsensical letter combo";
		break;
	case 3:
		banReason = "invalid name format";
		break;
	case 4:
		banReason = "name not describing person";
		break;
	case 5:
		banReason = "name of celebrity";
		break;
	case 6:
		banReason = "name referring to country";
		break;
	case 7:
		banReason = "namefaking player identity";
		break;
	case 8:
		banReason = "namefaking official position";
		break;
	case 9:
		banReason = "offensive statement";
		break;
	case 10:
		banReason = "spamming";
		break;
	case 11:
		banReason = "advertisement not related to game";
		break;
	case 12:
		banReason = "real money advertisement";
		break;
	case 13:
		banReason = "Non-English public statement";
		break;
	case 14:
		banReason = "off-topic public statement";
		break;
	case 15:
		banReason = "inciting rule violation";
		break;
	case 16:
		banReason = "bug abuse";
		break;
	case 17:
		banReason = "game weakness abuse";
		break;
	case 18:
		banReason = "using macro";
		break;
	case 19:
		banReason = "using unofficial software to play";
		break;
	case 20:
		banReason = "hacking";
		break;
	case 21:
		banReason = "multi-clienting";
		break;
	case 22:
		banReason = "account trading";
		break;
	case 23:
		banReason = "account sharing";
		break;
	case 24:
		banReason = "threatening gamemaster";
		break;
	case 25:
		banReason = "pretending to have official position";
		break;
	case 26:
		banReason = "pretending to have influence on gamemaster";
		break;
	case 27:
		banReason = "false report to gamemaster";
		break;
	case 28:
		banReason = "excessive unjustifed player killing";
		break;
	case 29:
		banReason = "destructive behaviour";
		break;
	case 30:
		banReason = "spoiling auction";
		break;
	case 31:
		banReason = "invalid payment";
		break;
	default:
		banReason = "nothing";
		break;
	}

	return banReason;
}

std::string getViolationActionString(violationAction_t actionId, bool ipBanishment)
{
	std::string action;
	switch(actionId)
	{
		case ACTION_NOTATION:
			action = "Notation";
			break;
		case ACTION_NAMEREPORT:
			action = "Name Report";
			break;
		case ACTION_BANREPORT:
			action = "Name Report + Banishment";
			break;
		case ACTION_BANFINAL:
			action = "Banishment + Final Warning";
			break;
		case ACTION_BANREPORTFINAL:
			action = "Name Report + Banishment + Final Warning";
			break;
		case ACTION_STATEMENT:
			action = "Statement Report";
			break;
		case ACTION_DELETION:
			action = "Deletion";
			break;
		case ACTION_BANISHMENT:
		default:
			action = "Banishment";
			break;
	}

	if(ipBanishment)
		action += " + IP Banishment";

	return action;
}

std::string playerSexAdjectiveString(PlayerSex_t sex)
{
	if(sex % 2 == 0){
		return "her";
	}
	else{
		return "his";
	}
}

std::string playerSexSubjectString(PlayerSex_t sex)
{
	if(sex % 2 == 0){
		return "She";
	}
	else{
		return "He";
	}
}

#define MOD_ADLER 65521
uint32_t adlerChecksum(uint8_t *data, int32_t len)
{
	if(len > NETWORKMESSAGE_MAXSIZE)
		return 0;
		
	if(len < 0){
		std::cout << "[Error] adlerChecksum. len < 0" << std::endl;
		return 0;
	}

	uint32_t a = 1, b = 0;
	while (len > 0)
	{
		size_t tlen = len > 5552 ? 5552 : len;
		len -= tlen;
		do
		{
			a += *data++;
			b += a;
		} while (--tlen);

		a %= MOD_ADLER;
		b %= MOD_ADLER;
		}

	return (b << 16) | a;
}

void showTime(std::stringstream& str, uint32_t time)
{
	if(time == 0xFFFFFFFF){
		str << "permanent";
	}
	else if(time == 0){
		str << "serversave";
	}
	else{
		char buffer[32];
		formatDate((time_t)time, buffer);
		str << buffer;
	}
}

uint32_t parseTime(const std::string& time)
{
	if(time == "serversave" || time == "shutdown"){
		return 0;
	}
	if(time == "permanent"){
		return 0xFFFFFFFF;
	}
	else{
		boost::char_separator<char> sep("+");
		tokenizer timetoken(time, sep);
		tokenizer::iterator timeit = timetoken.begin();
		if(timeit == timetoken.end()){
			return 0;
		}
		uint32_t number = atoi(timeit->c_str());
		uint32_t multiplier = 0;
		++timeit;
		if(timeit == timetoken.end()){
			return 0;
		}
		if(*timeit == "m") //minute
			multiplier = 60;
		if(*timeit == "h") //hour
			multiplier = 60*60;
		if(*timeit == "d") //day
			multiplier = 60*60*24;
		if(*timeit == "w") //week
			multiplier = 60*60*24*7;
		if(*timeit == "o") //month
			multiplier = 60*60*24*30;
		if(*timeit == "y") //year
			multiplier = 60*60*24*365;

		uint32_t currentTime = std::time(NULL);
		return currentTime + number*multiplier;
	}
}

std::string parseParams(tokenizer::iterator &it, tokenizer::iterator end)
{
	std::string tmp;
	if(it == end){
		return "";
	}
	else{
		tmp = *it;
		++it;
		if(tmp[0] == '"'){
			tmp.erase(0,1);
			while(it != end && tmp[tmp.length() - 1] != '"'){
				tmp += " " + *it;
				++it;
			}

			if(tmp.length() > 0 && tmp[tmp.length() - 1] == '"'){
				tmp.erase(tmp.length() - 1);
			}
		}
		return tmp;
	}
}

