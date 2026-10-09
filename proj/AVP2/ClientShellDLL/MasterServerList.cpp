// ----------------------------------------------------------------------- //
//
// MODULE  : MasterServerList.cpp
//
// PURPOSE : The internet server list from the AvP2 community's master server
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "MasterServerList.h"
#include "VRMgr.h"
#include <winsock.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MASTER_HOST				"master.avp2msp.com"
#define MASTER_PORT				28900
#define MASTER_TIMEOUT_MS		8000		// to connect, and for each reply
#define MASTER_MAX_SERVERS		1024

// One fetch, shared by the game and its thread: whichever is done with it last frees it
struct MasterListJob
{
	volatile LONG	nRefs;
	volatile LONG	nState;				// 0 = fetching, 1 = done, 2 = failed (written last by the thread)
	int				nServers;
	char			szServers[MASTER_MAX_SERVERS][24];	// "a.b.c.d:port"
	char			szError[128];
};

static void ReleaseJob(MasterListJob *pJob)
{
	if(pJob && InterlockedDecrement(&pJob->nRefs) == 0)
		delete pJob;
}

// Waits up to MASTER_TIMEOUT_MS for the socket to be readable (bRead) or connected
static LTBOOL WaitSocket(SOCKET s, LTBOOL bRead)
{
	fd_set fds, fdsErr;
	FD_ZERO(&fds);
	FD_SET(s, &fds);
	FD_ZERO(&fdsErr);
	FD_SET(s, &fdsErr);
	timeval tv = { MASTER_TIMEOUT_MS / 1000, (MASTER_TIMEOUT_MS % 1000) * 1000 };
	int n = bRead ? select(0, &fds, LTNULL, &fdsErr, &tv) : select(0, LTNULL, &fds, &fdsErr, &tv);
	return n > 0 && FD_ISSET(s, &fds) && !FD_ISSET(s, &fdsErr);
}

// The reply to "001": "101" 0x01 <count> 0x01 then <count> lines of "ip:port". Returns how many
// it read, or -1 if the reply isn't that (or isn't complete yet: *pbComplete is LTFALSE).
static int ParseServerList(const char *pData, int nLen, MasterListJob *pJob, LTBOOL *pbComplete)
{
	*pbComplete = LTFALSE;
	if(nLen < 3)
		return 0;
	if(strncmp(pData, "101", 3) != 0)
		return -1;
	const char *pEnd = pData + nLen;
	const char *p = (const char *)memchr(pData, 1, nLen);
	if(!p)
		return 0;
	const char *pCount = p + 1;
	p = (const char *)memchr(pCount, 1, pEnd - pCount);
	if(!p)
		return 0;
	int nCount = atoi(pCount);
	p++;

	pJob->nServers = 0;
	while(p < pEnd && pJob->nServers < nCount && pJob->nServers < MASTER_MAX_SERVERS)
	{
		const char *pLine = p;
		const char *pNl = (const char *)memchr(p, '\n', pEnd - p);
		if(!pNl)
			break;
		p = pNl + 1;
		int nLineLen = (int)(pNl - pLine);
		if(nLineLen > 0 && pLine[nLineLen - 1] == '\r')
			nLineLen--;
		unsigned int a, b, c, d, nPort;
		char szLine[32];
		if(nLineLen <= 0 || nLineLen >= (int)sizeof(szLine))
			continue;
		memcpy(szLine, pLine, nLineLen);
		szLine[nLineLen] = 0;
		if(sscanf(szLine, "%u.%u.%u.%u:%u", &a, &b, &c, &d, &nPort) != 5 || a > 255 || b > 255 || c > 255 || d > 255 ||
		   !nPort || nPort > 65535)
			continue;
		sprintf(pJob->szServers[pJob->nServers++], "%u.%u.%u.%u:%u", a, b, c, d, nPort);
	}
	*pbComplete = pJob->nServers >= nCount || pJob->nServers >= MASTER_MAX_SERVERS;
	return pJob->nServers;
}

static LTBOOL FetchList(MasterListJob *pJob)
{
	hostent *pHost = gethostbyname(MASTER_HOST);
	if(!pHost || !pHost->h_addr_list[0])
	{
		sprintf(pJob->szError, "can't find %s (error %d)", MASTER_HOST, WSAGetLastError());
		return LTFALSE;
	}

	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(s == INVALID_SOCKET)
	{
		sprintf(pJob->szError, "no socket (error %d)", WSAGetLastError());
		return LTFALSE;
	}

	LTBOOL bOK = LTFALSE;
	sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(MASTER_PORT);
	memcpy(&sa.sin_addr, pHost->h_addr_list[0], sizeof(sa.sin_addr));

	// Connect without blocking past the timeout
	u_long nNonBlocking = 1;
	ioctlsocket(s, FIONBIO, &nNonBlocking);
	if(connect(s, (sockaddr *)&sa, sizeof(sa)) == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
		sprintf(pJob->szError, "can't connect (error %d)", WSAGetLastError());
	else if(!WaitSocket(s, LTFALSE))
		sprintf(pJob->szError, "no connection within %d s", MASTER_TIMEOUT_MS / 1000);
	else
	{
		const int nDataSize = MASTER_MAX_SERVERS * 24 + 256;
		char *szData = new char[nDataSize];
		char szGreeting[256];
		int nLen = 0;

		// The greeting ("100Ayl Master Server Version 1.0"), then ask for the list
		int n = WaitSocket(s, LTTRUE) ? recv(s, szGreeting, sizeof(szGreeting) - 1, 0) : -1;
		if(n < 3 || strncmp(szGreeting, "100", 3) != 0)
			sprintf(pJob->szError, "no greeting (got %d bytes)", n);
		else if(send(s, "001", 3, 0) != 3)
			sprintf(pJob->szError, "can't send the request (error %d)", WSAGetLastError());
		else
		{
			// It keeps the connection open, so read until the list is complete
			LTBOOL bComplete = LTFALSE;
			while(!bComplete && nLen < nDataSize && WaitSocket(s, LTTRUE))
			{
				n = recv(s, szData + nLen, nDataSize - nLen, 0);
				if(n <= 0)
					break;
				nLen += n;
				if(ParseServerList(szData, nLen, pJob, &bComplete) < 0)
					break;
			}
			int nServers = ParseServerList(szData, nLen, pJob, &bComplete);
			if(nServers < 0)
				sprintf(pJob->szError, "unexpected reply (%d bytes)", nLen);
			else if(!bComplete && !nServers)
				sprintf(pJob->szError, "no list (%d bytes)", nLen);
			else
				bOK = LTTRUE;	// a cut-off list is still worth showing
		}
		delete[] szData;
	}
	closesocket(s);
	return bOK;
}

static DWORD WINAPI FetchThread(void *pParam)
{
	MasterListJob *pJob = (MasterListJob *)pParam;
	WSADATA wsa;
	LTBOOL bStarted = WSAStartup(MAKEWORD(1, 1), &wsa) == 0;
	LTBOOL bOK = bStarted && FetchList(pJob);
	if(!bStarted)
		strcpy(pJob->szError, "WSAStartup failed");
	if(bStarted)
		WSACleanup();
	InterlockedExchange(&pJob->nState, bOK ? 1 : 2);
	ReleaseJob(pJob);
	return 0;
}

// ----------------------------------------------------------------------- //

CMasterServerClientMgr::CMasterServerClientMgr()
{
	m_pJob = LTNULL;
}

CMasterServerClientMgr::~CMasterServerClientMgr()
{
	CancelMasterList();
}

void CMasterServerClientMgr::StartMasterList()
{
	CancelMasterList();
	ClearServers();

	MasterListJob *pJob = new MasterListJob;
	memset(pJob, 0, sizeof(MasterListJob));
	pJob->nRefs = 2;
	HANDLE hThread = CreateThread(LTNULL, 0, FetchThread, pJob, 0, LTNULL);
	if(!hThread)
	{
		pJob->nRefs = 1;
		pJob->nState = 2;
		strcpy(pJob->szError, "can't start the thread");
	}
	else
		CloseHandle(hThread);
	m_pJob = pJob;
	VRLog("Master server: getting the list from %s:%d", MASTER_HOST, MASTER_PORT);
}

CMasterServerClientMgr::MasterState CMasterServerClientMgr::UpdateMasterList()
{
	if(!m_pJob)
		return MASTER_IDLE;
	LONG nState = m_pJob->nState;
	if(nState == 0)
		return MASTER_FETCHING;

	MasterListJob *pJob = m_pJob;
	m_pJob = LTNULL;
	if(nState != 1)
	{
		VRLog("Master server: %s", pJob->szError);
		ReleaseJob(pJob);
		return MASTER_FAILED;
	}

	// Init leaves the GameSpy list to be made by the WON refresh, which isn't used
	if(!m_pServerList)
		CreateServerList();
	int nAdded = 0;
	for(int i = 0; i < pJob->nServers && m_pServerList; i++)
	{
		char szIP[24];
		strcpy(szIP, pJob->szServers[i]);
		char *pColon = strchr(szIP, ':');
		if(!pColon)
			continue;
		*pColon = 0;
		if(ServerListAuxUpdate(m_pServerList, szIP, atoi(pColon + 1), 1, qt_status) == GE_NOERROR)
			nAdded++;
	}
	VRLog("Master server: %d servers listed, %d being queried", pJob->nServers, nAdded);
	ReleaseJob(pJob);
	return MASTER_IDLE;
}

LTBOOL CMasterServerClientMgr::IsFetchingMasterList() const
{
	return m_pJob && m_pJob->nState == 0;
}

void CMasterServerClientMgr::CancelMasterList()
{
	ReleaseJob(m_pJob);
	m_pJob = LTNULL;
}
