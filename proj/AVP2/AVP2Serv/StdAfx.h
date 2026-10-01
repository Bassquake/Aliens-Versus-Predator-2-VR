// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__C6916249_FA1F_11D0_B46B_00A024805738__INCLUDED_)
#define AFX_STDAFX_H__C6916249_FA1F_11D0_B46B_00A024805738__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdisp.h>        // MFC OLE automation classes
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

#include "tchar.h"

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
#include "ltcompat.h"
#include "ModelLT.h"
#include "TransformLT.h"
#include "Physics_LT.h"
#include "Math_LT.h"


//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__C6916249_FA1F_11D0_B46B_00A024805738__INCLUDED_)
