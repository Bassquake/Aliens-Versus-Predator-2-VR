// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#ifndef __STDAFX_H__
#define __STDAFX_H__

#pragma warning( disable : 4786 )
#pragma warning(disable : 4503)

#ifdef _WIN32

	#define WIN32_LEAN_AND_MEAN

	#include <windows.h>

#endif

#include <stdio.h>
#include <limits.h>

// Standard containers. The VC6-era STLport headers pulled these in transitively
// (mostly via butemgr.h), and much of the game code relies on that.
#include <vector>
#include <list>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <algorithm>
#include <functional>
#include <unordered_map>

// VC6's pre-standard iostreams (<iostream.h>, <strstrea.h>) put these in the
// global namespace; game code uses them unqualified.
#include <iostream>
#include <fstream>
#include <strstream>
using std::istream;
using std::ostream;
using std::iostream;
using std::ifstream;
using std::ofstream;
using std::istrstream;
using std::ostrstream;
using std::strstream;
using std::ios;
using std::endl;
using std::ends;

#include "mfcstub.h"

#include "iltclient.h"
#include "iltserver.h"
#include "iltmessage.h"

#include "modellt.h"
#include "transformlt.h"
#include "physics_lt.h"
#include "math_lt.h"

#include "Globals.h"
#include "CommonUtilities.h"
#include "MemoryUtils.h"
#include "PerfInfo.h"

#endif // __STDAFX_H__
