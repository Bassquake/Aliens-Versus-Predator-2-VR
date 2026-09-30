// ----------------------------------------------------------------------- //
//
// MODULE  : PerfInfo.h
//
// PURPOSE : Definitions for performance info.
//
// CREATED : 3/4/01
//
// (c) 1998-2001 Monolith Productions, Inc.  All Rights Reserved
//
// ----------------------------------------------------------------------- //

#ifndef __PERFINFO_H__
#define __PERFINFO_H__

// Using the PERFINFO macros requires the program to be run on
// Win NT 4.0 or higher.  You must have the Platform SDK to build.
#ifdef ENABLE_PERFINFO

#include <Psapi.h>
#pragma comment (lib, "psapi.lib")

struct PerfInfo
{
	SIZE_T			m_nMemSize;
	LARGE_INTEGER	m_nTimer;
};

// Get baseline memory information.
#define PERFINFO_START( ) \
	PerfInfo _perfInfo; \
	{ \
		PROCESS_MEMORY_COUNTERS pmc; \
		pmc.cb = sizeof( PROCESS_MEMORY_COUNTERS ); \
		GetProcessMemoryInfo( GetCurrentProcess( ), &pmc, sizeof( PROCESS_MEMORY_COUNTERS )); \
		QueryPerformanceCounter( &_perfInfo.m_nTimer ); \
		TRACE( "PERFINFO_START:  %dk at %s(%d)\n", pmc.WorkingSetSize / 1024, __FILE__, __LINE__ ); \
		_perfInfo.m_nMemSize = pmc.WorkingSetSize; \
	}

// Check how much memory has changed since the last CHECK or START call.
#define PERFINFO_REPORT( ) \
	{ \
		PROCESS_MEMORY_COUNTERS pmc; \
		LARGE_INTEGER curTime, nFreq; \
		LONGLONG nTimeElapsed; \
		double fMs; \
		GetProcessMemoryInfo( GetCurrentProcess( ), &pmc, sizeof( PROCESS_MEMORY_COUNTERS )); \
		QueryPerformanceCounter( &curTime ); \
		QueryPerformanceFrequency( &nFreq ); \
		nTimeElapsed = curTime.QuadPart - _perfInfo.m_nTimer.QuadPart; \
		fMs = ( double )( nTimeElapsed * ( LONGLONG )1000 ) / nFreq.QuadPart; \
		TRACE( "PERFINFO_REPORT:  %dk %+ik, %.3fms at %s(%d)\n", pmc.WorkingSetSize / 1024, ( pmc.WorkingSetSize - _perfInfo.m_nMemSize ) / 1024, fMs, __FILE__, __LINE__ ); \
		_perfInfo.m_nMemSize = pmc.WorkingSetSize; \
		_perfInfo.m_nTimer = curTime; \
	}

#else // ENABLE_PERFINFO

#define PERFINFO_START( ) (( void * )0)
#define PERFINFO_REPORT( ) (( void * )0)

#endif // ENABLE_PERFINFO

#endif // __PERFINFO_H__
