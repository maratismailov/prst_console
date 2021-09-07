/*******************************************************************************
 This file is part of PRESTo Early Warning System
 Copyright (C) 2009-2015 Luca Elia

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*******************************************************************************/

/*******************************************************************************

	3D Vectors, rotations and planes

*******************************************************************************/

#ifndef GEOMETRY_H_DEF
#define GEOMETRY_H_DEF

#include <cmath>

#include "global.h"

// When deciding the side of a plane a point lies in, distances smaller
// than the following constant are considered zero
#define PLANE_LITTLEDIST	(KM/1000.0f)

/*******************************************************************************

	vec3_t - a 3D vector of floats

*******************************************************************************/

struct vec3_t
{
	// Data members
	float x,y,z;

	// Constructors / Destructor
	vec3_t()	{	}
	vec3_t(float _x, float _y, float _z) : x(_x),y(_y),z(_z)	{	}

	// Arithmetic operators
	const vec3_t operator + (const vec3_t & v) const
	{
		return vec3_t(x + v.x, y + v.y, z + v.z);
	}

	const vec3_t operator + () const
	{
		return *this;
	}

	const vec3_t operator - (const vec3_t & v) const
	{
		return vec3_t(x - v.x, y - v.y, z - v.z);
	}

	const vec3_t operator - () const
	{
		return vec3_t(-x, -y, -z);
	}

	const vec3_t operator * (const float a) const
	{
		return vec3_t(x*a, y*a, z*a);
	}

	const vec3_t operator / (const float a) const
	{
		return vec3_t(x/a, y/a, z/a);
	}

	// Assignment operators
	vec3_t operator += (const vec3_t & v)
	{
		x += v.x;	y += v.y;	z += v.z;
		return *this;
	}

	vec3_t operator -= (const vec3_t & v)
	{
		x -= v.x;	y -= v.y;	z -= v.z;
		return *this;
	}

	vec3_t operator *= (const float a)
	{
		x *= a;	y *= a;	z *= a;
		return *this;
	}

	// Cross product
	vec3_t operator *= (const vec3_t & v)
	{
		*this = *this * v;
		return *this;
	}

	vec3_t operator /= (const float a)
	{
		x /= a;	y /= a;	z /= a;
		return *this;
	}

	// Cross product
	const vec3_t operator * (const vec3_t & v) const
	{
		return vec3_t(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
	}


	// Normalization
	float Len2() const
	{
		return x*x + y*y + z*z;
	}

	float Len() const
	{
		return sqrt(x*x + y*y + z*z);
	}

	const vec3_t Dir() const
	{
		float len = Len();
		return vec3_t(x / len, y / len, z / len);
	}

	void Normalize()
	{
		float len = Len();
		x /= len;	y /= len;	z /= len;
	}

	// Tests
	bool operator == (const vec3_t & v) const
	{
		return (x == v.x) && (y == v.y) && (z == v.z);
	}

	bool operator != (const vec3_t & v) const
	{
		return (x != v.x) || (y != v.y) || (z != v.z);
	}

	// Rotation (ccw) around an axis or using a matrix
	const vec3_t Rotate			(const vec3_t & axis, const float rads)	const;

	const vec3_t Rotate			(const float m[4 * 4])					const;
	const vec3_t RotateInverse	(const float m[4 * 4])					const;

	// Misc
	float& operator [] (const int i) /*const*/
	{
		switch (i)
		{
			case 0:	return x;
			case 1:	return y;
			default:
			case 2:	return z;
		}
	}

};

inline float DotProduct(const vec3_t & v1, const vec3_t & v2)
{
	return v1.x*v2.x + v1.y*v2.y + v1.z*v2.z;
}


/*******************************************************************************

	rot3_t	-	A 3D rotation implemented as a quaternion

*******************************************************************************/

class rot3_t
{
public:

	// Constructor needs an axis and an angle in radians.
	rot3_t(const vec3_t & axis, float rads);

	// Concatenate with another rotation
	void Concatenate(const vec3_t & axis, float rads);

	// Generate a 4x4 transformation matrix and change the active OpenGL matrix by multiplying with it
	void MultMatrix() const;

private:

	vec3_t v;
	float w;
};


/*******************************************************************************

	plane_t -	a plane is a normal (defining the orientation of the plane
				and a positive half space) and a signed distance from the
				origin (positive if along the direction of the normal)

*******************************************************************************/

enum PLANESIDE_T { PLANESIDE_BACK, PLANESIDE_FRONT, PLANESIDE_BOTH, PLANESIDE_ON };

class plane_t
{

public:

	// Data members
	vec3_t normal;
	float distance;

	bool operator == (const plane_t & rhs) const;
	bool operator != (const plane_t & rhs) const;

	// Constructors/Destructor
	plane_t()	{	};

	plane_t(const vec3_t & _normal, float _distance) : normal(_normal),distance(_distance) {	};

	// Construct a plane containing the points v0,v1,v2.
	// v0->v1->v2 is a clockwise rotation as "seen" from the normal
	plane_t(const vec3_t & v0, const vec3_t & v1, const vec3_t & v2)
	:	normal( ( (v2 - v0) * (v1 - v0) ).Dir() ),
		distance( DotProduct( v0, normal ) )
	{
	}

	plane_t(float A, float B, float C, float D)
	{
		float len = sqrt(A*A + B*B + C*C);
		if (len)
		{
			normal		=	vec3_t(A/len,B/len,C/len);
			distance	=	-D/len;
		}
	}

	// Distance from the plane.
	// It's positive if v lies in the half space that the normal points to.
	float SignedDistance(const vec3_t & v) const
	{
		return DotProduct(v,normal) - distance;
	}

	float UnsignedDistance(const vec3_t & v) const
	{
		return fabs(DotProduct(v,normal) - distance);
	}

	// Projection of a point on the plane
	vec3_t Projection(const vec3_t & v) const
	{
		return v - normal * SignedDistance(v);
	}

	// Which side of the plane a vector lies in?
	// It's FRONT if v lies in the half space that the normal points to.
	PLANESIDE_T Side(const vec3_t & v) const
	{
		float sdist = SignedDistance(v);

		if      (sdist > PLANE_LITTLEDIST)	return PLANESIDE_FRONT;
		else if (sdist < -PLANE_LITTLEDIST)	return PLANESIDE_BACK;
		else								return PLANESIDE_ON;
	}

	// Return true if the segment e0->e1 intersects the plane.
	// In that case t will be the fraction of the total length of the segment
	// that separates e0 from the intersection point (e.g. from 0.0 to 1.0)
	bool IntersectsEdge(const vec3_t & e0, const vec3_t & e1, float *t) const;
};



/*******************************************************************************

	frustum_t

*******************************************************************************/

class frustum_t
{
private:

	plane_t		left_plane,
				right_plane,
				up_plane,
				down_plane,
				near_plane,
				far_plane;

public:

	frustum_t(const plane_t &lp,const plane_t &rp,const plane_t &up,const plane_t &dp,const plane_t &np,const plane_t &fp);
	frustum_t() { }

	plane_t Plane(int i) const;
	bool Contains(const vec3_t & pos) const;
};

frustum_t CalcFrustum();



/*******************************************************************************

	Misc geometric routines

*******************************************************************************/

// Return the angle (in radians) between the x axis and the vector (x,y)
// The angle is between 0 and 2 * PI
float CalcRads(const float x, const float y);

#endif
