/*      DESKFUN.C       08/30/84 - 05/30/85             Lee Lorenzen    */
/*                      10/2/86  - 01/16/87             MDF             */
/*      merge source    5/27/87  - 5/28/87              mdf             */
/*      for 2.3         6/11/87                         mdf             */

/*
*       Copyright 1999, Caldera Thin Clients, Inc.
*                 2001 John Elliott
*                 2002-2019 The EmuTOS development team
*
*       This software is licenced under the GNU Public License.
*       Please see LICENSE.TXT for further information.
*
*                  Historical Copyright
*       -------------------------------------------------------------
*       GEM Desktop                                       Version 2.3
*       Serial No.  XXXX-0000-654321              All Rights Reserved
*       Copyright (C) 1985 - 1987               Digital Research Inc.
*       -------------------------------------------------------------
*/

/* #define ENABLE_KDEBUG */

#include "emutos.h"
#include <stdarg.h>
#include "obdefs.h"
#include "gemdos.h"
#include "optimize.h"

#include "aesdefs.h"
#include "deskbind.h"
#include "deskglob.h"
#include "deskapp.h"
#include "deskfpd.h"
#include "deskwin.h"
#include "aesbind.h"
#include "deskmain.h"
#include "desksupp.h"
#include "deskdir.h"
#include "deskfun.h"
#include "deskinf.h"
#include "deskins.h"
#include "deskpro.h"
#include "deskrsrc.h"
#include "biosdefs.h"

#include "string.h"
#include "gemerror.h"


/*
 * the following global is initialised to FALSE in fun_drag().
 * it will be set to TRUE by file2desk() (called indirectly by
 * fun_drag()) when a file has been dropped onto a desktop icon
 * representing an executable program.
 *
 * the global is returned by fun_drag(), and if TRUE, the desktop
 * will subsequently exit to allow the program to run.
 */
static BOOL exit_desktop;

#if CONF_WITH_PRINTER_ICON
#define PRTBUFSIZE  16384       /* buffer size */
#endif

/*
 *  Issue an alert
 */
WORD fun_alert(WORD defbut, WORD stnum)
{
    return form_alert(defbut, desktop_str_addr(stnum));
}


/*
 *  Issue an alert after merging in a variable
 *
 *  The following way of handling multiple types for the variable to be
 *  merged is a bit of a kludge, but at least we make an attempt to
 *  avoid obvious problems ...
 */
WORD fun_alert_merge(WORD defbut, WORD stnum, ...)
{
    va_list ap;
    _Static_assert(sizeof(void *) >= sizeof(long),"incompatible type sizes");

    va_start(ap, stnum);
    sprintf(G.g_work, desktop_str_addr(stnum), va_arg(ap,void *));
    va_end(ap);

    return form_alert(defbut, G.g_work);
}


void fun_msg(WORD type, WORD w3, WORD w4, WORD w5, WORD w6, WORD w7)
{
    /* keep DESKTOP messages internal to DESKTOP -- no AES call     */
    G.g_rmsg[0] = type;
    G.g_rmsg[1] = gl_apid;
    G.g_rmsg[2] = 0;
    G.g_rmsg[3] = w3;
    G.g_rmsg[4] = w4;
    G.g_rmsg[5] = w5;
    G.g_rmsg[6] = w6;
    G.g_rmsg[7] = w7;
    hndl_msg();
}


/*
 *  Mark window nodes for rebuild
 */
void fun_mark_for_rebld(char *path)
{
    WNODE *pwin;

    for (pwin = G.g_wfirst; pwin; pwin = pwin->w_next)
    {
        /* if opened and same path then mark */
        if ( (pwin->w_id) && (strcmp(pwin->w_pnode.p_spec, path)==0) )
            pwin->w_flags |= WN_REBUILD;
    }
}


/*
 *  Rebuild a window
 */
static void rebuild_window(WNODE *pwin)
{
    GRECT gr;

    pn_active(&pwin->w_pnode, TRUE);
    desk_verify(pwin->w_id, TRUE);
    win_sinfo(pwin, FALSE);
    wind_get_grect(pwin->w_id, WF_WXYWH, &gr);
    fun_msg(WM_REDRAW, pwin->w_id, gr.g_x, gr.g_y, gr.g_w, gr.g_h);
}


/*
 *  Rebuild marked windows
 */
void fun_rebld_marked(void)
{
    WNODE *pwin;

    desk_busy_on();

    /* check all wnodes     */
    for (pwin = G.g_wfirst; pwin; pwin = pwin->w_next)
    {
        if (pwin->w_flags & WN_REBUILD)
        {
            rebuild_window(pwin);
            pwin->w_flags &= ~WN_REBUILD;
        }
    }

    desk_busy_off();
}


/*
 *  Rebuild any windows with matching path
 */
void fun_rebld(char *ptst)
{
    WNODE *pwin;

    desk_busy_on();

    /* check all wnodes     */
    for (pwin = G.g_wfirst; pwin; pwin = pwin->w_next)
    {
        /* if opened and same path then rebuild */
        if ( (pwin->w_id) && (strcmp(pwin->w_pnode.p_spec, ptst)==0) )
        {
            rebuild_window(pwin);
        } /* if */
    } /* for */

    desk_busy_off();
} /* fun_rebld */


#if CONF_WITH_FILEMASK
/*
 *  Routine to update the file mask for the current window
 */
void fun_mask(WNODE *pw)
{
    char *maskptr, filemask[LEN_ZFNAME];
    OBJECT *tree;

    tree = G.a_trees[ADFMASK];

    /*
     * get current filemask & insert in dialog
     */
    maskptr = filename_start(pw->w_pnode.p_spec);
    set_tedinfo_name(tree, FMMASK, maskptr);

    /*
     * get user input
     */
    inf_show(tree, ROOT);

    /*
     * if 'OK', extract filemask from dialog, update pnode/display
     */
    if (inf_what(tree, FMOK, FMCANCEL) == 1)
    {
        inf_sget(tree, FMMASK, filemask);
        unfmt_str(filemask, maskptr);
        refresh_window(pw);
    }
}
#endif


/*
 *  Routine that creates a new directory in the specified window/path
 */
WORD fun_mkdir(WNODE *pw_node)
{
    PNODE *pp_node;
    OBJECT *tree;
    WORD  len;
    char  fnew_name[LEN_ZFNAME], unew_name[LEN_ZFNAME], *ptmp;
    char  path[MAXPATHLEN];

    tree = G.a_trees[ADMKDBOX];
    pp_node = &pw_node->w_pnode;
    ptmp = path;
    strcpy(ptmp, pp_node->p_spec);

    while(1)
    {
        fnew_name[0] = '\0';
        inf_sset(tree, MKNAME, fnew_name);
        start_dialog(tree);
        form_do(tree, 0);
        if (inf_what(tree, MKOK, MKCNCL) == 0)
            break;

        inf_sget(tree, MKNAME, fnew_name);
        unfmt_str(fnew_name, unew_name);

        if (unew_name[0] == '\0')
            break;

        ptmp = add_fname(path, unew_name);
        if (dos_mkdir(path) == 0)   /* mkdir succeeded */
        {
            fun_rebld(pw_node->w_pnode.p_spec);
            break;
        }

        len = strlen(path); /* before we restore old path */
        restore_path(ptmp); /* restore original path */
        if (len >= LEN_ZPATH-3)
        {
            fun_alert(1,STDEEPPA);
            break;
        }

        /*
         * mkdir failed with a recoverable error:
         * prompt for Cancel or Retry
         */
        if (fun_alert(2,STFOFAIL) == 1)     /* Cancel */
            break;
    }

    end_dialog(tree);
    return TRUE;
}


#if CONF_WITH_PRINTER_ICON
/*
 *  Print one or more files dropped onto the printer icon
 *  (we silently ignore icons that aren't files)
 *
 *  returns FALSE iff the user cancelled the print
 */
static BOOL fun_print(WORD sobj, LONG bufsize, char *iobuf)
{
    ANODE *pa;
    FNODE *pf;
    WNODE *pw;
    char path[MAXPATHLEN];

    pa = i_find(G.g_cwin, sobj, &pf, NULL);

    if (!pa)    /* "can't happen" */
        return TRUE;

    if (pa->a_type != AT_ISFILE)
        return TRUE;

    if (pf)     /* it's a window icon */
    {
        pw = win_find(G.g_cwin);    /* build the path */
        strcpy(path, pw->w_pnode.p_spec);
        strcpy(filename_start(path), pf->f_name);
    }
    else        /* it's a desktop icon */
    {
        strcpy(path,pa->a_pdata);   /* the path is in the anode */
    }

    return print_file(path, bufsize, iobuf);
}
#endif


/*
 *  Perform the operation 'op' on all the files & folders in the
 *  path associated with 'pspath'.  'op' can be OP_DELETE, OP_COPY,
 *  OP_MOVE.
 */
WORD fun_op(WORD op, WORD icontype, PNODE *pspath, char *pdest)
{
    DIRCOUNT count;
    WORD more;

    switch(op)
    {
    case OP_COPY:
    case OP_MOVE:
    case OP_DELETE:
        /* first, count source files */
        more = dir_op(OP_COUNT, icontype, pspath, pdest, &count);
        if (!more)
            return illegal_op_msg();
        if ((count.files+count.dirs) == 0)
            break;
        dir_op(op, icontype, pspath, pdest, &count);        /* do the operation     */
        return TRUE;
    }

    return FALSE;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   D E S K 1   r o u t i n e s                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void w_setpath(WNODE *pw, char *pathname)
{
    WORD icx, icy;
    GRECT rc;

    wind_get_grect(pw->w_id,WF_WXYWH, &rc);

    icx = rc.g_x + (rc.g_w / 2) - (G.g_wicon / 2);
    icy = rc.g_y + (rc.g_h / 2) - (G.g_hicon / 2);
    graf_shrinkbox(icx, icy, G.g_wicon, G.g_hicon,
                    rc.g_x, rc.g_y, rc.g_w, rc.g_h);

    /* we're closing a folder, so we never want a new window */
    pw->w_cvrow = 0;        /* must reset slider */
    do_fopen(pw, 0, pathname, FALSE);
}


static void fun_full_close(WNODE *pw)
{
    WORD icx, icy;
    GRECT rc;

    wind_get_grect(pw->w_id,WF_WXYWH, &rc);
    wind_close(pw->w_id);

    /*
     * only do the shrinkbox effect if there is a matching icon
     */
    if (pw->w_obid > 0)
    {
        icx = G.g_screen[pw->w_obid].ob_x;
        icy = G.g_screen[pw->w_obid].ob_y;
        graf_shrinkbox(icx, icy, G.g_wicon, G.g_hicon,
                        rc.g_x, rc.g_y, rc.g_w, rc.g_h);
    }

    pn_close(&pw->w_pnode);
    win_free(pw);

    /*
     * update current window etc
     */
    pw = win_ontop();
    desk_verify(pw ? pw->w_id : DESKWH, FALSE);
}


/*
 *  Removes the lowest level of folder from a pathname, assumed
 *  to be of the form:
 *      D:\X\Y\Z\F.E
 *  where X,Y,Z are folders and F.E is a filename.  In the above
 *  example, this would change D:\X\Y\Z\F.E to D:\X\Y\F.E
 */
static void remove_one_level(char *pathname)
{
    char *stop = pathname+2;    /* the first path separator */
    char *filename, *prev;

    filename = filename_start(pathname);
    if (filename-1 <= stop)     /* already at the root */
        return;

    for (prev = filename-2; prev >= stop; prev--)
        if (*prev == '\\')
            break;

    strcpy(prev+1,filename);
}


/*
 * full or partial close of desktop window
 */
void fun_close(WNODE *pw, WORD closetype)
{
    char pathname[MAXPATHLEN];
    char *fname;

    desk_busy_on();

    /*
     * handle CLOSE_FOLDER and CLOSE_TO_ROOT
     *
     * if already in the root, change CLOSE_FOLDER to CLOSE_WINDOW
     * (but don't change CLOSE_TO_ROOT!)
     */
    if (closetype != CLOSE_WINDOW)
    {
        strcpy(pathname,pw->w_pnode.p_spec);
        fname = filename_start(pathname);
        if (closetype == CLOSE_TO_ROOT)
            strcpy(pathname+3,fname);
        else if (pathname+3 == fname)
            closetype = CLOSE_WINDOW;
        else    /* we need to go up one level */
            remove_one_level(pathname);
    }

    if (closetype == CLOSE_WINDOW)
        fun_full_close(pw);
    else
        w_setpath(pw,pathname);

    desk_busy_off();
}


/*
 * builds the path corresponding to the first selected file in the
 * specified PNODE.  the path will be the filename only, or the full
 * pathname, depending on the desktop configuration settings.
 *
 * returns FALSE if no file is selected (probable program bug)
 */
static BOOL build_selected_path(PNODE *pn, char *pathname)
{
    FNODE *fn;

    for (fn = pn->p_flist; fn; fn = fn->f_next)
    {
        if (fnode_is_selected(fn))
            break;
    }
    if (!fn)
        return FALSE;

#if CONF_WITH_DESKTOP_CONFIG
    if (G.g_fullpath)
    {
        strcpy(pathname,pn->p_spec);
        add_fname(pathname,fn->f_name);
    }
    else
#endif
    {
        strcpy(pathname,fn->f_name);
    }

    return TRUE;
}


/*
 *  Routine to call when several icons have been dragged from a
 *  window to another window (it might be the same window) and
 *  dropped on a particular icon or open space.
 *
 *  This can be invoked when copying/moving files, or when launching
 *  a program via drag-and-drop.
 *
 *  Note that this is NEVER called if either the source or destination
 *  is the desktop.  Thus 'datype' can ONLY be AT_ISFILE or AT_ISFOLD.
 */
static void fun_win2win(WORD src_wh, WORD dst_wh, WORD dst_ob, WORD keystate)
{
    WORD  ret, datype, op;
    WNODE *psw, *pdw;
    ANODE *pda;
    FNODE *pdf;
    char  destpath[MAXPATHLEN];

    op = (keystate&MODE_CTRL) ? OP_MOVE : OP_COPY;
    psw = win_find(src_wh);
    if (!psw)
        return;
    pdw = win_find(dst_wh);
    if (!pdw)
        return;

    pda = i_find(dst_wh, dst_ob, &pdf, NULL);

    if (pda)
    {
        if (pda->a_aicon >= 0)      /* dropping file on to an application */
        {
            if (build_selected_path(&psw->w_pnode, destpath))
            {
                /* set global so desktop will exit if do_aopen() succeeds */
                exit_desktop = do_aopen(pda, 1, dst_ob, pdw->w_pnode.p_spec, pdf->f_name, destpath);
                return;
            }
        }
        datype = pda->a_type;
    }
    else
    {
        datype = AT_ISFILE;
    }

    /* set up default destination path name */
    strcpy(destpath, pdw->w_pnode.p_spec);

    /* if destination is folder, insert folder name in path */
    if (datype == AT_ISFOLD)
        add_path(destpath, pdf->f_name);

    ret = fun_op(op, -1, &psw->w_pnode, destpath);

    if (ret)
    {
        if (src_wh != dst_wh)
            desk_clear(src_wh);
        if (op == OP_MOVE)
            fun_rebld(psw->w_pnode.p_spec);
        fun_rebld(pdw->w_pnode.p_spec);
        /*
         * if we copied into a folder, we must redraw any windows with
         * a matching path
         */
        if (datype == AT_ISFOLD)
            fun_rebld(destpath);
    }
}


static WORD fun_file2desk(PNODE *pn_src, WORD icontype_src, ANODE *an_dest, WORD dobj, WORD keystate)
{
    ICONBLK *dicon;
    char pathname[MAXPATHLEN];
    WORD operation, ret;

    pathname[1] = ':';      /* set up everything except drive letter */
    strcpy(pathname+2, "\\*.*");

    operation = -1;
    if (an_dest)
    {
        switch(an_dest->a_type)
        {
#if CONF_WITH_DESKTOP_SHORTCUTS
        char tail[MAXPATHLEN];

        case AT_ISFILE:     /* dropping something onto a file */
            if (an_dest->a_aicon < 0)       /* is target a program? */
                break;                      /* no, do nothing */

            /* build the full tail to pass to the target program */
            if (!build_selected_path(pn_src,tail))
                break;

            /* build pathname for do_aopen() */
            strcpy(pathname,an_dest->a_pdata);
            strcpy(filename_start(pathname),"*.*");

            /* set global so desktop will exit if do_aopen() succeeds */
            exit_desktop = do_aopen(an_dest, 1, dobj, pathname, an_dest->a_pappl, tail);
            break;
        case AT_ISFOLD:     /* dropping file on folder - copy or move */
            strcpy(pathname,an_dest->a_pdata);
            strcat(pathname,"\\*.*");
            operation = (keystate&MODE_CTRL) ? OP_MOVE : OP_COPY;
            break;
#endif
        case AT_ISDISK:
            dicon = (ICONBLK *)G.g_screen[dobj].ob_spec;
            pathname[0] = LOBYTE(dicon->ib_char);
            operation = (keystate&MODE_CTRL) ? OP_MOVE : OP_COPY;
            break;
        case AT_ISTRSH:
            if (icontype_src >= 0)      /* source is desktop icon */
                if (wants_to_delete_files() == FALSE)
                    return FALSE;       /* i.e. remove icons or cancel */
            pathname[0] = pn_src->p_spec[0];
            operation = OP_DELETE;
            break;
#if CONF_WITH_PRINTER_ICON
        case AT_ISPRNT:
            {
            WORD sobj = 0;
            char *prtbuf = dos_alloc_anyram(PRTBUFSIZE);

            if (!prtbuf)
            {
                malloc_fail_alert();
                break;
            }
            while((sobj = win_isel(G.g_screen, G.g_croot, sobj)))
            {
                if (!fun_print(sobj, PRTBUFSIZE, prtbuf))
                    if (fun_alert(1, STABORT) != 2) /* quit unless "No" */
                        break;
            }
            dos_free(prtbuf);
            }
            break;
#endif
        default:            /* "can't happen" */
            illegal_op_msg();
        }
    }

    if (operation >= 0)
        ret = fun_op(operation, icontype_src, pn_src, pathname);
    else ret = FALSE;

    /*
     * if operation succeeded, rebuild any corresponding open windows
     */
    if (ret)
        fun_rebld(pathname);

    return ret;
}


static WORD fun_file2win(PNODE *pn_src, char  *spec, ANODE *an_dest, FNODE *fn_dest)
{
    char *p;
    char pathname[MAXPATHLEN];

    strcpy(pathname, spec);

    p = filename_start(pathname);

    if (an_dest && an_dest->a_type == AT_ISFOLD)
    {
        strcpy(p, fn_dest->f_name);
        strcat(p, "\\*.*");
    }
    else
    {
        strcpy(p, "*.*");
    }

    return fun_op(OP_COPY, -1, pn_src, pathname);
}


static void fun_win2desk(WORD wh, WORD obj, WORD keystate)
{
    WNODE *wn_src;
    ANODE *an_dest;

    an_dest = app_afind_by_id(obj);
    if (!an_dest)   /* "can't happen" */
        return;

    wn_src = win_find(wh);
    if (!wn_src)
        return;

    if (fun_file2desk(&wn_src->w_pnode, -1, an_dest, obj, keystate))
        fun_rebld(wn_src->w_pnode.p_spec);
}


static WORD fun_file2any(WORD sobj, WNODE *wn_dest, ANODE *an_dest, FNODE *fn_dest,
                  WORD dobj, WORD keystate)
{
    WORD icontype, okay = 0;
    FNODE *bp8;
    ICONBLK * ib_src;
    PNODE *pn_src;
    ANODE *an_src;
    char path[MAXPATHLEN];

    an_src = i_find(DESKWH, sobj, NULL, NULL);

#if CONF_WITH_DESKTOP_SHORTCUTS
    if ((an_src->a_type == AT_ISFILE) || (an_src->a_type == AT_ISFOLD))
    {
        strcpy(path, an_src->a_pdata);
    }
    else
#endif
    {
        ib_src = (ICONBLK *)G.g_screen[sobj].ob_spec;
        build_root_path(path, ib_src->ib_char);
        strcat(path,"*.*");
    }

    pn_src = pn_open(path, NULL);

    if (pn_src)
    {
        okay = pn_active(pn_src, FALSE);

        if (pn_src->p_flist)
        {
            /* mark all files as selected */
            for (bp8 = pn_src->p_flist; bp8; bp8 = bp8->f_next)
                bp8->f_selected = TRUE;
            if (wn_dest)    /* we are dragging a desktop icon to a window */
            {
                okay = fun_file2win(pn_src, wn_dest->w_pnode.p_spec, an_dest, fn_dest);
            }
            else    /* we are dragging a desktop item to another desktop item */
            {
                icontype = an_src ? an_src->a_type : -1;
                okay = fun_file2desk(pn_src, icontype, an_dest, dobj, keystate);
            }
        }
        pn_close(pn_src);
        desk_clear(DESKWH);
    }

    return okay;
}


static void fun_desk2win(WORD wh, WORD dobj, WORD keystate)
{
    WNODE *wn_dest;
    FNODE *fn_dest;
    WORD sobj, copied;
    ANODE *an_src, *an_dest;

    wn_dest = win_find(wh);
    if (!wn_dest)
        return;

    an_dest = i_find(wh, dobj, &fn_dest, NULL);
    sobj = 0;
    while ((sobj = win_isel(G.g_screen, DROOT, sobj)))
    {
        an_src = i_find(DESKWH, sobj, NULL, NULL);
        if (an_src)
        {
            switch(an_src->a_type)
            {
            case AT_ISDISK:
                if (!valid_drive(an_src->a_letter))
                    continue;
                break;
#if CONF_WITH_PRINTER_ICON
            case AT_ISPRNT:
#endif
            case AT_ISTRSH:
                continue;
            }
        }
        copied = fun_file2any(sobj, wn_dest, an_dest, fn_dest, dobj, keystate);
        if (copied)
            fun_rebld(wn_dest->w_pnode.p_spec);
    }
}


static void fun_desk2desk(WORD dobj, WORD keystate)
{
    WORD sobj;
    ANODE *source;
    ANODE *target;

    target = app_afind_by_id(dobj);
    if (!target)    /* "can't happen" */
        return;

    /* check if dropping icon onto non-existent disk */
    if ((target->a_type == AT_ISDISK) && !valid_drive(target->a_letter))
        return;

    sobj  = 0;
    while ((sobj = win_isel(G.g_screen, DROOT, sobj)))
    {
        source = i_find(DESKWH, sobj, NULL, NULL);
        if (!source || (source == target))
            continue;
        switch(source->a_type)
        {
            case AT_ISDISK:
                if (!valid_drive(source->a_letter))
                    continue;
                if ((target->a_type == AT_ISDISK) && (target->a_letter == source->a_letter))
                {
                    illegal_op_msg();
                    continue;
                }
                break;
#if CONF_WITH_PRINTER_ICON
            case AT_ISPRNT:
#endif
            case AT_ISTRSH:
                continue;
        }
        fun_file2any(sobj, NULL, target, NULL, dobj, keystate);
    }
}


BOOL fun_drag(WORD wh, WORD dest_wh, WORD sobj, WORD dobj, WORD mx, WORD my, WORD keystate)
{
    exit_desktop = FALSE;   /* may be set to TRUE by fun_file2desk() */

    if (wh)
    {
        if (dest_wh)    /* dragging from window to window, */
        {               /* e.g. copy/move files/folders    */
            fun_win2win(wh, dest_wh, dobj, keystate);
        }
        else            /* dragging from window to desktop */
        {
            if (dobj == DROOT)  /* dropping onto desktop surface */
            {
#if CONF_WITH_DESKTOP_SHORTCUTS
                ins_shortcut(wh, mx, my);
#else
                fun_alert(1, STNODRA1);
#endif
            }
            else                /* dropping onto desktop icon */
                fun_win2desk(wh, dobj, keystate);
        }
    }
    else    /* Dragging something from desk */
    {
        if (dest_wh)    /* dragging from desktop to window,  */
        {               /* e.g. copying a disk into a folder */
            fun_desk2win(dest_wh, dobj, keystate);
        }
        else            /* dragging from desktop to desktop,   */
        {               /* e.g. copying a disk to another disk */
            fun_desk2desk(dobj, keystate);
        }
    }

    return exit_desktop;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   e n d   o f   D E S K 1   r o u t i n e s                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


/*
 * Function called to delete a file or the contents of a folder or disk
 */
static WORD delete_ffd(char *path, WORD icontype)
{
    PNODE *pn;
    FNODE *fn;
    WORD ret = 0;

    pn = pn_open(path, NULL);
    if (pn == NULL)     /* "can't happen" - pathname too long! */
        return 0;

    desk_busy_on();
    pn_active(pn, FALSE);
    if (pn->p_flist)
    {
        for (fn = pn->p_flist; fn; fn = fn->f_next)
            fn->f_selected = TRUE;
        ret = fun_op(OP_DELETE, icontype, pn, NULL);
    }
    pn_close(pn);
    desk_busy_off();

    return ret;
}

/*
 *  This routine is called when the 'Delete' menu item is selected
 */
void fun_del(WORD sobj)
{
    char path[MAXPATHLEN];
    ANODE *pa;
    WNODE *pw;
    WORD item_found = 0;

    /*
     * if the item selected is on the desktop, there may be other desktop
     * items that have been selected; make sure we process all of them
     */
    if ( (i_find(DESKWH, sobj, NULL, NULL)) )
    {
        if (wants_to_delete_files() == FALSE)   /* i.e. remove icons or cancel */
            return;
        for ( ; sobj; sobj = win_isel(G.g_screen, DROOT, sobj))
        {
            pa = i_find(DESKWH,sobj,NULL,NULL);
            if (!pa)
                continue;
            switch(pa->a_type) {
            case AT_ISFILE:
            case AT_ISFOLD:
                strcpy(path, pa->a_pdata);
                break;
            case AT_ISDISK:
                build_root_path(path, pa->a_letter);
                strcat(path,"*.*");
                break;
            default:        /* "can't happen" */
                continue;
            }
            item_found++;
            if (delete_ffd(path, pa->a_type))
                refresh_drive(path[0]);
        }
        if (item_found)
        {
            desk_clear(DESKWH);
            return;
        }
    }

    /*
     * otherwise, process path associated with selected window icon, if any
     */
    pw = win_find(G.g_cwin);

    if (pw)
    {
        if (fun_op(OP_DELETE, -1, &pw->w_pnode, NULL))
            fun_rebld(pw->w_pnode.p_spec);
    }
}

/*
 * prompt for delete files or remove icons
 *
 * if user selects Delete, returns TRUE
 * if user selects Remove, sends a message to remove icons & returns FALSE
 * else returns FALSE
 */
BOOL wants_to_delete_files(void)
{
    WORD ret;

    ret = fun_alert(1,STRMVDEL);

    if (ret == 2)       /* Delete */
        return TRUE;

    if (ret == 1)       /* Remove */
        fun_msg(MN_SELECTED,OPTNMENU,RICNITEM,0,0,0);

    return FALSE;       /* Remove or Cancel */
}
