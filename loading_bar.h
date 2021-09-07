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


	Functions to have a progress bar displayed during disk loading


*******************************************************************************/

#ifndef LOADING_BAR_H_DEF
#define LOADING_BAR_H_DEF

#include "global.h"

/*
 Every function that loads data from disk will call LoadingBar_Start before
 doing any loading, and LoadingBar_End after loading the last file.

 Between the two calls, LoadingBar_SetNextPercent is placed before loading
 any item, with the parameter, an integer percentage between 0 and 100,
 telling how much of the whole loading the function has to perform
 will be reached when that item will be fully loaded.

 Example:

	void Load_Three_Items()
	{
		LoadingBar_Start();

		LoadingBar_SetNextPercent(33);
		.. code to load item 1 of 3 ..

		LoadingBar_SetNextPercent(66);
		.. code to load item 2 of 3 ..

		LoadingBar_SetNextPercent(100);
		.. code to load item 3 of 3 ..

		LoadingBar_End();
	}

 If loading an item, e.g item 2 above, consists of loading several files, e.g. 2,
 a function just like the above can be devised:

	void Load_Item2()
	{
		LoadingBar_Start();

		LoadingBar_SetNextPercent(50);
		.. code to load subitem 1 of 2 ..

		LoadingBar_SetNextPercent(100);
		.. code to load subitem 2 of 2 ..

		LoadingBar_End();
	}

 to show the loading progress with an even finer granularity.

*/

void LoadingBar_Start();
void LoadingBar_SetNextPercent(float percent);
void LoadingBar_End();

#endif
