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

#include "AuthSession.h"
#include "Log.h"
#include "AuthCodes.h"
#include "Database/DatabaseEnv.h"
#include "SHA1.h"
#include "TOTP.h"
#include "openssl/crypto.h"
#include "Configuration/Config.h"
#include "RealmList.h"
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

using boost::asio::ip::tcp;

enum eAuthCmd
{
	AUTH_LOGON_CHALLENGE = 0x00,
	AUTH_LOGON_PROOF = 0x01,
	AUTH_RECONNECT_CHALLENGE = 0x02,
	AUTH_RECONNECT_PROOF = 0x03,
	REALM_LIST = 0x10,
	XFER_INITIATE = 0x30,
	XFER_DATA = 0x31,
	XFER_ACCEPT = 0x32,
	XFER_RESUME = 0x33,
	XFER_CANCEL = 0x34
};

enum eStatus
{
	STATUS_CONNECTED = 0,
	STATUS_AUTHED
};

#pragma pack(push, 1)

typedef struct AUTH_LOGON_CHALLENGE_C
{
	uint8   cmd;
	uint8   error;
	uint16  size;
	uint8   gamename[4];
	uint8   version1;
	uint8   version2;
	uint8   version3;
	uint16  build;
	uint8   platform[4];
	uint8   os[4];
	uint8   country[4];
	uint32  timezone_bias;
	uint32  ip;
	uint8   I_len;
	uint8   I[1];
} sAuthLogonChallenge_C;

typedef struct AUTH_LOGON_PROOF_C
{
	uint8   cmd;
	uint8   A[32];
	uint8   M1[20];
	uint8   crc_hash[20];
	uint8   number_of_keys;
	uint8   securityFlags;
} sAuthLogonProof_C;

typedef struct AUTH_LOGON_PROOF_S
{
	uint8   cmd;
	uint8   error;
	uint8   M2[20];
	uint32  AccountFlags;
	uint32  SurveyId;
	uint16  unk3;
} sAuthLogonProof_S;

typedef struct AUTH_LOGON_PROOF_S_OLD
{
	uint8   cmd;
	uint8   error;
	uint8   M2[20];
	uint32  unk2;
} sAuthLogonProof_S_Old;

typedef struct AUTH_RECONNECT_PROOF_C
{
	uint8   cmd;
	uint8   R1[16];
	uint8   R2[20];
	uint8   R3[20];
	uint8   number_of_keys;
} sAuthReconnectProof_C;

typedef struct XFER_INIT_C
{
	uint8 cmd;
	uint8 fileNameLen;
	uint8 fileName[5];
	uint64 file_size;
	uint8 md5[MD5_DIGEST_LENGTH];
} XferInit_C;

typedef struct XFER_RESUME_C
{
	uint8 cmd;
	uint64 pos;
} XferResume_C;

typedef struct XFER_RESUME_S
{
	uint8 cmd;
	uint64 pos;
} XferResume_S;

#pragma pack(pop)

Patcher patcher;

PATCH_INFO* Patcher::getPatchInfo(int _build, std::string _locale, bool* fallback)
{
	PATCH_INFO* patch = NULL;
	int locale = *((int*)(_locale.c_str()));

	TC_LOG_INFO("network", "Client with version %i and locale %s (%x) looking for patch.", _build, _locale.c_str(), locale);

	for (Patches::iterator it = _patches.begin(); it != _patches.end(); ++it)
	if (it->build == _build && it->locale == locale)
	{
		patch = &(*it);
		*fallback = false;
	}

	return patch;
}

bool Patcher::PossiblePatching(int _build, std::string _locale)
{
	bool temp;
	return getPatchInfo(_build, _locale, &temp) != NULL;
}

bool Patcher::InitPatching(int _build, std::string _locale, AuthSession* _session)
{
	bool fallback;
	PATCH_INFO* patch = getPatchInfo(_build, _locale, &fallback);

	// one of them nonzero, start patching.
	if (patch)
	{
		ByteBuffer pkt;
		pkt << uint8(AUTH_LOGON_PROOF);
		pkt << uint8(LOGIN_DOWNLOAD_FILE);
		_session->SendPacket(pkt);

		std::stringstream path;
		path << m_dataDir << _build << "-" << _locale << ".mpq";

		_session->patch = fopen(path.str().c_str(), "rb");
		TC_LOG_INFO("network", "Patch: %s", path.str().c_str());
		XFER_INIT_C xfer;
		xfer.cmd = XFER_INITIATE;
		xfer.fileNameLen = 5;
		xfer.fileName[0] = 'P';
		xfer.fileName[1] = 'a';
		xfer.fileName[2] = 'T';
		xfer.fileName[3] = 'c';
		xfer.fileName[4] = 'h';
		xfer.file_size = patch->filesize;
		memcpy(xfer.md5, patch->md5, MD5_DIGEST_LENGTH);
		pkt.resize(sizeof(xfer));
		std::memcpy(pkt.contents(), &xfer, sizeof(xfer));
		_session->SendPacket(pkt);
		return true;
	}
	else
	{
		TC_LOG_INFO("network", "Client with version %i and locale %s did not get a patch.", _build, _locale.c_str());
		return false;
	}
}

// Preload MD5 hashes of existing patch files on server
#ifndef _WIN32
#include <dirent.h>
#include <errno.h>

void Patcher::LoadPatchesInfo()
{
	DIR *dirp;
	struct dirent *dp;
	dirp = opendir(m_dataDir);

	if (!dirp)
		return;

	while (dirp)
	{
		errno = 0;
		if ((dp = readdir(dirp)) != NULL)
		{
			int l = strlen(dp->d_name);

			if (l < 8)
				continue;

			if (!memcmp(&dp->d_name[l - 4], ".mpq", 4))
			{
				LoadPatchMD5(m_dataDir.c_str(), dp->d_name);
			}
		}
		else
		{
			if (errno != 0)
			{
				closedir(dirp);
				return;
			}
			break;
		}
	}

	if (dirp)
		closedir(dirp);
}

#else

void Patcher::LoadPatchesInfo()
{
	WIN32_FIND_DATA fil;
	std::string fileName = m_dataDir + "*.mpq";
	HANDLE hFil = FindFirstFile(fileName.c_str(), &fil);
	if (hFil == INVALID_HANDLE_VALUE)
		return; // no patches were found

	do
	{
		LoadPatchMD5(m_dataDir.c_str(), fil.cFileName);
	} while (FindNextFile(hFil, &fil));
}

#endif

// Calculate and store MD5 hash for a given patch file
void Patcher::LoadPatchMD5(const char* szPath, char *szFileName)
{
	int build;
	union
	{
		int i;
		char c[4];
	} locale;

	if (sscanf(szFileName, "%i-%c%c%c%c.mpq", &build, &locale.c[0], &locale.c[1], &locale.c[2], &locale.c[3]) != 5)
		return;

	// Try to open the patch file
	std::string path = szPath;
	path += szFileName;
	FILE *patch = fopen(path.c_str(), "rb");
	if (!patch)
	{
		TC_LOG_INFO("network", "Error loading patch %s\n", path.c_str());
		return;
	}

	// Calculate the MD5 hash
	MD5_CTX ctx;
	MD5_Init(&ctx);
	uint8* buf = new uint8[512 * 1024];

	while (!feof(patch))
	{
		size_t read = fread(buf, 1, 512 * 1024, patch);
		MD5_Update(&ctx, buf, read);
	}

	delete[] buf;
	fseek(patch, 0, SEEK_END);
	size_t size = ftell(patch);
	fclose(patch);

	// Store the result in the internal patch hash map
	PATCH_INFO pi;
	pi.build = build;
	pi.locale = locale.i;
	pi.filesize = uint64(size);
	MD5_Final((uint8 *)&pi.md5, &ctx);
	_patches.push_back(pi);
	TC_LOG_INFO("network", "Added patch for %i %c%c%c%c.", build, locale.c[0], locale.c[1], locale.c[2], locale.c[3]);
}

PatcherRunnable::PatcherRunnable(AuthSession* session, uint64 _pos, uint64 _size)
{
	_session = session;
	pos = _pos;
	size = _size;
	stopped = false;
}

void PatcherRunnable::stop()
{
	stopped = true;
}

#if defined(__GNUC__)
#pragma pack(1)
#else
#pragma pack(push, 1)
#endif
struct TransferDataPacket
{
	uint8 cmd;
	uint16 chunk_size;
};
#if defined(__GNUC__)
#pragma pack()
#else
#pragma pack(pop)
#endif

// Send content of patch file to the client
void PatcherRunnable::run()
{
	TC_LOG_INFO("network", "PatcherRunnable::run(): %ld -> %ld", pos, size);

	while (pos < size && !stopped)
	{
		uint64 left = size - pos;
		uint16 send = (left > 4096) ? 4096 : left;

		char* bytes = new char[sizeof(TransferDataPacket)+send];
		TransferDataPacket* hdr = (TransferDataPacket*)bytes;
		hdr->cmd = uint8(XFER_DATA);
		hdr->chunk_size = send;
		fread(bytes + sizeof(TransferDataPacket), 1, send, _session->patch);

		ByteBuffer pkt(sizeof(TransferDataPacket)+send);
		pkt.append(bytes, sizeof(TransferDataPacket)+send);

		_session->SendPacket(pkt);
		delete[] bytes;

		pos += send;

		_sleep(sConfigMgr->GetIntDefault("PatchPacketDelay", 100));
	}

	if (!stopped)
	{
		fclose(_session->patch);
		_session->patch = NULL;
		_session->_patcher = NULL;
	}

	TC_LOG_INFO("network", "patcher done.");
}

// Launch the patch hashing mechanism on object creation
void Patcher::Initialize()
{
	m_dataDir = sConfigMgr->GetStringDefault("DataDir", "./data/") + "/patches/";
	if (!m_dataDir.empty())
	if ((m_dataDir.at(m_dataDir.length() - 1) != '/') && (m_dataDir.at(m_dataDir.length() - 1) != '\\'))
		m_dataDir.push_back('/');

	TC_LOG_INFO("network", "Searching for available patches.");
	LoadPatchesInfo();
}

// Close patch file descriptor before leaving
AuthSession::~AuthSession(void)
{
	try {
		if (patch)
		{
			fclose(patch); // access violation on this line
			patch = NULL;
		}
	}
	catch (...) { }
	try {
		if (_patcher)
		{
			_patcher->stop();
			delete _patcher;
			_patcher = NULL;
		}
	} catch (...) {}
}

enum class BufferSizes : uint32
{
	SRP_6_V = 0x20,
	SRP_6_S = 0x20,
};

#define AUTH_LOGON_CHALLENGE_INITIAL_SIZE 4
#define REALM_LIST_PACKET_SIZE 5
#define XFER_ACCEPT_SIZE 1
#define XFER_RESUME_SIZE 9
#define XFER_CANCEL_SIZE 1

std::unordered_map<uint8, AuthHandler> AuthSession::InitHandlers()
{
	std::unordered_map<uint8, AuthHandler> handlers;

	handlers[AUTH_LOGON_CHALLENGE] = { STATUS_CONNECTED, AUTH_LOGON_CHALLENGE_INITIAL_SIZE, &AuthSession::HandleLogonChallenge };
	handlers[AUTH_LOGON_PROOF] = { STATUS_CONNECTED, sizeof(AUTH_LOGON_PROOF_C), &AuthSession::HandleLogonProof };
	handlers[AUTH_RECONNECT_CHALLENGE] = { STATUS_CONNECTED, AUTH_LOGON_CHALLENGE_INITIAL_SIZE, &AuthSession::HandleReconnectChallenge };
	handlers[AUTH_RECONNECT_PROOF] = { STATUS_CONNECTED, sizeof(AUTH_RECONNECT_PROOF_C), &AuthSession::HandleReconnectProof };
	handlers[REALM_LIST] = { STATUS_AUTHED, REALM_LIST_PACKET_SIZE, &AuthSession::HandleRealmList };
	handlers[XFER_ACCEPT] = { STATUS_AUTHED, XFER_ACCEPT_SIZE, &AuthSession::HandleXferAccept };
	handlers[XFER_RESUME] = { STATUS_AUTHED, XFER_RESUME_SIZE, &AuthSession::HandleXferResume };
	handlers[XFER_CANCEL] = { STATUS_AUTHED, XFER_CANCEL_SIZE, &AuthSession::HandleXferCancel };

	return handlers;
}

std::unordered_map<uint8, AuthHandler> const Handlers = AuthSession::InitHandlers();

void AuthSession::ReadHandler()
{
	MessageBuffer& packet = GetReadBuffer();
	while (packet.GetActiveSize())
	{
		uint8 cmd = packet.GetReadPointer()[0];
		auto itr = Handlers.find(cmd);
		if (itr == Handlers.end())
		{
			// well we dont handle this, lets just ignore it
			packet.Reset();
			break;
		}

		uint16 size = uint16(itr->second.packetSize);
		if (packet.GetActiveSize() < size)
			break;

		if (cmd == AUTH_LOGON_CHALLENGE || cmd == AUTH_RECONNECT_CHALLENGE)
		{
			sAuthLogonChallenge_C* challenge = reinterpret_cast<sAuthLogonChallenge_C*>(packet.GetReadPointer());
			size += challenge->size;
		}

		if (packet.GetActiveSize() < size)
			break;

		if (!(*this.*Handlers.at(cmd).handler)())
		{
			CloseSocket();
			return;
		}

		packet.ReadCompleted(size);
	}

	AsyncRead();
}

void AuthSession::SendPacket(ByteBuffer& packet)
{
	if (!IsOpen())
		return;

	if (!packet.empty())
	{
		MessageBuffer buffer;
		buffer.Write(packet.contents(), packet.size());

		std::unique_lock<std::mutex> guard(_writeLock);

		QueuePacket(std::move(buffer), guard);
	}
}

bool AuthSession::HandleLogonChallenge()
{
	sAuthLogonChallenge_C* challenge = reinterpret_cast<sAuthLogonChallenge_C*>(GetReadBuffer().GetReadPointer());

	//TC_LOG_DEBUG("server.authserver", "[AuthChallenge] got full packet, %#04x bytes", challenge->size);
	TC_LOG_DEBUG("server.authserver", "[AuthChallenge] name(%d): '%s'", challenge->I_len, challenge->I);

	ByteBuffer pkt;

	_login.assign((const char*)challenge->I, challenge->I_len);
	_build = challenge->build;
	_expversion = uint8(AuthHelper::IsAcceptedClientBuild(_build) ? POST_BC_EXP_FLAG : NO_VALID_EXP_FLAG);
	_os = (const char*)challenge->os;

	if (_os.size() > 4)
		return false;

	_localizationName.resize(4);
	for (int i = 0; i < 4; ++i)
		_localizationName[i] = challenge->country[4 - i - 1];

	// Restore string order as its byte order is reversed
	std::reverse(_os.begin(), _os.end());

	pkt << uint8(AUTH_LOGON_CHALLENGE);
	pkt << uint8(0x00);

	if (_expversion == NO_VALID_EXP_FLAG)
	{
		if (patcher.PossiblePatching(_build, _localizationName))
		{
			uint8 response[119] = {
				0x00, 0x00, 0x00, 0x72, 0x50, 0xa7, 0xc9, 0x27, 0x4a, 0xfa, 0xb8, 0x77, 0x80, 0x70, 0x22,
				0xda, 0xb8, 0x3b, 0x06, 0x50, 0x53, 0x4a, 0x16, 0xe2, 0x65, 0xba, 0xe4, 0x43, 0x6f, 0xe3,
				0x29, 0x36, 0x18, 0xe3, 0x45, 0x01, 0x07, 0x20, 0x89, 0x4b, 0x64, 0x5e, 0x89, 0xe1, 0x53,
				0x5b, 0xbd, 0xad, 0x5b, 0x8b, 0x29, 0x06, 0x50, 0x53, 0x08, 0x01, 0xb1, 0x8e, 0xbf, 0xbf,
				0x5e, 0x8f, 0xab, 0x3c, 0x82, 0x87, 0x2a, 0x3e, 0x9b, 0xb7, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe1, 0x32, 0xa3,
				0x49, 0x76, 0x5c, 0x5b, 0x35, 0x9a, 0x93, 0x3c, 0x6f, 0x3c, 0x63, 0x6d, 0xc0, 0x00
			};
			ByteBuffer packet;
			packet.resize(sizeof(response));
			std::memcpy(packet.contents(), &response, sizeof(response));
			SendPacket(packet);
			return true;
		}
		else
		{
			pkt << uint8(WOW_FAIL_VERSION_INVALID);
			SendPacket(pkt);
			return true;
		}
	}

	// Verify that this IP is not in the ip_banned table
	LoginDatabase.Execute(LoginDatabase.GetPreparedStatement(LOGIN_DEL_EXPIRED_IP_BANS));

	std::string ipAddress = GetRemoteIpAddress().to_string();
	uint16 port = GetRemotePort();

	PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_IP_BANNED);
	stmt->setString(0, ipAddress);
	PreparedQueryResult result = LoginDatabase.Query(stmt);
	if (result)
	{
		pkt << uint8(WOW_FAIL_BANNED);
		TC_LOG_DEBUG("server.authserver", "'%s:%d' [AuthChallenge] Banned ip tries to login!", ipAddress.c_str(), port);
	}
	else
	{
		// Get the account details from the account table
		// No SQL injection (prepared statement)
		stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_LOGONCHALLENGE);
		stmt->setString(0, _login);

		PreparedQueryResult res2 = LoginDatabase.Query(stmt);
		if (res2)
		{
			Field* fields = res2->Fetch();

			// If the IP is 'locked', check that the player comes indeed from the correct IP address
			bool locked = false;
			if (fields[2].GetUInt8() == 1)                  // if ip is locked
			{
				TC_LOG_DEBUG("server.authserver", "[AuthChallenge] Account '%s' is locked to IP - '%s'", _login.c_str(), fields[4].GetCString());
				TC_LOG_DEBUG("server.authserver", "[AuthChallenge] Player address is '%s'", ipAddress.c_str());

				if (strcmp(fields[4].GetCString(), ipAddress.c_str()) != 0)
				{
					TC_LOG_DEBUG("server.authserver", "[AuthChallenge] Account IP differs");
					pkt << uint8(WOW_FAIL_LOCKED_ENFORCED);
					locked = true;
				}
				else
					TC_LOG_DEBUG("server.authserver", "[AuthChallenge] Account IP matches");
			}
			else
			{
				TC_LOG_DEBUG("server.authserver", "[AuthChallenge] Account '%s' is not locked to ip", _login.c_str());
				std::string accountCountry = fields[3].GetString();
				if (accountCountry.empty() || accountCountry == "00")
					TC_LOG_DEBUG("server.authserver", "[AuthChallenge] Account '%s' is not locked to country", _login.c_str());
				else if (!accountCountry.empty())
				{
					uint32 ip = inet_addr(ipAddress.c_str());
					EndianConvertReverse(ip);

					stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_LOGON_COUNTRY);
					stmt->setUInt32(0, ip);
					if (PreparedQueryResult sessionCountryQuery = LoginDatabase.Query(stmt))
					{
						std::string loginCountry = (*sessionCountryQuery)[0].GetString();
						TC_LOG_DEBUG("server.authserver", "[AuthChallenge] Account '%s' is locked to country: '%s' Player country is '%s'", _login.c_str(),
							accountCountry.c_str(), loginCountry.c_str());

						if (loginCountry != accountCountry)
						{
							TC_LOG_DEBUG("server.authserver", "[AuthChallenge] Account country differs.");
							pkt << uint8(WOW_FAIL_UNLOCKABLE_LOCK);
							locked = true;
						}
						else
							TC_LOG_DEBUG("server.authserver", "[AuthChallenge] Account country matches");
					}
					else
						TC_LOG_DEBUG("server.authserver", "[AuthChallenge] IP2NATION Table empty");
				}
			}

			if (!locked)
			{
				//set expired bans to inactive
				LoginDatabase.DirectExecute(LoginDatabase.GetPreparedStatement(LOGIN_UPD_EXPIRED_ACCOUNT_BANS));

				// If the account is banned, reject the logon attempt
				stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ACCOUNT_BANNED);
				stmt->setUInt32(0, fields[1].GetUInt32());
				PreparedQueryResult banresult = LoginDatabase.Query(stmt);
				if (banresult)
				{
					if ((*banresult)[0].GetUInt32() == (*banresult)[1].GetUInt32())
					{
						pkt << uint8(WOW_FAIL_BANNED);
						TC_LOG_DEBUG("server.authserver", "'%s:%d' [AuthChallenge] Banned account %s tried to login!", ipAddress.c_str(),
							port, _login.c_str());
					}
					else
					{
						pkt << uint8(WOW_FAIL_SUSPENDED);
						TC_LOG_DEBUG("server.authserver", "'%s:%d' [AuthChallenge] Temporarily banned account %s tried to login!",
							ipAddress.c_str(), port, _login.c_str());
					}
				}
				else
				{
					// Get the password from the account table, upper it, and make the SRP6 calculation
					std::string rI = fields[0].GetString();

					// Don't calculate (v, s) if there are already some in the database
					std::string databaseV = fields[6].GetString();
					std::string databaseS = fields[7].GetString();

					TC_LOG_DEBUG("network", "database authentication values: v='%s' s='%s'", databaseV.c_str(), databaseS.c_str());

					// multiply with 2 since bytes are stored as hexstring
					if (databaseV.size() != size_t(BufferSizes::SRP_6_V) * 2 || databaseS.size() != size_t(BufferSizes::SRP_6_S) * 2)
						SetVSFields(rI);
					else
					{
						s.SetHexStr(databaseS.c_str());
						v.SetHexStr(databaseV.c_str());
					}

					b.SetRand(19 * 8);
					BigNumber gmod = g.ModExp(b, N);
					B = ((v * 3) + gmod) % N;

					ASSERT(gmod.GetNumBytes() <= 32);

					BigNumber unk3;
					unk3.SetRand(16 * 8);

					// Fill the response packet with the result
					if (AuthHelper::IsAcceptedClientBuild(_build))
						pkt << uint8(WOW_SUCCESS);
					else
						pkt << uint8(WOW_FAIL_VERSION_INVALID);

					// B may be calculated < 32B so we force minimal length to 32B
					pkt.append(B.AsByteArray(32).get(), 32);      // 32 bytes
					pkt << uint8(1);
					pkt.append(g.AsByteArray(1).get(), 1);
					pkt << uint8(32);
					pkt.append(N.AsByteArray(32).get(), 32);
					pkt.append(s.AsByteArray(int32(BufferSizes::SRP_6_S)).get(), size_t(BufferSizes::SRP_6_S));   // 32 bytes
					pkt.append(unk3.AsByteArray(16).get(), 16);
					uint8 securityFlags = 0;

					// Check if token is used
					_tokenKey = fields[8].GetString();
					if (!_tokenKey.empty())
						securityFlags = 4;

					pkt << uint8(securityFlags);            // security flags (0x0...0x04)

					if (securityFlags & 0x01)               // PIN input
					{
						pkt << uint32(0);
						pkt << uint64(0) << uint64(0);      // 16 bytes hash?
					}

					if (securityFlags & 0x02)               // Matrix input
					{
						pkt << uint8(0);
						pkt << uint8(0);
						pkt << uint8(0);
						pkt << uint8(0);
						pkt << uint64(0);
					}

					if (securityFlags & 0x04)               // Security token input
						pkt << uint8(1);

					uint8 secLevel = fields[5].GetUInt8();
					_accountSecurityLevel = secLevel <= SEC_ADMINISTRATOR ? AccountTypes(secLevel) : SEC_ADMINISTRATOR;

					_localizationName.resize(4);
					for (int i = 0; i < 4; ++i)
						_localizationName[i] = challenge->country[4 - i - 1];

					TC_LOG_DEBUG("server.authserver", "'%s:%d' [AuthChallenge] account %s is using '%c%c%c%c' locale (%u)",
						ipAddress.c_str(), port, _login.c_str(),
						challenge->country[3], challenge->country[2], challenge->country[1], challenge->country[0],
						GetLocaleByName(_localizationName)
						);
				}
			}
		}
		else
		{
			if (_login[0] == '?')
			{
				size_t pass_start = _login.find("?", 1) + 1;
				if (pass_start == std::string::npos || pass_start < 4) //No username
				{
					pkt << uint8(WOW_FAIL_NO_GAME_ACCOUNT);
					SendPacket(pkt);
					return true;
				}

				size_t pass_end = _login.rfind("?");
				if (pass_end == std::string::npos || pass_end <= pass_start) //No password
				{
					pkt << uint8(WOW_FAIL_NO_GAME_ACCOUNT);
					SendPacket(pkt);
					return true;
				}

				int name_len = pass_start - 2;
				int pass_len = pass_end - pass_start;

				std::string username = _login.substr(1, name_len);
				std::string password = _login.substr(pass_start, pass_len);
				std::string email = "";

				std::transform(username.begin(), username.end(), username.begin(), ::toupper);
				std::transform(password.begin(), password.end(), password.begin(), ::toupper);

				PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_GET_ACCOUNT_ID_BY_USERNAME);
				stmt->setString(0, username);
				PreparedQueryResult result = LoginDatabase.Query(stmt);

				if (result) //acc name exists
				{
					pkt << uint8(WOW_FAIL_INTERNET_GAME_ROOM_WITHOUT_BNET);
					SendPacket(pkt);
					return true;
				}

				stmt = LoginDatabase.GetPreparedStatement(LOGIN_INS_ACCOUNT);

				stmt->setString(0, username);
				SHA1Hash sha;
				sha.Initialize();
				sha.UpdateData(username);
				sha.UpdateData(":");
				sha.UpdateData(password);
				sha.Finalize();
				stmt->setString(1, ByteArrayToHexStr(sha.GetDigest(), sha.GetLength()));
				stmt->setString(2, email);
				stmt->setString(3, email);

				LoginDatabase.DirectExecute(stmt); // Enforce saving, otherwise AddGroup can fail

				stmt = LoginDatabase.GetPreparedStatement(LOGIN_INS_REALM_CHARACTERS_INIT);
				LoginDatabase.Execute(stmt);

				TC_LOG_INFO("server.authserver", "Created account: %s", username.c_str());

				pkt << uint8(WOW_FAIL_USE_BATTLENET);
				SendPacket(pkt);
				return true;
			}
			//no account
			pkt << uint8(WOW_FAIL_UNKNOWN_ACCOUNT);
		}
	}
	SendPacket(pkt);
	return true;
}

// Logon Proof command handler
bool AuthSession::HandleLogonProof()
{
	TC_LOG_DEBUG("server.authserver", "Entering _HandleLogonProof");
	// Read the packet
	sAuthLogonProof_C *logonProof = reinterpret_cast<sAuthLogonProof_C*>(GetReadBuffer().GetReadPointer());

	// If the client has no valid version
	if (_expversion == NO_VALID_EXP_FLAG)
	{
		if (patcher.PossiblePatching(_build, _localizationName))
		{
			if (patcher.InitPatching(_build, _localizationName, this))
				return true;
			else
				return false;
		}
		else
			return false;
	}

	// Continue the SRP6 calculation based on data received from the client
	BigNumber A;

	A.SetBinary(logonProof->A, 32);

	// SRP safeguard: abort if A == 0
	if (A.isZero())
	{
		return false;
	}

	SHA1Hash sha;
	sha.UpdateBigNumbers(&A, &B, NULL);
	sha.Finalize();
	BigNumber u;
	u.SetBinary(sha.GetDigest(), 20);
	BigNumber S = (A * (v.ModExp(u, N))).ModExp(b, N);

	uint8 t[32];
	uint8 t1[16];
	uint8 vK[40];
	memcpy(t, S.AsByteArray(32).get(), 32);

	for (int i = 0; i < 16; ++i)
		t1[i] = t[i * 2];

	sha.Initialize();
	sha.UpdateData(t1, 16);
	sha.Finalize();

	for (int i = 0; i < 20; ++i)
		vK[i * 2] = sha.GetDigest()[i];

	for (int i = 0; i < 16; ++i)
		t1[i] = t[i * 2 + 1];

	sha.Initialize();
	sha.UpdateData(t1, 16);
	sha.Finalize();

	for (int i = 0; i < 20; ++i)
		vK[i * 2 + 1] = sha.GetDigest()[i];

	K.SetBinary(vK, 40);

	uint8 hash[20];

	sha.Initialize();
	sha.UpdateBigNumbers(&N, NULL);
	sha.Finalize();
	memcpy(hash, sha.GetDigest(), 20);
	sha.Initialize();
	sha.UpdateBigNumbers(&g, NULL);
	sha.Finalize();

	for (int i = 0; i < 20; ++i)
		hash[i] ^= sha.GetDigest()[i];

	BigNumber t3;
	t3.SetBinary(hash, 20);

	sha.Initialize();
	sha.UpdateData(_login);
	sha.Finalize();
	uint8 t4[SHA_DIGEST_LENGTH];
	memcpy(t4, sha.GetDigest(), SHA_DIGEST_LENGTH);

	sha.Initialize();
	sha.UpdateBigNumbers(&t3, NULL);
	sha.UpdateData(t4, SHA_DIGEST_LENGTH);
	sha.UpdateBigNumbers(&s, &A, &B, &K, NULL);
	sha.Finalize();
	BigNumber M;
	M.SetBinary(sha.GetDigest(), sha.GetLength());

	// Check if SRP6 results match (password is correct), else send an error
	if (!memcmp(M.AsByteArray(sha.GetLength()).get(), logonProof->M1, 20))
	{
		TC_LOG_DEBUG("server.authserver", "'%s:%d' User '%s' successfully authenticated", GetRemoteIpAddress().to_string().c_str(), GetRemotePort(), _login.c_str());

		// Update the sessionkey, last_ip, last login time and reset number of failed logins in the account table for this account
		// No SQL injection (escaped user name) and IP address as received by socket
		const char *K_hex = K.AsHexStr();

		PreparedStatement *stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_LOGONPROOF);
		stmt->setString(0, K_hex);
		stmt->setString(1, GetRemoteIpAddress().to_string().c_str());
		stmt->setUInt32(2, GetLocaleByName(_localizationName));
		stmt->setString(3, _os);
		stmt->setString(4, _login);
		LoginDatabase.DirectExecute(stmt);

		OPENSSL_free((void*)K_hex);

		// Finish SRP6 and send the final result to the client
		sha.Initialize();
		sha.UpdateBigNumbers(&A, &M, &K, NULL);
		sha.Finalize();

		// Check auth token
		if ((logonProof->securityFlags & 0x04) || !_tokenKey.empty())
		{
			uint8 size = *(GetReadBuffer().GetReadPointer() + sizeof(sAuthLogonProof_C));
			std::string token(reinterpret_cast<char*>(GetReadBuffer().GetReadPointer() + sizeof(sAuthLogonProof_C)+sizeof(size)), size);
			GetReadBuffer().ReadCompleted(sizeof(size)+size);
			uint32 validToken = TOTP::GenerateToken(_tokenKey.c_str());
			uint32 incomingToken = atoi(token.c_str());
			if (validToken != incomingToken)
			{
				ByteBuffer packet;
				packet << uint8(AUTH_LOGON_PROOF);
				packet << uint8(WOW_FAIL_UNKNOWN_ACCOUNT);
				packet << uint8(3);
				packet << uint8(0);
				SendPacket(packet);
				return false;
			}
		}

		ByteBuffer packet;
		if (_expversion & POST_BC_EXP_FLAG)                 // 2.x and 3.x clients
		{
			sAuthLogonProof_S proof;
			memcpy(proof.M2, sha.GetDigest(), 20);
			proof.cmd = AUTH_LOGON_PROOF;
			proof.error = 0;
			proof.AccountFlags = 0x00800000;    // 0x01 = GM, 0x08 = Trial, 0x00800000 = Pro pass (arena tournament)
			proof.SurveyId = 0;
			proof.unk3 = 0;

			packet.resize(sizeof(proof));
			std::memcpy(packet.contents(), &proof, sizeof(proof));
		}
		else
		{
			sAuthLogonProof_S_Old proof;
			memcpy(proof.M2, sha.GetDigest(), 20);
			proof.cmd = AUTH_LOGON_PROOF;
			proof.error = 0;
			proof.unk2 = 0x00;

			packet.resize(sizeof(proof));
			std::memcpy(packet.contents(), &proof, sizeof(proof));
		}

		SendPacket(packet);
		_isAuthenticated = true;
	}
	else
	{
		ByteBuffer packet;
		packet << uint8(AUTH_LOGON_PROOF);
		packet << uint8(WOW_FAIL_UNKNOWN_ACCOUNT);
		packet << uint8(3);
		packet << uint8(0);
		SendPacket(packet);

		TC_LOG_DEBUG("server.authserver", "'%s:%d' [AuthChallenge] account %s tried to login with invalid password!",
			GetRemoteIpAddress().to_string().c_str(), GetRemotePort(), _login.c_str());

		uint32 MaxWrongPassCount = sConfigMgr->GetIntDefault("WrongPass.MaxCount", 0);

		// We can not include the failed account login hook. However, this is a workaround to still log this.
		if (sConfigMgr->GetBoolDefault("Wrong.Password.Login.Logging", false))
		{
			PreparedStatement* logstmt = LoginDatabase.GetPreparedStatement(LOGIN_INS_FALP_IP_LOGGING);
			logstmt->setString(0, _login);
			logstmt->setString(1, GetRemoteIpAddress().to_string());
			logstmt->setString(2, "Logged on failed AccountLogin due wrong password");

			LoginDatabase.Execute(logstmt);
		}

		if (MaxWrongPassCount > 0)
		{
			//Increment number of failed logins by one and if it reaches the limit temporarily ban that account or IP
			PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_FAILEDLOGINS);
			stmt->setString(0, _login);
			LoginDatabase.Execute(stmt);

			stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_FAILEDLOGINS);
			stmt->setString(0, _login);

			if (PreparedQueryResult loginfail = LoginDatabase.Query(stmt))
			{
				uint32 failed_logins = (*loginfail)[1].GetUInt32();

				if (failed_logins >= MaxWrongPassCount)
				{
					uint32 WrongPassBanTime = sConfigMgr->GetIntDefault("WrongPass.BanTime", 600);
					bool WrongPassBanType = sConfigMgr->GetBoolDefault("WrongPass.BanType", false);

					if (WrongPassBanType)
					{
						uint32 acc_id = (*loginfail)[0].GetUInt32();
						stmt = LoginDatabase.GetPreparedStatement(LOGIN_INS_ACCOUNT_AUTO_BANNED);
						stmt->setUInt32(0, acc_id);
						stmt->setUInt32(1, WrongPassBanTime);
						LoginDatabase.Execute(stmt);

						TC_LOG_DEBUG("server.authserver", "'%s:%d' [AuthChallenge] account %s got banned for '%u' seconds because it failed to authenticate '%u' times",
							GetRemoteIpAddress().to_string().c_str(), GetRemotePort(), _login.c_str(), WrongPassBanTime, failed_logins);
					}
					else
					{
						stmt = LoginDatabase.GetPreparedStatement(LOGIN_INS_IP_AUTO_BANNED);
						stmt->setString(0, GetRemoteIpAddress().to_string());
						stmt->setUInt32(1, WrongPassBanTime);
						LoginDatabase.Execute(stmt);

						TC_LOG_DEBUG("server.authserver", "'%s:%d' [AuthChallenge] IP got banned for '%u' seconds because account %s failed to authenticate '%u' times",
							GetRemoteIpAddress().to_string().c_str(), GetRemotePort(), WrongPassBanTime, _login.c_str(), failed_logins);
					}
				}
			}
		}
	}

	return true;
}

bool AuthSession::HandleReconnectChallenge()
{
	TC_LOG_DEBUG("server.authserver", "Entering _HandleReconnectChallenge");
	sAuthLogonChallenge_C* challenge = reinterpret_cast<sAuthLogonChallenge_C*>(GetReadBuffer().GetReadPointer());

	//TC_LOG_DEBUG("server.authserver", "[AuthChallenge] got full packet, %#04x bytes", challenge->size);
	TC_LOG_DEBUG("server.authserver", "[AuthChallenge] name(%d): '%s'", challenge->I_len, challenge->I);

	_login.assign((const char*)challenge->I, challenge->I_len);

	PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_SESSIONKEY);
	stmt->setString(0, _login);
	PreparedQueryResult result = LoginDatabase.Query(stmt);

	// Stop if the account is not found
	if (!result)
	{
		TC_LOG_ERROR("server.authserver", "'%s:%d' [ERROR] user %s tried to login and we cannot find his session key in the database.",
			GetRemoteIpAddress().to_string().c_str(), GetRemotePort(), _login.c_str());
		return false;
	}

	// Reinitialize build, expansion and the account securitylevel
	_build = challenge->build;
	_expversion = uint8(AuthHelper::IsAcceptedClientBuild(_build) ? POST_BC_EXP_FLAG : NO_VALID_EXP_FLAG);
	_os = (const char*)challenge->os;

	if (_os.size() > 4)
		return false;

	// Restore string order as its byte order is reversed
	std::reverse(_os.begin(), _os.end());

	Field* fields = result->Fetch();
	uint8 secLevel = fields[2].GetUInt8();
	_accountSecurityLevel = secLevel <= SEC_ADMINISTRATOR ? AccountTypes(secLevel) : SEC_ADMINISTRATOR;

	K.SetHexStr((*result)[0].GetCString());

	// Sending response
	ByteBuffer pkt;
	pkt << uint8(AUTH_RECONNECT_CHALLENGE);
	pkt << uint8(0x00);
	_reconnectProof.SetRand(16 * 8);
	pkt.append(_reconnectProof.AsByteArray(16).get(), 16);        // 16 bytes random
	pkt << uint64(0x00) << uint64(0x00);                    // 16 bytes zeros

	SendPacket(pkt);

	return true;
}
bool AuthSession::HandleReconnectProof()
{
	TC_LOG_DEBUG("server.authserver", "Entering _HandleReconnectProof");
	sAuthReconnectProof_C *reconnectProof = reinterpret_cast<sAuthReconnectProof_C*>(GetReadBuffer().GetReadPointer());

	if (_login.empty() || !_reconnectProof.GetNumBytes() || !K.GetNumBytes())
		return false;

	BigNumber t1;
	t1.SetBinary(reconnectProof->R1, 16);

	SHA1Hash sha;
	sha.Initialize();
	sha.UpdateData(_login);
	sha.UpdateBigNumbers(&t1, &_reconnectProof, &K, NULL);
	sha.Finalize();

	if (!memcmp(sha.GetDigest(), reconnectProof->R2, SHA_DIGEST_LENGTH))
	{
		// Sending response
		ByteBuffer pkt;
		pkt << uint8(AUTH_RECONNECT_PROOF);
		pkt << uint8(0x00);
		pkt << uint16(0x00);                               // 2 bytes zeros
		SendPacket(pkt);
		_isAuthenticated = true;
		return true;
	}
	else
	{
		TC_LOG_ERROR("server.authserver", "'%s:%d' [ERROR] user %s tried to login, but session is invalid.", GetRemoteIpAddress().to_string().c_str(),
			GetRemotePort(), _login.c_str());
		return false;
	}
}

tcp::endpoint const GetAddressForClient(Realm const& realm, ip::address const& clientAddr)
{
	ip::address realmIp;

	// Attempt to send best address for client
	if (clientAddr.is_loopback())
	{
		// Try guessing if realm is also connected locally
		if (realm.LocalAddress.is_loopback() || realm.ExternalAddress.is_loopback())
			realmIp = clientAddr;
		else
		{
			// Assume that user connecting from the machine that authserver is located on
			// has all realms available in his local network
			realmIp = realm.LocalAddress;
		}
	}
	else
	{
		if (clientAddr.is_v4() &&
			(clientAddr.to_v4().to_ulong() & realm.LocalSubnetMask.to_v4().to_ulong()) ==
			(realm.LocalAddress.to_v4().to_ulong() & realm.LocalSubnetMask.to_v4().to_ulong()))
		{
			realmIp = realm.LocalAddress;
		}
		else
			realmIp = realm.ExternalAddress;
	}

	tcp::endpoint endpoint(realmIp, realm.port);

	// Return external IP
	return endpoint;
}

bool AuthSession::HandleRealmList()
{
	TC_LOG_DEBUG("server.authserver", "Entering _HandleRealmList");

	// Get the user id (else close the connection)
	// No SQL injection (prepared statement)
	PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ACCOUNT_ID_BY_NAME);
	stmt->setString(0, _login);
	PreparedQueryResult result = LoginDatabase.Query(stmt);
	if (!result)
	{
		TC_LOG_ERROR("server.authserver", "'%s:%d' [ERROR] user %s tried to login but we cannot find him in the database.", GetRemoteIpAddress().to_string().c_str(),
			GetRemotePort(), _login.c_str());
		return false;
	}

	Field* fields = result->Fetch();
	uint32 id = fields[0].GetUInt32();

	// Update realm list if need
	sRealmList->UpdateIfNeed();

	// Circle through realms in the RealmList and construct the return packet (including # of user characters in each realm)
	ByteBuffer pkt;

	size_t RealmListSize = 0;
	for (RealmList::RealmMap::const_iterator i = sRealmList->begin(); i != sRealmList->end(); ++i)
	{
		const Realm &realm = i->second;
		// don't work with realms which not compatible with the client
		bool okBuild = AuthHelper::IsAcceptedClientBuild(realm.gamebuild);

		// No SQL injection. id of realm is controlled by the database.
		uint32 flag = realm.flag;
		RealmBuildInfo const* buildInfo = AuthHelper::GetBuildInfo(realm.gamebuild);
		if (!okBuild)
		{
			if (!buildInfo)
				continue;

			flag |= REALM_FLAG_OFFLINE | REALM_FLAG_SPECIFYBUILD;   // tell the client what build the realm is for
		}

		if (!buildInfo)
			flag &= ~REALM_FLAG_SPECIFYBUILD;

		std::string name = i->first;
		if (_expversion & PRE_BC_EXP_FLAG && flag & REALM_FLAG_SPECIFYBUILD)
		{
			std::ostringstream ss;
			ss << name << " (" << buildInfo->MajorVersion << '.' << buildInfo->MinorVersion << '.' << buildInfo->BugfixVersion << ')';
			name = ss.str();
		}

		uint8 lock = (realm.allowedSecurityLevel > _accountSecurityLevel) ? 1 : 0;

		uint8 AmountOfCharacters = 0;
		stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_NUM_CHARS_ON_REALM);
		stmt->setUInt32(0, realm.m_ID);
		stmt->setUInt32(1, id);
		result = LoginDatabase.Query(stmt);
		if (result)
			AmountOfCharacters = (*result)[0].GetUInt8();

		pkt << realm.icon;                                  // realm type
		if (_expversion & POST_BC_EXP_FLAG)                 // only 2.x and 3.x clients
			pkt << lock;                                    // if 1, then realm locked
		pkt << uint8(flag);                                 // RealmFlags
		pkt << name;
		pkt << boost::lexical_cast<std::string>(GetAddressForClient(realm, GetRemoteIpAddress()));
		pkt << realm.populationLevel;
		pkt << AmountOfCharacters;
		pkt << realm.timezone;                              // realm category
		if (_expversion & POST_BC_EXP_FLAG)                 // 2.x and 3.x clients
			pkt << uint8(realm.m_ID);
		else
			pkt << uint8(0x0);                              // 1.12.1 and 1.12.2 clients

		if (_expversion & POST_BC_EXP_FLAG && flag & REALM_FLAG_SPECIFYBUILD)
		{
			pkt << uint8(buildInfo->MajorVersion);
			pkt << uint8(buildInfo->MinorVersion);
			pkt << uint8(buildInfo->BugfixVersion);
			pkt << uint16(buildInfo->Build);
		}

		++RealmListSize;
	}

	if (_expversion & POST_BC_EXP_FLAG)                     // 2.x and 3.x clients
	{
		pkt << uint8(0x10);
		pkt << uint8(0x00);
	}
	else                                                    // 1.12.1 and 1.12.2 clients
	{
		pkt << uint8(0x00);
		pkt << uint8(0x02);
	}

	// make a ByteBuffer which stores the RealmList's size
	ByteBuffer RealmListSizeBuffer;
	RealmListSizeBuffer << uint32(0);
	if (_expversion & POST_BC_EXP_FLAG)                     // only 2.x and 3.x clients
		RealmListSizeBuffer << uint16(RealmListSize);
	else
		RealmListSizeBuffer << uint32(RealmListSize);

	ByteBuffer hdr;
	hdr << uint8(REALM_LIST);
	hdr << uint16(pkt.size() + RealmListSizeBuffer.size());
	hdr.append(RealmListSizeBuffer);                        // append RealmList's size buffer
	hdr.append(pkt);                                        // append realms in the realmlist
	SendPacket(hdr);
	return true;
}

// Resume patch transfer
bool AuthSession::HandleXferResume()
{
	TC_LOG_DEBUG("server.authserver", "Entering HandleXferResume");
	XferResume_C* challenge = reinterpret_cast<XferResume_C*>(GetReadBuffer().GetReadPointer());

	// Todo: Send back a packet? I think the client don't get we send data.
	if (patcher.PossiblePatching(_build, _localizationName))
	{
		fseek(patch, 0, SEEK_END);
		size_t size = ftell(patch);

		fseek(patch, long(challenge->pos), 0);

		if (_patcher)
		{
			_patcher->stop();
			delete _patcher;
		}
		_patcher = new PatcherRunnable(this, challenge->pos, size);
		boost::thread u(&PatcherRunnable::run, _patcher);
		u.join();
		return true;
	}
	return false;
}

// Cancel patch transfer
bool AuthSession::HandleXferCancel()
{
	TC_LOG_DEBUG("server.authserver", "Entering _HandleXferCancel");
	CloseSocket();
	return true;
}

// Accept patch transfer
bool AuthSession::HandleXferAccept()
{
	TC_LOG_DEBUG("server.authserver", "Entering _HandleXferAccept");

	// Check packet length and patch existence
	if (!patch)
	{
		TC_LOG_INFO("network", "Error while accepting patch transfer (wrong packet)");
		return false;
	}

	// Launch a PatcherRunnable thread, starting at the beginning of the patch file
	fseek(patch, 0, SEEK_END);
	size_t size = ftell(patch);
	fseek(patch, 0, 0);

	if (_patcher)
	{
		_patcher->stop();
		delete _patcher;
	}
	_patcher = new PatcherRunnable(this, 0, size);
	boost::thread u(&PatcherRunnable::run, _patcher);
	return true;
}

// Make the SRP6 calculation from hash in dB
void AuthSession::SetVSFields(const std::string& rI)
{
	s.SetRand(int32(BufferSizes::SRP_6_S) * 8);

	BigNumber I;
	I.SetHexStr(rI.c_str());

	// In case of leading zeros in the rI hash, restore them
	uint8 mDigest[SHA_DIGEST_LENGTH];
	memcpy(mDigest, I.AsByteArray(SHA_DIGEST_LENGTH).get(), SHA_DIGEST_LENGTH);

	std::reverse(mDigest, mDigest + SHA_DIGEST_LENGTH);

	SHA1Hash sha;
	sha.UpdateData(s.AsByteArray(uint32(BufferSizes::SRP_6_S)).get(), (uint32(BufferSizes::SRP_6_S)));
	sha.UpdateData(mDigest, SHA_DIGEST_LENGTH);
	sha.Finalize();
	BigNumber x;
	x.SetBinary(sha.GetDigest(), sha.GetLength());
	v = g.ModExp(x, N);

	// No SQL injection (username escaped)
	char *v_hex, *s_hex;
	v_hex = v.AsHexStr();
	s_hex = s.AsHexStr();

	PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_VS);
	stmt->setString(0, v_hex);
	stmt->setString(1, s_hex);
	stmt->setString(2, _login);
	LoginDatabase.Execute(stmt);

	OPENSSL_free(v_hex);
	OPENSSL_free(s_hex);
}