/* vattr.c - Manages the user-defined attributes. */
/* $Id$ */

#include "copyright.h"
#include "autoconf.h"
#include "config.h"

#include "alloc.h"	/* required by mudconf */
#include "flags.h"	/* required by mudconf */
#include "htab.h"	/* required by mudconf */
#include "mudconf.h"	/* required by code */

#include "db.h"		/* required by externs */
#include "externs.h"	/* required by code */

#include "vattr.h"	/* required by code */
#include "attrs.h"	/* required by code */

static void FDECL(fixcase, (char *));
static char FDECL(*store_string, (char *));
extern int anum_alc_top;

/*
 * Allocate space for strings in lumps this big. 
 */

#define STRINGBLOCK 1000

/*
 * Current block we're putting stuff in 
 */

static char *stringblock = (char *)0;

/*
 * High water mark. 
 */

static int stringblock_hwm = 0;

void NDECL(vattr_init)
{
	hashinit(&mudstate.vattr_name_htab, VATTR_HASH_SIZE);
	mudstate.vattr_name_htab.nostrdup = 1;
}

VATTR *vattr_find(name)
char *name;
{
	register VATTR *vp;

	if (!ok_attr_name(name))
		return (NULL);

	vp = (VATTR *)hashfind(name, &mudstate.vattr_name_htab);
	
	/*
	 * vp is NULL or the right thing. It's right, either way. 
	 */
	return (vp);
}

VATTR *vattr_alloc(name, flags)
char *name;
int flags;
{
	int number;

	if (((number = mudstate.attr_next++) & 0x7f) == 0)
		number = mudstate.attr_next++;
	anum_extend(number);
	flags |= AF_DIRTY;
	return (vattr_define(name, number, flags));
}

VATTR *vattr_define(name, number, flags)
char *name;
int number, flags;
{
	VATTR *vp;

	/* Be ruthless. */

	if (strlen(name) >= VNAME_SIZE)
		name[VNAME_SIZE - 1] = '\0';

	fixcase(name);
	if (!ok_attr_name(name))
		return (NULL);

	if ((vp = vattr_find(name)) != NULL)
		return (vp);

	vp = (VATTR *) XMALLOC(sizeof(VATTR), "vattr_define");

	vp->name = store_string(name);
	vp->flags = flags;
	vp->number = number;

	hashadd(vp->name, (int *) vp, &mudstate.vattr_name_htab, 0);
	
	anum_extend(vp->number);
	anum_set(vp->number, (ATTR *) vp);
	return (vp);
}

void do_dbclean(player, cause, key)
dbref player, cause;
int key;
{
#ifndef STANDALONE

    VATTR *vp, *vpx;
    dbref i, end;
    int ca;
    char *as, *str;
    int *used_table;
    ATTR **new_table;

    raw_broadcast(0,
	      "GAME: Cleaning database. Game may freeze for a few minutes.");

    used_table = (int *) XCALLOC(mudstate.attr_next, sizeof(int),
				 "dbclean.used_table");

    /* Non-user-defined attributes are always considered used. */

    for (i = 0; i < A_USER_START; i++)
	used_table[i] = i;

    /* Walk the database. Mark all the attribute numbers in use. */

    atr_push();
    DO_WHOLE_DB(i) {
	for (ca = atr_head(i, &as); ca; ca = atr_next(&as)) {
	    used_table[ca] = ca;
	}
    }
    atr_pop();

    /* Walk the vattr table. If a number isn't in use, zorch it. */

    vp = vattr_first();
    while (vp) {
        vpx = vp;
	vp = vattr_next(vp);
	if (used_table[vpx->number] == 0) {
	    anum_set(vpx->number, NULL);
	    hashdelete(vpx->name, &mudstate.vattr_name_htab);
	    XFREE(vpx, "dbclean.vpx");
	}
    }

    /* Walk the table we've created of used statuses. When we find free
     * slots, walk backwards to the first used slot at the end of the
     * table. Write the number of the free slot into that used slot.
     */

    for (i = A_USER_START, end = mudstate.attr_next - 1;
	 (i < mudstate.attr_next) && (i < end); i++) {
	if (used_table[i] == 0) {
	    while ((end > i) && (used_table[end] == 0)) {
		end--;
	    }
	    if (end > i) {
		used_table[end] = used_table[i] = i;
		end--;
	    }
	}
    }

    /* Renumber the necessary attributes in the vattr tables. */

    for (i = A_USER_START; i < mudstate.attr_next; i++) {
	if (used_table[i] != i) {
	    vp = (VATTR *) anum_get(i);
	    if (vp) {
		vp->number = used_table[i];
		vp->flags |= AF_DIRTY;
		anum_set(used_table[i], (ATTR *) vp);
		anum_set(i, NULL);
	    }
	}
    }

    /* Now we walk the database. For every object, if we have an attribute
     * we're renumbering (the slot number is not equal to the array value
     * at that slot), we delete the old attribute and add the new one.
     */

    atr_push();
    DO_WHOLE_DB(i) {
	for (ca = atr_head(i, &as); ca; ca = atr_next(&as)) {
	    if (used_table[ca] != ca) {
		str = atr_get_raw(i, ca);
		atr_add_raw(i, used_table[ca], str);
		atr_clr(i, ca);
	    }
	}
    }
    atr_pop();

    /* The new end of the attribute table is the first thing we've
     * renumbered.
     */

    for (end = A_USER_START;
	 ((end == used_table[end]) && (end < mudstate.attr_next));
	 end++)
	;
    mudstate.attr_next = end;

    /* We might be able to shrink the size of the attribute table.
     * If the current size of the table is less than the initial
     * size, shrink it back down to the initial size.
     * Otherwise, shrink it down so it's the current top plus the
     * initial size, as if we'd just called anum_extend() for it.
     */

    if (anum_alc_top > mudconf.init_size + A_USER_START) {
	if (mudstate.attr_next < mudconf.init_size + A_USER_START) {
	    end = mudconf.init_size + A_USER_START;
	} else {
	    end = mudstate.attr_next + mudconf.init_size;
	}
	if (end < anum_alc_top) {
	    new_table = (ATTR **) XCALLOC(end + 1, sizeof(ATTR *),
					  "dbclean.new_table");
	    for (i = 0; i < mudstate.attr_next; i++)
		new_table[i] = anum_table[i];
	    XFREE(anum_table, "dbclean.anum_table");
	    anum_table = new_table;
	    anum_alc_top = end;
	}
    }

    /* Clean up. */

    XFREE(used_table, "dbclean.used_table");

    raw_broadcast(0, "GAME: Database cleaning complete.");

#endif /* STANDALONE */
}
		
void vattr_delete(name)
char *name;
{
	VATTR *vp;
	int number;

	fixcase(name);
	if (!ok_attr_name(name))
		return;

	number = 0;

	vp = (VATTR *)hashfind(name, &mudstate.vattr_name_htab);
	
	if (vp) {
		number = vp->number;
		anum_set(number, NULL);
		hashdelete(name, &mudstate.vattr_name_htab);
		XFREE(vp, "vattr_delete");
	}
	
	return;
}

VATTR *vattr_rename(name, newname)
char *name, *newname;
{
	VATTR *vp;

	fixcase(name);
	if (!ok_attr_name(name))
		return (NULL);

	/*
	 * Be ruthless. 
	 */

	if (strlen(newname) >= VNAME_SIZE)
		newname[VNAME_SIZE - 1] = '\0';

	fixcase(newname);
	if (!ok_attr_name(newname))
		return (NULL);
	
	/* We must explicitly delete and add the name to the hashtable,
	 * since we are changing the data.
	 */

	vp = (VATTR *)hashfind(name, &mudstate.vattr_name_htab);

	if (vp) {
	    vp->name = store_string(newname);
	    hashdelete(name, &mudstate.vattr_name_htab);
	    hashadd(newname, (int *) vp, &mudstate.vattr_name_htab, 0);
	}

	return (vp);
}

VATTR *NDECL(vattr_first)
{
	return (VATTR *)hash_firstentry(&mudstate.vattr_name_htab);
}

VATTR *vattr_next(vp)
VATTR *vp;
{
	if (vp == NULL)
		return (vattr_first());

	return ((VATTR *)hash_nextentry(&mudstate.vattr_name_htab));
}

static void fixcase(name)
char *name;
{
	char *cp = name;

	while (*cp) {
		*cp = toupper(*cp);
		cp++;
	}

	return;
}


/*
 * Some goop for efficiently storing strings we expect to
 * keep forever. There is no freeing mechanism.
 */

static char *store_string(str)
char *str;
{
	int len;
	char *ret;

	len = strlen(str);

	/*
	 * If we have no block, or there's not enough room left in the
	 * current one, get a new one. 
	 */

	if (!stringblock || (STRINGBLOCK - stringblock_hwm) < (len + 1)) {
		stringblock = (char *)XMALLOC(STRINGBLOCK, "store_string");
		if (!stringblock)
			return ((char *)0);
		stringblock_hwm = 0;
	}
	ret = stringblock + stringblock_hwm;
	StringCopy(ret, str);
	stringblock_hwm += (len + 1);
	return (ret);
}
