/*
 * EmuTOS desktop
 *
 * Copyright (C) 2002-2016 The EmuTOS development team
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

#ifndef _DESKACT_H
#define _DESKACT_H

/* Prototypes: */
WORD act_chg(WORD wh, OBJECT *tree, WORD root, WORD obj, GRECT *pc,
             WORD dochg, WORD dodraw);
void act_allchg(WORD wh, OBJECT *tree, WORD root, WORD ex_obj, GRECT *pt, GRECT *pc,
                WORD chgvalue, WORD dochg, WORD dodraw);
void act_bsclick(WORD wh, OBJECT *tree, WORD root, WORD mx, WORD my, WORD keystate,
                 GRECT *pc, WORD dclick);
WORD act_bdown(WORD wh, OBJECT *tree, WORD root, WORD *in_mx, WORD *in_my,
               WORD *keystate, GRECT *pc, WORD *pdobj);

#endif  /* _DESKACT_H */
