// Checks that the game's GameSpy library (GameSpyClientMgr.lib) can list servers it's given by
// address, as the Join screen does with the AvP2MSP master server's list: Init, then
// ServerListAuxUpdate for each "ip:port" on the command line, pumping Update until it's idle.
// Build: build_gstest.bat. Run: bin\gstest.exe 51.68.204.103:15101 15.204.225.129:27889

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "GameSpyClientMgr.h"

// Init doesn't make the server list (the WON refresh did); CreateServerList is protected
struct ListMgr : CGameSpyClientMgr
{
	BOOL MakeList() { return m_pServerList || CreateServerList(); }
};

int main(int argc, char** argv)
{
	ListMgr mgr;
	if (!mgr.Init("avp2", "Df3M6Z", "1.0.9.6"))
	{
		printf("Init failed\n");
		return 1;
	}
	printf("Init ok, server list %p\n", mgr.m_pServerList);
	if (!mgr.MakeList())
		printf("CreateServerList failed\n");
	printf("Init ok, server list %p, state %d\n", mgr.m_pServerList, mgr.GetState());
	if (!mgr.m_pServerList)
		return 1;

	for (int i = 1; i < argc; ++i)
	{
		char ip[64];
		strncpy_s(ip, argv[i], _TRUNCATE);
		char* colon = strchr(ip, ':');
		if (!colon)
			continue;
		*colon = 0;
		GError e = ServerListAuxUpdate(mgr.m_pServerList, ip, atoi(colon + 1), 1, qt_status);
		printf("AuxUpdate %s:%s -> %d, state %d\n", ip, colon + 1, e, mgr.GetState());
	}

	DWORD start = GetTickCount();
	while (GetTickCount() - start < 8000)
	{
		mgr.Update();
		if (mgr.GetState() == sl_idle && GetTickCount() - start > 200)
			break;
		Sleep(10);
	}
	printf("State %d after %lu ms, %d servers\n", mgr.GetState(), GetTickCount() - start, mgr.GetNumServers());
	for (CGameSpyServer* s = mgr.GetFirstServer(); s; s = mgr.GetNextServer())
		printf("  %s  %s:%d  map %s  type %s  players %d/%d  ver %s  ping %d\n", s->GetName(), s->GetAddress(),
			s->GetIntValue("hostport"), s->GetMap(), s->GetGameType(), s->GetIntValue("numplayers"),
			s->GetIntValue("maxplayers"), s->GetStringValue("gamever"), s->GetIntValue("ping", -1));
	return 0;
}
