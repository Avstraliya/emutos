/*
 * fsdir.c - directory routines for the file system
 *
 * Copyright (c) 2001 Lineo, Inc.
 *               2002-2014 The EmuTOS development team
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */


/*
** NOTE:
**      mods with "SCC.XX.NN" are mods which try to merge fixes to a special
**      post 1.0 / pre 1.1 version.  The notation refers to a DRI internal
**      document (see SCC), which is a change log.  SCC refers to the
**      originator of the fix.  The XX refers to the module in which the
**      fix was originally made, fs.c (FS), sup.c (SUP), etc.  The NN is
**      the fix number to that module as indicated on the change log.  For
**      the most part, these numbers are meaningless, and serve only to
**      correspond code to particular problems.
**
**  mods
**     date     who mod                 fix/change/note
**  ----------- --  ------------------  -------------------------------
**  06 May 1986 ktb M01.01.SCC.FS.03    logical drive select fix
**  06 May 1986 ktb M01.01.SCC.FS.04    fix to xmkdir for time/date stamp swp
**  06 May 1986 ktb M01.01.SCC.FS.06    fix to xmkdir for time/date stamp swp
**  06 May 1986 ktb M01.01.SCC.FS.07    replaced some routines per change log.
**  06 May 1986 ktb M01.01.SCC.FS.08    fix to match()
**  06 May 1986 ktb M01.01.SCC.FS.09    fix to xrmdir re: rmovg . & ..
**  11 May 1986 ktb M01.01.KTB.SCC.01   fix to SCC DND alloc scheme [1]
**  11 May 1986 ktb M01.01.0512.01      changed the complex if statement in
**                                      scan to something a little more readable
**
**  12 May 1986 ktb M01.01.KTB.SCC.02   makdnd: dir is in use if files are
**                                      open in it.
**
**  27 May 1986 ktb M01.01.0527.01      adding definitions of a structure for
**                                      the info kept in the dta between
**                                      search-first and search-next calls.
**
**  27 May 1986 ktb M01.01.0527.02      moved makbuf from fsdir to here.
**
**  27 May 1986 ktb M01.01.0527.03      changed match's return type
**
**  27 May 1986 ktb M01.01.0527.04      moved xcmps here from fsmain.c
**
**  27 May 1986 ktb M01.01.0527.06      new subroutine for searching for DND's
**
**  27 May 1986 ktb M01.01.0529.01      findit(), scan(): removed all ref's
**                                      to O_COMPLETE flag, as we follow
**                                      different algorithms now.
**
**  08 Jul 1986 ktb M01.01a.0708.01     removed all references to d_scan
**                                      field in DND.
**
**  08 Jul 1986 ktb M01.01a.0708.02     moved def of dirscan() here from fs.h
**
**  08 Jul 1986 ktb M01.01a.0708.01     removed all references to d_scan
**
**  14 Jul 1986 ktb M01.01a.0714.01     clean up some code
**
**  21 Jul 1986 ktb M01.01.0721.02      paranoia code
**
**  31 Jul 1986 ktb M01.01.0731.01      bug in xgsdtof, writes to the file,
**                                      but only needed to update OFD.
**
**  18 Sep 1986 scc M01.01.0918.01      Completion of M01.01.0731.01:  The OFD
**                                      needed to be marked as O_DIRTY so that
**                                      the directory entry would be rewritten.
**                                      Also, the user buffer was left byte
**                                      swapped after a 'set' operation.
**
**  24 Oct 1986 scc M01.01.1024.02      Addition of buffer length check to xgetdir()
**                                      and dopath().
**
**  31 Oct 1986 scc M01.01.1031.01      Changed reference to ValidDrv() in xgetdir()
**                                      to call bios 'drive map' directly.
**
**                                      Added freednd() routine to completely remove
**                                      partially installed DNDs.  It is used in
**                                      xmkdir().
**
**   3 Nov 1986 scc M01.01.1103.01      Added code to delete written directory entry
**                                      for partially installed new directory in
**                                      xmkdir() when it cannot be fully created.
**                                      Also, zero out parent DND's d_left if we've
**                                      gotten that far.  Also made a number of changes
**                                      from NULL to NULLPTR where we really wanted a
**                                      long zero.
**
**   7 Nov 1986 scc M01.01.1107.01      Added code to xmkdir() to check for and disallow
**                                      the creation of a directory which would make
**                                      the path length longer than 63 characters.  Also
**                                      added the routine namlen() which returns the
**                                      length of 1 subdirectory name.
**
**   9 Dec 1986 scc M01.01.1209.01      Modified xsfirst() and xsnext() to flag and to
**                                      check for an initialized DTA, so that doing
**                                      a Search_Next after an unsuccessful Search_First
**                                      will fail correctly.
**
**  12 Dec 1986 scc M01.01.1212.01      Modified dcrack() to return a negative error
**                                      code from when it calls ckdrv().  Modified
**                                      findit() to return a negative error code from
**                                      when it calls dcrack().  Modified xrmdir(),
**                                      xchmod(), xrename(), xchdir(), and ixsfirst()
**                                      to check for negative error code from calls to
**                                      findit().
**
**  14 Dec 1986 scc M01.01.1214.01      Further modification to M01.01.1212.01 so that
**                                      both the negative error code and a 0 (for BDOS
**                                      level error) are checked for.
**
**                  M01.01.1214.02      Added declaration of ckdrv() as long.
**
** [1]  the scheme had a small hole, where not all searches for entries
**      started at the start of the dir (d_scan !always= 0 on entry to
**      scan)..
*/

/* #define ENABLE_KDEBUG */

#include        "config.h"
#include        "portab.h"
#include        "asm.h"
#include        "fs.h"
#include        "time.h"
#include        "mem.h"
#include        "gemerror.h"
#include        "biosbind.h"
#include        "string.h"
#include        "kprint.h"

#ifndef M0101052901
#define M0101052901     TRUE
#endif
#ifndef M0101052705
#define M0101052705     TRUE
#endif
#ifndef M0101071401
#define M0101071401     TRUE
#endif

#ifndef M0101073101
#define M0101073101     TRUE
#endif

/*
 * forward prototypes
 */

static int namlen(char *s11);
static char *dopath(DND *p, char *buf, int *len);
static DND *makdnd(DND *p, FCB *b);
static DND *dcrack(const char **np);
static int getpath(const char *p, char *d, int dirspec);
static BOOL match(char *s1, char *s2);
static void makbuf(FCB *f, DTAINFO *dt);
static int xcmps(char *s, char *d);
static DND *getdnd(char *n, DND *d);
static void freednd(DND *dn);

/*
 *  local macros
 */

#define dirscan(a,c) ((DND *) scan(a,c,FA_SUBDIR,(LONG*)&negone))


/*
**  dots
*/

static const char dots[22] = ".          ";


/*
 * namlen - parameter points to a character string of 11 bytes max
 */

static int namlen(char *s11)                            /* M01.01.1107.01 */
{
    int i, len;

    for ( i = len = 1; i <= 11; i++, s11++ )
    {
        if ( *s11 && (*s11 != ' ') )
        {
            len++;
            if ( i==9 )
                len++;
        }
    }
    return ( len );
}



/*
**  xmkdir - make a directory, given path 's'
**
**      Function 0x39   d_create
**
*/

long xmkdir(char *s)
{
        register OFD *f;
        register FCB *f2;
        OFD     *fd,*f0;
        FCB     *b;
        DND     *dn;
        int     h,cl,plen;
        long    rc;

        if ((h = rc = ixcreat(s,FA_SUBDIR)) < 0)
                return(rc);

        f = getofd(h);

        /* build a DND in the tree */

        fd = f->o_dirfil;

        ixlseek(fd,f->o_dirbyt);
        b = (FCB *) ixread(fd,32L,NULLPTR);

        /* is the total path length too long? */     /* M01.01.1107.01 */

        plen = namlen( b->f_name );
        for ( dn = f->o_dnode; dn; dn = dn->d_parent )
                plen += namlen( dn->d_name );
        if ( plen >= (LEN_ZPATH-3) )
        {
                ixdel( f->o_dnode, b, f->o_dirbyt );
                return ( EACCDN );
        }

        dn = makdnd(f->o_dnode,b);  /* makdnd() only returns if it succeeds */

        dn->d_ofd = f0 = makofd(dn);/* makofd() only returns if it succeeds */

        /* initialize dir cluster */

        if (nextcl(f0,1))
        {
                ixdel( f->o_dnode, b, f->o_dirbyt );    /* M01.01.1103.01 */
                f->o_dnode->d_left = NULLPTR;            /* M01.01.1103.01 */
                freednd(dn);                    /* M01.01.1031.02 */
                return(EACCDN);
        }

        f2 = dirinit(dn);                       /* pointer to dirty dir block */

        /* write identifier */

        memcpy(f2, dots, 22);
        f2->f_attrib = FA_SUBDIR;
        f2->f_time = time;
        swpw( f2->f_time ) ;           /*  M01.01.SCC.FS.04  */
        f2->f_date = date;
        swpw( f2->f_date ) ;           /*  M01.01.SCC.FS.04  */
        cl = f0->o_strtcl;
        swpw(cl);
        f2->f_clust = cl;
        f2->f_fileln = 0;
        f2++;

        /* write parent entry .. */

        memcpy(f2, dots, 22);
        f2->f_name[1] = '.';           /* This is .. */
        f2->f_attrib = FA_SUBDIR;
        f2->f_time = time;
        swpw( f2->f_time ) ;           /*  M01.01.SCC.FS.06  */
        f2->f_date = date;
        swpw( f2->f_date ) ;           /*  M01.01.SCC.FS.06  */
        cl = f->o_dirfil->o_strtcl;

        if (!fd->o_dnode)              /* if creating a folder in the root, the */
                cl = 0;                /*  cluster# of the .. entry must be 0   */

        swpw(cl);
        f2->f_clust = cl;
        f2->f_fileln = 0;
        memcpy(f, f0, sizeof(OFD));
        f->o_flag |= O_DIRTY;
        ixclose(f,CL_DIR | CL_FULL);    /* force flush and write */
        xmfreblk((char*)f);
        sft[h-NUMSTD].f_own = 0;
        sft[h-NUMSTD].f_ofd = 0;
        return(E_OK);
}


/*
**  xrmdir - remove (delete) a directory
**
**      Function 0x3A   d_delete
**
**      Error returns
**              EPTHNF
**              EACCDN
**              EINTRN
**
*/

long xrmdir(char *p)
{
        register DND *d;
        DND     *d1,**q;
        FCB     *f;
        OFD     *fd,*f2;                /* M01.01.03 */
        long    pos;
        const char *s;

        if ((long)(d = findit(p,&s,1)) < 0)             /* M01.01.1212.01 */
                return( (long)d );
        if (!d)                                                 /* M01.01.1214.01 */
                return( EPTHNF );

        /*  M01.01.SCC.FS.09  */
        if( ! d->d_parent )                     /*  Can't delete root  */
                return( EACCDN ) ;
        /*  end M01.01.SCC.FS.09  */

        if (!(fd = d->d_ofd))
                d->d_ofd = (fd = makofd(d));    /* makofd() only returns if it succeeds */

        ixlseek(fd,0x40L);
        do
        {
                if (!(f = (FCB *) ixread(fd,32L,NULLPTR)))
                        break;
        } while (f->f_name[0] == (char)0x0e5 || f->f_attrib == FA_LFN);


        if( f != (FCB*)NULLPTR  &&  f->f_name[0] != 0 )
                return(EACCDN);

        for(d1 = *(q = &d->d_parent->d_left); d1 != d; d1 = *(q = &d1->d_right))
                ; /* follow sib-links */

        if( d1 != d )
                return(EINTRN);         /* internal error */

        if (d->d_files)
                return(EINTRN);         /* open files ? - internal error */

        if (d->d_left)
                return(EINTRN);         /* subdir - internal error */


        /* take him out ! */

        *q = d->d_right;

        if (d->d_ofd)
        {
                xmfreblk((char *)d->d_ofd);
        }

        d1 = d->d_parent;
        xmfreblk((char *)d);
        ixlseek((f2 = fd->o_dirfil),(pos = fd->o_dirbyt));
        f = (FCB *) ixread(f2,32L,NULLPTR);

        return(ixdel(d1,f,pos));
}

/*
**  xchmod - change/get attrib of path p
**              if wrt = 1, set; else get
**
**      Function 0x43   f_attrib
**
**      Error returns
**              EPTHNF
**              EFILNF
**
*/

long xchmod(char *p, int wrt, char mod)
{
        OFD *fd;
        DND *dn;                                        /*  M01.01.03   */
        const char *s;
        long pos;

        if ((long)(dn = findit(p,&s,0)) < 0)            /* M01.01.1212.01 */
                return( (long)dn );
        if (!(long)dn)                                  /* M01.01.1214.01 */
                return( EPTHNF );

        pos = 0;


        if( ! scan( dn , s , FA_NORM , &pos )  )        /*  M01.01.03   */
                return( EFILNF ) ;


        pos -= 21;                           /* point at attribute in file */
        fd = dn->d_ofd;
        ixlseek(fd,pos);
        if (!wrt)
                ixread(fd,1L,&mod);
        else
        {
                ixwrite(fd,1L,&mod);
                ixclose(fd,CL_DIR); /* for flush */
        }
        return(mod);
}



/*
 *  ixsfirst - search for first dir entry that matches pattern
 *      search first for matching name, into specified address.  if
 *      address = 0L, caller wants search only, no buffer info
 *
 * Arguments:
 *   *name - name of file to match
 *   att   - attribute of file
 *   *addr - ptr to dta info
 *
 *  returns:
 *      error code.
 */

long ixsfirst(char *name, register WORD att, register DTAINFO *addr)
{
    const char *s;              /*  M01.01.03                   */
    DND *dn;
    FCB *f;
    long pos;

    if (att != FA_VOL)
        att |= (FA_ARCHIVE|FA_RO);


#if     M0101071401
    if ( (long)(dn = findit(name,&s,0))  < 0 )          /* M01.01.1212.01 */
        return( (long)dn );

    if ( dn == (DND*)NULLPTR )                                  /* M01.01.1214.01 */
        return( EFILNF );
#else
    if ((long)(dn = findit(name,&s,0)) < 0)             /* M01.01.1212.01 */
        return( dn );
    if (!dn)                                            /* M01.01.1214.01 */
        return( EFILNF );
#endif

    /* now scan for filename from start of directory */

    pos = 0;

#if     M0101071401
    if(  (f = scan(dn,s,att,&pos))      ==  (FCB*)NULLPTR  )
        return(EFILNF);
#else
    if (dn)
    {
        if (!(f = scan(dn,s,att,&pos)))
            return(EFILNF);
    }
    else
        return(EFILNF);
#endif

    KDEBUG(("\nixfirst(%s, DND=%p, DTA=%p)",s,dn,addr));

    if (addr)
    {
        memcpy(&addr->dt_name[0], s, 12);
        addr->dt_attr = att ;
        addr->dt_pos = pos ;
        addr->dt_dnd = dn ;

        makbuf( f , addr ) ;

    }

    return(E_OK);
}



/*
 *  xsfirst - search first for matching name, into dta
 *
 *      Function 0x4E   f_sfirst
 *
 *      Error returns EFILNF
 */

long xsfirst(char *name, int att)
{
        long    result;
        DTAINFO *dt;                                            /* M01.01.1209.01 */

        dt = (DTAINFO *)(run->p_xdta);                          /* M01.01.1209.01 */

        /* set an indication of 'uninitialized DTA' */
        dt->dt_dnd = (DND*)NULLPTR;                                     /* M01.01.1209.01 */

        KDEBUG(("\nxsfirst(%s, DTA=%p)",name,dt));

        result = ixsfirst(name , att , dt);                   /* M01.01.1209.01 */

        return result;
}

/*
 *  xsnext - search next, return into dta
 *
 *      Function 0x4F   f_snext
 *
 *      Error returns ENMFIL
 */

long xsnext(void)
{
        register FCB        *f;
        register DTAINFO    *dt;

        dt = (DTAINFO *)run->p_xdta;                            /* M01.01.1209.01 */

        /* has the DTA been initialized? */
        if (dt->dt_dnd == (DND*)NULLPTR)                        /* M01.01.1209.01 */
                return ENMFIL;                                  /* M01.01.1209.01 */

        KDEBUG(("\n xsnext(pos=%ld DTA=%p DND=%p)",dt->dt_pos,dt,dt->dt_dnd));

        f = scan(dt->dt_dnd,&dt->dt_name[0],dt->dt_attr,&dt->dt_pos);

        if (f == (FCB*)NULLPTR)
                return ENMFIL;

        makbuf(f,(DTAINFO *)run->p_xdta);
        return E_OK;
}


/*
**  xgsdtof - get/set date/time of file into of from buffer
**
**      Function 0x57   f_datime
*/

long xgsdtof(int *buf, int h, int wrt)
{
        register OFD *f ;
        register int *b ;

        b = buf ;
        f = getofd(h) ;

        if ( !wrt )
        {
                b[0] = f->o_time ;
                b[1] = f->o_date ;
        }

        swpw(b[0]);
        swpw(b[1]);

        if ( wrt )
        {
                f->o_time = b[0] ;
                f->o_date = b[1] ;
                f->o_flag |= O_DIRTY;           /* M01.01.0918.01 */
                swpw(b[0]);                    /* M01.01.0918.01 */
                swpw(b[1]);                    /* M01.01.0918.01 */
        }

        return E_OK;
}



#ifdef  NEWCODE
/*  M01.01.03  */
#define isnotdelim(x)   ((x) && (x!='*') && (x!=SLASH) && (x!='.') && (x!=' '))

#define MAXFNCHARS      8


/*
**  builds - build a directory style file spec from a portion of a path name
**      the string at 's1' is expected to be a path spec in the form of
**      (xxx/yyy/zzz).  *builds* will take the string and crack it
**      into the form 'ffffffffeee' where 'ffffffff' is a non-terminated
**      string of characters, padded on the right, specifying the filename
**      portion of the file spec.  (The file spec terminates with the first
**      occurrence of a SLASH or NULL, the filename portion of the file spec
**      terminates with SLASH, NULL, PERIOD or WILDCARD-CHAR).  'eee' is the
**      file extension portion of the file spec, and is terminated with
**      any of the above.  The file extension portion is left justified into
**      the last three characters of the destination (11 char) buffer, but is
**      padded on the right.  The padding character depends on whether or not
**      the filename or file extension was terminated with a separator
**      (NULL, SLASH, PERIOD) or a WILDCARD-CHAR.
**
*/

/* s1 source
 * s2 dest
 */

void builds(char *s1, char *s2)
{
        register int i;
        char    c;

        /*
        ** copy filename part of pathname to destination buffer until a
        **      delimiter is found
        */

        for( i = 0 ; (i < MAXFNCHARS) && isnotdelim(*s1) ; i++ )
                *s2++ = uc(*s1++) ;

        /*
        **  if we have reached the max number of characters for the filename
        **      part, skip the rest until we reach a delimiter
        */

        if( i == MAXFNCHARS )
                while (*s1 && (*s1 != '.') && (*s1 != SLASH))
                        s1++;

        /*
        **  if the current character is a wildcard character, set the padding
        **      char with a "?" (wildcard), otherwise replace it with a space
        */

        c =    (*s1 == '*')  ?  '?'  :  ' '   ;


        if (*s1 == '*')                 /*  skip over wildcard char     */
                s1++;

        if (*s1 == '.')                 /*  skip over extension delim   */
                s1++;

        /*
        **  now that we've parsed out the filename part, pad out the
        **      destination with "?" wildcard chars
        */

        for( ; i < MAXFNCHARS ; i++ )
                *s2++ = c;

        /*
        **  copy extension part of file spec up to max number of characters
        **      or until we find a delimiter
        */

        for( i = 0 ; i < 3 && isnotdelim(*s1) ; i++ )
                *s2++ = uc(*s1++);

        /*
        **  if the current character is a wildcard character, set the padding
        **      char with a "?" (wildcard), otherwise replace it with a space
        */

        c = ((*s1 == '*') ? '?' : ' ');

        /*
        **  pad out the file extension
        */

        for( ; i < 3 ; i++ )
                *s2++ = c;
}

#else

/*
**  builds -
**
**      Last modified   LTG     23 Jul 85
*/

/* s1 is source, s2 dest */
void builds(const char *s1, char *s2)
{
        int i;
        char c;

        for (i=0; (i<8) && (*s1) && (*s1 != '*') && (*s1 != SLASH) &&
            (*s1 != '.') && (*s1 != ' '); i++)
                *s2++ = uc(*s1++);

        if (i == 8)
                while (*s1 && (*s1 != '.') && (*s1 != SLASH))
                        s1++;

        c = ((*s1 == '*') ? '?' : ' ');

        if (*s1 == '*')
                s1++;

        if (*s1 == '.')
                s1++;

        for (; i < 8; i++)
                *s2++ = c;

        for (i=0;(i<3) && (*s1) && (*s1 != '*') && (*s1 != SLASH) &&
            (*s1 != '.') && (*s1 != ' '); i++)
                *s2++ = uc(*s1++);

        c = ((*s1 == '*') ? '?' : ' ');

        for (; i < 3; i++)
                *s2++ = c;
}

#endif



/*
**  xrename - rename a file,
**      oldpath p1, new path p2
**
**      Function 0x56   f_rename
**
**      Error returns
**              EPTHNF
**              EACCDN
**              ENSAME
**
*/
/* rename file, n unused, old path p1, new path p2 */
/*ARGSUSED*/
long xrename(int n, char *p1, char *p2)
{
        OFD     *fd, *fd2;
        FCB     *f;
        DND     *dn1, *dn2;
        DMD     *dmd1, *dmd2;
        CLNO    strtcl1, strtcl2, temp;
        const char *s1, *s2;
        char    buf[11], att;
        int     hnew;
        long    posp;
        UWORD   time, date;
        CLNO    clust;
        LONG    fileln;

        if (!ixsfirst(p2,0,(DTAINFO *)0L))  /* check if new path exists */
                return(EACCDN);

        if ((long)(dn1 = findit(p1,&s1,0)) < 0)          /* M01.01.1212.01 */
                return( (long)dn1 );
        if (!dn1)                                        /* M01.01.1214.01 */
                return( EPTHNF );
        dmd1 = dn1->d_drv;          /* remember the drive and */
        strtcl1 = dn1->d_strtcl;    /* starting cluster for old path */

        /* scan DND for matching name */
        posp = 0L;
        f = scan(dn1,s1,0xff,&posp);
        if (!f)                     /* old path doesn't exist */
                return EPTHNF;

        /* at this point:
         *   f -> FCB for old path
         *   dn1->d_ofd -> OFD for the directory containing the old path
         *   posp = offset of FCB from start of directory in bytes, plus 32
         */
        fd = dn1->d_ofd;
        posp -= 32;                 /* adjust to start of FCB */

        /* get old attribute & time/date/cluster/length */
        att = f->f_attrib;
        time = f->f_time;
        swpw(time);                 /* convert from little-endian format */
        date = f->f_date;
        swpw(date);
        clust = f->f_clust;
        swpw(clust);
        fileln = f->f_fileln;
        swpl(fileln);

        if ((long)(dn2 = findit(p2,&s2,0)) < 0)          /* M01.01.1212.01 */
                return( (long)dn2 );
        if (!dn2)                                        /* M01.01.1214.01 */
                return( EPTHNF );
        dmd2 = dn2->d_drv;          /* remember the drive and */
        strtcl2 = dn2->d_strtcl;    /* starting cluster for new path */

        if (contains_illegal_characters(s2))
                return( EACCDN ) ;

        /* disallow cross-device rename */
        if (dmd1 != dmd2)
                return ENSAME;

        /*
         * check for cross-directory rename
         */
        if (strtcl1 != strtcl2)
        {
                /* erase (0xe5) old file */
                buf[0] = (char)0xe5;
                ixlseek(fd,posp);
                ixwrite(fd,1L,buf);

                /* create new directory entry with old info.  even if
                 * we're renaming a folder, we call xcreat() to create
                 * a normal file.  we'll fix it up later.
                 */
                hnew = xcreat(p2,att);
                if (hnew < 0)
                        return EPTHNF;
                fd2 = getofd(hnew); /* fd2 is the OFD for the new file/folder */

                /* copy the time/date/cluster/length to the OFD */
                fd2->o_time = time;
                fd2->o_date = date;
                fd2->o_strtcl = clust;
                fd2->o_fileln = fileln;

                /* if this is really a folder we're moving, we need to
                 * do two things: fix up the parent directory pointer in
                 * this folder, and fix up the attribute of the folder
                 * in the parent directory.
                 */
                if (att&FA_SUBDIR) {
                        fd2->o_fileln = 0x7fffffffL;    /* fake size for dirs */

                        /* set .. entry to point to new parent */
                        temp = fd2->o_dirfil->o_strtcl; /* parent's start cluster */
                        swpw(temp);                     /* convert to disk format */
                        ixlseek(fd2,32+26);             /* seek to cluster field  */
                        ixwrite(fd2,2l,&temp);          /*  & write it            */

                        /* set attribute for this file in parent directory */
                        ixlseek(fd2->o_dirfil,fd2->o_dirbyt+11);
                        ixwrite(fd2->o_dirfil,1L,&att);
                }
                fd2->o_flag |= O_DIRTY;
                if (att&FA_SUBDIR) {
                        ixclose(fd2,CL_DIR|CL_FULL);    /* force flush & write */
                        xmfreblk((char *)fd2);          /* free OFD */
                        sft[hnew-NUMSTD].f_own = 0;     /* free handle */
                        sft[hnew-NUMSTD].f_ofd = 0;
                } else xclose(hnew);
                ixclose(fd2->o_dirfil,CL_DIR);
        }
        else
        {
                builds(s2,buf);             /* build disk version of name */
                ixlseek(fd,posp);           /* and just overwrite the FCB */
                ixwrite(fd,11L,buf);
        }

        return(ixclose(fd,CL_DIR));
}

/*
**  xchdir - change current dir to path p
**
**      Function 0x3B   d_setpath
**
**      Error returns
**              EPTHNF
**              ckdrv()
**
*/

long xchdir(char *p)
{
        DND     *dnd;
        long    rc;
        int     olddir, newdir, dlog;
        const char *s;

        if (p[1] == ':')
                dlog = uc(p[0]) - 'A';
        else
                dlog = run->p_curdrv;

        /*
         * remember old current directory pointer
         */
        olddir = run->p_curdir[dlog];

        /*
         * get the DND for the new directory
         */
        rc = (long)(dnd = findit(p,&s,1));
        if (rc < 0L)
                return rc;
        if (!dnd)
                return EPTHNF;

        /*
         * search dirtbl[]: if entry matches, update usage count;
         * otherwise, create new entry
         */
        newdir = incr_curdir_usage(dnd);
        if (newdir < 0)                 /* no space in dirtbl[] */
                return EPTHNF;
        run->p_curdir[dlog] = newdir;   /* link to process  */

        /*
         * fixup old current directory
         */
        if (olddir)
                decr_curdir_usage(olddir);

        return E_OK;
}

/*
 * search dirtbl[]: if entry matches, update usage count;
 * otherwise, create new entry & update usage count.
 *
 * returns error if no space for new entry,
 * otherwise returns index of entry found
 */
int incr_curdir_usage(DND *dnd)
{
    DIRTBL_ENTRY *p;
    int i;

    if (!dnd)               /* precautionary paranoia */
        return EINTRN;

    for (i = 1, p = dirtbl+1; i < NCURDIR; i++, p++)    /* look for matching DND */
        if (p->dnd == dnd)
            break;

    if (i >= NCURDIR)       /* not found, so look for free slot */
        for (i = 1, p = dirtbl+1; i < NCURDIR; i++, p++)
            if (!p->use)
                break;

    if (i >= NCURDIR)       /* no slot available */
        return EINTRN;

    p->use++;               /* update use count */
    p->dnd = dnd;           /* link to DND      */

    return i;
}

/*
 * decrements usage count of dirtbl[], ensuring it never goes negative
 */
void decr_curdir_usage(int n)
{
    DIRTBL_ENTRY *p;

    if ((n <= 0) || (n >= NCURDIR))
    {
        KDEBUG(("Decrement for invalid slot %d has been ignored\n",n));
        return;
    }

    p = &dirtbl[n];

    p->use--;
    if (p->use < 0)
    {
        p->use = 0;
        KDEBUG(("Negative usage count for slot %d has been fixed\n",n));
    }

    if (p->use == 0)        /* clean out empty slots */
        p->dnd = NULL;
}

/*
**  xgetdir -
**
**      Function 0x47   d_getpath
**
**      Error returns
**              EDRIVE
*/

/* return text of current dir into specified buffer */
long    xgetdir(char *buf, int drv)
{
        DND     *p;
        int     n;
        int     len;                                            /* M01.01.1024.02 */

        drv = (drv == 0) ? run->p_curdrv : drv-1 ;

        if( !(Drvmap() & (1<<drv)) || (ckdrv(drv) < 0) )     /* M01.01.1031.01 */
        {
                *buf = 0;
                return(EDRIVE);
        }

        n = run->p_curdir[drv];
        p = dirtbl[n].dnd;
        len = LEN_ZPATH - 3;                                    /* M01.01.1024.02 */
        buf = dopath(p,buf,&len);                               /* M01.01.1024.02 */
        *--buf = 0;     /* null as last char, not slash */

        return(E_OK);
}



/*
 *  dirinit -
 */

/* dn: dir descr for dir */
FCB     *dirinit(DND *dn)
{
        OFD     *fd;            /*  ofd for this dir                    */
        int     num;
        RECNO   i2;
        char    *s1;
        DMD     *dm;
        FCB     *f1;

        fd = dn->d_ofd;                                 /*  OFD for dir */
        num = (dm = fd->o_dmd)->m_recsiz;               /*  bytes/rec   */

        /*
        **  for each record in the current cluster, besides the first record,
        **      get the record and zero it out
        */

        for (i2 = 1; i2 < dm->m_clsiz; i2++)
        {
                KDEBUG(("dirinit i2 = %li\n",i2));
                s1 = getrec(fd->o_currec+i2,fd,1);
                bzero( s1 , num ) ;
        }

        /*
        **  now zero out the first record and return a pointer to it
        */

        f1 = (FCB *) (s1 = getrec(fd->o_currec,fd,1));

        bzero( s1 , num ) ;
        return(f1);
}


/*
**  dopath -
**
**      M01.01.1024.02
*/

static char *dopath(DND *p, char *buf, int *len)
{
        char    temp[LEN_ZFNAME];
        char    *tp;
        long    tlen;

        if ( p->d_parent )
                buf = dopath(p->d_parent,buf,len);

        tlen = (long)packit(p->d_name,temp) - (long)temp;
        tp = temp;
        while ( *len )
        {
                (*len)--;                               /* len must never go < 0 */
                if ( tlen-- )
                        *buf++ = *tp++;
                else
                {
                        *buf++ = SLASH;
                        break;
                }
        }
        return(buf);
}



/*
 *  negone - for use as parameter
 */

static const long negone = { -1L } ;


/*
 *  findit - find a file/dir entry
 *      M01.01.SCC.FS.07        (routine replaced for this fix)
 */
/*  name: name of file/dir
 * dflag: T: name is for a directory
 */
DND     *findit(char *name, const char **sp, int dflag)
{
    register DND *p;
    const char *n;
    DND *pp,*newp;
    int i;
    char        s[11];

    /* crack directory and drive */

    n = name;
    KDEBUG(("findit(%s)\n",n));

    if ((long)(p = dcrack(&n)) < 0)                     /* M01.01.1214.01 */
        return( p );

    /*
     *  Force scan() to read from the beginning of the directory again,
     *  since we have gone to a scheme of keeping fewer DNDs in memory.
     */

#if     ! M0101052901
    if (p->d_ofd)
        p->d_ofd->o_flag &= ~O_COMPLETE;
#endif

    do
    {
        if (!(i = getpath(n,s,dflag)))
            break;

        if (i < 0)
        {       /*  path is '.' or '..'  */

            if (i == -2)                /*  go to parent (..)  */
                p = p->d_parent;

            i = -i;             /*  num chars is 1 or 2  */
            goto scanxt;
        }

        /*
         **  go down a level in the path...
         **     save a pointer to the current DND, which will
         **     become the parent, and get the node on the left,
         **     which is the first child.
         */

        pp = p;                 /*  save ptr to parent dnd      */

        if (!(newp = p->d_left))
        {                               /*  [1]                 */
            /*  make sure children      */
            newp = dirscan(p,n);        /*  are logged in       */
        }

        if (!(p = newp))        /*  If no children, exit loop */
            break;

        /*
         **  check all subdirectories at this level.  if we run out
         **     of siblings in the DND list (p->d_right == NULLPTR), then
         **     we should rescan the whole directory and make sure they
         **     are all logged in.
         */

        while( p && (xcmps(s,p->d_name) == FALSE) )
        {
            newp = p->d_right ; /*  next sibling        */

            if(newp == NULLPTR)  /* if no more siblings  */
            {
                p = 0;
                if (pp)
                {
#if     M0101052901
                    p = dirscan(pp,n);

#else
                    if (!(pp->d_ofd->o_flag & O_COMPLETE))
                    {   /*  M01.01.KTB.SCC.01  [1]  */
                        pp->d_scan = 0L ;    /*start*/
                        p = dirscan(pp,n);   /*over */
                    }
#endif
                }
            }
            else
                p = newp;
        }

    scanxt:     if (*(n = n + i))
        n++;
    else
        break;
    } while (p && i);

    /* p = 0 ==> not found
     i = 0 ==> found at p (dnd entry)
     n = points at filename */

    *sp = n;

    return(p);
}
/*
** [1]  The first call to dirscan is if there are no children logged in.
**      However, we need to call dirscan if children are logged in and we still
**      didn't find the desired node, as the desired child may've been flushed.
**      This is a terrible thing to have happen to a child.  However, we can't
**      afford to have all these kids around here, so when new ones come in, we
**      see which we can flush out (see makdnd()).  This is a hack -- no doubt
**      about that; the cached DND scheme needs to be redesigned all around.
**      Anyway, the second call to dirscan backs up to the parent (note that n
**      has not yet been bumped, so is still pointing to the current subdir's
**      name -- in effect, starting us at this level all over again.
**                      -- ktb
*/



/*
 *  scan - scan a directory for an entry with the desired name.
 *      scans a directory indicated by a DND.  attributes figure in matching
 *      as well as the entry's name.  posp is an indicator as to where to start
 *      searching.  A posp of -1 means to use the scan pointer in the dnd, and
 *      return the pointer to the DND, not the FCB.
 */

FCB     *scan(register DND *dnd, const char *n, WORD att, LONG *posp)
{
        char    name[12];
        register FCB *fcb;
        OFD     *fd;
        DND     *dnd1;
        BOOL    m;              /*  T: found a matching FCB             */

        KDEBUG(("scan(%p,'%s',0x%x,%p)\n",dnd,n,att,posp));

        m = 0;                  /*  have_match = false                  */
        builds(n,name);         /*  format name into dir format         */
        name[11] = att;

        dnd1 = 0; /* dummy to avoid warning */

        /*
        **  if there is no open file descr for this directory, make one
        */

        if (!(fd = dnd->d_ofd))
                dnd->d_ofd = (fd = makofd(dnd));/* makofd() only returns if it succeeds */

        /*
        **  seek to desired starting position.  If posp == -1, then start at
        **      the beginning.
        */

        ixlseek( fd , (*posp == -1) ? 0L : *posp ) ;

        /*
        **  scan thru the directory file, looking for a match
        */

        while ((fcb = (FCB *) ixread(fd,32L,NULLPTR)) && (fcb->f_name[0]))
        {
                /*
                **  Add New DND.
                **  ( iff after scan ptr && complete flag not set && not a .
                **  or .. && subdirectory && not deleted ) M01.01.0512.01
                */

                if( (fcb->f_attrib & FA_SUBDIR)         &&
#if     ! M0101052901
                    (!(fd->o_flag & O_COMPLETE))        &&
#endif
                    (fcb->f_name[0] != '.')             &&
                    (fcb->f_name[0] != (char)0xe5)
                )
                {       /*  see if we already have it  */
                        dnd1 = getdnd( &fcb->f_name[0] , dnd ) ;
                        if (!dnd1)
                                dnd1 = makdnd(dnd,fcb);   /* always succeeds */
                }

                if ( (m = match( name , fcb->f_name )) )
                                        break;
        }

        KDEBUG(("\n   scan(pos=%ld DND=%p DNDfoundFile=%p name=%s name=%s, %d)",
                (long)fd->o_bytnum,dnd,dnd1,fcb->f_name,name,m));

        /* restore directory scanning pointer */

        if( *posp != -1L )
                *posp = fd->o_bytnum ;

        /*
        **  if there was no match, but we were looking for a deleted entry,
        **  then return a pointer to a deleted fcb.  Otherwise, if there was
        **  no match, return a null pointer
        */

        if (!m)
        {       /*  assumes that (*n != 0xe5) (if posp == -1)  */
                if( fcb && (*n == (char)0xe5) )
                        return(fcb) ;
#if     ! M0101052901
                fd->o_flag |= O_COMPLETE;
#endif
                return( (FCB *) 0 );
        }

        if (*posp == -1)
        {       /*  seek to position of found entry  */
                ixlseek(fd,fd->o_bytnum - 32);
                return(((FCB *) dnd1));
        }

        return(fcb);
}


/*
**  makdnd - make a child subdirectory of directory p
**              M01.01.SCC.FS.07
*/

static DND *makdnd(DND *p, FCB *b)
{
        DIRTBL_ENTRY *dt;
        register DND *p1;
        register DND **prev;
        OFD     *fd;
        register int i;

        fd = p->d_ofd;

        /*
        **  scavenge a DND at this level if we can find one that has not
        **  d_left
        */
        for (prev = &p->d_left; (p1 = *prev) ; prev = &p1->d_right)
        {
                if (!p1->d_left)
                {
                        /* check dirtbl[] to see if anyone is using this guy */
                        for (i = 1, dt = dirtbl+1; i < NCURDIR; i++, dt++)
                                if (dt->use && (dt->dnd == p1))
                                        break;

                        KDEBUG(("\n makdnd check dirtbl (%d)",i));

                        if ((i >= NCURDIR) && (p1->d_files == NULLPTR))
                        {       /*  M01.01.KTB.SCC.02  */
                                /* clean out this DND for reuse */

                                p1->d_flag = 0;
                                p1->d_scan = 0L;
                                p1->d_files = (OFD *) 0;
                                if (p1->d_ofd)
                                {
                                        xmfreblk((char *)p1->d_ofd);
                                }
                                break;
                        }
                }
        }

        /* we didn't find one that qualifies, so allocate a new one */

        if (!p1)
        {
                KDEBUG(("\n makdnd new"));

                p1 = MGET(DND); /* MGET(DND) only returns if it succeeds */

                /* do this init only on a newly allocated DND */
                p1->d_right = p->d_left;
                p->d_left = p1;
                p1->d_parent = p;
        }

        /* complete the initialization */

        p1->d_ofd = (OFD *) 0;
        p1->d_strtcl = b->f_clust;
        swpw(p1->d_strtcl);
        p1->d_drv = p->d_drv;
        p1->d_dirfil = fd;
        p1->d_dirpos = fd->o_bytnum - 32;
        p1->d_time = b->f_time;
        p1->d_date = b->f_date;
        memcpy(p1->d_name, b->f_name, 11);

        KDEBUG(("\n makdnd(%p)",p1));

        return(p1);
}



/*
 *  dcrack - parse out start of 1st path element, get DND
 *      if needed, logs in the drive specified (explicitly or implicitly) in
 *      the path spec pointed to by 'np', parses out the first path element
 *      in that path spec, and adjusts 'np' to point to the first char in that
 *      path element.
 *
 *  returns
 *      ptr to DND for 1st element in path, or error
 */

static DND *dcrack(const char **np)
{
    register const char *n;
    DND *p;
    register int d;
    LONG l;                                             /* M01.01.1212.01 */

    KDEBUG(("\n dcrack(%p -> '%s')",np,*np));

    /*
     **  get drive spec (or default) and make sure drive is logged in
     */

    n = *np;                    /*  get ptr to name             */
    if (n[1] == ':')            /*  if we start with drive spec */
    {
        d = uc(n[0]) - 'A';     /*    compute drive number      */
        n += 2;                 /*    bump past drive number    */
    }
    else                                /*  otherwise                   */
        d = run->p_curdrv;      /*    assume default            */

    /* M01.01.1212.01 */
    if ((l = ckdrv(d)) < 0)     /*  check for valid drive & log */
        return((DND*) l );              /*    in.  abort if error       */

    /*
     **  if the pathspec begins with SLASH, then the first element is
     **  the root.  Otherwise, it is the current default directory.     Get
     **  the proper DND for this element
     */

    if (*n == SLASH)
    {   /* [D:]\path */
        p = drvtbl[d]->m_dtl;   /*  get root dir for log drive  */
        n++;                    /*  skip over slash             */
    }
    else
    {
        int curdir = run->p_curdir[d];
        p = dirtbl[curdir].dnd; /*  else use curr dir   */
    }

    /* whew ! */ /*  <= thankyou, Jason, for that wonderful comment */

    *np = n;
    return( (DND*)p );
}



/*
**  getpath - get a path element
**      The buffer pointed to by 'd' must be at least the size of the file
**      spec buffer in a directory entry (including file type), and will
**      be filled with the directory style format of the path element if
**      no error has occurred.
**
**  returns
**      -1 if '.'
**      -2 if '..'
**       0 if p => name of a file (no trailing SLASH or !dirspec)
**      >0 (nbr of chars in path element (up to SLASH)) && buffer 'd' filled.
**
*/

/* p: start of path element to crack
 * d: ptr to destination buffer
 * dirspec: true = no file name, just dir path
 */
static int getpath(const char *p, char *d, int dirspec)
{
        register int    i, i2 ;
        register const char *p1 ;

        for( i = 0 , p1 = p ; *p1 && (*p1 != SLASH) ; p1++ , i++ )
                ;

        /*
        **  If the string we have just scanned over is a directory name, it
        **      will either be terminated by a SLASH, or 'dirspec' will be set
        **      indicating that we are dealing with a directory path only
        **      (no file name at the end).
        */

        if( *p1 != '\0'  ||  dirspec )
        {       /*  directory name  */
                i2 = 0 ;
                if (p[0] == '.')                /*  dots in name        */
                {
                        i2--;                   /*  -1 for dot          */
                        if (p[1] == '.')
                                i2--;           /*  -2 for dotdot       */
                        return(i2);
                }

                if( i )                         /*  if not null path el */
                        builds(p,d);            /*  d => dir style fn   */

                return(i);                      /*  return nbr chars    */
        }

        return(0);              /*  if string is a file name            */
}


/*
**  match - utility routine to compare file names
*/
/* char *s1  -   name we are checking */
/* char *s2  -   name in fcb */

static BOOL match(char *s1, char *s2)
{
    register int i;

    /*
     **  skip VFAT long file name entries
     */

    if (s2[11] == FA_LFN)
        return( FALSE );

    /*
     **  check for deleted entry.  wild cards don't match deleted entries,
     **  only specific requests for deleted entries do.
     */

    if (*s2 == (char)0xe5)
    {
        if (*s1 == '?')
            return( FALSE );
        else if (*s1 == (char)0xe5)
            return( TRUE );
    }

    /*
     **  compare names
     */

    for (i=0; i < 11 ; i++, s1++, s2++)
        if (*s1 != '?')
            if (uc(*s1) != uc(*s2))
                return( FALSE );

    /*
     **  check attribute match   M01.01.SCC.FS.08
     ** volume labels and subdirs must be specifically asked for
     */

    if(  (*s1 != FA_VOL)  &&  (*s1 != FA_SUBDIR)  )
        if (!(*s2))
            return( TRUE );

    return( *s1 & *s2 ? TRUE : FALSE ) ;
}


/*                              M01.01.0527.02
**  makbuf - copy info from FCB into DTA info area
*/

static void makbuf(FCB *f, DTAINFO *dt)
{                                       /*  M01.01.03   */
    dt->dt_fattr = f->f_attrib ;
    dt->dt_time = f->f_time ;
    swpw(dt->dt_time) ;
    dt->dt_date = f->f_date ;
    swpw(dt->dt_date) ;
    dt->dt_fileln = f->f_fileln ;
    swpl( dt->dt_fileln ) ;

    if( f->f_attrib & FA_VOL ) {
        memcpy(&dt->dt_fname[0], &f->f_name[0], 11);
        dt->dt_fname[11] = '\0' ;
    } else {
        packit(&f->f_name[0],&dt->dt_fname[0]);
    }
}



/*
**  xcmps - utility routine to compare two 11-character strings
**
**      Last modified   19 Jul 85       SCC
*/

static int xcmps(char *s, char *d)
{
        register int i;

        for (i = 0; i < 11; i++)
                if (uc(*s++) != uc(*d++))
                        return(0);
        return(1);
}


/*
**  getdnd - find a dnd with matching name
*/

static DND *getdnd(char *n, DND *d)
{
        register DND *dnd ;

        for( dnd = d->d_left ; dnd ; dnd = dnd->d_right )
        {
                if( xcmps( n , &dnd->d_name[0] ) )
                        return( dnd ) ;
        }
        return( (DND*)NULLPTR ) ;

}


/*
**  freednd - free an allocated and linked-in DND
**
*/

static void freednd(DND *dn)                    /* M01.01.1031.02 */
{
    DND **prev;

    if ( dn->d_ofd ) {                /* free associated OFD if it's linked */
        xmfreblk( (char *)(dn->d_ofd) );
    }
    for ( prev = &(dn->d_parent->d_left);
          *prev != dn;
          prev = &((*prev)->d_right) )
        ;                             /* find the predecessor to this DND */
    *prev = dn->d_right;              /* then cut this DND out of the list */

    while ( dn->d_left ) {            /* is this step really necessary? */
        freednd( dn->d_left );
    }
    xmfreblk( (char *)dn );           /* finally free this DND */
}
