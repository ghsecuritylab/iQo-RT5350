/* $Id: sp_respond.h,v 1.1 2010-02-02 09:42:58 yy Exp $ */
/*
** Copyright (C) 2002-2009 Sourcefire, Inc.
** Copyright (C) 1998-2002 Martin Roesch <roesch@sourcefire.com>
** Copyright (C) 1999,2000,2001 Christian Lademann <cal@zls.de>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License Version 2 as
** published by the Free Software Foundation.  You may not use, modify or
** distribute this program under any other version of the GNU General
** Public License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/*  I N C L U D E S
**********************************************************/

/*  D E F I N E S
************************************************************/
#ifndef __RESPOND_H__
#define __RESPOND_H__

#ifdef ENABLE_RESPOND
void SetupRespond(void);
void RespondFree(void* d);
uint32_t RespondHash(void* d);
int RespondCompare(void *l, void *r);
#endif /* ENABLE_RESPOND */

#endif /* __RESPOND_H__ */
