/****************************************************************************
;
;	 MODULE:		GAMESPYCLIENTMGR (.H)
;
;	PURPOSE:		Game Spy Client Manager
;
;	HISTORY:		08/01/00  [blg]  This file was created
;
;	COMMENT:		Copyright (c) 2000, LithTech, Inc.
;
****************************************************************************/

#pragma once


// Includes...

#ifdef GSCM_LOCAL_BUILD
	#include "..\cengine\goaceng.h"
	#include "gamespyserver.h"
#else
	#include "..\gamespy\cengine\goaceng.h"
	#include "..\gamespy\gamespyclientmgr\gamespyserver.h"
#endif


// Libraries...

#pragma comment(lib, "wsock32.lib")


// Defines...

#define	GSCM_MAX_STRING		128
#define	GSCM_MAX_UPDATES	16


#define VERSIONCHECKSTATE_NONE					0
#define VERSIONCHECKSTATE_INPROGRESS			1
#define VERSIONCHECKSTATE_FAILED				2
#define VERSIONCHECKSTATE_REQUIREPATCH			3
#define VERSIONCHECKSTATE_OPTIONALPATCH			4
#define VERSIONCHECKSTATE_SUCCESS				5


// Classes...

class CGameSpyClientMgr
{
	// Member functions...

public:

	CGameSpyClientMgr();
	~CGameSpyClientMgr() { Term(); }

	BOOL				Init(const char* sGameName, const char* sSecretKey, const char* sVersion);
	void				Term();

	const char*			GetVersion()						{ return m_sVersion; }

	BOOL				GetCDKey(char *szCDKey);
	BOOL				SetupCDKey(char *szCDKey = NULL);
	BOOL				IsCDKeyValid();

	BOOL				SetupUserIdentity();
	BOOL				SetupDirServers();
	BOOL				SetupMOTD(char *szSysMsg, char *szGameMsg, int nMaxLength);
	BOOL				SetupVersionCheck();
	BOOL				SetupAuthentication(char *szFailedMsg, int nMaxLength);

	BOOL				AddDirServer(const char* sHostAndPortString);

	BOOL				IsInitialized()						{ return m_bInitialized; }

	void				SetLANOnly(BOOL bLAN)				{ m_bLANOnly = bLAN; }
	BOOL				GetLANOnly()						{ return m_bLANOnly; }

	void				SetVersionCheckState(int nState)	{ m_nVersionCheckState = nState; }
	int					GetVersionCheckState()				{ return m_nVersionCheckState; }

	int					GetNumServers();
	int					GetState();
	int					GetProgress()						{ return m_nProgress; }

	void				ClearServers();
	BOOL				RefreshServers(BOOL bAsync = TRUE);
	BOOL				RefreshLANServers();

	CGameSpyServer*		GetServer(int nIndex);
	CGameSpyServer*		GetServerFromHandle(void* pHandle);
	CGameSpyServer*		GetFirstServer();
	CGameSpyServer*		GetNextServer();

	BOOL				ExistNewServer();

	void				SortServersByName(BOOL bAscending = TRUE);
	void				SortServersByPing(BOOL bAscending = TRUE);
	void				SortServersByPlayers(BOOL bAscending = FALSE);
	void				SortServersByMap(BOOL bAscending = TRUE);
	void				SortServersByGameType(BOOL bAscending = TRUE);
	void				SortServers(BOOL bAscending, char* sSortKey, GCompareMode eCompareMode);

	void				Update();

	void				HandleCallback(GServerList pServerList, int nMsg, void* pParam1, void* pParam2);

	void*				GetAuthContext()		{ return m_pAuthContext; }

	void				KillGetServersOp();

protected:

	void				Clear();

	BOOL				CreateServerList();
	void				FreeServerList();

	CGameSpyServer*		CreateGameSpyServer(GServer pGServer);


	// Member variables...

public:

	BOOL				m_bInitialized;
	BOOL				m_bLANOnly;
	BOOL				m_bForceCDKeyValid;

	int					m_nVersionCheckState;

	GServerList			m_pServerList;
	char				m_sGameName[GSCM_MAX_STRING];
	char				m_sSecretKey[GSCM_MAX_STRING];
	char				m_sVersion[GSCM_MAX_STRING];
	int					m_nGetServerIndex;
	int					m_nProgress;
	BOOL				m_bNewServer;
	CGameSpyServer		m_CurServer;

	void				*m_pAuthContext;
};






