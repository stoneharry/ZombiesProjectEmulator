/*
* Copyright (C) 2008-2015 TrinityCore <http://www.trinitycore.org/>
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

#include "AuthCodes.h"
#include "Database/DatabaseEnv.h"
#include <cstddef>
#include <map>

typedef std::map<int, RealmBuildInfo*> RealmBuildContainer;

namespace AuthHelper
{
	RealmBuildContainer AcceptedClientBuilds;

	void InitAcceptedClientBuilds()
	{
		AcceptedClientBuilds.clear();

		PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_VERSIONS);
		PreparedQueryResult result = LoginDatabase.Query(stmt);

		if (!result)
			TC_LOG_ERROR("server.authserver", "Table `versions` is empty. No one will be able to log in.");

		do {
			Field* fields = result->Fetch();
			RealmBuildInfo* newBuild = new RealmBuildInfo;
			newBuild->Build = fields[0].GetUInt32();
			newBuild->MajorVersion = fields[1].GetUInt32();
			newBuild->MinorVersion = fields[2].GetUInt32();
			newBuild->BugfixVersion = fields[3].GetUInt32();
			newBuild->HotfixVersion = fields[4].GetUInt32();
			AcceptedClientBuilds[newBuild->Build] = newBuild;
		} while (result->NextRow());
	}

	bool IsAcceptedClientBuild(int build)
	{
		for (RealmBuildContainer::iterator itr = AcceptedClientBuilds.begin(); itr != AcceptedClientBuilds.end(); itr++)
		if (itr->second->Build == build)
			return true;

		return false;
	}

	RealmBuildInfo const* GetBuildInfo(int build)
	{
		for (RealmBuildContainer::iterator itr = AcceptedClientBuilds.begin(); itr != AcceptedClientBuilds.end(); itr++)
		if (itr->second->Build == build)
			return itr->second;

		return NULL;
	}
}