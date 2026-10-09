// ----------------------------------------------------------------------- //
//
// MODULE  : MasterServerList.h
//
// PURPOSE : The internet server list from the AvP2 community's master server
//
// ----------------------------------------------------------------------- //
//
// The game's internet list came from Sierra's WON directory servers, gone since 2004. Master
// Server Patch 2.4 (www.avp2msp.com) lists servers on master.avp2msp.com port 28900 instead: it
// greets with "100..." and answers "001" with "101" 0x01 <count> 0x01, then <count> lines of
// "ip:port". Those are the servers' own ports, which answer the usual GameSpy queries, so each is
// given to the GameSpy list (ServerListAuxUpdate) and the Join screen shows and joins them as before.
//
// The list is fetched on a thread, so the menu keeps running while it connects.

#ifndef __MASTER_SERVER_LIST_H__
#define __MASTER_SERVER_LIST_H__

#include "GameSpyClientMgr.h"

struct MasterListJob;

class CMasterServerClientMgr : public CGameSpyClientMgr
{
	public:
		CMasterServerClientMgr();
		~CMasterServerClientMgr();

		enum MasterState
		{
			MASTER_IDLE,		// no fetch, or its servers have been given to the list to query
			MASTER_FETCHING,	// still getting the list from the master server
			MASTER_FAILED,		// couldn't get it (just now): the list stays empty
		};

		// Empties the server list and starts getting the internet list
		void		StartMasterList();

		// Call while the Join screen is getting servers: once the list has arrived, its servers
		// are added to the GameSpy list, which then queries them (GetState() is no longer idle)
		MasterState	UpdateMasterList();

		// Stops waiting for the list (leaving the Join screen)
		void		CancelMasterList();
		LTBOOL		IsFetchingMasterList() const;

	private:
		MasterListJob	*m_pJob;		// the fetch in progress, shared with its thread
};

#endif // __MASTER_SERVER_LIST_H__
