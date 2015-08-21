/*
* Copyright (C) 2008-2015 TrinityCore <http://www.trinitycore.org/>
* Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __AUTHSESSION_H__
#define __AUTHSESSION_H__

#include "Common.h"
#include "ByteBuffer.h"
#include "Socket.h"
#include "BigNumber.h"
#include <memory>
#include <boost/asio/ip/tcp.hpp>
#include <boost/thread.hpp>

#include <openssl/md5.h>

using boost::asio::ip::tcp;

struct AuthHandler;

class AuthSession;

// clientpatching

typedef struct PATCH_INFO
{
	int build;
	int locale;
	uint64 filesize;
	uint8 md5[MD5_DIGEST_LENGTH];
} PATCH_INFO;

class Patcher
{
	typedef std::vector<PATCH_INFO> Patches;
public:
	void Initialize();

	void LoadPatchMD5(const char*, char*);
	bool GetHash(char * pat, uint8 mymd5[16]);

	bool InitPatching(int _build, std::string _locale, AuthSession* _session);
	bool PossiblePatching(int _build, std::string _locale);

private:
	PATCH_INFO* getPatchInfo(int _build, std::string _locale, bool* fallback);

	void LoadPatchesInfo();
	Patches _patches;
	std::string m_dataDir;
};

// Launch a thread to transfer a patch to the client
class PatcherRunnable
{
public:
	PatcherRunnable(AuthSession* session, uint64 pos, uint64 size);
	void run();
	void stop();

private:
	AuthSession* _session;
	uint64 pos;
	uint64 size;
	bool stopped;
};

class AuthSession : public Socket<AuthSession>
{
public:
	static std::unordered_map<uint8, AuthHandler> InitHandlers();

	AuthSession(tcp::socket&& socket) : Socket(std::move(socket)),
		_isAuthenticated(false), _build(0), _expversion(0), _accountSecurityLevel(SEC_PLAYER), _patcher(NULL), patch(NULL)
	{
		N.SetHexStr("894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7");
		g.SetDword(7);
	}
	~AuthSession();

	void Start() override
	{
		AsyncRead();
	}

	void SendPacket(ByteBuffer& packet);

	FILE* patch;
	PatcherRunnable *_patcher;

protected:
	void ReadHandler() override;

private:
	bool HandleLogonChallenge();
	bool HandleLogonProof();
	bool HandleReconnectChallenge();
	bool HandleReconnectProof();
	bool HandleRealmList();

	//data transfer handle for patch
	bool HandleXferResume();
	bool HandleXferCancel();
	bool HandleXferAccept();

	void SetVSFields(const std::string& rI);

	BigNumber N, s, g, v;
	BigNumber b, B;
	BigNumber K;
	BigNumber _reconnectProof;

	bool _isAuthenticated;
	std::string _tokenKey;
	std::string _login;
	std::string _localizationName;
	std::string _os;
	uint16 _build;
	uint8 _expversion;

	AccountTypes _accountSecurityLevel;
};

#pragma pack(push, 1)

struct AuthHandler
{
	uint32 status;
	size_t packetSize;
	bool (AuthSession::*handler)();
};

#pragma pack(pop)

#endif