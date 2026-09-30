#if !defined(_BUTEMGR_H_)
#define _BUTEMGR_H_

// ButeMgr - LithTech attribute file ("bute file") reader/writer.
//
// Source reimplementation of the original prebuilt ButeMgr library, which was
// compiled with VC6 against STLport and cannot be linked into code built with
// modern compilers. The public interface is unchanged; everything that used to
// be STL-based lives behind m_pImpl (see butemgr.cpp).
//
// Attribute file syntax:
//
//     // line comment            /* block comment */
//     [TagName]
//     IntKey      = 10
//     DwordKey    = 0x0000FFFF
//     DoubleKey   = 1.5
//     BoolKey     = TRUE              (or FALSE)
//     StringKey   = "some text"
//     PointKey    = (10, 20)
//     RectKey     = (0, 0, 640, 480)
//     VectorKey   = <1.0, 2.0, 3.0>
//     RangeKey    = [1.0, 2.0]
//
// Tag and key names are case insensitive. Numeric getters convert between the
// numeric types (int, dword, byte, bool, float, double).
//
// Encrypted attribute files (the cryptKey / decryptCode overloads) are not
// supported by this implementation; those calls fail with an error message.

#pragma warning(disable: 4786)

#include <limits.h>
#include <float.h>
#include <string.h>
#include <ctype.h>
#include <istream>

// CString, CRect and CPoint come from MFC when it is in use, otherwise from
// the MFC stub library.
#if !defined(__AFX_H__)
#include "../incs/mfcstub.h"
#endif

#include "../butemgr/avector.h"
#include "../butemgr/arange.h"

#if defined(_USE_REZFILE_)
#include "rezmgr.h"
#endif


struct equal_str
{
	bool operator()(const char* s1, const char* s2) const
	{
		return strcmp(s1, s2) == 0;
	}
};

struct equal_str_nocase
{
	bool operator()(const char* s1, const char* s2) const
	{
		return stricmp(s1, s2) == 0;
	}
};

struct hash_str_nocase
{
	unsigned long operator()(const char* str) const
	{
		unsigned long hash = 0;
		for ( ; *str; ++str)
			hash = 5*hash + tolower(*str);

		return hash;
	}
};


struct CButeMgrImpl;

class CButeMgr
{

public:

	enum SymTypes { NullType, IntType, DwordType, ByteType, BoolType, DoubleType, FloatType, StringType, RectType, PointType, VectorType, RangeType };

	class CSymTabItem
	{
	public:

		SymTypes SymType;

		CSymTabItem( ) { SymType = NullType; data.d = 0; }
		~CSymTabItem( ) { Term( ); }

		void Init(SymTypes t, int val)				{ Term( ); SymType = t; data.i = val; }
		void Init(SymTypes t, DWORD val)			{ Term( ); SymType = t; data.dw = val; }
		void Init(SymTypes t, BYTE val)				{ Term( ); SymType = t; data.byte = val; }
		void Init(SymTypes t, bool val)				{ Term( ); SymType = t; data.b = val; }
		void Init(SymTypes t, float val)			{ Term( ); SymType = t; data.f = val; }
		void Init(SymTypes t, double val)			{ Term( ); SymType = t; data.d = val; }
		void Init(SymTypes t, const CString& val)	{ Term( ); SymType = t; data.s = new CString( val ); }
		void Init(SymTypes t, const CRect& val)		{ Term( ); SymType = t; data.r = new CRect( val ); }
		void Init(SymTypes t, const CPoint& val)	{ Term( ); SymType = t; data.point = new CPoint( val ); }
		void Init(SymTypes t, const CAVector& val)	{ Term( ); SymType = t; data.v = new CAVector( val ); }
		void Init(SymTypes t, const CARange& val)	{ Term( ); SymType = t; data.range = new CARange( val ); }

		void Term( )
		{
			switch (SymType)
			{
			case StringType:	delete data.s;		break;
			case RectType:		delete data.r;		break;
			case PointType:		delete data.point;	break;
			case VectorType:	delete data.v;		break;
			case RangeType:		delete data.range;	break;
			default:								break;
			}
			SymType = NullType;
			data.d = 0;
		}

		union
		{
			int i;
			DWORD dw;
			BYTE byte;
			bool b;
			double d;
			float f;
			CString const* s;
			CRect* r;
			CPoint* point;
			CAVector* v;
			CARange* range;
		} data;

	private:

		// Not copyable.
		CSymTabItem(const CSymTabItem& sti);
		const CSymTabItem& operator=(const CSymTabItem& sti);
	};

public:

	CButeMgr();
	virtual ~CButeMgr();

	void Init();
	void Init(void (*pF)(const char* szMsg));

	void Term();

	DWORD GetChecksum() { return m_checksum; }
	void SetDisplayFunc(void (*pF)(const char* szMsg)) { m_pDisplayFunc = pF; }
	CString GetErrorString() { return m_sErrorString; }

	bool Parse( std::istream& iStream, int decryptCode = 0);
	bool Parse( std::istream& iCrypt, int nLen, const char* cryptKey);

#if defined(_USE_REZFILE_)
	bool Parse(CRezItm* pItem, int decryptCode = 0);
	bool Parse(CRezItm* pItem, const char* cryptKey);
#endif
	bool Parse(CString sAttributeFilename, int decryptCode = 0);
	bool Parse(CString sAttributeFilename, const char* cryptKey);
	bool Parse(void* pData, unsigned long size, int decryptCode = 0);
	bool Parse(void* pData, unsigned long size, const char* cryptKey);

	bool Save(const char* szNewFileName = NULL);

	typedef bool (*GetTagsCallback)( const char* pszTagName, void* pContext );
	void GetTags( GetTagsCallback pCallback, void* pContext = NULL);

	typedef bool (*GetKeysCallback)( const char* pszKeyName, CSymTabItem* pItem, void* pContext );
	void GetKeys( const char* pszTagName, GetKeysCallback pCallback, void* pContext = NULL);

	int GetInt(const char* szTagName, const char* szAttName, int defVal);
	int GetInt(const char* szTagName, const char* szAttName);
	void SetInt(const char* szTagName, const char* szAttName, int val);

	DWORD GetDword(const char* szTagName, const char* szAttName, DWORD defVal);
	DWORD GetDword(const char* szTagName, const char* szAttName);
	void SetDword(const char* szTagName, const char* szAttName, DWORD val);

	BYTE GetByte(const char* szTagName, const char* szAttName, BYTE defVal);
	BYTE GetByte(const char* szTagName, const char* szAttName);
	void SetByte(const char* szTagName, const char* szAttName, BYTE val);

	bool GetBool(const char* szTagName, const char* szAttName, bool defVal);
	bool GetBool(const char* szTagName, const char* szAttName);
	void SetBool(const char* szTagName, const char* szAttName, bool val);

	float GetFloat(const char* szTagName, const char* szAttName, float defVal);
	float GetFloat(const char* szTagName, const char* szAttName);
	void SetFloat(const char* szTagName, const char* szAttName, float val);

	double GetDouble(const char* szTagName, const char* szAttName, double defVal);
	double GetDouble(const char* szTagName, const char* szAttName);
	void SetDouble(const char* szTagName, const char* szAttName, double val);

	CString GetString(const char* szTagName, const char* szAttName, const CString& defVal);
	CString GetString(const char* szTagName, const char* szAttName);
	void SetString(const char* szTagName, const char* szAttName, const CString& val);

	void GetString(const char* szTagName, const char* szAttName, const char* defVal, char *szResult, DWORD maxLen);
	void GetString(const char* szTagName, const char* szAttName, char *szResult, DWORD maxLen);

	CRect& GetRect(const char* szTagName, const char* szAttName, CRect& defVal);
	CRect& GetRect(const char* szTagName, const char* szAttName);
	void SetRect(const char* szTagName, const char* szAttName, const CRect& val);

	CPoint& GetPoint(const char* szTagName, const char* szAttName, CPoint& defVal);
	CPoint& GetPoint(const char* szTagName, const char* szAttName);
	void SetPoint(const char* szTagName, const char* szAttName, const CPoint& val);

	CAVector& GetVector(const char* szTagName, const char* szAttName, CAVector& defVal);
	CAVector& GetVector(const char* szTagName, const char* szAttName);
	void SetVector(const char* szTagName, const char* szAttName, const CAVector& val);

	CARange& GetRange(const char* szTagName, const char* szAttName, CARange& defVal);
	CARange& GetRange(const char* szTagName, const char* szAttName);
	void SetRange(const char* szTagName, const char* szAttName, const CARange& val);

	bool AddTag(const char *szTagName);

	CButeMgr::SymTypes GetType(const char* szTagName, const char* szAttName);  // returns NullType if tag/key doesn't exist

	bool Success() { return m_bSuccess; }
	bool Exist(const char* szTagName, const char* szAttName = NULL);

private:

	// Not copyable.
	CButeMgr(const CButeMgr&);
	const CButeMgr& operator=(const CButeMgr&);

	void Reset();
	void DisplayMessage(const char* szMsg, ...);

	bool ParseText(const char* pText, unsigned long nLen);

	CSymTabItem* Find(const char* szTagName, const char* szAttName);
	CSymTabItem* FindOrReport(const char* szTagName, const char* szAttName);
	CSymTabItem* FindOrCreate(const char* szTagName, const char* szAttName);

	bool GetNumber(CSymTabItem* pItem, double& dVal);

	DWORD m_checksum;
	bool m_bSuccess;
	CString m_sErrorString;
	CString m_sAttributeFilename;
	void (*m_pDisplayFunc)(const char* szMsg);

	CButeMgrImpl* m_pImpl;
};


#endif
