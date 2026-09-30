// vc6compat.cpp - Runtime symbols expected by the prebuilt VC6 libraries.
//
// The LithTech libraries in lithshared\libs (GameSpy*, WONAPI) were compiled
// with Visual C++ 6. They reference two runtime symbols that the modern CRT no
// longer provides under the same names:
//
//   ::exception  VC6 declared class exception in the global namespace (std::
//                only aliased it). Modern MSVC defines std::exception, which
//                has a different decorated name.
//   _timezone    VC6 exported this as a global variable. The UCRT replaced it
//                with a function (__timezone()), keeping _timezone only as a
//                macro in <time.h>.
//   _chkesp      VC6's /GZ stack-pointer check, called by the debug libraries.
//                The modern CRT's version lives in an object that collides
//                with LIBCMTD (duplicate __CRT_RTC_INIT), so it is defined here.
//
// This file must not include <exception>, <time.h> or <math.h>: each of them
// declares one of these names differently.

#include <stdlib.h>
#include <string.h>


// ----------------------------------------------------------------------- //
//	::exception, with the VC6 layout: vptr, const char* _m_what, int _m_doFree
// ----------------------------------------------------------------------- //

class exception
{
public:
	exception();
	exception(const char* const& szWhat);
	exception(const exception& rhs);
	exception& operator=(const exception& rhs);
	virtual ~exception();
	virtual const char* what() const;

private:
	void Copy(const exception& rhs);

	const char* _m_what;
	int _m_doFree;
};

exception::exception() : _m_what(NULL), _m_doFree(0)
{
}

exception::exception(const char* const& szWhat) : _m_what(NULL), _m_doFree(0)
{
	if (szWhat)
	{
		char* p = (char*)malloc(strlen(szWhat) + 1);
		if (p)
		{
			strcpy(p, szWhat);
			_m_what = p;
			_m_doFree = 1;
		}
	}
}

exception::exception(const exception& rhs) : _m_what(NULL), _m_doFree(0)
{
	Copy(rhs);
}

exception& exception::operator=(const exception& rhs)
{
	if (this != &rhs)
	{
		if (_m_doFree)
			free((void*)_m_what);
		_m_what = NULL;
		_m_doFree = 0;
		Copy(rhs);
	}
	return *this;
}

exception::~exception()
{
	if (_m_doFree)
		free((void*)_m_what);
}

const char* exception::what() const
{
	return _m_what ? _m_what : "Unknown exception";
}

void exception::Copy(const exception& rhs)
{
	if (rhs._m_doFree && rhs._m_what)
	{
		char* p = (char*)malloc(strlen(rhs._m_what) + 1);
		if (p)
		{
			strcpy(p, rhs._m_what);
			_m_what = p;
			_m_doFree = 1;
		}
	}
	else
	{
		_m_what = rhs._m_what;
	}
}


// ----------------------------------------------------------------------- //
//	_timezone: seconds west of UTC, filled in from the UCRT at startup
// ----------------------------------------------------------------------- //

extern "C" void __cdecl _tzset(void);
extern "C" int __cdecl _get_timezone(long* pSeconds);

extern "C" long _timezone = 0;

// ----------------------------------------------------------------------- //
//	_chkesp: VC6 emits "cmp esi, esp / call __chkesp" after calls and
//	expected ZF set. The check is a debugging aid only, so this version just
//	returns (flags and registers untouched).
// ----------------------------------------------------------------------- //

extern "C" __declspec(naked) void __cdecl _chkesp(void)
{
	__asm ret
}


namespace
{
	struct TimezoneInit
	{
		TimezoneInit()
		{
			_tzset();
			_get_timezone(&_timezone);
		}
	} g_TimezoneInit;
}
