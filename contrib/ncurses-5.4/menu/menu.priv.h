/****************************************************************************
 * Copyright (c) 1998-2002,2003 Free Software Foundation, Inc.              *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *   Author:  Juergen Pfeifer, 1995,1997                                    *
 ****************************************************************************/

/* $Id: menu.priv.h,v 1.16 2003/10/25 15:24:29 tom Exp $ */

/***************************************************************************
* Module menu.priv.h                                                       *
* Top level private header file for all libnmenu modules                   *
***************************************************************************/

#ifndef MENU_PRIV_H_incl
#define MENU_PRIV_H_incl 1

#include <ncurses_cfg.h>
#include <curses.h>

#include "mf_common.h"
#include "menu.h"

/* Backspace code */
#define BS (8)

extern NCURSES_EXPORT_VAR(ITEM) _nc_Default_Item;
extern NCURSES_EXPORT_VAR(MENU) _nc_Default_Menu;

/* Normalize item to default if none was given */
#define Normalize_Item( item ) ((item)=(item)?(item):&_nc_Default_Item)

/* Normalize menu to default if none was given */
#define Normalize_Menu( menu ) ((menu)=(menu)?(menu):&_nc_Default_Menu)

/* Get the user defined (framing) window of the menu */
#define Get_Menu_UserWin(menu) ((menu)->userwin ? (menu)->userwin : stdscr)

/* Normalize menu window */
#define Get_Menu_Window(  menu ) \
   ((menu)->usersub  ? (menu)->usersub  : Get_Menu_UserWin(menu))

/* menu specific status flags */
#define _LINK_NEEDED    (0x04)
#define _MARK_ALLOCATED (0x08)

#define ALL_MENU_OPTS (                 \
		       O_ONEVALUE     | \
		       O_SHOWDESC     | \
		       O_ROWMAJOR     | \
		       O_IGNORECASE   | \
		       O_SHOWMATCH    | \
		       O_NONCYCLIC    )

#define ALL_ITEM_OPTS (O_SELECTABLE)

/* Move to the window position of an item and draw it */
#define Move_And_Post_Item(menu,item) \
  {wmove((menu)->win,(menu)->spc_rows*(item)->y,((menu)->itemlen+(menu)->spc_cols)*(item)->x);\
   _nc_Post_Item((menu),(item));}

#define Move_To_Current_Item(menu,item) \
  if ( (item) != (menu)->curitem)\
    {\
      Move_And_Post_Item(menu,item);\
      Move_And_Post_Item(menu,(menu)->curitem);\
    }

/* This macro ensures, that the item becomes visible, if possible with the
   specified row as the top row of the window. If this is not possible,
   the top row will be adjusted and the value is stored in the row argument.
*/
#define Adjust_Current_Item(menu,row,item) \
  { if ((item)->y < row) \
      row = (item)->y;\
    if ( (item)->y >= (row + (menu)->arows) )\
      row = ( (item)->y < ((menu)->rows - row) ) ? \
            (item)->y : (menu)->rows - (menu)->arows;\
    _nc_New_TopRow_and_CurrentItem(menu,row,item); }

/* Reset the match pattern buffer */
#define Reset_Pattern(menu) \
  { (menu)->pindex = 0; \
    (menu)->pattern[0] = '\0'; }

/* Internal functions. */
extern NCURSES_EXPORT(void) _nc_Draw_Menu (const MENU *);
extern NCURSES_EXPORT(void) _nc_Show_Menu (const MENU *);
extern NCURSES_EXPORT(void) _nc_Calculate_Item_Length_and_Width (MENU *);
extern NCURSES_EXPORT(void) _nc_Post_Item (const MENU *, const ITEM *);
extern NCURSES_EXPORT(bool) _nc_Connect_Items (MENU *, ITEM **);
extern NCURSES_EXPORT(void) _nc_Disconnect_Items (MENU *);
extern NCURSES_EXPORT(void) _nc_New_TopRow_and_CurrentItem (MENU *,int, ITEM *);
extern NCURSES_EXPORT(void) _nc_Link_Items (MENU *);
extern NCURSES_EXPORT(int)  _nc_Match_Next_Character_In_Item_Name (MENU*,int,ITEM**);
extern NCURSES_EXPORT(int)  _nc_menu_cursor_pos (const MENU* menu, const ITEM* item,
				int* pY, int* pX);

#endif /* MENU_PRIV_H_incl */
