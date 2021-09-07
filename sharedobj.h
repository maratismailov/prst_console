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

	Reference counted objects (textures, sounds) inherit from this class

*******************************************************************************/

#ifndef SHAREDOBJ_H_DEF
#define SHAREDOBJ_H_DEF

#include "global.h"

class sharedobj_t
{
public:

	sharedobj_t(const string & _filename) : num(1), filename(_filename)	{ }
	virtual ~sharedobj_t()	{ }

	void IncNum()	{ ++num; }

	bool DecNum()
	{
		if (!--num)
		{
			delete this;
			return true;
		}
		return false;
	}

	string GetFilename() const	{ return filename; }

private:

	int num;
	string filename;
};


/*
 A pointer to a reference counted object of class T (i.e. a smart pointer).
 Class sharedptr_t<T> is responsible of creating objects of class T
 given their filenames (e.g loading textures), and keeps a list of already
 created objects, so that if a new pointer to an already created object
 is requested (i.e. same filename), it will point to the already created
 object in memory.
*/

template <class T>
class sharedptr_t
{
public:

	typedef vector<T*> pool_t;

	sharedptr_t () : ptr(NULL)	{ }
	sharedptr_t (T * _ptr) : ptr(_ptr)	{ }

	~sharedptr_t ()
	{
		DecNum();
	}

	void IncNum()
	{
		if (ptr)	ptr->IncNum();
	}

	void DecNum()
	{
		if (ptr && ptr->DecNum())
		{
			for (typename pool_t :: iterator i = pool.begin(); i != pool.end(); i++)
			{
				if ((*i) == ptr)
				{
					pool.erase(i);
					break;
				}
			}
		}
	}

	sharedptr_t (const sharedptr_t<T> & sptr)
	:	ptr(sptr.ptr)
	{
		IncNum();
	}

	sharedptr_t<T>& operator= (const sharedptr_t<T> & sptr)
	{
		if (sptr.ptr != ptr)
		{
			DecNum();
			ptr = sptr.ptr;
			IncNum();
		}
		return *this;
	}

	sharedptr_t (const string & filename)
	{
		string filename1 = StripPath(filename);
		if ( filename1 == filename )
			filename1 = path + filename1;
		else
			filename1 = filename;
		if (GetFromPool(filename1))	return;
		ptr = new T(filename1);
		pool.push_back(ptr);
	}
	sharedptr_t (const string & filename, bool flag)
	{
		string filename1 = path + StripPath(filename);
		if (GetFromPool(filename1))	return;
		ptr = new T(filename1, flag);
		pool.push_back(ptr);
	}

	sharedptr_t (int _w, int _h, void *pixels, int components, int format, int type)
	{
		ptr = new T(_w,_h,pixels,components,format,type);
		pool.push_back(ptr);
	}

static void AllObjects(void (T::*method) ())
{
	for (typename pool_t::iterator i = pool.begin(); i != pool.end(); i++)
		(**i.*method) ();
}
static void AllObjects(void (T::*method) (bool), bool parameter)
{
	for (typename pool_t::iterator i = pool.begin(); i != pool.end(); i++)
		(**i.*method) (parameter);
}
static void AllObjects(void (T::*method) (bool,bool), bool parameter1, bool parameter2)
{
	for (typename pool_t::iterator i = pool.begin(); i != pool.end(); i++)
		(**i.*method) (parameter1,parameter2);
}


	T& operator*()  const	{ return *ptr; }
	T* operator->() const	{ return ptr; }

	bool IsNULL() const	{ return ptr == NULL; }

	bool operator == (const sharedptr_t<T> & sptr) const	{ return ptr == sptr.ptr; }
	bool operator != (const sharedptr_t<T> & sptr) const	{ return ptr != sptr.ptr; }

	static const string path;

private:

	static pool_t pool;

	T * ptr;

	bool GetFromPool(const string & filename)
	{
		for (typename pool_t :: iterator i = pool.begin(); i != pool.end(); i++)
		{
			if ((*i)->GetFilename() == filename)
			{
				ptr = *i;
				IncNum();
				return true;
			}
		}
		return false;
	}
};

template <class T>
vector<T*> sharedptr_t<T>::pool;

#endif
