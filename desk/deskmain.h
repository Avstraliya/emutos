/*
 * EmuTOS desktop
 *
 * Copyright (C) 2002-2019 The EmuTOS development team
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

#ifndef _DESKMAIN_H
#define _DESKMAIN_H

extern char     gl_amstr[4];
extern char     gl_pmstr[4];

extern WORD     gl_apid;

extern GRECT    gl_savewin[];
extern GRECT    gl_normwin;


WORD hndl_msg(void);
WORD deskmain(void);
void centre_title(OBJECT *tree);

#endif  /* _DESKMAIN_H */
