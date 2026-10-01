// butemgr.cpp - LithTech attribute file reader/writer (see butemgr.h).

#if defined(BUTEMGR_MFC)
#include <afxwin.h>
#endif

#include "butemgr.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <map>
#include <sstream>


// ----------------------------------------------------------------------- //
//	Storage
// ----------------------------------------------------------------------- //

namespace
{
	struct NoCaseLess
	{
		bool operator()(const std::string& a, const std::string& b) const
		{
			return stricmp(a.c_str(), b.c_str()) < 0;
		}
	};

	struct Key
	{
		std::string name;
		CButeMgr::CSymTabItem* pItem;
	};

	struct Tag
	{
		std::string name;
		std::vector<Key> keys;							// in file order
		std::map<std::string, size_t, NoCaseLess> index;	// name -> keys[]
	};
}

struct CButeMgrImpl
{
	std::vector<Tag*> tags;								// in file order
	std::map<std::string, size_t, NoCaseLess> index;	// name -> tags[]

	// Returned by reference-returning getters when the attribute is missing.
	CRect		dummyRect;
	CPoint		dummyPoint;
	CAVector	dummyVector;
	CARange		dummyRange;

	~CButeMgrImpl() { Clear(); }

	void Clear()
	{
		for (size_t t = 0; t < tags.size(); ++t)
		{
			for (size_t k = 0; k < tags[t]->keys.size(); ++k)
				delete tags[t]->keys[k].pItem;
			delete tags[t];
		}
		tags.clear();
		index.clear();
	}

	Tag* FindTag(const char* szTagName)
	{
		if (!szTagName) return NULL;
		std::map<std::string, size_t, NoCaseLess>::iterator it = index.find(szTagName);
		return (it == index.end()) ? NULL : tags[it->second];
	}

	Tag* FindOrCreateTag(const char* szTagName)
	{
		Tag* pTag = FindTag(szTagName);
		if (pTag) return pTag;
		pTag = new Tag;
		pTag->name = szTagName;
		index[pTag->name] = tags.size();
		tags.push_back(pTag);
		return pTag;
	}

	static CButeMgr::CSymTabItem* FindItem(Tag* pTag, const char* szAttName)
	{
		if (!pTag || !szAttName) return NULL;
		std::map<std::string, size_t, NoCaseLess>::iterator it = pTag->index.find(szAttName);
		return (it == pTag->index.end()) ? NULL : pTag->keys[it->second].pItem;
	}

	static CButeMgr::CSymTabItem* FindOrCreateItem(Tag* pTag, const char* szAttName)
	{
		CButeMgr::CSymTabItem* pItem = FindItem(pTag, szAttName);
		if (pItem) return pItem;
		Key key;
		key.name = szAttName;
		key.pItem = new CButeMgr::CSymTabItem;
		pTag->index[key.name] = pTag->keys.size();
		pTag->keys.push_back(key);
		return key.pItem;
	}
};


// ----------------------------------------------------------------------- //
//	Construction
// ----------------------------------------------------------------------- //

CButeMgr::CButeMgr()
{
	m_checksum = 0;
	m_bSuccess = true;
	m_pDisplayFunc = NULL;
	m_pImpl = new CButeMgrImpl;
	m_pImpl->dummyRect = CRect(0, 0, 0, 0);
	m_pImpl->dummyPoint = CPoint(0, 0);
	m_pImpl->dummyVector.Set(0.0, 0.0, 0.0);
	m_pImpl->dummyRange.Set(0.0, 0.0);
}

CButeMgr::~CButeMgr()
{
	delete m_pImpl;
}

void CButeMgr::Init()
{
	Reset();
}

void CButeMgr::Init(void (*pF)(const char* szMsg))
{
	Reset();
	m_pDisplayFunc = pF;
}

void CButeMgr::Term()
{
	Reset();
	m_sAttributeFilename = "";
}

void CButeMgr::Reset()
{
	m_pImpl->Clear();
	m_checksum = 0;
	m_bSuccess = true;
	m_sErrorString = "";
}

void CButeMgr::DisplayMessage(const char* szMsg, ...)
{
	char szBuffer[1024];
	va_list args;
	va_start(args, szMsg);
	_vsnprintf(szBuffer, sizeof(szBuffer) - 1, szMsg, args);
	va_end(args);
	szBuffer[sizeof(szBuffer) - 1] = '\0';

	m_sErrorString = szBuffer;
	if (m_pDisplayFunc)
		m_pDisplayFunc(szBuffer);
}


// ----------------------------------------------------------------------- //
//	Parsing
// ----------------------------------------------------------------------- //

namespace
{
	class Scanner
	{
	public:

		Scanner(const char* pText, unsigned long nLen)
			: m_p(pText), m_pEnd(pText + nLen), m_nLine(1) {}

		int Line() const { return m_nLine; }
		bool AtEnd() { SkipSpace(); return m_p >= m_pEnd; }
		char Peek() { SkipSpace(); return (m_p < m_pEnd) ? *m_p : '\0'; }

		bool Accept(char c)
		{
			if (Peek() != c) return false;
			++m_p;
			return true;
		}

		// Accepts a type prefix such as "(DWORD)" (case-insensitive). The original ButeMgr writes
		// DWORD and BYTE values that way, and its reader requires it.
		bool AcceptTypePrefix(const char* szType)
		{
			SkipSpace();
			size_t n = strlen(szType);
			if (m_pEnd - m_p < (ptrdiff_t)(n + 2) || m_p[0] != '(' || strnicmp(m_p + 1, szType, n) != 0 || m_p[n + 1] != ')')
				return false;
			m_p += n + 2;
			return true;
		}

		// Reads up to (not including) any character in szStop, or whitespace.
		std::string ReadWord(const char* szStop)
		{
			SkipSpace();
			const char* pStart = m_p;
			while (m_p < m_pEnd && !isspace((unsigned char)*m_p) && !strchr(szStop, *m_p))
				++m_p;
			return std::string(pStart, m_p);
		}

		// Reads up to cEnd on the current line, trimmed. Consumes cEnd.
		bool ReadUntil(char cEnd, std::string& sOut)
		{
			SkipSpace();
			const char* pStart = m_p;
			while (m_p < m_pEnd && *m_p != cEnd && *m_p != '\n')
				++m_p;
			if (m_p >= m_pEnd || *m_p != cEnd) return false;
			const char* pStop = m_p++;
			while (pStop > pStart && isspace((unsigned char)pStop[-1]))
				--pStop;
			sOut.assign(pStart, pStop);
			return true;
		}

		// Reads a double-quoted string. There are no escape sequences:
		// backslashes are kept as-is (attribute files use them in paths).
		bool ReadQuoted(std::string& sOut)
		{
			if (!Accept('"')) return false;
			const char* pStart = m_p;
			while (m_p < m_pEnd && *m_p != '"')
			{
				if (*m_p == '\n') ++m_nLine;
				++m_p;
			}
			if (m_p >= m_pEnd) return false;
			sOut.assign(pStart, m_p++);
			return true;
		}

		bool ReadNumber(double& dVal, bool& bIsReal, bool& bIsHex)
		{
			SkipSpace();
			const char* pStart = m_p;
			std::string sNum = ReadWord(",)>]=;/");
			if (sNum.empty()) return false;

			const char* s = sNum.c_str();
			char* pStop = NULL;
			const char* pDigits = (*s == '+' || *s == '-') ? s + 1 : s;
			bIsHex = (pDigits[0] == '0' && (pDigits[1] == 'x' || pDigits[1] == 'X'));
			if (bIsHex)
			{
				dVal = (double)strtoul(s, &pStop, 16);
				bIsReal = false;
			}
			else
			{
				dVal = strtod(s, &pStop);
				bIsReal = (strpbrk(s, ".eE") != NULL);
			}

			// Allow a C-style float suffix ("1.5f").
			if (pStop && (*pStop == 'f' || *pStop == 'F') && pStop[1] == '\0')
				++pStop;

			if (pStop == s || !pStop || *pStop != '\0')
			{
				m_p = pStart;
				return false;
			}
			return true;
		}

	private:

		void SkipSpace()
		{
			for (;;)
			{
				while (m_p < m_pEnd && isspace((unsigned char)*m_p))
				{
					if (*m_p == '\n') ++m_nLine;
					++m_p;
				}
				if (m_p + 1 < m_pEnd && m_p[0] == '/' && m_p[1] == '/')
				{
					while (m_p < m_pEnd && *m_p != '\n')
						++m_p;
					continue;
				}
				if (m_p + 1 < m_pEnd && m_p[0] == '/' && m_p[1] == '*')
				{
					m_p += 2;
					while (m_p + 1 < m_pEnd && !(m_p[0] == '*' && m_p[1] == '/'))
					{
						if (*m_p == '\n') ++m_nLine;
						++m_p;
					}
					m_p = (m_p + 1 < m_pEnd) ? m_p + 2 : m_pEnd;
					continue;
				}
				break;
			}
		}

		const char* m_p;
		const char* m_pEnd;
		int m_nLine;
	};

	// Reads "n, n, ... n" followed by cClose. Returns the number of values read.
	int ReadList(Scanner& scan, char cClose, double* pVals, int nMax)
	{
		int nCount = 0;
		for (;;)
		{
			double dVal;
			bool bReal, bHex;
			if (nCount >= nMax || !scan.ReadNumber(dVal, bReal, bHex))
				return -1;
			pVals[nCount++] = dVal;
			if (scan.Accept(cClose))
				return nCount;
			if (!scan.Accept(','))
				return -1;
		}
	}
}


bool CButeMgr::ParseText(const char* pText, unsigned long nLen)
{
	Reset();

	for (unsigned long i = 0; i < nLen; ++i)
		m_checksum += (unsigned char)pText[i];

	Scanner scan(pText, nLen);
	Tag* pTag = NULL;

	while (!scan.AtEnd())
	{
		// [TagName]
		if (scan.Accept('['))
		{
			std::string sTag;
			if (!scan.ReadUntil(']', sTag) || sTag.empty())
			{
				DisplayMessage("ButeMgr: line %d: bad tag name", scan.Line());
				return false;
			}
			pTag = m_pImpl->FindOrCreateTag(sTag.c_str());
			continue;
		}

		// Key = Value
		std::string sKey = scan.ReadWord("=[\"");
		if (sKey.empty() || !scan.Accept('='))
		{
			DisplayMessage("ButeMgr: line %d: expected 'attribute = value'", scan.Line());
			return false;
		}
		if (!pTag)
		{
			DisplayMessage("ButeMgr: line %d: attribute '%s' is not inside a [tag]", scan.Line(), sKey.c_str());
			return false;
		}

		CSymTabItem* pItem = CButeMgrImpl::FindOrCreateItem(pTag, sKey.c_str());
		double vals[4];
		int nVals;
		CButeMgr::SymTypes eTyped = CButeMgr::NullType;
		char cOpen = scan.Peek();

		if (cOpen == '"')
		{
			std::string sVal;
			if (!scan.ReadQuoted(sVal))
			{
				DisplayMessage("ButeMgr: line %d: unterminated string for '%s'", scan.Line(), sKey.c_str());
				return false;
			}
			pItem->Init(StringType, CString(sVal.c_str()));
		}
		else if (cOpen == '<')
		{
			scan.Accept('<');
			if (ReadList(scan, '>', vals, 3) != 3)
			{
				DisplayMessage("ButeMgr: line %d: bad vector for '%s'", scan.Line(), sKey.c_str());
				return false;
			}
			pItem->Init(VectorType, CAVector(vals[0], vals[1], vals[2]));
		}
		else if (cOpen == '(' && (scan.AcceptTypePrefix("DWORD") ? (eTyped = DwordType, true) :
								  scan.AcceptTypePrefix("BYTE") ? (eTyped = ByteType, true) : false))
		{
			// "(DWORD)n" / "(BYTE)n": a typed number
			double dVal;
			bool bReal, bHex;
			if (!scan.ReadNumber(dVal, bReal, bHex))
			{
				DisplayMessage("ButeMgr: line %d: bad number for '%s'", scan.Line(), sKey.c_str());
				return false;
			}
			if (eTyped == DwordType)
				pItem->Init(DwordType, (DWORD)dVal);
			else
				pItem->Init(ByteType, (BYTE)dVal);
		}
		else if (cOpen == '(')
		{
			scan.Accept('(');
			nVals = ReadList(scan, ')', vals, 4);
			if (nVals == 2)
				pItem->Init(PointType, CPoint((int)vals[0], (int)vals[1]));
			else if (nVals == 4)
				pItem->Init(RectType, CRect((int)vals[0], (int)vals[1], (int)vals[2], (int)vals[3]));
			else
			{
				DisplayMessage("ButeMgr: line %d: bad point/rect for '%s'", scan.Line(), sKey.c_str());
				return false;
			}
		}
		else if (cOpen == '[')
		{
			scan.Accept('[');
			if (ReadList(scan, ']', vals, 2) != 2)
			{
				DisplayMessage("ButeMgr: line %d: bad range for '%s'", scan.Line(), sKey.c_str());
				return false;
			}
			pItem->Init(RangeType, CARange(vals[0], vals[1]));
		}
		else
		{
			double dVal;
			bool bReal, bHex;
			if (scan.ReadNumber(dVal, bReal, bHex))
			{
				if (bReal)		pItem->Init(DoubleType, dVal);
				else if (bHex)	pItem->Init(DwordType, (DWORD)dVal);
				else			pItem->Init(IntType, (int)dVal);
			}
			else
			{
				std::string sWord = scan.ReadWord(";/");
				if (stricmp(sWord.c_str(), "TRUE") == 0)
					pItem->Init(BoolType, true);
				else if (stricmp(sWord.c_str(), "FALSE") == 0)
					pItem->Init(BoolType, false);
				else
				{
					DisplayMessage("ButeMgr: line %d: bad value '%s' for '%s'", scan.Line(), sWord.c_str(), sKey.c_str());
					return false;
				}
			}
		}

		// Optional statement terminator.
		scan.Accept(';');
	}

	return true;
}


bool CButeMgr::Parse(std::istream& iStream, int decryptCode)
{
	if (decryptCode != 0)
	{
		DisplayMessage("ButeMgr: encrypted attribute files are not supported");
		return false;
	}
	std::ostringstream oss;
	oss << iStream.rdbuf();
	std::string sText = oss.str();
	return ParseText(sText.data(), (unsigned long)sText.size());
}

bool CButeMgr::Parse(std::istream& /*iCrypt*/, int /*nLen*/, const char* /*cryptKey*/)
{
	DisplayMessage("ButeMgr: encrypted attribute files are not supported");
	return false;
}

bool CButeMgr::Parse(CString sAttributeFilename, int decryptCode)
{
	FILE* fp = fopen(sAttributeFilename, "rb");
	if (!fp)
		return false;

	std::string sText;
	char buf[8192];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
		sText.append(buf, n);
	fclose(fp);

	m_sAttributeFilename = sAttributeFilename;
	return Parse((void*)sText.data(), (unsigned long)sText.size(), decryptCode);
}

bool CButeMgr::Parse(CString sAttributeFilename, const char* /*cryptKey*/)
{
	DisplayMessage("ButeMgr: encrypted attribute files are not supported (%s)", (const char*)sAttributeFilename);
	return false;
}

bool CButeMgr::Parse(void* pData, unsigned long size, int decryptCode)
{
	if (!pData)
		return false;
	if (decryptCode != 0)
	{
		DisplayMessage("ButeMgr: encrypted attribute files are not supported");
		return false;
	}
	return ParseText((const char*)pData, size);
}

bool CButeMgr::Parse(void* /*pData*/, unsigned long /*size*/, const char* /*cryptKey*/)
{
	DisplayMessage("ButeMgr: encrypted attribute files are not supported");
	return false;
}

#if defined(_USE_REZFILE_)
bool CButeMgr::Parse(CRezItm* pItem, int decryptCode)
{
	if (!pItem)
		return false;
	bool bRet = Parse(pItem->Load(), pItem->GetSize(), decryptCode);
	pItem->UnLoad();
	return bRet;
}

bool CButeMgr::Parse(CRezItm* /*pItem*/, const char* /*cryptKey*/)
{
	DisplayMessage("ButeMgr: encrypted attribute files are not supported");
	return false;
}
#endif


// ----------------------------------------------------------------------- //
//	Saving
// ----------------------------------------------------------------------- //

namespace
{
	// Formats a double so that it reads back as a double (always has a '.').
	void FormatReal(char* szOut, size_t nSize, double d)
	{
		_snprintf(szOut, nSize - 1, "%.9g", d);
		szOut[nSize - 1] = '\0';
		if (!strpbrk(szOut, ".eEn"))
			strncat(szOut, ".0", nSize - strlen(szOut) - 1);
	}
}

bool CButeMgr::Save(const char* szNewFileName)
{
	const char* szFile = szNewFileName ? szNewFileName : (const char*)m_sAttributeFilename;
	if (!szFile || !szFile[0])
	{
		DisplayMessage("ButeMgr: Save called with no filename");
		return false;
	}

	FILE* fp = fopen(szFile, "wt");
	if (!fp)
	{
		DisplayMessage("ButeMgr: could not open '%s' for writing", szFile);
		return false;
	}

	char a[64], b[64], c[64];
	for (size_t t = 0; t < m_pImpl->tags.size(); ++t)
	{
		Tag* pTag = m_pImpl->tags[t];
		fprintf(fp, "%s[%s]\n\n", t ? "\n" : "", pTag->name.c_str());

		for (size_t k = 0; k < pTag->keys.size(); ++k)
		{
			CSymTabItem* p = pTag->keys[k].pItem;
			fprintf(fp, "%s = ", pTag->keys[k].name.c_str());
			switch (p->SymType)
			{
			case IntType:		fprintf(fp, "%d", p->data.i);										break;
			// Typed as the original ButeMgr writes them: its reader (in the retail game DLLs)
			// can't parse "0x..." and would read a plain number as an int.
			case DwordType:		fprintf(fp, "(DWORD)%lu", (unsigned long)p->data.dw);				break;
			case ByteType:		fprintf(fp, "(BYTE)%d", (int)p->data.byte);							break;
			case BoolType:		fprintf(fp, "%s", p->data.b ? "TRUE" : "FALSE");					break;
			case DoubleType:	FormatReal(a, sizeof(a), p->data.d); fprintf(fp, "%s", a);			break;
			case FloatType:		FormatReal(a, sizeof(a), p->data.f); fprintf(fp, "%s", a);			break;
			case StringType:	fprintf(fp, "\"%s\"", (const char*)*p->data.s);						break;
			case RectType:		fprintf(fp, "(%ld, %ld, %ld, %ld)", (long)p->data.r->left, (long)p->data.r->top,
									(long)p->data.r->right, (long)p->data.r->bottom);				break;
			case PointType:		fprintf(fp, "(%ld, %ld)", (long)p->data.point->x, (long)p->data.point->y);	break;
			case VectorType:
				FormatReal(a, sizeof(a), p->data.v->Geti());
				FormatReal(b, sizeof(b), p->data.v->Getj());
				FormatReal(c, sizeof(c), p->data.v->Getk());
				fprintf(fp, "<%s, %s, %s>", a, b, c);
				break;
			case RangeType:
				FormatReal(a, sizeof(a), p->data.range->GetMin());
				FormatReal(b, sizeof(b), p->data.range->GetMax());
				fprintf(fp, "[%s, %s]", a, b);
				break;
			default:			fprintf(fp, "0");													break;
			}
			fprintf(fp, "\n");
		}
	}

	bool bOk = (ferror(fp) == 0);
	fclose(fp);
	return bOk;
}


// ----------------------------------------------------------------------- //
//	Enumeration
// ----------------------------------------------------------------------- //

// The original ButeMgr kept tags (and each tag's keys) in STLport hash_maps, and GetTags/GetKeys
// walked those maps, so callers saw hash order rather than file order. Game code numbers things
// by that order - character sets, weapons, barrels, ammo, sounds, animations - and those numbers
// travel between client and server, which may be the retail (original ButeMgr) build. So the
// order must match exactly. This replays STLport's (SGI) hashtable: 193 buckets to start (the
// first prime >= 100), growth to the next prime before an insert that would exceed the bucket
// count, new nodes at the head of their bucket's chain, rehashing that relinks each old chain
// head-first, and iteration bucket by bucket from each chain's head.

static unsigned long HashNoCase(const char* s)
{
	// hash_str_nocase from the original butemgr.h
	unsigned long h = 0;
	for (; *s; ++s)
		h = 5 * h + tolower(*s);
	return h;
}

static unsigned long NextHashSize(unsigned long n)
{
	static const unsigned long primes[] =
	{
		53ul, 97ul, 193ul, 389ul, 769ul, 1543ul, 3079ul, 6151ul, 12289ul, 24593ul, 49157ul,
		98317ul, 196613ul, 393241ul, 786433ul, 1572869ul, 3145739ul, 6291469ul, 12582917ul,
		25165843ul, 50331653ul, 100663319ul, 201326611ul, 402653189ul, 805306457ul,
		1610612741ul, 3221225473ul, 4294967291ul
	};
	for (size_t i = 0; i < sizeof(primes) / sizeof(primes[0]); ++i)
		if (primes[i] >= n)
			return primes[i];
	return primes[sizeof(primes) / sizeof(primes[0]) - 1];
}

// Given names in insertion order, returns their indices in STLport hash_map iteration order.
template <class GetName>
static std::vector<size_t> HashMapOrder(size_t nCount, GetName getName)
{
	std::vector< std::vector<size_t> > buckets(NextHashSize(100));	// each chain, head first
	for (size_t i = 0; i < nCount; ++i)
	{
		if (i + 1 > buckets.size())
		{
			unsigned long nNew = NextHashSize((unsigned long)(i + 1));
			if (nNew > buckets.size())
			{
				std::vector< std::vector<size_t> > rehashed(nNew);
				for (size_t b = 0; b < buckets.size(); ++b)
					for (size_t c = 0; c < buckets[b].size(); ++c)	// old chain head first,
					{												// each pushed to a new head
						std::vector<size_t>& chain = rehashed[HashNoCase(getName(buckets[b][c])) % nNew];
						chain.insert(chain.begin(), buckets[b][c]);
					}
				buckets.swap(rehashed);
			}
		}
		std::vector<size_t>& chain = buckets[HashNoCase(getName(i)) % buckets.size()];
		chain.insert(chain.begin(), i);
	}

	std::vector<size_t> order;
	order.reserve(nCount);
	for (size_t b = 0; b < buckets.size(); ++b)
		order.insert(order.end(), buckets[b].begin(), buckets[b].end());
	return order;
}

void CButeMgr::GetTags(GetTagsCallback pCallback, void* pContext)
{
	if (!pCallback) return;
	std::vector<Tag*>& tags = m_pImpl->tags;
	std::vector<size_t> order = HashMapOrder(tags.size(), [&](size_t i) { return tags[i]->name.c_str(); });
	for (size_t t = 0; t < order.size(); ++t)
	{
		if (!pCallback(tags[order[t]]->name.c_str(), pContext))
			break;
	}
}

void CButeMgr::GetKeys(const char* pszTagName, GetKeysCallback pCallback, void* pContext)
{
	Tag* pTag = m_pImpl->FindTag(pszTagName);
	if (!pTag || !pCallback) return;
	std::vector<size_t> order = HashMapOrder(pTag->keys.size(), [&](size_t i) { return pTag->keys[i].name.c_str(); });
	for (size_t k = 0; k < order.size(); ++k)
	{
		if (!pCallback(pTag->keys[order[k]].name.c_str(), pTag->keys[order[k]].pItem, pContext))
			break;
	}
}


// ----------------------------------------------------------------------- //
//	Lookup helpers
// ----------------------------------------------------------------------- //

CButeMgr::CSymTabItem* CButeMgr::Find(const char* szTagName, const char* szAttName)
{
	return CButeMgrImpl::FindItem(m_pImpl->FindTag(szTagName), szAttName);
}

CButeMgr::CSymTabItem* CButeMgr::FindOrReport(const char* szTagName, const char* szAttName)
{
	CSymTabItem* pItem = Find(szTagName, szAttName);
	if (!pItem)
	{
		m_bSuccess = false;
		DisplayMessage("ButeMgr: attribute '%s' not found in tag '%s'",
			szAttName ? szAttName : "", szTagName ? szTagName : "");
	}
	return pItem;
}

CButeMgr::CSymTabItem* CButeMgr::FindOrCreate(const char* szTagName, const char* szAttName)
{
	return CButeMgrImpl::FindOrCreateItem(m_pImpl->FindOrCreateTag(szTagName), szAttName);
}

bool CButeMgr::GetNumber(CSymTabItem* pItem, double& dVal)
{
	switch (pItem->SymType)
	{
	case IntType:		dVal = pItem->data.i;				return true;
	case DwordType:		dVal = pItem->data.dw;				return true;
	case ByteType:		dVal = pItem->data.byte;			return true;
	case BoolType:		dVal = pItem->data.b ? 1.0 : 0.0;	return true;
	case DoubleType:	dVal = pItem->data.d;				return true;
	case FloatType:		dVal = pItem->data.f;				return true;
	default:												return false;
	}
}

bool CButeMgr::Exist(const char* szTagName, const char* szAttName)
{
	if (!szAttName)
		return m_pImpl->FindTag(szTagName) != NULL;
	return Find(szTagName, szAttName) != NULL;
}

bool CButeMgr::AddTag(const char* szTagName)
{
	if (!szTagName || !szTagName[0]) return false;
	m_pImpl->FindOrCreateTag(szTagName);
	return true;
}

CButeMgr::SymTypes CButeMgr::GetType(const char* szTagName, const char* szAttName)
{
	CSymTabItem* pItem = Find(szTagName, szAttName);
	return pItem ? pItem->SymType : NullType;
}


// ----------------------------------------------------------------------- //
//	Numeric getters/setters
// ----------------------------------------------------------------------- //

// With a default: missing or non-numeric attributes quietly return the default.
// Without: they also report an error through the display function.
#define BUTEMGR_NUMERIC(Name, T, eType, Member)											\
	T CButeMgr::Get##Name(const char* szTagName, const char* szAttName, T defVal)			\
	{																						\
		m_bSuccess = true;																	\
		CSymTabItem* pItem = Find(szTagName, szAttName);									\
		double d;																			\
		if (!pItem || !GetNumber(pItem, d)) { m_bSuccess = false; return defVal; }			\
		if (pItem->SymType == eType) return (T)pItem->data.Member;						\
		return (T)d;																		\
	}																						\
	T CButeMgr::Get##Name(const char* szTagName, const char* szAttName)						\
	{																						\
		m_bSuccess = true;																	\
		CSymTabItem* pItem = FindOrReport(szTagName, szAttName);							\
		double d;																			\
		if (!pItem) return (T)0;															\
		if (!GetNumber(pItem, d))															\
		{																					\
			m_bSuccess = false;																\
			DisplayMessage("ButeMgr: attribute '%s' in tag '%s' is not a number",			\
				szAttName, szTagName);														\
			return (T)0;																	\
		}																					\
		if (pItem->SymType == eType) return (T)pItem->data.Member;						\
		return (T)d;																		\
	}																						\
	void CButeMgr::Set##Name(const char* szTagName, const char* szAttName, T val)			\
	{																						\
		FindOrCreate(szTagName, szAttName)->Init(eType, val);								\
	}

BUTEMGR_NUMERIC(Int,	int,	IntType,	i)
BUTEMGR_NUMERIC(Dword,	DWORD,	DwordType,	dw)
BUTEMGR_NUMERIC(Byte,	BYTE,	ByteType,	byte)
BUTEMGR_NUMERIC(Float,	float,	FloatType,	f)
BUTEMGR_NUMERIC(Double,	double,	DoubleType,	d)

#undef BUTEMGR_NUMERIC

bool CButeMgr::GetBool(const char* szTagName, const char* szAttName, bool defVal)
{
	m_bSuccess = true;
	CSymTabItem* pItem = Find(szTagName, szAttName);
	double d;
	if (!pItem || !GetNumber(pItem, d)) { m_bSuccess = false; return defVal; }
	return d != 0.0;
}

bool CButeMgr::GetBool(const char* szTagName, const char* szAttName)
{
	m_bSuccess = true;
	CSymTabItem* pItem = FindOrReport(szTagName, szAttName);
	double d;
	if (!pItem) return false;
	if (!GetNumber(pItem, d))
	{
		m_bSuccess = false;
		DisplayMessage("ButeMgr: attribute '%s' in tag '%s' is not a bool", szAttName, szTagName);
		return false;
	}
	return d != 0.0;
}

void CButeMgr::SetBool(const char* szTagName, const char* szAttName, bool val)
{
	FindOrCreate(szTagName, szAttName)->Init(BoolType, val);
}


// ----------------------------------------------------------------------- //
//	Strings
// ----------------------------------------------------------------------- //

CString CButeMgr::GetString(const char* szTagName, const char* szAttName, const CString& defVal)
{
	m_bSuccess = true;
	CSymTabItem* pItem = Find(szTagName, szAttName);
	if (!pItem || pItem->SymType != StringType) { m_bSuccess = false; return defVal; }
	return *pItem->data.s;
}

CString CButeMgr::GetString(const char* szTagName, const char* szAttName)
{
	m_bSuccess = true;
	CSymTabItem* pItem = FindOrReport(szTagName, szAttName);
	if (!pItem) return CString("");
	if (pItem->SymType != StringType)
	{
		m_bSuccess = false;
		DisplayMessage("ButeMgr: attribute '%s' in tag '%s' is not a string", szAttName, szTagName);
		return CString("");
	}
	return *pItem->data.s;
}

void CButeMgr::SetString(const char* szTagName, const char* szAttName, const CString& val)
{
	FindOrCreate(szTagName, szAttName)->Init(StringType, val);
}

static void CopyResult(char* szResult, DWORD maxLen, const char* szSrc)
{
	if (!szResult || maxLen == 0) return;
	strncpy(szResult, szSrc ? szSrc : "", maxLen - 1);
	szResult[maxLen - 1] = '\0';
}

void CButeMgr::GetString(const char* szTagName, const char* szAttName, const char* defVal, char* szResult, DWORD maxLen)
{
	m_bSuccess = true;
	CSymTabItem* pItem = Find(szTagName, szAttName);
	if (!pItem || pItem->SymType != StringType)
	{
		m_bSuccess = false;
		CopyResult(szResult, maxLen, defVal);
		return;
	}
	CopyResult(szResult, maxLen, *pItem->data.s);
}

void CButeMgr::GetString(const char* szTagName, const char* szAttName, char* szResult, DWORD maxLen)
{
	CString s = GetString(szTagName, szAttName);
	CopyResult(szResult, maxLen, m_bSuccess ? (const char*)s : "");
}


// ----------------------------------------------------------------------- //
//	Compound types (returned by reference, as in the original interface)
// ----------------------------------------------------------------------- //

#define BUTEMGR_COMPOUND(Name, T, eType, Member, Dummy)									\
	T& CButeMgr::Get##Name(const char* szTagName, const char* szAttName, T& defVal)			\
	{																						\
		m_bSuccess = true;																	\
		CSymTabItem* pItem = Find(szTagName, szAttName);									\
		if (!pItem || pItem->SymType != eType) { m_bSuccess = false; return defVal; }		\
		return *pItem->data.Member;															\
	}																						\
	T& CButeMgr::Get##Name(const char* szTagName, const char* szAttName)					\
	{																						\
		m_bSuccess = true;																	\
		CSymTabItem* pItem = FindOrReport(szTagName, szAttName);							\
		if (!pItem) return m_pImpl->Dummy;													\
		if (pItem->SymType != eType)														\
		{																					\
			m_bSuccess = false;																\
			DisplayMessage("ButeMgr: attribute '%s' in tag '%s' has the wrong type",		\
				szAttName, szTagName);														\
			return m_pImpl->Dummy;															\
		}																					\
		return *pItem->data.Member;															\
	}																						\
	void CButeMgr::Set##Name(const char* szTagName, const char* szAttName, const T& val)	\
	{																						\
		FindOrCreate(szTagName, szAttName)->Init(eType, val);								\
	}

BUTEMGR_COMPOUND(Rect,		CRect,		RectType,	r,		dummyRect)
BUTEMGR_COMPOUND(Point,		CPoint,		PointType,	point,	dummyPoint)
BUTEMGR_COMPOUND(Vector,	CAVector,	VectorType,	v,		dummyVector)
BUTEMGR_COMPOUND(Range,		CARange,	RangeType,	range,	dummyRange)

#undef BUTEMGR_COMPOUND
