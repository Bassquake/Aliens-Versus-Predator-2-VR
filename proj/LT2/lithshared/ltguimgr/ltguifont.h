// LTGUIFont.h: interface for the CLTGUIFont class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_LTGUIFONT_H__30D313E1_736B_11D2_BDBE_0060971BDC6D__INCLUDED_)
#define AFX_LTGUIFONT_H__30D313E1_736B_11D2_BDBE_0060971BDC6D__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "lithfontdefs.h"

// Defines...

#define LGFT_NONE			0	// font types
#define LGFT_BITMAP			1
#define LGFT_ENGINE			2
#define LGFT_ARRAY			3
#define LGFT_MENU			4

#define LGFI_NORMAL			0	// font indices
#define	LGFI_SELECTED		1
#define	LGFI_DISABLED		2

#define LGF_MAX_FONTS		3	// font maximums
#define LGF_MAX_LENGTH		128


// Duel purpose font class.  It can either be inited to use an english bitmap font
// or use an engine font for other languages.
class CLTGUIFont
{
public:
	CLTGUIFont();
	virtual ~CLTGUIFont();

	// Initialize a bitmap font
    LTBOOL   Init(ILTClient *pClientDE, LITHFONTCREATESTRUCT *lfCS); // ALM(7/12/99)

	// Initialize a menu font
    LTBOOL   Init(ILTClient *pClientDE, LITHFONTCREATESTRUCT *sNormalFont, LITHFONTCREATESTRUCT *sSelectedFont, LITHFONTCREATESTRUCT *sDisabledFont); // ALM(7/12/99)


	// Termination
	void	Term();

	// Drawing functions.  Note that engine fonts are always solid.  Calling Draw or DrawFormat
	// on an engine font will just draw the text white.
	void	Draw(HSTRING hString, HSURFACE hDest, int x, int y, int justify, HLTCOLOR color = LTNULL);
	void	Draw(char *szString, HSURFACE hDest, int x, int y, int justify, HLTCOLOR color = LTNULL);

	void	DrawFormat(HSTRING hString, HSURFACE hDest, int x, int y, int nWidth, HLTCOLOR color = LTNULL);
	void	DrawFormat(char *szString, HSURFACE hDest, int x, int y, int nWidth, HLTCOLOR color = LTNULL);

    void    Draw(HSTRING hString, HSURFACE hDest, LITHFONTDRAWDATA *lfDD, int x, int y, LITHFONTSAVEDATA *lfSD = LTNULL);
    void    Draw(char *szString, HSURFACE hDest, LITHFONTDRAWDATA *lfDD, int x, int y, LITHFONTSAVEDATA *lfSD = LTNULL);

	void	DrawMenu(char *szString, HSURFACE hDest, int x, int y, int justify, int state);
	void	DrawMenu(HSTRING hString, HSURFACE hDest, int x, int y, int justify, int state);

	void	DrawMenuFormat(char *szString, HSURFACE hDest, int x, int y, int width, int state);
	void	DrawMenuFormat(HSTRING hString, HSURFACE hDest, int x, int y, int width, int state);


	// Returns the height and width of a text string
    LTIntPt  GetTextExtents(HSTRING hString);
    LTIntPt  GetTextExtents(char* szString);

    LTIntPt  GetTextExtentsFormat(HSTRING hString, int nWidth);
    LTIntPt  GetTextExtentsFormat(char* szString, int nWidth);

    LTIntPt  GetPositionFromIndex(HSTRING hString, int nIndex, int nWidth = -1);
	int		GetIndexFromPosition(HSTRING hString, int x, int y);

	// Returns the height of the font
	int		GetHeight();

	LTBOOL	IsChromaKeyed();
	LTBOOL	IsSystemFont();

	// Returns whether or not the font is "menu friendly"
    LTBOOL   IsMenuFriendly() { return(m_nType == LGFT_MENU); }


	// Sets the wrapping method between using spaces and non-space based wrapping (Japanese)
	// Note that non-space based wrapping only works with engine fonts.
    static  void    SetWrapMethod(LTBOOL bUseSpaces)     { s_bWrapUseSpaces=bUseSpaces; }

protected:
    ILTClient   *m_pClientDE;   // Pointer to the client interface
	int			m_nFontHeight;	// The height passed in when creating an engine font.
	int			m_nType;		// Type of font (bitmap, TT, array, individual)
	LithFont*	m_pLithFonts[LGF_MAX_FONTS];

    static LTBOOL    s_bWrapUseSpaces;   // TRUE if the wrapping method is to use a space as a word separator
};

#endif // !defined(AFX_LTGUIFONT_H__30D313E1_736B_11D2_BDBE_0060971BDC6D__INCLUDED_)