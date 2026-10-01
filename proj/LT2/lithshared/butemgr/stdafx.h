#ifdef _NOMFC

#ifndef __LINUX
	#include <windows.h>
#endif

#include "mfcstub.h"

#else
	#include "afxwin.h"
	#include "afxext.h"
	#include "afxcmn.h"
#endif
