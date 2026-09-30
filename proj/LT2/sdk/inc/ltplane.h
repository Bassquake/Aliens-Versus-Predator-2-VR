//***************************************************************************************************
//
// PROJECT:	Lithtech
//
// UPDATED:	5/2/00
//
//***************************************************************************************************

#ifndef __LTPLANE_H__
#define __LTPLANE_H__

//***************************************************************************************************
	
#define LTPLANE_INTERSECT_NONE			0
#define LTPLANE_INTERSECT_HIT			1

#define LTPLANE_INTERSECT_FRONT			1		// The intersect happened from the front of the plane
#define LTPLANE_INTERSECT_ON			2		// The intersect happened exactly on the plane
#define LTPLANE_INTERSECT_BACK			3		// The intersect happened from the back of the plane

//***************************************************************************************************

#define LTPLANE_RAY_INTERSECT_OFFSET	5000.0f

//***************************************************************************************************

template<class T>
class _CPlane
{
	public:
		
		_CPlane<T>()						{}
		_CPlane<T>( _CVector<T> n, T dist )	{ m_Normal=n; m_Dist=dist; }

		void operator=(_CPlane<T> p)	{ m_Normal=p.m_Normal; m_Dist=(T)p.m_Dist; }
//			void operator=(_CPlane<double> p)	{ m_Normal=p.m_Normal; m_Dist=(T)p.m_Dist; }
		_CPlane<T>	operator-()				{ return _CPlane<T>(-m_Normal, -m_Dist); }


		T					DistTo(_CVector<T> &vec) {return (m_Normal.x*vec.x + m_Normal.y*vec.y + m_Normal.z*vec.z) - m_Dist;}

		_CVector<T>&		Normal()		{ return m_Normal; }
		T&					Dist()			{ return m_Dist; }
		
		_CVector<T>			m_Normal;
		T					m_Dist;

};


class LTPlane
{

public:

	inline LTPlane() {}

	inline LTPlane(float x, float y, float z, float dist)
		: m_Normal(x,y,z),
		  m_Dist(dist)     {}

	inline LTPlane(const LTVector & normal, float dist)
		: m_Normal(normal),
		  m_Dist(dist) { }

	inline void	Init(const LTVector & vec, float dist)
	{
		m_Normal = vec;
		m_Dist = dist;
	}

	inline void Init(const LTVector &pt1, const LTVector &pt2, const LTVector &pt3)
	{
		LTVector vAxis1 = pt3 - pt1;
		LTVector vAxis2 = pt2 - pt1;

		m_Normal = vAxis1.Cross(vAxis2);
		m_Normal.Norm();

		m_Dist = m_Normal.Dot(pt1);
	}

	inline float DistTo(const LTVector & vec) const
	{
		return m_Normal.Dot(vec) - m_Dist;
	}

	inline LTPlane	operator-()	const {return LTPlane(-m_Normal, -m_Dist);}

	inline LTBOOL	operator==(const LTPlane & rhs) const {return (m_Normal == rhs.m_Normal) && (m_Dist == rhs.m_Dist);}
	inline LTBOOL	operator!=(const LTPlane & rhs) const {return (m_Normal != rhs.m_Normal) || (m_Dist != rhs.m_Dist);}

	inline const LTVector&	Normal()	const {return m_Normal;}
	inline float			Dist()		const {return m_Dist;}

	//----------------------------------------------------------------------------------------------
	// Intersection helping functions

	int RayIntersect(const LTVector &vOrigin, const LTVector &vDir, int &pInfo, LTVector *vIntersectPt = 0);
	int LineIntersect(const LTVector &vStart, const LTVector &vEnd, int &pInfo, LTVector *vIntersectPt = 0);
	int SphereIntersect(const LTVector &vOrigin, const float fRadius, int &pInfo, LTVector *vIntersectPt = 0, float *fIntersectRadius = 0);

public:

	LTVector	m_Normal;
	float		m_Dist;
};

//***************************************************************************************************

inline int LTPlane::RayIntersect(const LTVector &vOrigin, const LTVector &vDir, int &pInfo, LTVector *vIntersectPt)
{
	// Get the side of the plane that the origin is on
	float fOriginDist = DistTo(vOrigin);

	// If the origin lies on the plane, return true collision info
	if(fOriginDist == 0.0f)
	{
		pInfo = LTPLANE_INTERSECT_ON;
		if(vIntersectPt)	*vIntersectPt = vOrigin;
		return LTPLANE_INTERSECT_HIT;
	}

	// Get the direction of a point on the plane forming a perpendicular line with the origin
	LTVector vPerpDir = m_Normal * -fOriginDist;

	// Get the angle of the direction with respect to that point
	float fAngle = vPerpDir.Dot(vDir);

	// If the direction is perpendicular to the plane or facing away, return false collision info
	if(fAngle <= 0.0f)
	{
		pInfo = (fOriginDist > 0.0f) ? LTPLANE_INTERSECT_FRONT : LTPLANE_INTERSECT_BACK;
		return LTPLANE_INTERSECT_NONE;
	}

	// If we don't want collsion point information, just return here
	if(!vIntersectPt)
		return LTPLANE_INTERSECT_HIT;

	// Otherwise, look for the first available point which is on the other side of the plane
	LTVector vOffset = vDir * LTPLANE_RAY_INTERSECT_OFFSET;
	LTVector vDest = vOrigin + vOffset;
	float fDestDist = DistTo(vDest);

	if(fOriginDist > 0.0f)
	{
		while(fDestDist >= 0.0f)
		{
			vDest += vOffset;
			fDestDist = DistTo(vDest);
		}
	}
	else
	{
		while(fDestDist <= 0.0f)
		{
			vDest += vOffset;
			fDestDist = DistTo(vDest);
		}
	}

	// Now there is a cross collision with the plane
	pInfo = (fOriginDist > 0.0f) ? LTPLANE_INTERSECT_FRONT : LTPLANE_INTERSECT_BACK;

	// Calculate the intersection point
	LTVector vFinalDir = vDest - vOrigin;
	float fLength = vFinalDir.Mag();
	float fT = fOriginDist * fLength / (fOriginDist - fDestDist);

	// Set the direction to the appropriate collision length
	vFinalDir.Norm(fT);
	*vIntersectPt = vOrigin + vFinalDir;

	return LTPLANE_INTERSECT_HIT;
}

//***************************************************************************************************

inline int LTPlane::LineIntersect(const LTVector &vStart, const LTVector &vEnd, int &pInfo, LTVector *vIntersectPt)
{
	// Get the distances of the points from the plane
	float fStartDist = DistTo(vStart);
	float fEndDist = DistTo(vEnd);

	// If the points are on the same side of the plane, return false collision info
	if((fStartDist > 0.0f && fEndDist > 0.0f) || (fStartDist < 0.0f && fEndDist < 0.0f))
	{
		pInfo = (fStartDist > 0.0f) ? LTPLANE_INTERSECT_FRONT : LTPLANE_INTERSECT_BACK;
		return LTPLANE_INTERSECT_NONE;
	}

	// If the start point is on the plane... return true collision info with the start point
	if(fStartDist == 0.0f)
	{
		pInfo = LTPLANE_INTERSECT_ON;
		if(vIntersectPt)	*vIntersectPt = vStart;
		return LTPLANE_INTERSECT_HIT;
	}

	// If the end point is on the plane... return true collision info with the end point
	if(fEndDist == 0.0f)
	{
		pInfo = (fStartDist > 0.0f) ? LTPLANE_INTERSECT_FRONT : LTPLANE_INTERSECT_BACK;
		if(vIntersectPt)	*vIntersectPt = vEnd;
		return LTPLANE_INTERSECT_HIT;
	}

	// Otherwise there was a cross collision with the plane
	pInfo = (fStartDist > 0.0f) ? LTPLANE_INTERSECT_FRONT : LTPLANE_INTERSECT_BACK;

	// If we don't want collsion point information, just return here
	if(!vIntersectPt)
		return LTPLANE_INTERSECT_HIT;

	// Calculate the intersection point
	LTVector vDir = vEnd - vStart;
	float fLength = vDir.Mag();
	float fT = fStartDist * fLength / (fStartDist - fEndDist);

	// Set the direction to the appropriate collision length
	vDir.Norm(fT);
	*vIntersectPt = vStart + vDir;

	return LTPLANE_INTERSECT_HIT;
}

//***************************************************************************************************

inline int LTPlane::SphereIntersect(const LTVector &vOrigin, const float fRadius, int &pInfo, LTVector *vIntersectPt, float *fIntersectRadius)
{
	// Get the side of the plane that the origin is on
	float fOriginDist = DistTo(vOrigin);

	// If the sphere is completely on the front side, return false collision info
	if(fOriginDist > fRadius)
	{
		pInfo = LTPLANE_INTERSECT_FRONT;
		return LTPLANE_INTERSECT_NONE;
	}

	// If the sphere is completely on the back side, return false collision info
	if(fOriginDist < -fRadius)
	{
		pInfo = LTPLANE_INTERSECT_BACK;
		return LTPLANE_INTERSECT_NONE;
	}

	// If the sphere lies directly on the plane, return true collision info
	if(fOriginDist == 0.0f)
	{
		pInfo = LTPLANE_INTERSECT_ON;
		if(vIntersectPt)		*vIntersectPt = vOrigin;
		if(fIntersectRadius)	*fIntersectRadius = fRadius;
		return LTPLANE_INTERSECT_HIT;
	}

	// If we don't want other collision info, just return here
	if(!vIntersectPt)
		return LTPLANE_INTERSECT_HIT;

	// Otherwise, calculate the intersect point
	*vIntersectPt = vOrigin + (m_Normal * -fOriginDist);

	// Now calculate the intersection radius
	if(fIntersectRadius)
	{
		float fUnitX = fOriginDist / fRadius;
		*fIntersectRadius = (float)sin(fUnitX);
	}

	return LTPLANE_INTERSECT_HIT;
}

//***************************************************************************************************

#endif


