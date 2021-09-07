/*
 * @file printlog.c software log
 * 
 * Copyright (C) 2006-2007 Claudio Satriano <satriano@na.infn.it>
 * This file is part of RTLoc.
 * 
 * RTLoc is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * RTLoc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with RTLoc; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */ 

//luca
#include "../global.h"
#include <stdio.h>

#include "rtloclib.h"
#include <stdarg.h>

extern char *logfilename;

void printlog (const char *format, ...)
{
//luca
#if 0
	static FILE *logfile = NULL;
	va_list ap;

	if (logfile == NULL) {
		logfile = fopen (logfilename, "w");
		if (logfile == NULL) {
			perror(logfilename);
			exit(-1);
		}
	}

	//va_start(ap, format);
	//vfprintf (stderr, format, ap);
	//va_end(ap);

	va_start(ap, format);
	vfprintf (logfile, format, ap);
	fflush(logfile);
	va_end(ap);
#endif
cout << SecsToString(SecsNow()) << ": RTLOC ";
va_list ap;
va_start(ap, format);
vfprintf (stdout, format, ap);
va_end(ap);
}
