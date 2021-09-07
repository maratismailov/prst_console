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

#include <cstring>
#include "SDL_opengl.h"

#include "geometry.h"

/*******************************************************************************

	vec3_t - a 3D vector of floats

*******************************************************************************/

// Rotate (ccw) around an axis
const vec3_t vec3_t :: Rotate(const vec3_t & axis, const float rads) const
{
	float sine		=	sinf(rads);
	float cosine	=	cosf(rads);

	float cos1x		=	(1-cosine) * axis.x;
	float cos1y		=	(1-cosine) * axis.y;
	float cos1z		=	(1-cosine) * axis.z;

	float cos1xy	=	cos1x * axis.y;
	float cos1xz	=	cos1x * axis.z;
	float cos1yz	=	cos1y * axis.z;

	float sinx		=	sine * axis.x;
	float siny		=	sine * axis.y;
	float sinz		=	sine * axis.z;

	return vec3_t (
		x * (cos1x * axis.x + cosine)	+	y * (cos1xy - sinz)				+	z * (cos1xz + siny),
		x * (cos1xy + sinz)				+	y * (cos1y * axis.y + cosine)	+	z * (cos1yz - sinx),
		x * (cos1xz - siny)				+	y * (cos1yz + sinx)				+	z * (cos1z * axis.z + cosine)
	);
}


// Multiply for the rotational part of a 4x4 matrix in memory
// The first 4 elements are the first row of the matrix (e.g. OpenGL convention)
const vec3_t vec3_t :: Rotate (const float m[4 * 4]) const
{
	return vec3_t (
		m[0] * x + m[4] * y + m[8]  * z,
		m[1] * x + m[5] * y + m[9]  * z,
		m[2] * x + m[6] * y + m[10] * z
	);
}

const vec3_t vec3_t :: RotateInverse (const float m[4 * 4]) const
{
	return vec3_t (
		m[0] * x + m[1] * y + m[2]  * z,
		m[4] * x + m[5] * y + m[6]  * z,
		m[8] * x + m[9] * y + m[10] * z
	);
}



/*******************************************************************************

	rot3_t	-	A 3D rotation implemented as a quaternion

*******************************************************************************/

rot3_t :: rot3_t(const vec3_t & axis, float rads)
{
	rads	/=	2;

	v		=	axis.Dir() * sinf(rads);
	w		=	cosf(rads);
}

void rot3_t :: Concatenate(const vec3_t & axis, float rads)
{
	if (axis.Len2() < 0.0001f)
		return;

	rot3_t r(axis, rads);

	float new_w  = r.w * w - DotProduct( r.v , v );
	v = r.v * w + v * r.w - v * r.v;
	w = new_w;

	float len = sqrt(w*w + v.Len2());
	if (len)
	{
		w /= len;
		v /= len;
	}
}

void rot3_t :: MultMatrix() const
{
	float wx = w * v.x;
	float wy = w * v.y;
	float wz = w * v.z;

	float x2 = v.x * v.x;
	float y2 = v.y * v.y;
	float z2 = v.z * v.z;

	float xy = v.x * v.y;
	float xz = v.x * v.z;
	float yz = v.y * v.z;

	GLfloat m[4 * 4] =
	{
		1 - 2 * (y2 + z2),	2 * (xy + wz),		2 * (xz - wy),		0,
		2 * (xy - wz),		1 - 2 * (x2 + z2),	2 * (yz + wx),		0,
		2 * (xz + wy),		2 * (yz - wx),		1 - 2 * (x2 + y2),	0,
		0,					0,					0,					1
	};

	glMultMatrixf( m );
}


/*******************************************************************************

	plane_t -	a plane is a normal (defining the orientation of the plane
				and a positive half space) and a signed distance from the
				origin (positive if along the direction of the normal)

*******************************************************************************/

bool plane_t :: operator == (const plane_t & rhs) const
{
	return (distance == rhs.distance) && (normal == rhs.normal);
}
bool plane_t :: operator != (const plane_t & rhs) const
{
	return (distance != rhs.distance) || (normal != rhs.normal);
}

bool plane_t :: IntersectsEdge(const vec3_t & e0, const vec3_t & e1, float *t) const
{
	float sdist0 = SignedDistance(e0);
	float sdist1 = SignedDistance(e1);

	// No intersection if e0 and e1 are on the same side
	// (their distances to the plane have the same sign)

	if (sdist0 * sdist1 > 0)
		return false;

	float len = DotProduct(e0 - e1, normal);

	if (len)	*t = sdist0 / len;
	else		*t = 0;

	return true;
}


/*******************************************************************************

	frustum_t

*******************************************************************************/

frustum_t :: frustum_t(const plane_t & lp,const plane_t & rp,const plane_t & up,const plane_t & dp,const plane_t & np,const plane_t & fp)
:   left_plane(lp),
	right_plane(rp),
	up_plane(up),
	down_plane(dp),
	near_plane(np),
	far_plane(fp)
{
}

plane_t frustum_t :: Plane(int i) const
{
	switch (i)
	{
		case 0:	return left_plane;
		case 1:	return right_plane;
		case 2:	return up_plane;
		case 3:	return down_plane;
		case 4:	return near_plane;
        default:
		case 5:	return far_plane;
	}
}

bool frustum_t :: Contains(const vec3_t & pos) const
{
	return
		(left_plane		.	Side(pos)	==	PLANESIDE_FRONT) &&
		(right_plane	.	Side(pos)	==	PLANESIDE_FRONT) &&
		(up_plane		.	Side(pos)	==	PLANESIDE_FRONT) &&
		(down_plane		.	Side(pos)	==	PLANESIDE_FRONT) &&
		(near_plane		.	Side(pos)	==	PLANESIDE_FRONT) &&
		(far_plane		.	Side(pos)	==	PLANESIDE_FRONT) ;
}


void MatrixProduct(GLfloat m1[], GLfloat m2[])
{
	GLfloat res[4 * 4];

	res[ 0] = m1[ 0] * m2[ 0] + m1[ 1] * m2[ 4] + m1[ 2] * m2[ 8] + m1[ 3] * m2[12];
	res[ 1] = m1[ 0] * m2[ 1] + m1[ 1] * m2[ 5] + m1[ 2] * m2[ 9] + m1[ 3] * m2[13];
	res[ 2] = m1[ 0] * m2[ 2] + m1[ 1] * m2[ 6] + m1[ 2] * m2[10] + m1[ 3] * m2[14];
	res[ 3] = m1[ 0] * m2[ 3] + m1[ 1] * m2[ 7] + m1[ 2] * m2[11] + m1[ 3] * m2[15];

	res[ 4] = m1[ 4] * m2[ 0] + m1[ 5] * m2[ 4] + m1[ 6] * m2[ 8] + m1[ 7] * m2[12];
	res[ 5] = m1[ 4] * m2[ 1] + m1[ 5] * m2[ 5] + m1[ 6] * m2[ 9] + m1[ 7] * m2[13];
	res[ 6] = m1[ 4] * m2[ 2] + m1[ 5] * m2[ 6] + m1[ 6] * m2[10] + m1[ 7] * m2[14];
	res[ 7] = m1[ 4] * m2[ 3] + m1[ 5] * m2[ 7] + m1[ 6] * m2[11] + m1[ 7] * m2[15];

	res[ 8] = m1[ 8] * m2[ 0] + m1[ 9] * m2[ 4] + m1[10] * m2[ 8] + m1[11] * m2[12];
	res[ 9] = m1[ 8] * m2[ 1] + m1[ 9] * m2[ 5] + m1[10] * m2[ 9] + m1[11] * m2[13];
	res[10] = m1[ 8] * m2[ 2] + m1[ 9] * m2[ 6] + m1[10] * m2[10] + m1[11] * m2[14];
	res[11] = m1[ 8] * m2[ 3] + m1[ 9] * m2[ 7] + m1[10] * m2[11] + m1[11] * m2[15];

	res[12] = m1[12] * m2[ 0] + m1[13] * m2[ 4] + m1[14] * m2[ 8] + m1[15] * m2[12];
	res[13] = m1[12] * m2[ 1] + m1[13] * m2[ 5] + m1[14] * m2[ 9] + m1[15] * m2[13];
	res[14] = m1[12] * m2[ 2] + m1[13] * m2[ 6] + m1[14] * m2[10] + m1[15] * m2[14];
	res[15] = m1[12] * m2[ 3] + m1[13] * m2[ 7] + m1[14] * m2[11] + m1[15] * m2[15];

	memcpy(m1,res,sizeof(res));
}


frustum_t CalcFrustum()
{
	GLfloat m[4 * 4], p[4 * 4];

	glGetFloatv( GL_MODELVIEW_MATRIX,	m );
	glGetFloatv( GL_PROJECTION_MATRIX,	p );

	MatrixProduct(m,p);

	plane_t right_plane	(	m[3] - m[0], m[7] - m[4], m[11] - m[8],  m[15] - m[12]	);
	plane_t left_plane	(	m[3] + m[0], m[7] + m[4], m[11] + m[8],  m[15] + m[12]	);
	plane_t up_plane	(	m[3] - m[1], m[7] - m[5], m[11] - m[9],  m[15] - m[13]	);
	plane_t down_plane	(	m[3] + m[1], m[7] + m[5], m[11] + m[9],  m[15] + m[13]	);
	plane_t far_plane	(	m[3] - m[2], m[7] - m[6], m[11] - m[10], m[15] - m[14]	);
	plane_t near_plane	(	m[3] + m[2], m[7] + m[6], m[11] + m[10], m[15] + m[14]	);

	return frustum_t(left_plane,right_plane,up_plane,down_plane,near_plane,far_plane);
}

/*******************************************************************************

	Misc geometric routines

*******************************************************************************/

// Return the angle (in radians) between the x axis and the vector (x,y)
// The angle is between 0 and 2PI
float CalcRads(float x,float y)
{
	float ah = (float) atan2(y,x);
	if (ah >= 0)
	{
		if (y < 0)	return ah + FLOAT_PI;
		else		return ah;
	}
	else
	{
		if (y > 0)	return ah + FLOAT_PI;
		else		return ah + 2 * FLOAT_PI;
	}
}

