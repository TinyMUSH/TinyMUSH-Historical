/* funceval.c - MUX function handlers */
/* $Id$ */

#include "copyright.h"
#include "autoconf.h"

#include <limits.h>
#include <math.h>

#include "mudconf.h"
#include "config.h"
#include "db.h"
#include "flags.h"
#include "powers.h"
#include "attrs.h"
#include "externs.h"
#include "match.h"
#include "command.h"
#include "functions.h"
#include "misc.h"
#include "alloc.h"
#include "ansi.h"
#include "comsys.h"

extern NAMETAB indiv_attraccess_nametab[];
extern char *FDECL(trim_space_sep, (char *, char));
extern char *FDECL(next_token, (char *, char));
extern char *FDECL(split_token, (char **, char));
extern dbref FDECL(match_thing, (dbref, char *));
extern int FDECL(countwords, (char *, char));
extern int FDECL(check_read_perms, (dbref, dbref, ATTR *, int, int, char *, char **));
extern void FDECL(arr2list, (char **, int, char *, char **, char));
extern void FDECL(make_portlist, (dbref, dbref, char *, char **));
extern INLINE char *FDECL(get_mail_message, (int));

/* This is the prototype for functions */

#define	FUNCTION(x)	\
	void x(buff, bufc, player, cause, fargs, nfargs, cargs, ncargs) \
	char *buff, **bufc; \
	dbref player, cause; \
	char *fargs[], *cargs[]; \
	int nfargs, ncargs;

/* This is for functions that take an optional delimiter character.
 *
 * Call varargs_preamble("FUNCTION", max_args) for functions which
 * take either max_args - 1 args, or, with a delimiter, max_args args.
 *
 * Call mvarargs_preamble("FUNCTION", min_args, max_args) if there can
 * be more variable arguments than just the delimiter.
 *
 * Call evarargs_preamble("FUNCTION", min_args, max_args) if the delimiters
 * need to be evaluated.
 *
 * Call svarargs_preamble("FUNCTION", max_args) if the second to last and
 * last arguments are delimiters.
 *
 * Call xvarargs_preamble("FUNCTION", min_args, max_args) if this is varargs
 * but does not involve a delimiter.
 */

#define xvarargs_preamble(xname,xminargs,xnargs)                \
if (!fn_range_check(xname, nfargs, xminargs, xnargs, buff, bufc))     \
return;

#define varargs_preamble(xname,xnargs)	                        \
if (!fn_range_check(xname, nfargs, xnargs-1, xnargs, buff, bufc))	\
return;							        \
if (!delim_check(fargs, nfargs, xnargs, &sep, buff, bufc, 0,		\
    player, cause, cargs, ncargs))                              \
return;

#define mvarargs_preamble(xname,xminargs,xnargs)	        \
if (!fn_range_check(xname, nfargs, xminargs, xnargs, buff, bufc))	\
return;							        \
if (!delim_check(fargs, nfargs, xnargs, &sep, buff, bufc, 0,          \
    player, cause, cargs, ncargs))                              \
return;

#define evarargs_preamble(xname, xminargs, xnargs)              \
if (!fn_range_check(xname, nfargs, xminargs, xnargs, buff, bufc))	\
return;							        \
if (!delim_check(fargs, nfargs, xnargs - 1, &sep, buff, bufc, 1,      \
    player, cause, cargs, ncargs))                              \
return;							        \
if (!delim_check(fargs, nfargs, xnargs, &osep, buff, bufc, 1,         \
    player, cause, cargs, ncargs))                              \
return;

#define svarargs_preamble(xname,xnargs)                         \
if (!fn_range_check(xname, nfargs, xnargs-2, xnargs, buff, bufc))	\
return;							        \
if (!delim_check(fargs, nfargs, xnargs-1, &sep, buff, bufc, 0,        \
    player, cause, cargs, ncargs))                              \
return;							        \
if (nfargs < xnargs)				                \
    osep = sep;				                        \
else if (!delim_check(fargs, nfargs, xnargs, &osep, buff, bufc, 0,    \
    player, cause, cargs, ncargs))                              \
return;

/* --------------------------------------------------------------------------
 * Auxiliary functions for stacks.
 */

typedef struct object_stack STACK;
  struct object_stack {
      char *data;
      STACK *next;
  };

#define stack_get(x)   ((STACK *) nhashfind(x, &mudstate.objstack_htab))

#define stack_object(p,x)				\
        x = match_thing(p, fargs[0]);			\
	if (!Good_obj(x)) {				\
	    return;					\
	}						\
	if (!Controls(p, x)) {				\
            notify_quiet(p, NOPERM_MESSAGE);		\
	    return;					\
	}
	
#ifdef USE_COMSYS
FUNCTION(fun_cwho)
{
	struct channel *ch;
	struct comuser *user;
	int len = 0;
	char *smbuf;

	if (!(ch = select_channel(fargs[0]))) {
		safe_str("#-1 CHANNEL NOT FOUND", buff, bufc);
		return;
	}
	if (!mudconf.have_comsys || (!Comm_All(player) && (player != ch->charge_who))) {
		safe_str("#-1 NO PERMISSION TO USE", buff, bufc);
		return;
	}

	smbuf = alloc_sbuf("fun_cwho");

	for (user = ch->on_users; user; user = user->on_next) {
		if (Connected(user->who)) {
			if (len) {
				sprintf(smbuf, " #%d", user->who);
				if ((strlen(smbuf) + len) > 990) {
					safe_str(" #-1", buff, bufc);
					free_sbuf(smbuf);
					return;
				}
				safe_str(smbuf, buff, bufc);
				len += strlen(smbuf);
			} else {
				safe_tprintf_str(buff, bufc, "#%d", user->who);
				len = strlen(buff);
			}
		}
	}
	free_sbuf(smbuf);
}
#endif

FUNCTION(fun_beep)
{
	safe_chr(BEEP_CHAR, buff, bufc);
}

/* This function was originally taken from PennMUSH 1.50 */

FUNCTION(fun_ansi)
{
	char *s = fargs[0];

	while (*s) {
		switch (*s) {
		case 'h':	/* hilite */
			safe_str(ANSI_HILITE, buff, bufc);
			break;
		case 'i':	/* inverse */
			safe_str(ANSI_INVERSE, buff, bufc);
			break;
		case 'f':	/* flash */
			safe_str(ANSI_BLINK, buff, bufc);
			break;
		case 'n':	/* normal */
			safe_str(ANSI_NORMAL, buff, bufc);
			break;
		case 'x':	/* black fg */
			safe_str(ANSI_BLACK, buff, bufc);
			break;
		case 'r':	/* red fg */
			safe_str(ANSI_RED, buff, bufc);
			break;
		case 'g':	/* green fg */
			safe_str(ANSI_GREEN, buff, bufc);
			break;
		case 'y':	/* yellow fg */
			safe_str(ANSI_YELLOW, buff, bufc);
			break;
		case 'b':	/* blue fg */
			safe_str(ANSI_BLUE, buff, bufc);
			break;
		case 'm':	/* magenta fg */
			safe_str(ANSI_MAGENTA, buff, bufc);
			break;
		case 'c':	/* cyan fg */
			safe_str(ANSI_CYAN, buff, bufc);
			break;
		case 'w':	/* white fg */
			safe_str(ANSI_WHITE, buff, bufc);
			break;
		case 'X':	/* black bg */
			safe_str(ANSI_BBLACK, buff, bufc);
			break;
		case 'R':	/* red bg */
			safe_str(ANSI_BRED, buff, bufc);
			break;
		case 'G':	/* green bg */
			safe_str(ANSI_BGREEN, buff, bufc);
			break;
		case 'Y':	/* yellow bg */
			safe_str(ANSI_BYELLOW, buff, bufc);
			break;
		case 'B':	/* blue bg */
			safe_str(ANSI_BBLUE, buff, bufc);
			break;
		case 'M':	/* magenta bg */
			safe_str(ANSI_BMAGENTA, buff, bufc);
			break;
		case 'C':	/* cyan bg */
			safe_str(ANSI_BCYAN, buff, bufc);
			break;
		case 'W':	/* white bg */
			safe_str(ANSI_BWHITE, buff, bufc);
			break;
		}
		s++;
	}
	safe_str(fargs[1], buff, bufc);
	safe_str(ANSI_NORMAL, buff, bufc);
}

FUNCTION(fun_zone)
{
	dbref it;

	if (!mudconf.have_zones) {
		return;
	}
	it = match_thing(player, fargs[0]);
	if (it == NOTHING || !Examinable(player, it)) {
		safe_str("#-1", buff, bufc);
		return;
	}
	safe_tprintf_str(buff, bufc, "#%d", Zone(it));
}

#ifdef SIDE_EFFECT_FUNCTIONS

FUNCTION(fun_link)
{
	do_link(player, cause, 0, fargs[0], fargs[1]);
}

FUNCTION(fun_tel)
{
	do_teleport(player, cause, 0, fargs[0], fargs[1]);
}

FUNCTION(fun_wipe)
{
	do_wipe(player, cause, 0, fargs[0]);
}

FUNCTION(fun_pemit)
{
	do_pemit_list(player, fargs[0], fargs[1], 0);
}

FUNCTION(fun_remit)
{
    	do_pemit_list(player, fargs[0], fargs[1], 1);
}

FUNCTION(fun_force)
{
	do_force(player, cause, 0, fargs[0], fargs[1], cargs, ncargs);
}

FUNCTION(fun_trigger)
{
	if (nfargs < 1) {
		safe_str("#-1 TOO FEW ARGUMENTS", buff, bufc);
		return;
	}
	do_trigger(player, cause, 0, fargs[0], &(fargs[1]), nfargs - 1);
}

/*------------------------------------------------------------------------
 * fun_create: Creates a room, thing or exit
 */

static int check_command(player, name, buff, bufc)
dbref player;
char *name, *buff, **bufc;
{
	CMDENT *cmd;

	if ((cmd = (CMDENT *) hashfind(name, &mudstate.command_htab)))
		if (!check_access(player, cmd->perms)) {
			safe_str("#-1 PERMISSION DENIED", buff, bufc);
			return (1);
		}
	return (0);
}

FUNCTION(fun_create)
{
	dbref thing;
	int cost;
	char sep, *name;

	varargs_preamble("CREATE", 3);
	name = fargs[0];

	if (!name || !*name) {
		safe_str("#-1 ILLEGAL NAME", buff, bufc);
		return;
	}
	if (fargs[2] && *fargs[2])
		sep = *fargs[2];
	else
		sep = 't';

	switch (sep) {
	case 'r':
		if (check_command(player, "@dig", buff, bufc)) {
			safe_str("#-1 PERMISSION DENIED", buff, bufc);
			return;
		}
		thing = create_obj(player, TYPE_ROOM, name, 0);
		break;
	case 'e':
		if (check_command(player, "@open", buff, bufc)) {
			safe_str("#-1 PERMISSION DENIED", buff, bufc);
			return;
		}
		thing = create_obj(player, TYPE_EXIT, name, 0);
		if (thing != NOTHING) {
			s_Exits(thing, player);
			s_Next(thing, Exits(player));
			s_Exits(player, thing);
		}
		break;
	default:
		if (check_command(player, "@create", buff, bufc)) {
			safe_str("#-1 PERMISSION DENIED", buff, bufc);
			return;
		}
		if (fargs[1] && *fargs[1])
			cost = atoi(fargs[1]);

		if (cost < mudconf.createmin || cost > mudconf.createmax) {
			safe_str("#-1 COST OUT OF RANGE", buff, bufc);
			return;
		} else {
			cost = mudconf.createmin;
		}
		thing = create_obj(player, TYPE_THING, name, cost);
		if (thing != NOTHING) {
			move_via_generic(thing, player, NOTHING, 0);
			s_Home(thing, new_home(player));
		}
		break;
	}
	safe_tprintf_str(buff, bufc, "#%d", thing);
}

/*---------------------------------------------------------------------------
 * fun_set: sets an attribute on an object
 */

static void set_attr_internal(player, thing, attrnum, attrtext, key, buff, bufc)
dbref player, thing;
int attrnum, key;
char *attrtext, *buff;
char **bufc;
{
	dbref aowner;
	int aflags, could_hear;
	ATTR *attr;

	attr = atr_num(attrnum);
	atr_pget_info(thing, attrnum, &aowner, &aflags);
	if (attr && Set_attr(player, thing, attr, aflags)) {
		if ((attr->check != NULL) &&
		    (!(*attr->check) (0, player, thing, attrnum, attrtext))) {
		        safe_str("#-1 PERMISSION DENIED", buff, bufc);
			return;
		}
		could_hear = Hearer(thing);
		atr_add(thing, attrnum, attrtext, Owner(player), aflags);
		handle_ears(thing, could_hear, Hearer(thing));
		if (!(key & SET_QUIET) && !Quiet(player) && !Quiet(thing))
			notify_quiet(player, "Set.");
	} else {
		safe_str("#-1 PERMISSION DENIED.", buff, bufc);
	}
}

FUNCTION(fun_set)
{
	dbref thing, thing2, aowner;
	char *p, *buff2;
	int atr, atr2, aflags, clear, flagvalue, could_hear;
	ATTR *attr, *attr2;

	/* obj/attr form? */

	if (parse_attrib(player, fargs[0], &thing, &atr)) {
		if (atr != NOTHING) {

			/* must specify flag name */

			if (!fargs[1] || !*fargs[1]) {

				safe_str("#-1 UNSPECIFIED PARAMETER", buff, bufc);
			}
			/* are we clearing? */

			clear = 0;
			if (*fargs[0] == NOT_TOKEN) {
				fargs[0]++;
				clear = 1;
			}
			/* valid attribute flag? */

			flagvalue = search_nametab(player,
					indiv_attraccess_nametab, fargs[1]);
			if (flagvalue < 0) {
				safe_str("#-1 CAN NOT SET", buff, bufc);
				return;
			}
			/* make sure attribute is present */

			if (!atr_get_info(thing, atr, &aowner, &aflags)) {
				safe_str("#-1 ATTRIBUTE NOT PRESENT ON OBJECT", buff, bufc);
				return;
			}
			/* can we write to attribute? */

			attr = atr_num(atr);
			if (!attr || !Set_attr(player, thing, attr, aflags)) {
				safe_str("#-1 PERMISSION DENIED", buff, bufc);
				return;
			}
			/* just do it! */

			if (clear)
				aflags &= ~flagvalue;
			else
				aflags |= flagvalue;
			could_hear = Hearer(thing);
			atr_set_flags(thing, atr, aflags);

			return;
		}
	}
	/* find thing */

	if ((thing = match_controlled(player, fargs[0])) == NOTHING) {
		safe_str("#-1", buff, bufc);
		return;
	}
	/* check for attr set first */
	for (p = fargs[1]; *p && (*p != ':'); p++) ;

	if (*p) {
		*p++ = 0;
		atr = mkattr(fargs[1]);
		if (atr <= 0) {
			safe_str("#-1 UNABLE TO CREATE ATTRIBUTE", buff, bufc);
			return;
		}
		attr = atr_num(atr);
		if (!attr) {
			safe_str("#-1 PERMISSION DENIED", buff, bufc);
			return;
		}
		atr_get_info(thing, atr, &aowner, &aflags);
		if (!Set_attr(player, thing, attr, aflags)) {
			safe_str("#-1 PERMISSION DENIED", buff, bufc);
			return;
		}
		buff2 = alloc_lbuf("fun_set");

		/* check for _ */
		if (*p == '_') {
			strcpy(buff2, p + 1);
			if (!parse_attrib(player, p + 1, &thing2, &atr2) ||
			    (atr == NOTHING)) {
				free_lbuf(buff2);
				safe_str("#-1 NO MATCH", buff, bufc);
				return;
			}
			attr2 = atr_num(atr);
			p = buff2;
			atr_pget_str(buff2, thing2, atr2, &aowner, &aflags);

			if (!attr2 ||
			 !See_attr(player, thing2, attr2, aowner, aflags)) {
				free_lbuf(buff2);
				safe_str("#-1 PERMISSION DENIED", buff, bufc);
				return;
			}
		}
		/* set it */

		set_attr_internal(player, thing, atr, p, 0, buff, bufc);
		free_lbuf(buff2);
		return;
	}
	/* set/clear a flag */
	flag_set(thing, player, fargs[1], 0);
}
#endif

/* Borrowed from DarkZone */
FUNCTION(fun_zwho)
{
	dbref i, it = match_thing(player, fargs[0]);
	int len = 0;
	char *smbuf;
	
	if (!mudconf.have_zones || (!Controls(player, it) && !WizRoy(player))) {
		safe_str("#-1 NO PERMISSION TO USE", buff, bufc);
		return;
	}
	for (i = 0; i < mudstate.db_top; i++)
		if (Typeof(i) == TYPE_PLAYER)
			if (Zone(i) == it)
				if (len) {
					smbuf = alloc_sbuf("fun_zwho");

					sprintf(smbuf, " #%d", i);
					if ((strlen(smbuf) + len) > 990) {
						safe_str(" #-1", buff, bufc);
						free_sbuf(smbuf);
						return;
					}
					safe_str(smbuf, buff, bufc);
					len += strlen(smbuf);
					free_sbuf(smbuf);
				} else {
					safe_tprintf_str(buff, bufc, "#%d", i);
					len = strlen(buff);
				}
}

/* Borrowed from DarkZone */
FUNCTION(fun_inzone)
{
	dbref i, it = match_thing(player, fargs[0]);
	int len = 0;
	char *smbuf;

	if (!mudconf.have_zones || (!Controls(player, it) && !WizRoy(player))) {
		safe_str("#-1 NO PERMISSION TO USE", buff, bufc);
		return;
	}
	for (i = 0; i < mudstate.db_top; i++)
		if (Typeof(i) == TYPE_ROOM)
			if (db[i].zone == it)
				if (len) {
					smbuf = alloc_sbuf("fun_inzone");;

					sprintf(smbuf, " #%d", i);
					if ((strlen(smbuf) + len) > 990) {
						safe_str(" #-1", buff, bufc);
						free_sbuf(smbuf);
						return;
					}
					safe_str(smbuf, buff, bufc);
					len += strlen(smbuf);
					free_sbuf(smbuf);
				} else {
					safe_tprintf_str(buff, bufc, "#%d", i);
					len = strlen(buff);
				}
}

/* Borrowed from DarkZone */
FUNCTION(fun_children)
{
	dbref i, it = match_thing(player, fargs[0]);
	int len = 0;
	char *smbuf;

	if (!(Controls(player, it)) || !(WizRoy(player))) {
		safe_str("#-1 NO PERMISSION TO USE", buff, bufc);
		return;
	}
	for (i = 0; i < mudstate.db_top; i++)
		if (Parent(i) == it)
			if (len) {
				smbuf = alloc_sbuf("fun_children");

				sprintf(smbuf, " #%d", i);
				if ((strlen(smbuf) + len) > 990) {
					safe_str(" #-1", buff, bufc);
					free_sbuf(smbuf);
					return;
				}
				safe_str(smbuf, buff, bufc);
				len += strlen(smbuf);
				free_sbuf(smbuf);
			} else {
				safe_tprintf_str(buff, bufc, "#%d", i);
				len = strlen(buff);
			}
}

FUNCTION(fun_objeval)
{
	dbref obj;
	char *name, *bp, *str;

	if (!*fargs[0]) {
		return;
	}
	name = bp = alloc_lbuf("fun_objeval");
	str = fargs[0];
	exec(name, &bp, 0, player, cause, EV_FCHECK | EV_STRIP | EV_EVAL, &str,
	     cargs, ncargs);
	*bp = '\0';
	obj = match_thing(player, name);

	/* In order to evaluate from something else's viewpoint, you must
	 * have the same owner as it, or be a wizard. Otherwise, we default
	 * to evaluating from our own viewpoint. Also, you cannot evaluate
	 * things from the point of view of God.
	 */
	if ((obj == NOTHING) || (obj == GOD) ||
	    ((Owner(obj) != Owner(player)) && !Wizard(player))) {
		obj = player;
	}

	str = fargs[1];
	exec(buff, bufc, 0, obj, cause, EV_FCHECK | EV_STRIP | EV_EVAL, &str,
	     cargs, ncargs);
	free_lbuf(name);
}

/* ---------------------------------------------------------------------------
 * fun_null: Just eat the contents of the string. Handy for those times
 *           when you've output a bunch of junk in a function call and
 *           just want to dispose of the output (like if you've done an
 *           iter() that just did a bunch of side-effects, and now you have
 *           bunches of spaces you need to get rid of.
 */

FUNCTION(fun_null)
{
    return;
}

/* ---------------------------------------------------------------------------
 * fun_squish: Squash occurrences of a given character down to 1.
 *             We do this both on leading and trailing chars, as well as
 *             internal ones; if the player wants to trim off the leading
 *             and trailing as well, they can always call trim().
 */

FUNCTION(fun_squish)
{
    char *tp, sep;

    if (nfargs == 0) {
	return;
    }

    varargs_preamble("SQUISH", 2);

    tp = fargs[0];

    while (*tp) {

	while (*tp && (*tp != sep))       /* copy non-seps */
	    *(*bufc)++ = *tp++;

	if (!*tp) {		/* end of string */
	    return;
	}

	/* We've hit a sep char. Copy it, then skip to the next non-sep. */
	*(*bufc)++ = *tp++;
	while (*tp && (*tp == sep))
	    tp++;
    }
}


FUNCTION(fun_stripansi)
{
	safe_str((char *)strip_ansi(fargs[0]), buff, bufc);
}

/* Borrowed from PennMUSH 1.50 */
FUNCTION(fun_zfun)
{
	dbref aowner;
	int a, aflags;
	int attrib;
	char *tbuf1, *str;

	dbref zone = Zone(player);

	if (!mudconf.have_zones) {
		safe_str("#-1 ZONES DISABLED", buff, bufc);
		return;
	}
	if (zone == NOTHING) {
		safe_str("#-1 INVALID ZONE", buff, bufc);
		return;
	}
	if (!fargs[0] || !*fargs[0])
		return;

	/* find the user function attribute */
	attrib = get_atr(upcasestr(fargs[0]));
	if (!attrib) {
		safe_str("#-1 NO SUCH USER FUNCTION", buff, bufc);
		return;
	}
	tbuf1 = atr_pget(zone, attrib, &aowner, &aflags);
	if (!See_attr(player, zone, (ATTR *) atr_num(attrib), aowner, aflags)) {
		safe_str("#-1 NO PERMISSION TO GET ATTRIBUTE", buff, bufc);
		free_lbuf(tbuf1);
		return;
	}
	str = tbuf1;
	exec(buff, bufc, 0, zone, player, EV_EVAL | EV_STRIP | EV_FCHECK, &str, &(fargs[1]),
	     nfargs - 1);
	free_lbuf(tbuf1);
}

FUNCTION(fun_columns)
{
	int spaces, number, ansinumber, count, i, indent = 0;
	int isansi = 0, rturn = 1, cr = 0;
	char *p, *q, *buf, *curr, *objstring, *bp, *cp, sep, *str;

        if (!fn_range_check("COLUMNS", nfargs, 2, 4, buff, bufc))
		return;
	if (!delim_check(fargs, nfargs, 3, &sep, buff, bufc, 1,                
	    player, cause, cargs, ncargs))
		return;
		
	number = safe_atoi(fargs[1]);
	indent = safe_atoi(fargs[3]);

	if ((indent < 0) || (indent > 77)) {
		indent = 1;
	}
	
	if ((number < 1) || ((number + indent) > 78)) {
		safe_str("#-1 OUT OF RANGE", buff, bufc);
		return;
	}
	cp = curr = bp = alloc_lbuf("fun_columns");
	str = fargs[0];
	exec(curr, &bp, 0, player, cause, EV_STRIP | EV_FCHECK | EV_EVAL, &str,
	     cargs, ncargs);
	*bp = '\0';
	cp = trim_space_sep(cp, sep);
	if (!*cp) {
		free_lbuf(curr);
		return;
	}
	
	for (i = 0; i < indent; i++)
		safe_chr(' ', buff, bufc);

	buf = alloc_lbuf("fun_columns");
	
	while (cp) {
		objstring = split_token(&cp, sep);
		ansinumber = number;
		if (ansinumber > strlen((char *)strip_ansi(objstring)))
			ansinumber = strlen((char *)strip_ansi(objstring));

		p = objstring;
		q = buf;
		count = 0;
		while (p && *p) {
			if (count == number) {
				break;
			}
			if (*p == ESC_CHAR) {
				/* Start of ANSI code. Skip to end. */
				isansi = 1;
				while (*p && !isalpha(*p))
					*q++ = *p++;
				if (*p)
					*q++ = *p++;
			} else {
				*q++ = *p++;
				count++;
			}
		}
		if (isansi)
			safe_str(ANSI_NORMAL, buf, &q);
		*q = '\0';
		isansi = 0;

		spaces = number - strlen((char *)strip_ansi(objstring));

		/* Sanitize number of spaces */

		if (spaces > LBUF_SIZE) {
			spaces = LBUF_SIZE;
		}
		safe_str(buf, buff, bufc);
		for (i = 0; i < spaces; i++)
			safe_chr(' ', buff, bufc);

		if (!(rturn % (int)((78 - indent) / number))) {
			safe_str((char *)"\r\n", buff, bufc);
			cr = 1;
			for (i = 0; i < indent; i++)
				safe_chr(' ', buff, bufc);
		} else {
			cr = 0;
		}

		rturn++;
	}
	
	if (!cr) {
		safe_str((char *)"\r\n", buff, bufc);
	}
	
	free_lbuf(buf);
	free_lbuf(curr);
}

/* Code for objmem and playmem borrowed from PennMUSH 1.50 */

static int mem_usage(thing)
dbref thing;
{
	int k;
	int ca;
	char *as, *str;
	ATTR *attr;

	k = sizeof(struct object);

	k += strlen(Name(thing)) + 1;
	for (ca = atr_head(thing, &as); ca; ca = atr_next(&as)) {
		str = atr_get_raw(thing, ca);
		if (str && *str)
			k += strlen(str);
		attr = atr_num(ca);
		if (attr) {
			str = (char *)attr->name;
			if (str && *str)
				k += strlen(((ATTR *) atr_num(ca))->name);
		}
	}
	return k;
}

static int mem_usage_attr(player, str)
    dbref player;
    char *str;
{
    dbref thing, aowner;
    int atr, aflags;
    char *abuf;
    ATTR *ap;
    int bytes_atext = 0;

    if (parse_attrib_wild(player, str, &thing, 0, 0, 1)) {
	for (atr = olist_first(); atr != NOTHING; atr = olist_next()) {
	    ap = atr_num(atr);
	    if (!ap)
		continue;
	    abuf = atr_get(thing, atr, &aowner, &aflags);
	    /* Player must be able to read attribute with 'examine' */
	    if (Examinable(player, thing) &&
		Read_attr(player, thing, ap, aowner, aflags))
		bytes_atext += strlen(abuf);
	    free_lbuf(abuf);
	}
    }

    return bytes_atext;
}

FUNCTION(fun_objmem)
{
	dbref thing;

	if (index(fargs[0], '/') != NULL) {
	    safe_ltos(buff, bufc, mem_usage_attr(player, fargs[0]));
	    return;
	}

	thing = match_thing(player, fargs[0]);
	if (thing == NOTHING || !Examinable(player, thing)) {
		safe_str("#-1 PERMISSION DENIED", buff, bufc);
		return;
	}
	safe_ltos(buff, bufc, mem_usage(thing));
}

FUNCTION(fun_playmem)
{
	int tot = 0;
	dbref thing;
	dbref j;

	thing = match_thing(player, fargs[0]);
	if (thing == NOTHING || !Examinable(player, thing)) {
		safe_str("#-1 PERMISSION DENIED", buff, bufc);
		return;
	}
	DO_WHOLE_DB(j)
		if (Owner(j) == thing)
		tot += mem_usage(j);
	safe_ltos(buff, bufc, tot);
}

/* Code for andflags() and orflags() borrowed from PennMUSH 1.50 */
static int handle_flaglists(player, cause, name, fstr, type)
dbref player, cause;
char *name;
char *fstr;
int type;			/* 0 for orflags, 1 for andflags */
{
	char *s;
	char flagletter[2];
	FLAGSET fset;
	FLAG p_type;
	int negate, temp;
	int ret = type;
	dbref it = match_thing(player, name);

	negate = temp = 0;

	if (it == NOTHING)
		return 0;
	if (! (mudconf.pub_flags || Examinable(player, it) || (it == cause)))
		return 0;
		
	for (s = fstr; *s; s++) {

		/* Check for a negation sign. If we find it, we note it and 
		 * increment the pointer to the next character. 
		 */

		if (*s == '!') {
			negate = 1;
			s++;
		} else {
			negate = 0;
		}

		if (!*s) {
			return 0;
		}
		flagletter[0] = *s;
		flagletter[1] = '\0';

		if (!convert_flags(player, flagletter, &fset, &p_type)) {

			/* Either we got a '!' that wasn't followed by a
			 * letter, or we couldn't find that flag. For
			 * AND, since we've failed a check, we can
			 * return false. Otherwise we just go on. 
			 */

			if (type == 1)
				return 0;
			else
				continue;

		} else {

			/* does the object have this flag? */

			if ((Flags(it) & fset.word1) ||
			    (Flags2(it) & fset.word2) ||
			    (Typeof(it) == p_type)) {
				if (isPlayer(it) && (fset.word2 == CONNECTED) &&
				    ((Flags(it) & (WIZARD | DARK)) == (WIZARD | DARK)) &&
				    !Wizard(player))
					temp = 0;
				else
					temp = 1;
			} else {
				temp = 0;
			}
			
			if ((type == 1) && ((negate && temp) || (!negate && !temp))) {

				/* Too bad there's no NXOR function... At
				 * this point we've either got a flag
				 * and we don't want it, or we don't
				 * have a flag and we want it. Since
				 * it's AND, we return false. 
				 */
				return 0;

			} else if ((type == 0) &&
				 ((!negate && temp) || (negate && !temp))) {

				/* We've found something we want, in an OR. */

				return 1;
			}
			/* Otherwise, we don't need to do anything. */
		}
	}
	return (ret);
}

FUNCTION(fun_orflags)
{
	safe_ltos(buff, bufc, handle_flaglists(player, cause, fargs[0], fargs[1], 0));
}

FUNCTION(fun_andflags)
{
	safe_ltos(buff, bufc, handle_flaglists(player, cause, fargs[0], fargs[1], 1));
}

FUNCTION(fun_strtrunc)
{
	int number, count = 0;
	char *p = (char *)fargs[0];
	char *q, *buf;
	int isansi = 0;

	q = buf = alloc_lbuf("fun_strtrunc");
	number = atoi(fargs[1]);
	if (number > strlen((char *)strip_ansi(fargs[0])))
		number = strlen((char *)strip_ansi(fargs[0]));

	if (number < 0) {
		safe_str("#-1 OUT OF RANGE", buff, bufc);
		free_lbuf(buf);
		return;
	}
	while (p && *p) {
		if (count == number) {
			break;
		}
		if (*p == ESC_CHAR) {
			/* Start of ANSI code. Skip to end. */
			isansi = 1;
			while (*p && !isalpha(*p))
				*q++ = *p++;
			if (*p)
				*q++ = *p++;
		} else {
			*q++ = *p++;
			count++;
		}
	}
	if (isansi)
		safe_str(ANSI_NORMAL, buf, &q);
	*q = '\0';
	safe_str(buf, buff, bufc);
	free_lbuf(buf);
}

FUNCTION(fun_ifelse)
{
	/* This function now assumes that its arguments have not been
	   evaluated. */
	
	char *str, *mbuff, *bp;
	
	mbuff = bp = alloc_lbuf("fun_ifelse");
	str = fargs[0];
	exec(mbuff, &bp, 0, player, cause, EV_STRIP | EV_FCHECK | EV_EVAL,
		&str, cargs, ncargs);
	*bp = '\0';
	
	if (!mbuff || !*mbuff || !xlate(mbuff)) {
		str = fargs[2];
		exec(buff, bufc, 0, player, cause, EV_STRIP | EV_FCHECK | EV_EVAL,
			&str, cargs, ncargs);
	} else {
		str = fargs[1];
		exec(buff, bufc, 0, player, cause, EV_STRIP | EV_FCHECK | EV_EVAL,
			&str, cargs, ncargs);
	}
	free_lbuf(mbuff);
}

FUNCTION(fun_nonzero)
{
	/* MUX-style ifelse -- rather than bool check, check if the
	* string is non-null/non-zero.
	*/
	
	char *str, *mbuff, *bp;
	
	mbuff = bp = alloc_lbuf("fun_nonzero");
	str = fargs[0];
	exec(mbuff, &bp, 0, player, cause, EV_STRIP | EV_FCHECK | EV_EVAL,
		&str, cargs, ncargs);
	*bp = '\0';
	
	if (!mbuff || !*mbuff || ((atoi(mbuff) == 0) && is_number(mbuff))) {
		str = fargs[2];
		exec(buff, bufc, 0, player, cause, EV_STRIP | EV_FCHECK | EV_EVAL,
			&str, cargs, ncargs);
	} else {
		str = fargs[1];
		exec(buff, bufc, 0, player, cause, EV_STRIP | EV_FCHECK | EV_EVAL,
			&str, cargs, ncargs);
	}
	free_lbuf(mbuff);
}

FUNCTION(fun_inc)
{
	safe_ltos(buff, bufc, atoi(fargs[0]) + 1);
}

FUNCTION(fun_dec)
{
	safe_ltos(buff, bufc, atoi(fargs[0]) - 1);
}

#ifdef USE_MAIL
/* Mail functions borrowed from DarkZone */
FUNCTION(fun_mail)
{
	/* This function can take one of three formats: 1.  mail(num)  -->
	 * returns message <num> for privs. 2.  mail(player)  -->
	 * returns number of messages for <player>. 3.
	 * mail(player, num)  -->  returns message <num> for
	 * <player>. 
	 *
	 * It can now take one more format: 4.  mail() --> returns number of
	 * messages for executor 
	 */

	struct mail *mp;
	dbref playerask;
	int num, rc, uc, cc;
#ifdef RADIX_COMPRESSION
	char *msgbuff;
#endif

	/* make sure we have the right number of arguments */
	if ((nfargs != 0) && (nfargs != 1) && (nfargs != 2)) {
		safe_str("#-1 FUNCTION (MAIL) EXPECTS 0 OR 1 OR 2 ARGUMENTS", buff, bufc);
		return;
	}
	if ((nfargs == 0) || !fargs[0] || !fargs[0][0]) {
		count_mail(player, 0, &rc, &uc, &cc);
		safe_ltos(buff, bufc, rc + uc);
		return;
	}
	if (nfargs == 1) {
		if (!is_number(fargs[0])) {
			/* handle the case of wanting to count the number of
			 * messages 
			 */
			if ((playerask = lookup_player(player, fargs[0], 1)) == NOTHING) {
				safe_str("#-1 NO SUCH PLAYER", buff, bufc);
				return;
			} else if ((player != playerask) && !Wizard(player)) {
				safe_str("#-1 PERMISSION DENIED", buff, bufc);
				return;
			} else {
				count_mail(playerask, 0, &rc, &uc, &cc);
				safe_tprintf_str(buff, bufc, "%d %d %d", rc, uc, cc);
				return;
			}
		} else {
			playerask = player;
			num = atoi(fargs[0]);
		}
	} else {
		if ((playerask = lookup_player(player, fargs[0], 1)) == NOTHING) {
			safe_str("#-1 NO SUCH PLAYER", buff, bufc);
			return;
		} else if ((player != playerask) && !God(player)) {
			safe_str("#-1 PERMISSION DENIED", buff, bufc);
			return;
		}
		num = atoi(fargs[1]);
	}

	if ((num < 1) || (Typeof(playerask) != TYPE_PLAYER)) {
		safe_str("#-1 NO SUCH MESSAGE", buff, bufc);
		return;
	}
	mp = mail_fetch(playerask, num);
	if (mp != NULL) {
#ifdef RADIX_COMPRESSION
		msgbuff = alloc_lbuf("fun_mail");
		string_decompress(get_mail_message(mp->number), msgbuff);
		safe_str(msgbuff, buff, bufc);
		free_lbuf(msgbuff);
#else
		safe_str(get_mail_message(mp->number), buff, bufc);
#endif
		return;
	}
	/* ran off the end of the list without finding anything */
	safe_str("#-1 NO SUCH MESSAGE", buff, bufc);
}

FUNCTION(fun_mailfrom)
{
	/* This function can take these formats: 1) mailfrom(<num>) 2)
	 * mailfrom(<player>,<num>) It returns the dbref of the player the
	 * mail is from 
	 */
	struct mail *mp;
	dbref playerask;
	int num;

	/* make sure we have the right number of arguments */
	if ((nfargs != 1) && (nfargs != 2)) {
		safe_str("#-1 FUNCTION (MAILFROM) EXPECTS 1 OR 2 ARGUMENTS", buff, bufc);
		return;
	}
	if (nfargs == 1) {
		playerask = player;
		num = atoi(fargs[0]);
	} else {
		if ((playerask = lookup_player(player, fargs[0], 1)) == NOTHING) {
			safe_str("#-1 NO SUCH PLAYER", buff, bufc);
			return;
		} else if ((player != playerask) && !Wizard(player)) {
			safe_str("#-1 PERMISSION DENIED", buff, bufc);
			return;
		}
		num = atoi(fargs[1]);
	}

	if ((num < 1) || (Typeof(playerask) != TYPE_PLAYER)) {
		safe_str("#-1 NO SUCH MESSAGE", buff, bufc);
		return;
	}
	mp = mail_fetch(playerask, num);
	if (mp != NULL) {
		safe_tprintf_str(buff, bufc, "#%d", mp->from);
		return;
	}
	/* ran off the end of the list without finding anything */
	safe_str("#-1 NO SUCH MESSAGE", buff, bufc);
}
#endif

/* ---------------------------------------------------------------------------
 * fun_hasattr: does object X have attribute Y.
 */

FUNCTION(fun_hasattr)
{
	dbref thing, aowner;
	int aflags;
	ATTR *attr;
	char *tbuf;

	thing = match_thing(player, fargs[0]);
	if (thing == NOTHING) {
		safe_str("#-1 NO MATCH", buff, bufc);
   		return;
	} else if (!Examinable(player, thing)) {
		safe_str("#-1 PERMISSION DENIED", buff, bufc);
		return;
	}
	attr = atr_str(fargs[1]);
	if (!attr) {
		safe_chr('0', buff, bufc);
		return;
	}
	atr_get_info(thing, attr->number, &aowner, &aflags);
	if (!See_attr(player, thing, attr, aowner, aflags)) {
		safe_chr('0', buff, bufc);
	} else {
		tbuf = atr_get(thing, attr->number, &aowner, &aflags);
		if (*tbuf) {
			safe_chr('1', buff, bufc);
		} else {
			safe_chr('0', buff, bufc);
		}
		free_lbuf(tbuf);
	}
}

FUNCTION(fun_hasattrp)
{
	dbref thing, aowner;
	int aflags;
	ATTR *attr;
	char *tbuf;

	thing = match_thing(player, fargs[0]);
	if (thing == NOTHING) {
		safe_str("#-1 NO MATCH", buff, bufc);
		return;
	} else if (!Examinable(player, thing)) {
		safe_str("#-1 PERMISSION DENIED", buff, bufc);
		return;
	}
	attr = atr_str(fargs[1]);
	if (!attr) {
		safe_chr('0', buff, bufc);
		return;
	}
	atr_pget_info(thing, attr->number, &aowner, &aflags);
	if (!See_attr(player, thing, attr, aowner, aflags)) {
		safe_chr('0', buff, bufc);
	} else {
		tbuf = atr_pget(thing, attr->number, &aowner, &aflags);
		if (*tbuf) {
			safe_chr('1', buff, bufc);
		} else {
			safe_chr('0', buff, bufc);
		}
		free_lbuf(tbuf);
	}
}

/* ---------------------------------------------------------------------------
 * fun_default, fun_edefault, and fun_udefault:
 * These check for the presence of an attribute. If it exists, then it
 * is gotten, via the equivalent of get(), get_eval(), or u(), respectively.
 * Otherwise, the default message is used.
 * In the case of udefault(), the remaining arguments to the function
 * are used as arguments to the u().
 */

FUNCTION(fun_default)
{
	dbref thing, aowner;
	int attrib, aflags;
	ATTR *attr;
	char *objname, *atr_gotten, *bp, *str;

	objname = bp = alloc_lbuf("fun_default");
	str = fargs[0];
	exec(objname, &bp, 0, player, cause, EV_EVAL | EV_STRIP | EV_FCHECK, &str,
	     cargs, ncargs);
	*bp = '\0';

	/* First we check to see that the attribute exists on the object.
	 * If so, we grab it and use it. 
	 */

	if (objname != NULL) {
		if (parse_attrib(player, objname, &thing, &attrib) &&
		    (attrib != NOTHING)) {
			attr = atr_num(attrib);
			if (attr && !(attr->flags & AF_IS_LOCK)) {
				atr_gotten = atr_pget(thing, attrib, &aowner, &aflags);
				if (*atr_gotten &&
				check_read_perms(player, thing, attr, aowner,
						 aflags, buff, bufc)) {
					safe_str(atr_gotten, buff, bufc);
					free_lbuf(atr_gotten);
					free_lbuf(objname);
					return;
				}
				free_lbuf(atr_gotten);
			}
		}
		free_lbuf(objname);
	}
	/* If we've hit this point, we've not gotten anything useful, so we
	 * go and evaluate the default. 
	 */

	str = fargs[1];
	exec(buff, bufc, 0, player, cause, EV_EVAL | EV_STRIP | EV_FCHECK, &str,
	     cargs, ncargs);
}

FUNCTION(fun_edefault)
{
	dbref thing, aowner;
	int attrib, aflags;
	ATTR *attr;
	char *objname, *atr_gotten, *defcase, *bp, *str;

	objname = bp = alloc_lbuf("fun_edefault");
	str = fargs[0];
	exec(objname, &bp, 0, player, cause, EV_EVAL | EV_STRIP | EV_FCHECK, &str,
	     cargs, ncargs);
	*bp = '\0';

	/* First we check to see that the attribute exists on the object. 
	 * If so, we grab it and use it. 
	 */

	if (objname != NULL) {
		if (parse_attrib(player, objname, &thing, &attrib) &&
		    (attrib != NOTHING)) {
			attr = atr_num(attrib);
			if (attr && !(attr->flags & AF_IS_LOCK)) {
				atr_gotten = atr_pget(thing, attrib, &aowner, &aflags);
				if (*atr_gotten &&
				check_read_perms(player, thing, attr, aowner,
						 aflags, buff, bufc)) {
					str = atr_gotten;
					exec(buff, bufc, 0, thing, player, EV_FIGNORE | EV_EVAL,
					     &str, (char **)NULL, 0);
					free_lbuf(atr_gotten);
					free_lbuf(objname);
					return;
				}
				free_lbuf(atr_gotten);
			}
		}
		free_lbuf(objname);
	}
	/* If we've hit this point, we've not gotten anything useful, so we
	 * go and evaluate the default. 
	 */

	str = fargs[1];
	exec(buff, bufc, 0, player, cause, EV_EVAL | EV_STRIP | EV_FCHECK, &str,
	     cargs, ncargs);
}

FUNCTION(fun_udefault)
{
	dbref thing, aowner;
	int aflags, anum, i, j;
	ATTR *ap;
	char *objname, *atext, *str, *bp, *xargs[NUM_ENV_VARS];


	if (nfargs < 2)		/* must have at least two arguments */
		return;

	str = fargs[0];
	objname = bp = alloc_lbuf("fun_udefault");
	exec(objname, &bp, 0, player, cause, EV_EVAL | EV_STRIP | EV_FCHECK, &str,
	     cargs, ncargs);
	*bp = '\0';

	/* First we check to see that the attribute exists on the object.
	 * If so, we grab it and use it. 
	 */

	if (objname != NULL) {
		if (parse_attrib(player, objname, &thing, &anum)) {
			if ((anum == NOTHING) || (!Good_obj(thing)))
				ap = NULL;
			else
				ap = atr_num(anum);
		} else {
			thing = player;
			ap = atr_str(objname);
		}
		if (ap) {
			atext = atr_pget(thing, ap->number, &aowner, &aflags);
			if (atext) {
				if (*atext &&
				    check_read_perms(player, thing, ap, aowner, aflags,
						     buff, bufc)) {
					/* Now we have a problem -- we've got to go eval
					 * all of those arguments to the function. 
					 */
					 for (i = 2, j = 0; j < NUM_ENV_VARS; i++, j++) {
					 	if ((i < NUM_ENV_VARS) && fargs[i]) {
					 		 bp = xargs[j] = alloc_lbuf("fun_udefault_args");
					 		 str = fargs[i];
					 		 exec(xargs[j], &bp, 0, player, cause,
					 		        EV_STRIP | EV_FCHECK | EV_EVAL,
					 		        &str, cargs, ncargs);
					 	} else {
					 		xargs[j] = NULL;
					 	} 
					 }
					 
					str = atext;
					exec(buff, bufc, 0, thing, cause, EV_FCHECK | EV_EVAL, &str,
					     xargs, nfargs - 2);
					
					/* Then clean up after ourselves. */
					for (j = 0; j < NUM_ENV_VARS; j++)
						if (xargs[j])
							free_lbuf(xargs[j]);
							
					free_lbuf(atext);
					free_lbuf(objname);
					return;
				}
				free_lbuf(atext);
			}
		}
		free_lbuf(objname);
	}
	/* If we've hit this point, we've not gotten anything useful, so we 
	 * go and evaluate the default. 
	 */

	str = fargs[1];
	exec(buff, bufc, 0, player, cause, EV_EVAL | EV_STRIP | EV_FCHECK, &str,
	     cargs, ncargs);
}

/* ---------------------------------------------------------------------------
 * fun_findable: can X locate Y
 */

/* Borrowed from PennMUSH 1.50 */
FUNCTION(fun_findable)
{
	dbref obj = match_thing(player, fargs[0]);
	dbref victim = match_thing(player, fargs[1]);

	if (obj == NOTHING)
		safe_str("#-1 ARG1 NOT FOUND", buff, bufc);
	else if (victim == NOTHING)
		safe_str("#-1 ARG2 NOT FOUND", buff, bufc);
	else
		safe_ltos(buff, bufc, locatable(obj, victim, obj));
}

/* ---------------------------------------------------------------------------
 * fun_visible:  Can X examine Y. If X does not exist, 0 is returned.
 *               If Y, the object, does not exist, 0 is returned. If
 *               Y the object exists, but the optional attribute does
 *               not, X's ability to return Y the object is returned.
 */

/* Borrowed from PennMUSH 1.50 */
FUNCTION(fun_visible)
{
	dbref it, thing, aowner;
	int aflags, atr;
	ATTR *ap;

	if ((it = match_thing(player, fargs[0])) == NOTHING) {
		safe_chr('0', buff, bufc);
		return;
	}
	if (parse_attrib(player, fargs[1], &thing, &atr)) {
		if (atr == NOTHING) {
			safe_ltos(buff, bufc, Examinable(it, thing));
			return;
		}
		ap = atr_num(atr);
		atr_pget_info(thing, atr, &aowner, &aflags);
		safe_ltos(buff, bufc, See_attr(it, thing, ap, aowner, aflags));
		return;
	}
	thing = match_thing(player, fargs[1]);
	if (!Good_obj(thing)) {
		safe_chr('0', buff, bufc);
		return;
	}
	safe_ltos(buff, bufc, Examinable(it, thing));
}

/* ---------------------------------------------------------------------------
 * fun_elements: given a list of numbers, get corresponding elements from
 * the list.  elements(ack bar eep foof yay,2 4) ==> bar foof
 * The function takes a separator, but the separator only applies to the
 * first list.
 */

/* Borrowed from PennMUSH 1.50 */
FUNCTION(fun_elements)
{
	int nwords, cur;
	char *ptrs[LBUF_SIZE / 2];
	char *wordlist, *s, *r, sep, osep, *oldp;

	svarargs_preamble("ELEMENTS", 4);
	oldp = *bufc;

	/* Turn the first list into an array. */

	wordlist = alloc_lbuf("fun_elements.wordlist");
	strcpy(wordlist, fargs[0]);
	nwords = list2arr(ptrs, LBUF_SIZE / 2, wordlist, sep);

	s = trim_space_sep(fargs[1], ' ');

	/* Go through the second list, grabbing the numbers and finding the
	 * corresponding elements. 
	 */

	do {
		r = split_token(&s, ' ');
		cur = atoi(r) - 1;
		if ((cur >= 0) && (cur < nwords) && ptrs[cur]) {
			if (oldp != *bufc)
				safe_chr(osep, buff, bufc);
			safe_str(ptrs[cur], buff, bufc);
		}
	} while (s);
	free_lbuf(wordlist);
}

/* ---------------------------------------------------------------------------
 * fun_grab: a combination of extract() and match(), sortof. We grab the
 *           single element that we match.
 *
 *   grab(Test:1 Ack:2 Foof:3,*:2)    => Ack:2
 *   grab(Test-1+Ack-2+Foof-3,*o*,+)  => Ack:2
 */

/* Borrowed from PennMUSH 1.50 */
FUNCTION(fun_grab)
{
	char *r, *s, sep;

	varargs_preamble("GRAB", 3);

	/* Walk the wordstring, until we find the word we want. */

	s = trim_space_sep(fargs[0], sep);
	do {
		r = split_token(&s, sep);
		if (quick_wild(fargs[1], r)) {
			safe_str(r, buff, bufc);
			return;
		}
	} while (s);
}

/* ---------------------------------------------------------------------------
 * fun_scramble:  randomizes the letters in a string.
 */

/* Borrowed from PennMUSH 1.50 */
FUNCTION(fun_scramble)
{
	int n, i, j;
	char c, *old;

	if (!fargs[0] || !*fargs[0]) {
		return;
	}
	old = *bufc;

	safe_str(fargs[0], buff, bufc);

	n = strlen(old);

	for (i = 0; i < n; i++) {
		j = (random() % (n - i)) + i;
		c = old[i];
		old[i] = old[j];
		old[j] = c;
	}
}

/* ---------------------------------------------------------------------------
 * fun_shuffle: randomize order of words in a list.
 */

/* Borrowed from PennMUSH 1.50 */
static void swap(p, q)
char **p;
char **q;
{
	/* swaps two points to strings */

	char *temp;

	temp = *p;
	*p = *q;
	*q = temp;
}

FUNCTION(fun_shuffle)
{
	char *words[LBUF_SIZE];
	int n, i, j;
	char sep, osep;

	if (!nfargs || !fargs[0] || !*fargs[0]) {
		return;
	}
	svarargs_preamble("SHUFFLE", 3);

	n = list2arr(words, LBUF_SIZE, fargs[0], sep);

	for (i = 0; i < n; i++) {
		j = (random() % (n - i)) + i;
		swap(&words[i], &words[j]);
	}
	arr2list(words, n, buff, bufc, osep);
}

static char ucomp_buff[LBUF_SIZE];
static dbref ucomp_cause;
static dbref ucomp_player;

static int u_comp(s1, s2)
const void *s1, *s2;
{
	/* Note that this function is for use in conjunction with our own 
	 * sane_qsort routine, NOT with the standard library qsort! 
	 */

	char *result, *tbuf, *elems[2], *bp, *str;
	int n;

	if ((mudstate.func_invk_ctr > mudconf.func_invk_lim) ||
	    (mudstate.func_nest_lev > mudconf.func_nest_lim))
		return 0;

	tbuf = alloc_lbuf("u_comp");
	elems[0] = (char *)s1;
	elems[1] = (char *)s2;
	strcpy(tbuf, ucomp_buff);
	result = bp = alloc_lbuf("u_comp");
	str = tbuf;
	exec(result, &bp, 0, ucomp_player, ucomp_cause, EV_STRIP | EV_FCHECK | EV_EVAL,
	     &str, &(elems[0]), 2);
	*bp = '\0';
	if (!result)
		n = 0;
	else {
		n = atoi(result);
		free_lbuf(result);
	}
	free_lbuf(tbuf);
	return n;
}

static void sane_qsort(array, left, right, compare)
void *array[];
int left, right;
int (*compare) ();
{
	/* Andrew Molitor's qsort, which doesn't require transitivity between
	 * comparisons (essential for preventing crashes due to
	 * boneheads who write comparison functions where a > b doesn't
	 * mean b < a).  
	 */

	int i, last;
	void *tmp;

      loop:
	if (left >= right)
		return;

	/* Pick something at random at swap it into the leftmost slot */
	/* This is the pivot, we'll put it back in the right spot later */

	i = random() % (1 + (right - left));
	tmp = array[left + i];
	array[left + i] = array[left];
	array[left] = tmp;

	last = left;
	for (i = left + 1; i <= right; i++) {

		/* Walk the array, looking for stuff that's less than our */
		/* pivot. If it is, swap it with the next thing along */

		if ((*compare) (array[i], array[left]) < 0) {
			last++;
			if (last == i)
				continue;

			tmp = array[last];
			array[last] = array[i];
			array[i] = tmp;
		}
	}

	/* Now we put the pivot back, it's now in the right spot, we never */
	/* need to look at it again, trust me.                             */

	tmp = array[last];
	array[last] = array[left];
	array[left] = tmp;

	/* At this point everything underneath the 'last' index is < the */
	/* entry at 'last' and everything above it is not < it.          */

	if ((last - left) < (right - last)) {
		sane_qsort(array, left, last - 1, compare);
		left = last + 1;
		goto loop;
	} else {
		sane_qsort(array, last + 1, right, compare);
		right = last - 1;
		goto loop;
	}
}

FUNCTION(fun_sortby)
{
	char *atext, *list, *ptrs[LBUF_SIZE / 2], sep, osep;
	int nptrs, aflags, anum;
	dbref thing, aowner;
	ATTR *ap;

	if ((nfargs == 0) || !fargs[0] || !*fargs[0]) {
		return;
	}
	svarargs_preamble("SORTBY", 4);

	if (parse_attrib(player, fargs[0], &thing, &anum)) {
		if ((anum == NOTHING) || !Good_obj(thing))
			ap = NULL;
		else
			ap = atr_num(anum);
	} else {
		thing = player;
		ap = atr_str(fargs[0]);
	}

	if (!ap) {
		return;
	}
	atext = atr_pget(thing, ap->number, &aowner, &aflags);
	if (!atext) {
		return;
	} else if (!*atext || !See_attr(player, thing, ap, aowner, aflags)) {
		free_lbuf(atext);
		return;
	}
	strcpy(ucomp_buff, atext);
	ucomp_player = thing;
	ucomp_cause = cause;

	list = alloc_lbuf("fun_sortby");
	strcpy(list, fargs[1]);
	nptrs = list2arr(ptrs, LBUF_SIZE / 2, list, sep);

	if (nptrs > 1)		/* pointless to sort less than 2 elements */
		sane_qsort((void **)ptrs, 0, nptrs - 1, u_comp);

	arr2list(ptrs, nptrs, buff, bufc, sep);
	free_lbuf(list);
	free_lbuf(atext);
}

/* ---------------------------------------------------------------------------
 * fun_last: Returns last word in a string
 */

FUNCTION(fun_last)
{
	char *s, *last, sep;
	int len, i;

	/* If we are passed an empty arglist return a null string */

	if (nfargs == 0) {
		return;
	}
	varargs_preamble("LAST", 2);
	s = trim_space_sep(fargs[0], sep);	/* trim leading spaces */

	/* If we're dealing with spaces, trim off the trailing stuff */

	if (sep == ' ') {
		len = strlen(s);
		for (i = len - 1; s[i] == ' '; i--) ;
		if (i + 1 <= len)
			s[i + 1] = '\0';
	}
	last = (char *)rindex(s, sep);
	if (last)
		safe_str(++last, buff, bufc);
	else
		safe_str(s, buff, bufc);
}

FUNCTION(fun_matchall)
{
	int wcount;
	char *r, *s, *old, sep, osep, tbuf[8];

	svarargs_preamble("MATCHALL", 4);

	/* SPECIAL CASE: If there's no output delimiter specified, we use
	 * a space, NOT the delimiter given for the list!
	 */
	if (nfargs < 4)
	    osep = ' ';

	old = *bufc;

	/* Check each word individually, returning the word number of all 
	 * that match. If none match, return a null string.
	 */

	wcount = 1;
	s = trim_space_sep(fargs[0], sep);
	do {
		r = split_token(&s, sep);
		if (quick_wild(fargs[1], r)) {
			ltos(tbuf, wcount);
			if (old != *bufc)
				safe_chr(osep, buff, bufc);
			safe_str(tbuf, buff, bufc);
		}
		wcount++;
	} while (s);
}

/* ---------------------------------------------------------------------------
 * fun_ports: Returns a list of ports for a user.
 */

/* Borrowed from TinyMUSH 2.2 */
FUNCTION(fun_ports)
{
	dbref target;

	if (!Wizard(player)) {
		return;
	}
	target = lookup_player(player, fargs[0], 1);
	if (!Good_obj(target) || !Connected(target)) {
		return;
	}
	make_portlist(player, target, buff, bufc);
}

/* ---------------------------------------------------------------------------
 * fun_mix: Like map, but operates on two lists or more lists simultaneously,
 * passing the elements as %0, %1, %2, etc.
 */

FUNCTION(fun_mix)
{
    dbref aowner, thing;
    int aflags, anum, i, lastn, nwords, wc;
    ATTR *ap;
    char *str, *atext, *os[10], *cp1, *cp2, *atextbuf, sep;
    char *cp[10];

    /* Check to see if we have an appropriate number of arguments.
     * If there are more than three arguments, the last argument is
     * ALWAYS assumed to be a delimiter.
     */

    if (!fn_range_check("MIX", nfargs, 3, 10, buff, bufc)) {
	return;
    }
    if (nfargs < 4) {
	sep = ' ';
	lastn = nfargs - 1;
    } else if (!delim_check(fargs, nfargs, nfargs, &sep, buff, bufc, 0,
			    player, cause, cargs, ncargs)) {
	return;
    } else {
	lastn = nfargs - 2;
    }

    /* Get the attribute, check the permissions. */

    if (parse_attrib(player, fargs[0], &thing, &anum)) {
	if ((anum == NOTHING) || !Good_obj(thing))
	    ap = NULL;
	else
	    ap = atr_num(anum);
    } else {
	thing = player;
	ap = atr_str(fargs[0]);
    }

    if (!ap) {
	return;
    }
    atext = atr_pget(thing, ap->number, &aowner, &aflags);
    if (!atext) {
	return;
    } else if (!*atext || !See_attr(player, thing, ap, aowner, aflags)) {
	free_lbuf(atext);
	return;
    }

    for (i = 0; i < 10; i++)
	cp[i] = NULL;

    /* process the lists, one element at a time. */

    for (i = 1; i <= lastn; i++) {
	cp[i] = trim_space_sep(fargs[i], sep);
    }
    nwords = countwords(cp[1], sep);
    for (i = 2; i<= lastn; i++) {
	if (countwords(cp[i], sep) != nwords) {
	    free_lbuf(atext);
	    safe_str("#-1 LISTS MUST BE OF EQUAL SIZE", buff, bufc);
	    return;
	}
    }
    atextbuf = alloc_lbuf("fun_mix");

    for (wc = 0;
	 (wc < nwords) && (mudstate.func_invk_ctr < mudconf.func_invk_lim);
	 wc++) {
	for (i = 1; i <= lastn; i++) {
	    os[i - 1] = split_token(&cp[i], sep);
	}
	strcpy(atextbuf, atext);
	
	if (*bufc != buff)
	    safe_chr(sep, buff, bufc);

	str = atextbuf;
	
	exec(buff, bufc, 0, player, cause, EV_STRIP | EV_FCHECK | EV_EVAL,
		      &str, &(os[0]), lastn);
    }
    free_lbuf(atext);
    free_lbuf(atextbuf);
}

/* ---------------------------------------------------------------------------
 * fun_foreach: like map(), but it operates on a string, rather than on a list,
 * calling a user-defined function for each character in the string.
 * No delimiter is inserted between the results.
 */

FUNCTION(fun_foreach)
{
    dbref aowner, thing;
    int aflags, anum;
    ATTR *ap;
    char *str, *atext, *atextbuf, *cp, *cbuf;
    char start_token, end_token;
    int in_string = 1;

    if (!fn_range_check("FOREACH", nfargs, 2, 4, buff, bufc))
	return;

    if (parse_attrib(player, fargs[0], &thing, &anum)) {
	if ((anum == NOTHING) || !Good_obj(thing))
	    ap = NULL;
	else
	    ap = atr_num(anum);
    } else {
	thing = player;
	ap = atr_str(fargs[0]);
    }

    if (!ap) {
	return;
    }
    atext = atr_pget(thing, ap->number, &aowner, &aflags);
    if (!atext) {
	return;
    } else if (!*atext || !See_attr(player, thing, ap, aowner, aflags)) {
	free_lbuf(atext);
	return;
    }
    atextbuf = alloc_lbuf("fun_foreach");
    cbuf = alloc_lbuf("fun_foreach.cbuf");
    cp = trim_space_sep(fargs[1], ' ');

    start_token = '\0';
    end_token = '\0';

    if (nfargs > 2) {
	in_string = 0;
	start_token = *fargs[2];
    }
    if (nfargs > 3) {
	end_token = *fargs[3];
    }

    while (cp && *cp && (mudstate.func_invk_ctr < mudconf.func_invk_lim)) {

	if (!in_string) {
	    /* Look for a start token. */
	    while (*cp && (*cp != start_token)) {
		safe_chr(*cp, buff, bufc);
		cp++;
	    }
	    if (!*cp)
		break;
	    /* Skip to the next character. Don't copy the start token. */
	    cp++;
	    if (!*cp)
		break;
	    in_string = 1;
	}
	if (*cp == end_token) {
	    /* We've found an end token. Skip over it. Note that it's
	     * possible to have a start and end token next to one
	     * another.
	     */
	    cp++;
	    in_string = 0;
	    continue;
	}

	cbuf[0] = *cp++;
	cbuf[1] = '\0';
	strcpy(atextbuf, atext);
	str = atextbuf; 
	exec(buff, bufc, 0, player, cause, EV_STRIP | EV_FCHECK | EV_EVAL,
		      &str, &cbuf, 1);
    }

    free_lbuf(atextbuf);
    free_lbuf(atext);
    free_lbuf(cbuf);
}

/* ---------------------------------------------------------------------------
 * fun_munge: combines two lists in an arbitrary manner.
 */

/* Borrowed from TinyMUSH 2.2 */
FUNCTION(fun_munge)
{
	dbref aowner, thing;
	int aflags, anum, nptrs1, nptrs2, nresults, i, j;
	ATTR *ap;
	char *list1, *list2, *rlist;
	char *ptrs1[LBUF_SIZE / 2], *ptrs2[LBUF_SIZE / 2], *results[LBUF_SIZE / 2];
	char *atext, *bp, *str, sep, osep, *oldp;

	oldp = *bufc;
	if ((nfargs == 0) || !fargs[0] || !*fargs[0]) {
		return;
	}
	svarargs_preamble("MUNGE", 5);

	/* Find our object and attribute */

	if (parse_attrib(player, fargs[0], &thing, &anum)) {
		if ((anum == NOTHING) || !Good_obj(thing))
			ap = NULL;
		else
			ap = atr_num(anum);
	} else {
		thing = player;
		ap = atr_str(fargs[0]);
	}

	if (!ap) {
		return;
	}
	atext = atr_pget(thing, ap->number, &aowner, &aflags);
	if (!atext) {
		return;
	} else if (!*atext || !See_attr(player, thing, ap, aowner, aflags)) {
		free_lbuf(atext);
		return;
	}
	/* Copy our lists and chop them up. */

	list1 = alloc_lbuf("fun_munge.list1");
	list2 = alloc_lbuf("fun_munge.list2");
	strcpy(list1, fargs[1]);
	strcpy(list2, fargs[2]);
	nptrs1 = list2arr(ptrs1, LBUF_SIZE / 2, list1, sep);
	nptrs2 = list2arr(ptrs2, LBUF_SIZE / 2, list2, sep);

	if (nptrs1 != nptrs2) {
		safe_str("#-1 LISTS MUST BE OF EQUAL SIZE", buff, bufc);
		free_lbuf(atext);
		free_lbuf(list1);
		free_lbuf(list2);
		return;
	}
	/* Call the u-function with the first list as %0. */

	bp = rlist = alloc_lbuf("fun_munge");
	str = atext;
	exec(rlist, &bp, 0, player, cause, EV_STRIP | EV_FCHECK | EV_EVAL, &str,
	     &fargs[1], 1);
	*bp = '\0';

	/* Now that we have our result, put it back into array form. Search
	 * through list1 until we find the element position, then 
	 * copy the corresponding element from list2. 
	 */

	nresults = list2arr(results, LBUF_SIZE / 2, rlist, sep);

	for (i = 0; i < nresults; i++) {
		for (j = 0; j < nptrs1; j++) {
			if (!strcmp(results[i], ptrs1[j])) {
				if (*bufc != oldp)
					safe_chr(osep, buff, bufc);
				safe_str(ptrs2[j], buff, bufc);
				ptrs1[j][0] = '\0';
				break;
			}
		}
	}
	free_lbuf(atext);
	free_lbuf(list1);
	free_lbuf(list2);
	free_lbuf(rlist);
}

/* die() code borrowed from PennMUSH 1.50 */
int getrandom(x)
int x;
{
	/* In order to be perfectly anal about not introducing any further
	 * sources of statistical bias, we're going to call random()
	 * until we get a number less than the greatest representable 
	 * multiple of x. We'll then return n mod x. 
	 */
	long n;

	if (x <= 0)
		return -1;

	do {
		n = random();
	} while (LONG_MAX - n < x);

/* N.B. This loop happens in randomized constant time, and pretty damn
 * fast randomized constant time too, since P(LONG_MAX - n < x) < 0.5
 * for any x, so for any X, the average number of times we should
 * have to call random() is less than 2.
 */
	return (n % x);
}

FUNCTION(fun_die)
{
	int n, die, count;
	int total = 0;

	if (!fargs[0] || !fargs[1]) {
	    safe_str("0", buff, bufc);
	    return;
	}

	n = atoi(fargs[0]);
	die = atoi(fargs[1]);

	if ((n == 0) || (die <= 0)) {
	    safe_str("0", buff, bufc);
	    return;
	}

	if ((n < 1) || (n > 100)) {
		safe_str("#-1 NUMBER OUT OF RANGE", buff, bufc);
		return;
	}
	for (count = 0; count < n; count++)
		total += getrandom(die) + 1;

	safe_ltos(buff, bufc, total);
}

/* Borrowed from PennMUSH 1.50 */
FUNCTION(fun_lit)
{
	/* Just returns the argument, literally */
	safe_str(fargs[0], buff, bufc);
}

/* shl() and shr() borrowed from PennMUSH 1.50 */
FUNCTION(fun_shl)
{
    safe_ltos(buff, bufc, atoi(fargs[0]) << atoi(fargs[1]));
}

FUNCTION(fun_shr)
{
    safe_ltos(buff, bufc, atoi(fargs[0]) >> atoi(fargs[1]));
}

FUNCTION(fun_band)
{
    safe_ltos(buff, bufc, atoi(fargs[0]) & atoi(fargs[1]));
}

FUNCTION(fun_bor)
{
    safe_ltos(buff, bufc, atoi(fargs[0]) | atoi(fargs[1]));
}

FUNCTION(fun_bnand)
{
    safe_ltos(buff, bufc, atoi(fargs[0]) & ~(atoi(fargs[1])));
}

/* ------------------------------------------------------------------------
 */

FUNCTION(fun_strcat)
{
	int i;
	
	safe_str(fargs[0], buff, bufc);
	for (i = 1; i < nfargs; i++) {
		safe_str(fargs[i], buff, bufc);
	}
}

/* grep() and grepi() code borrowed from PennMUSH 1.50 */
char *grep_util(player, thing, pattern, lookfor, len, insensitive)
dbref player, thing;
char *pattern;
char *lookfor;
int len;
int insensitive;
{
	/* returns a list of attributes which match <pattern> on <thing> 
	 * whose contents have <lookfor> 
	 */
	dbref aowner;
	char *tbuf1, *buf, *text, *attrib;
	char *bp, *bufc;
	int found;
	int ca, aflags;

	bp = tbuf1 = alloc_lbuf("grep_util");
	bufc = buf = alloc_lbuf("grep_util.parse_attrib");
	safe_tprintf_str(buf, &bufc, "#%d/%s", thing, pattern);
	if (parse_attrib_wild(player, buf, &thing, 0, 0, 1)) {
		for (ca = olist_first(); ca != NOTHING; ca = olist_next()) {
			attrib = atr_get(thing, ca, &aowner, &aflags);
			text = attrib;
			found = 0;
			while (*text && !found) {
				if ((!insensitive && !strncmp(lookfor, text, len)) ||
				    (insensitive && !strncasecmp(lookfor, text, len)))
					found = 1;
				else
					text++;
			}

			if (found) {
				if (bp != tbuf1)
					safe_chr(' ', tbuf1, &bp);

				safe_str((char *)(atr_num(ca))->name, tbuf1, &bp);
			}
			free_lbuf(attrib);
		}
	}
	free_lbuf(buf);
	*bp = '\0';
	return tbuf1;
}

FUNCTION(fun_grep)
{
	char *tp;

	dbref it = match_thing(player, fargs[0]);

	if (it == NOTHING) {
		safe_str("#-1 NO MATCH", buff, bufc);
		return;
	} else if (!(Examinable(player, it))) {
		safe_str("#-1 PERMISSION DENIED", buff, bufc);
		return;
	}
	/* make sure there's an attribute and a pattern */
	if (!fargs[1] || !*fargs[1]) {
		safe_str("#-1 NO SUCH ATTRIBUTE", buff, bufc);
		return;
	}
	if (!fargs[2] || !*fargs[2]) {
		safe_str("#-1 INVALID GREP PATTERN", buff, bufc);
		return;
	}
	tp = grep_util(player, it, fargs[1], fargs[2], strlen(fargs[2]), 0);
	safe_str(tp, buff, bufc);
	free_lbuf(tp);
}

FUNCTION(fun_grepi)
{
	char *tp;

	dbref it = match_thing(player, fargs[0]);

	if (it == NOTHING) {
		safe_str("#-1 NO MATCH", buff, bufc);
		return;
	} else if (!(Examinable(player, it))) {
		safe_str("#-1 PERMISSION DENIED", buff, bufc);
		return;
	}
	/* make sure there's an attribute and a pattern */
	if (!fargs[1] || !*fargs[1]) {
		safe_str("#-1 NO SUCH ATTRIBUTE", buff, bufc);
		return;
	}
	if (!fargs[2] || !*fargs[2]) {
		safe_str("#-1 INVALID GREP PATTERN", buff, bufc);
		return;
	}
	tp = grep_util(player, it, fargs[1], fargs[2], strlen(fargs[2]), 1);
	safe_str(tp, buff, bufc);
	free_lbuf(tp);
}

/* Borrowed from PennMUSH 1.50 */
FUNCTION(fun_art)
{
/* checks a word and returns the appropriate article, "a" or "an" */
	char c = tolower(*fargs[0]);

	if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u')
		safe_str("an", buff, bufc);
	else
		safe_chr('a', buff, bufc);
}

/* Borrowed from PennMUSH 1.50 */
FUNCTION(fun_alphamax)
{
	char *amax;
	int i = 1;

	if (!fargs[0]) {
		safe_str("#-1 TOO FEW ARGUMENTS", buff, bufc);
		return;
	} else
		amax = fargs[0];

	while ((i < 10) && fargs[i]) {
		amax = (strcmp(amax, fargs[i]) > 0) ? amax : fargs[i];
		i++;
	}

	safe_str(amax, buff, bufc);
}

/* Borrowed from PennMUSH 1.50 */
FUNCTION(fun_alphamin)
{
	char *amin;
	int i = 1;

	if (!fargs[0]) {
		safe_str("#-1 TOO FEW ARGUMENTS", buff, bufc);
		return;
	} else
		amin = fargs[0];

	while ((i < 10) && fargs[i]) {
		amin = (strcmp(amin, fargs[i]) < 0) ? amin : fargs[i];
		i++;
	}

	safe_str(amin, buff, bufc);
}

/* Borrowed from PennMUSH 1.50 */

FUNCTION(fun_valid)
{
/* Checks to see if a given <something> is valid as a parameter of a
 * given type (such as an object name).
 */

	if (!fargs[0] || !*fargs[0] || !fargs[1] || !*fargs[1]) {
		safe_chr('0', buff, bufc);
	} else if (!strcasecmp(fargs[0], "name"))
		safe_ltos(buff, bufc, ok_name(fargs[1]));
	else
		safe_str("#-1", buff, bufc);
}

/* Borrowed from PennMUSH 1.50 */
FUNCTION(fun_hastype)
{
	dbref it = match_thing(player, fargs[0]);

	if (it == NOTHING) {
		safe_str("#-1 NO MATCH", buff, bufc);
		return;
	} else if (!(Examinable(player, it))) {
		safe_str("#-1 PERMISSION DENIED", buff, bufc);
		return;
	}
	if (!fargs[1] || !*fargs[1]) {
		safe_str("#-1 NO SUCH TYPE", buff, bufc);
		return;
	}
	switch (*fargs[1]) {
	case 'r':
	case 'R':
		safe_str((Typeof(it) == TYPE_ROOM) ? "1" : "0", buff, bufc);
		break;
	case 'e':
	case 'E':
		safe_str((Typeof(it) == TYPE_EXIT) ? "1" : "0", buff, bufc);
		break;
	case 'p':
	case 'P':
		safe_str((Typeof(it) == TYPE_PLAYER) ? "1" : "0", buff, bufc);
		break;
	case 't':
	case 'T':
		safe_str((Typeof(it) == TYPE_THING) ? "1" : "0", buff, bufc);
		break;
	default:
		safe_str("#-1 NO SUCH TYPE", buff, bufc);
		break;
	};
}

/* Borrowed from PennMUSH 1.50 */
FUNCTION(fun_lparent)
{
	dbref it;
	dbref par;
	char tbuf1[20];

	it = match_thing(player, fargs[0]);
	if (!Good_obj(it)) {
		safe_str("#-1 NO MATCH", buff, bufc);
		return;
	} else if (!(Examinable(player, it))) {
		safe_str("#-1 PERMISSION DENIED", buff, bufc);
		return;
	}
	sprintf(tbuf1, "#%d", it);
	safe_str(tbuf1, buff, bufc);
	par = Parent(it);

	while (Good_obj(par) && Examinable(player, it)) {
		sprintf(tbuf1, " #%d", par);
		safe_str(tbuf1, buff, bufc);
		it = par;
		par = Parent(par);
	}
}

/* ---------------------------------------------------------------------------
 * Object stack functions.
 */

void stack_clr(thing)
    dbref thing;
{
    STACK *sp, *tp, *xp;

    sp = stack_get(thing);
    if (sp) {
	for (tp = sp; tp != NULL; ) {
	    XFREE(tp->data, "stack_clr_data");
	    xp = tp;
	    tp = tp->next;
	    XFREE(xp, "stack_clr");
	}
	nhashdelete(thing, &mudstate.objstack_htab);
    }
}

static void stack_set(thing, sp)
    dbref thing;
    STACK *sp;
{
    STACK *xsp;
    int stat;

    if (!sp) {
	nhashdelete(thing, &mudstate.objstack_htab);
	return;
    }

    xsp = stack_get(thing);
    if (xsp) {
	stat = nhashrepl(thing, (int *) sp, &mudstate.objstack_htab);
    } else {
	stat = nhashadd(thing, (int *) sp, &mudstate.objstack_htab);
    }
    if (stat < 0) {		/* failure for some reason */
	STARTLOG(LOG_BUGS, "STK", "SET")
	    log_name(thing);
	ENDLOG
	stack_clr(thing);
    }
}

FUNCTION(fun_empty)
{
    dbref it;

    xvarargs_preamble("EMPTY", 0, 1);

    if (!fargs[0]) {
	it = player;
    } else {
	stack_object(player, it);
    }

    stack_clr(it);
}

FUNCTION(fun_items)
{
    dbref it;
    int i;
    STACK *sp;

    if (!fargs[0]) {
	it = player;
    } else {
	stack_object(player, it);
    }

    for (i = 0, sp = stack_get(it); sp != NULL; sp = sp->next, i++)
	;
    safe_ltos(buff, bufc, i);
}
    

FUNCTION(fun_push)
{
    dbref it;
    char *data;
    STACK *sp;

    xvarargs_preamble("PUSH", 1, 2);

    if (!fargs[1]) {
	it = player;
	data = fargs[0];
    } else {
	stack_object(player, it);
	data = fargs[1];
    }

    sp = (STACK *) XMALLOC(sizeof(STACK), "stack_push");
    if (!sp)			/* out of memory, ouch */
	return;
    sp->next = stack_get(it);
    sp->data = (char *) XMALLOC(sizeof(char *) * (strlen(data) + 1),
				"stack_push_data");
    if (! sp->data)
	return;
    strcpy(sp->data, data);
    stack_set(it, sp);
}

FUNCTION(fun_dup)
{
    dbref it;
    char *data;
    STACK *hp;			/* head of stack */
    STACK *tp;			/* temporary stack pointer */
    STACK *sp;			/* new stack element */
    int pos, count = 0;

    xvarargs_preamble("DUP", 0, 2);

    if (!fargs[0]) {
	it = player;
    } else {
	stack_object(player, it);
    }

    if (!fargs[1] || !*fargs[1]) {
	pos = 0;
    } else {
	pos = atoi(fargs[1]);
    }

    hp = stack_get(it);
    for (tp = hp; (count != pos) && (tp != NULL); count++, tp = tp->next)
	;
    if (!tp) {
	notify_quiet(player, "No such item on stack.");
	return;
    }

    sp = (STACK *) XMALLOC(sizeof(STACK), "stack_dup");
    if (!sp)
	return;
    sp->next = hp;
    sp->data = (char *) XMALLOC(sizeof(char *) * (strlen(tp->data) + 1),
				"stack_dup_data");
    if (!sp->data)
	return;
    strcpy(sp->data, tp->data);
    stack_set(it, sp);
}

FUNCTION(fun_swap)
{
    dbref it;
    STACK *sp, *tp;

    xvarargs_preamble("SWAP", 0, 1);

    if (!fargs[0]) {
	it = player;
    } else {
	stack_object(player, it);
    }

    sp = stack_get(it);
    if (!sp || (sp->next == NULL)) {
	notify_quiet(player, "Not enough items on stack.");
	return;
    }

    tp = sp->next;
    sp->next = tp->next;
    tp->next = sp;
    stack_set(it, tp);
}

static void handle_pop(player, cause, buff, bufc, fargs, flag)
    dbref player, cause;
    char *buff;
    char **bufc;
    char *fargs[];
    int flag;			/* if flag, don't copy */
{
    dbref it;
    int pos, count = 0;
    STACK *sp;
    STACK *prev = NULL;

    if (!fargs[0]) {
	it = player;
    } else {
	stack_object(player, it);
    }
    if (!fargs[1] || !*fargs[1]) {
	pos = 0;
    } else {
	pos = atoi(fargs[1]);
    }

    sp = stack_get(it);
    if (!sp)
	return;

    while (count != pos) {
	if (!sp)
	    return;
	prev = sp;
	sp = sp->next;
	count++;
    }
    if (!sp)
	return;

    if (!flag) {
	safe_str(sp->data, buff, bufc);
    }
    if (count == 0) {
	stack_set(it, sp->next);
    } else {
	prev->next = sp->next;
    }
    XFREE(sp->data, "stack_pop_data");
    XFREE(sp, "stack_pop");
}

FUNCTION(fun_pop)
{
    xvarargs_preamble("POP", 0, 2);
    handle_pop(player, cause, buff, bufc, fargs, 0);
}

FUNCTION(fun_toss)
{
    xvarargs_preamble("TOSS", 0, 2);
    handle_pop(player, cause, buff, bufc, fargs, 1);
}

FUNCTION(fun_popn)
{
    dbref it;
    int pos, nitems, i, count = 0, over = 0, first = 0;
    STACK *sp, *prev, *tp, *xp;
    char sep;

    varargs_preamble("POPN", 4);

    stack_object(player, it);
    pos = atoi(fargs[1]);
    nitems = atoi(fargs[2]);

    sp = stack_get(it);
    if (!sp)
	return;

    while (count != pos) {
	if (!sp)
	    return;
	prev = sp;
	sp = sp->next;
	count++;
    }
    if (!sp)
	return;

    /* We've now hit the start item, the first item. Copy 'em off. */

    for (i = 0, tp = sp; (i < nitems) && (tp != NULL); i++) {
	if (!over) {
	    /* We have to pop off the items regardless of whether
	     * or not there's an overflow, but we can save ourselves
	     * some copying if so.
	     */
	    if (!first) {
		safe_chr(sep, buff, bufc);
		first = 1;
	    }
	    over = safe_str(tp->data, buff, bufc);
	}
	xp = tp;
	tp = tp->next;
	XFREE(xp->data, "stack_popn_data");
	XFREE(xp, "stack_popn");
    }

    /* Relink the chain. */

    if (count == 0) {
	stack_set(it, tp);
    } else {
	prev->next = tp;
    }
}

FUNCTION(fun_peek)
{
    dbref it;
    int pos, count = 0;
    STACK *sp;

    xvarargs_preamble("POP", 0, 2);

    if (!fargs[0]) {
	it = player;
    } else {
	stack_object(player, it);
    }
    if (!fargs[1] || !*fargs[1]) {
	pos = 0;
    } else {
	pos = atoi(fargs[1]);
    }

    sp = stack_get(it);
    if (!sp)
	return;

    while (count != pos) {
	if (!sp)
	    return;
	sp = sp->next;
	count++;
    }
    if (!sp)
	return;

    safe_str(sp->data, buff, bufc);
}

FUNCTION(fun_lstack)
{
    char sep;
    dbref it;
    STACK *sp;
    char *bp;
    int over = 0, first = 1;

    mvarargs_preamble("LSTACK", 0, 2);

    if (!fargs[0]) {
	it = player;
    } else {
	stack_object(player, it);
    }

    bp = buff;
    for (sp = stack_get(it); (sp != NULL) && !over; sp = sp->next) {
	if (!first) {
	    safe_chr(sep, buff, bufc);
	} else {
	    first = 0;
	}
	over = safe_str(sp->data, buff, bufc);
    }
}

/* ---------------------------------------------------------------------------
 * fun_regmatch: Return 0 or 1 depending on whether or not a regular
 * expression matches a string. If a third argument is specified, dump
 * the results of a regexp pattern match into a set of arbitrary r()-registers.
 *
 * regmatch(string, pattern, list of registers)
 * If the number of matches exceeds the registers, those bits are tossed
 * out.
 * If -1 is specified as a register number, the matching bit is tossed.
 * Therefore, if the list is "-1 0 3 5", the regexp $0 is tossed, and
 * the regexp $1, $2, and $3 become r(0), r(3), and r(5), respectively.
 *
 */

FUNCTION(fun_regmatch)
{
int i, nqregs, got_match, curq, len;
char *qregs[NSUBEXP];
int qnums[10];
regexp *re;
int matched;

	if (!fn_range_check("REGMATCH", nfargs, 2, 3, buff, bufc))
		return;

	if ((re = regcomp(fargs[1])) == NULL) {
		/* Matching error. */
		notify_quiet(player, (const char *) regexp_errbuf);
		safe_chr('0', buff, bufc);
		return;
	}

	matched = (int) regexec(re, fargs[0]);
	safe_ltos(buff, bufc, matched);

	/* If we don't have a third argument, we're done. */
	if (nfargs != 3) {
		free(re);
		return;
	}

	/* We need to parse the list of registers. Anything that we don't get
	 * is assumed to be -1. If we didn't match, or the match went wonky,
	 * then set the register to empty. Otherwise, fill the register
	 * with the subexpression.
	 */
	nqregs = list2arr(qregs, NSUBEXP, fargs[2], ' ');
	for (i = 0; i < nqregs; i++) {
	    if (qregs[i] && *qregs[i] && is_integer(qregs[i]))
		curq = atoi(qregs[i]);
	    else
		curq = -1;
	    if ((curq < 0) || (curq > 9))
		continue;
	    if (!mudstate.global_regs[curq]) {
		mudstate.global_regs[curq] = alloc_lbuf("fun_regmatch");
	    }
	    if (!matched || !re->startp[i] || !re->endp[i]) {
		mudstate.global_regs[curq][0] = '\0'; /* empty string */
	    } else {
		len = re->endp[i] - re->startp[i];
		if (len > LBUF_SIZE - 1)
		    len = LBUF_SIZE - 1;
		else if (len < 0)
		    len = 0;
		strncpy(mudstate.global_regs[curq], re->startp[i], len);
		mudstate.global_regs[curq][len] = '\0';	/* must null-terminate */
	    }
	}

	free(re);
}

/* ---------------------------------------------------------------------------
 * fun_translate: Takes a string and a second argument. If the second argument
 * is 0 or s, control characters are converted to spaces. If it's 1 or p,
 * they're converted to percent substitutions.
 */

FUNCTION(fun_translate)
{
	if (*fargs[0] && *fargs[1]) {
		safe_str(translate_string(fargs[0], atoi(fargs[1])), buff, bufc);
	}
}

/* ---------------------------------------------------------------------------
 * fun_lastcreate: Return the last object of type Y that X created.
 */

FUNCTION(fun_lastcreate)
{
    int i, aowner, aflags, obj_list[4], obj_type;
    char *obj_str, *p;
    dbref obj = match_thing(player, fargs[0]);

    if (!controls(player, obj)) {    /* Automatically checks for GoodObj */
	safe_str("#-1", buff, bufc);
	return;
    }

    switch (*fargs[1]) {
      case 'R':
      case 'r':
	obj_type = 0;
	break;
      case 'E':
      case 'e':
	obj_type = 1;;
	break;
      case 'T':
      case 't':
	obj_type = 2;
	break;
      case 'P':
      case 'p':
	obj_type = 3;
	break;
      default:
	notify_quiet(player, "Invalid object type.");
	safe_str("#-1", buff, bufc);
	return;
    }

    if ((obj_str = atr_get(obj, A_NEWOBJS, &aowner, &aflags)) == NULL) {
	safe_str("#-1", buff, bufc);
	return;
    }

    if (!*obj_str) {
	free_lbuf(obj_str);
	safe_str("#-1", buff, bufc);
	return;
    }

    for (p = (char *) strtok(obj_str, " "), i = 0;
	 p && (i < 4);
	 p = (char *) strtok(NULL, " "), i++) {
	obj_list[i] = atoi(p);
    }
    free_lbuf(obj_str);

    *(*bufc)++ = '#';
    safe_ltos(buff, bufc, obj_list[obj_type]);
}

/*---------------------------------------------------------------------------
 * Pueblo HTML-related functions.
 */

#ifdef PUEBLO_SUPPORT

FUNCTION(fun_html_escape)
{
    html_escape(fargs[0], buff, bufc);
}

FUNCTION(fun_html_unescape)
{
    const char *msg_orig;
    int ret = 0;

    for (msg_orig = fargs[0]; msg_orig && *msg_orig && !ret; msg_orig++) {
	switch (*msg_orig) {
	  case '&':
	    if (!strncmp(msg_orig, "&quot;", 6)) {
		ret = safe_chr_fn('\"', buff, bufc);
		msg_orig += 5;
	    } else if (!strncmp(msg_orig, "&lt;", 4)) {
		ret = safe_chr_fn('<', buff, bufc);
		msg_orig += 3;
	    } else if (!strncmp(msg_orig, "&gt;", 4)) {
		ret = safe_chr_fn('>', buff, bufc);
		msg_orig += 3;
	    } else if (!strncmp(msg_orig, "&amp;", 5)) {
		ret = safe_chr_fn('&', buff, bufc);
		msg_orig += 4;
	    }
	    break;
	  default:
	    ret = safe_chr_fn(*msg_orig, buff, bufc);
	    break;
	}
    }
}

FUNCTION(fun_url_escape)
{
    /* These are the characters which are converted to %<hex> */
    char *escaped_chars = "<>#%{}|\\^~[]';/?:@=&\"+";
    const char *msg_orig;
    int ret = 0;
    char tbuf[10];

    for (msg_orig = fargs[0]; msg_orig && *msg_orig && !ret; msg_orig++) {
	if (index(escaped_chars, *msg_orig)) {
	    sprintf(tbuf, "%%%2x", *msg_orig);
	    ret = safe_str(tbuf, buff, bufc);
	} else if (*msg_orig == ' ') {
	    ret = safe_chr_fn('+', buff, bufc);
	} else{
	    ret = safe_chr_fn(*msg_orig, buff, bufc);
	}
    }
}

FUNCTION(fun_url_unescape)
{
    const char *msg_orig;
    int ret = 0;
    unsigned int tempchar;
    char tempstr[10];

    for (msg_orig = fargs[0]; msg_orig && *msg_orig && !ret;) {
	switch (*msg_orig) {
	  case '+':
	    ret = safe_chr_fn(' ', buff, bufc);
	    msg_orig++;
	    break;
	  case '%':
	    strncpy(tempstr, msg_orig+1, 2);
	    tempstr[2] = '\0';
	    if (sscanf(tempstr, "%x", &tempchar) == 1)
		ret = safe_chr_fn(tempchar, buff, bufc);
	    if (*msg_orig)
		msg_orig++;	/* Skip the '%' */
	    if (*msg_orig) 	/* Skip the 1st hex character. */
		msg_orig++;
	    if (*msg_orig)	/* Skip the 2nd hex character. */
		msg_orig++;
	    break;
	  default:
	    ret = safe_chr_fn(*msg_orig, buff, bufc);
	    msg_orig++;
	    break;
	}
    }
    return;
}
#endif /* PUEBLO_SUPPORT */
