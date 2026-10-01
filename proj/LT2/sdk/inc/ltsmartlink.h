#ifndef __LTSMARTLINK_H__
#define __LTSMARTLINK_H__

/////////////////////////////////////////
//
//  LTSmartLink
//
//  LTSmartLink can be used just as an HOBJECT.  It will return a null HOBJECT when that object is 
//  destroyed.  It currently only works with server objects.  
//
//  An LTSmartLink may be constructed with an HOBJECT or assigned (operator=) an HOBJECT.  It has 
//  a conversion operator to HOBJECT so that it can be used as an argument to a function which
//  takes an HOBJECT.  It also has GetHOBJECT() to force the conversion when needed (like to check
//  if the HOBJECT has been nulled).
//
//  The boolean equality and less than operators are overloaded to compare the HOBJECT to which 
//  the LTSmartLink is refering.  The operators are also overloaded to take an LTSmartLink and
//  an HOBJECT.  An un-assigned LTSmartLink is equivalent to a null HOBJECT or an LTSmartLink
//  with a dead object (which returns a null HOBJECT).
//
//  IsValid() returns true iff LTSmartLink has been explicitly set to null.  Hence, an LTSmartLink
//  which returns a null HOBJECT may have been pointing to an object which has been subsequently
//  removed or may have been assigned a null.  While an invalid (IsValid() == false) LTSmartLink
//  has been explicitly assigned a null (through constructor or operator=).
//
/////////////////////////////////////////

	#include "iltserver.h"

	class CServerMgr;
	class LTObject;




	// LTSmartLink is implemented as much as possible within the header to inline it's dereferencing.
	struct LTSmartLink_Body
	{
		// Use incReferences and decReferences to increment and decrement m_Count.
		int m_nCount;
		LTObject  * m_pObject;
		ILTServer * m_pServer;

		LTSmartLink_Body(LTObject * object) 
			: m_nCount(0), 
			  m_pObject(object),
			  m_pServer(LTNULL) { }


		void decReferences()
		{
			--m_nCount;
			if( m_nCount <= 0  )
			{
				// If this isn't true, then the reference counting is messed up!
				ASSERT( m_nCount == 0 );

				ASSERT( m_pServer );
				if( !m_pServer ) return;

				// The following statement is an equivalent to "delete this".
				// So don't do anything after this line.
				m_pServer->ReleaseSmartLinkBody(this);
			}
		}
		
		void incReferences()
		{
			++m_nCount;
		}

	};

	class LTSmartLink
	{

	public:

		// Constructors and Destructor
		LTSmartLink( HOBJECT hObject = LTNULL, ILTServer * pServer = g_pLTServer )
			: m_pBody(LTNULL),
			  m_pServer(pServer)
		{
			if( hObject ) Init(hObject);
		}

		LTSmartLink(const LTSmartLink & other) 
			: m_pBody(other.m_pBody),
			  m_pServer(other.m_pServer)
		{
			if( other.m_pBody ) other.m_pBody->incReferences();
		}

		~LTSmartLink()
		{
			if( m_pBody )
			{
				m_pBody->decReferences();
			}
		}

		
		// Member Functions
		HOBJECT GetHOBJECT() const
		{
			return m_pBody ? m_pBody->m_pObject : 0;
		}

		LTBOOL IsValid() const
		{
			return m_pBody == 0;
		}


		// Operators, conversion, assignment and comparison are implemented.
		
		operator HOBJECT() const
		{
			return GetHOBJECT();
		}

		LTSmartLink & operator=(const LTSmartLink & other)
		{
			if( this != &other )
			{
				if( other.m_pBody != m_pBody )
				{
					if( other.m_pBody ) other.m_pBody->incReferences();
					if( m_pBody )		m_pBody->decReferences();
				
					m_pBody = other.m_pBody;
				}

			}
			return *this;
		}

		LTSmartLink & operator=(HOBJECT hObject)
		{
			if( m_pBody ) m_pBody->decReferences();

			Init(hObject);

			return *this;
		}

		
		
		friend inline bool operator==(const LTSmartLink & lhs, const LTSmartLink & rhs)
		{
			return    (lhs.m_pBody ? lhs.m_pBody->m_pObject : 0) == (rhs.m_pBody ? rhs.m_pBody->m_pObject : 0);
		}

		friend inline bool operator==(const LTSmartLink & lhs, const HOBJECT rhs)
		{
			return (lhs.m_pBody ? lhs.m_pBody->m_pObject : 0) == rhs;
		}

		friend inline bool operator==(const HOBJECT lhs, const LTSmartLink & rhs)
		{
			return lhs == (rhs.m_pBody ? rhs.m_pBody->m_pObject : 0);
		}


		
		friend inline bool operator<(const LTSmartLink & lhs, const LTSmartLink & rhs)
		{
			return (lhs.m_pBody ? lhs.m_pBody->m_pObject : 0) < (rhs.m_pBody ? rhs.m_pBody->m_pObject : 0);
		}

		friend inline bool operator<(const LTSmartLink & lhs, const HOBJECT rhs)
		{
			return (lhs.m_pBody ? lhs.m_pBody->m_pObject : 0) < rhs;
		}

		friend inline bool operator<(const HOBJECT lhs, const LTSmartLink & rhs)
		{
			return lhs < (rhs.m_pBody ? rhs.m_pBody->m_pObject : 0);
		}

	private :

		void Init(HOBJECT hObject)
		{
			ASSERT( m_pServer );
			if( !m_pServer ) return;

			if( hObject == LTNULL )
			{
				m_pBody = LTNULL;
				return;
			}

			if( LT_OK == m_pServer->GetSmartLinkBody(hObject, &m_pBody) )
			{
				if( m_pBody )
				{
					m_pBody->m_pServer = m_pServer;
					m_pBody->incReferences();
				}
			}
			else
			{
				ASSERT( 0 );
				m_pBody = LTNULL;
			}
		}


		LTSmartLink_Body *	m_pBody;
		ILTServer *			m_pServer;

	};


	inline ILTMessage & operator<<(ILTMessage & out, const LTSmartLink & val)
	{
		out.WriteObject(val);
		return out;
	}

	inline ILTMessage & operator>>(ILTMessage & in, LTSmartLink & val)
	{
		val = in.ReadObject();
		return in;
	}

#endif  // __LTSMARTLINK_H__
