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

#ifndef __OTSERV_PROTOCOL_H__
#define __OTSERV_PROTOCOL_H__

#include "definitions.h"
#include <cstdio>
#include <cstring>
#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>

class NetworkMessage;
class OutputMessage;
class Connection;
typedef boost::shared_ptr<OutputMessage> OutputMessage_ptr;
typedef boost::shared_ptr<Connection> Connection_ptr;
class RSA;

// Beware! This server is optimized to work with 7.72 protocol only
// Working on higher / lower versions will require work in the game protocol
#define CLIENT_VERSION_MIN 772
#define CLIENT_VERSION_MAX 772

#define STRING_CLIENT_VERSION "This server requires client version " OTSERV_CLIENT_VERSION "."

class Protocol : boost::noncopyable
{
public:
	Protocol(Connection_ptr connection)
	{
		m_connection = connection;
		m_encryptionEnabled = false;
		m_rawMessages = false;
		m_key[0] = 0; m_key[1] = 0; m_key[2] = 0; m_key[3] = 0;
		m_refCount = 0;
	}

	virtual ~Protocol() {}

	virtual void parsePacket(NetworkMessage& msg){};

	void onSendMessage(OutputMessage_ptr msg);
	void onRecvMessage(NetworkMessage& msg);
	virtual void onRecvFirstMessage(NetworkMessage& msg) = 0;
	virtual void onConnect() {} // Used by new gameworld to send first packet to client

	Connection_ptr getConnection() { return m_connection;}
	const Connection_ptr getConnection() const { return m_connection;}
	void setConnection(Connection_ptr connection) { m_connection = connection; }

	uint32_t getIP() const;
	int32_t addRef() {return ++m_refCount;}
	int32_t unRef() {return --m_refCount;}

protected:
	//Use this function for autosend messages only
	OutputMessage_ptr getOutputBuffer();

	void enableXTEAEncryption() { m_encryptionEnabled = true; }
	void disableXTEAEncryption() { m_encryptionEnabled = false; }
	void setXTEAKey(const uint32_t* key){
		memcpy(&m_key, key, sizeof(uint32_t)*4);
	}

	void XTEA_encrypt(OutputMessage& msg);
	bool XTEA_decrypt(NetworkMessage& msg);
	bool RSA_decrypt(NetworkMessage& msg);
	bool RSA_decrypt(RSA* rsa, NetworkMessage& msg);

	void setRawMessages(bool value) { m_rawMessages = value; }

	virtual void releaseProtocol();
	virtual void deleteProtocolTask();
	friend class Connection;
private:

	OutputMessage_ptr m_outputBuffer;
	Connection_ptr m_connection;
	bool m_encryptionEnabled;
	bool m_rawMessages;
	uint32_t m_key[4];
	uint32_t m_refCount;
};

#endif
