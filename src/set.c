/* set.c - commands which set parameters */
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

#include "match.h"	/* required by code */
#include "powers.h"	/* required by code */
#include "attrs.h"	/* required by code */
#include "ansi.h"	/* required by code */

extern NAMETAB indiv_attraccess_nametab[];

dbref match_controlled(player, name)
dbref player;
const char *name;
{
	dbref mat;

	init_match(player, name, NOTYPE);
	match_everything(MAT_EXIT_PARENTS);
	mat = noisy_match_result();
	if (Good_obj(mat) && !Controls(player, mat)) {
		notify_quiet(player, NOPERM_MESSAGE);
		return NOTHING;
	} else {
		return (mat);
	}
}

dbref match_controlled_quiet(player, name)
dbref player;
const char *name;
{
	dbref mat;

	init_match(player, name, NOTYPE);
	match_everything(MAT_EXIT_PARENTS);
	mat = match_result();
	if (Good_obj(mat) && !Controls(player, mat)) {
		return NOTHING;
	} else {
		return (mat);
	}
}

void do_chzone(player, cause, key, name, newobj)
dbref player, cause;
int key;
const char *name;
const char *newobj;
{
	dbref thing;
	dbref zone;

	if (!mudconf.have_zones) {
		notify(player, "Zones disabled.");
		return;
	}
	init_match(player, name, NOTYPE);
	match_everything(0);
	if ((thing = noisy_match_result()) == NOTHING)
		return;

	if (!newobj || !*newobj || !strcasecmp(newobj, "none"))
		zone = NOTHING;
	else {
		init_match(player, newobj, NOTYPE);
		match_everything(0);
		if ((zone = noisy_match_result()) == NOTHING)
			return;

		if ((Typeof(zone) != TYPE_THING) && (Typeof(zone) != TYPE_ROOM)) {
			notify(player, "Invalid zone object type.");
			return;
		}
	}

	if (!Wizard(player) && !(Controls(player, thing)) &&
	    !(check_zone_for_player(player, thing)) &&
	    !(db[player].owner == db[thing].owner)) {
		notify(player, "You don't have the power to shift reality.");
		return;
	}

	/* a player may change an object's zone to NOTHING or to an object he 
	 * owns 
	 */
	if ((zone != NOTHING) && !Wizard(player) &&
	    !(Controls(player, zone)) &&
	    !(db[player].owner == db[zone].owner)) {
		notify(player, "You cannot move that object to that zone.");
		return;
	}

	/* only rooms may be zoned to other rooms */

	if ((zone != NOTHING) &&
	    (Typeof(zone) == TYPE_ROOM) && Typeof(thing) != TYPE_ROOM) {
		notify(player, "Only rooms may have parent rooms.");
		return;
	}

	/* everything is okay, do the change */

	db[thing].zone = zone;
	if (Typeof(thing) != TYPE_PLAYER) {
	    /* We do not strip flags and powers on players, due to the
	     * inconvenience involved in resetting them. Players will just
	     * have to be careful when @chzone'ing players with special
	     * privileges.
	     * For all other objects, we behave like @chown does.
	     */
	    if (key & CHZONE_NOSTRIP) {
		if (!God(player))
		    s_Flags(thing, Flags(thing) & ~WIZARD);
	    } else {
		s_Flags(thing, Flags(thing) & ~mudconf.stripped_flags.word1);
		s_Flags2(thing, Flags2(thing) & ~mudconf.stripped_flags.word2);
		s_Flags3(thing, Flags3(thing) & ~mudconf.stripped_flags.word3);
	    }
	    if (!(key & CHZONE_NOSTRIP) || !God(player)) {
		s_Powers(thing, 0);
		s_Powers2(thing, 0);
	    }
	}
	notify(player, "Zone changed.");
	s_Modified(thing);
}

void do_name(player, cause, key, name, newname)
dbref player, cause;
int key;
const char *name;
char *newname;
{
	dbref thing;
	char *buff;

	if ((thing = match_controlled(player, name)) == NOTHING)
		return;

	/*
	 * check for bad name 
	 */
	if ((*newname == '\0') || (strlen(strip_ansi(newname)) == 0)) {
		notify_quiet(player, "Give it what new name?");
		return;
	}
	/*
	 * check for renaming a player 
	 */
	if (isPlayer(thing)) {

		buff = trim_spaces((char *)newname);
		if (!ok_player_name(buff) ||
		    !badname_check(buff)) {
			notify_quiet(player, "You can't use that name.");
			free_lbuf(buff);
			return;
		} else if (string_compare(buff, Name(thing)) &&
			   (lookup_player(NOTHING, buff, 0) != NOTHING)) {

			/*
			 * string_compare allows changing foo to Foo, etc. 
			 */

			notify_quiet(player, "That name is already in use.");
			free_lbuf(buff);
			return;
		}
		/*
		 * everything ok, notify 
		 */
		STARTLOG(LOG_SECURITY, "SEC", "CNAME")
			log_name(thing),
			log_printf(" renamed to %s", buff);
		ENDLOG
		if (Suspect(thing)) {
			raw_broadcast(WIZARD,
			   "[Suspect] %s renamed to %s", Name(thing), buff);
		}
		delete_player_name(thing, Name(thing));
		s_Name(thing, buff);
		add_player_name(thing, Name(thing));
		if (!Quiet(player) && !Quiet(thing))
			notify_quiet(player, "Name set.");
		free_lbuf(buff);
		s_Modified(thing);
		return;
	} else {
		if (!ok_name(newname)) {
			notify_quiet(player, "That is not a reasonable name.");
			return;
		}
		/*
		 * everything ok, change the name 
		 */
		s_Name(thing, newname);
		if (!Quiet(player) && !Quiet(thing))
			notify_quiet(player, "Name set.");
		s_Modified(thing);
	}
}

/*
 * ---------------------------------------------------------------------------
 * * do_alias: Make an alias for a player or object.
 */

void do_alias(player, cause, key, name, alias)
dbref player, cause;
int key;
char *name, *alias;
{
	dbref thing, aowner;
	int aflags, alen;
	ATTR *ap;
	char *oldalias, *trimalias;

	if ((thing = match_controlled(player, name)) == NOTHING)
		return;

	/*
	 * check for renaming a player 
	 */

	ap = atr_num(A_ALIAS);
	if (isPlayer(thing)) {

		/*
		 * Fetch the old alias 
		 */

		oldalias = atr_pget(thing, A_ALIAS, &aowner, &aflags, &alen);
		trimalias = trim_spaces(alias);

		if (!Controls(player, thing)) {

			/*
			 * Make sure we have rights to do it.  We can't do *
			 * * * * the normal Set_attr check because ALIAS is * 
			 * only * * * writable by GOD and we want to keep *
			 * people * from * * doing &ALIAS and bypassing the * 
			 * player * name checks. 
			 */

			notify_quiet(player, NOPERM_MESSAGE);
		} else if (!*trimalias) {

			/*
			 * New alias is null, just clear it 
			 */

			delete_player_name(thing, oldalias);
			atr_clr(thing, A_ALIAS);
			if (!Quiet(player))
				notify_quiet(player, "Alias removed.");
		} else if (lookup_player(NOTHING, trimalias, 0) != NOTHING) {

			/*
			 * Make sure new alias isn't already in use 
			 */

			notify_quiet(player, "That name is already in use.");
		} else if (!(badname_check(trimalias) &&
			     ok_player_name(trimalias))) {
			notify_quiet(player, "That's a silly name for a player!");
		} else {

			/*
			 * Remove the old name and add the new name 
			 */

			delete_player_name(thing, oldalias);
			atr_add(thing, A_ALIAS, trimalias, Owner(player),
				aflags);
			if (add_player_name(thing, trimalias)) {
				if (!Quiet(player))
					notify_quiet(player, "Alias set.");
			} else {
				notify_quiet(player,
					     "That name is already in use or is illegal, alias cleared.");
				atr_clr(thing, A_ALIAS);
			}
		}
		free_lbuf(trimalias);
		free_lbuf(oldalias);
	} else {
		atr_pget_info(thing, A_ALIAS, &aowner, &aflags);

		/*
		 * Make sure we have rights to do it 
		 */

		if (!Set_attr(player, thing, ap, aflags)) {
			notify_quiet(player, NOPERM_MESSAGE);
		} else {
			atr_add(thing, A_ALIAS, alias, Owner(player), aflags);
			if (!Quiet(player))
				notify_quiet(player, "Set.");
		}
	}
}

/*
 * ---------------------------------------------------------------------------
 * * do_lock: Set a lock on an object or attribute.
 */

void do_lock(player, cause, key, name, keytext)
dbref player, cause;
int key;
char *name, *keytext;
{
	dbref thing, aowner;
	int atr, aflags;
	ATTR *ap;
	struct boolexp *okey;
	char *p;

	if (parse_attrib(player, name, &thing, &atr)) {
		if (atr != NOTHING) {
			if (!atr_get_info(thing, atr, &aowner, &aflags)) {
				notify_quiet(player,
					"Attribute not present on object.");
				return;
			}
			ap = atr_num(atr);

			/*
			 * You may lock an attribute if:
			 * you could write the attribute if it were stored on
			 * yourself --and-- you own the attribute or are a
			 * wizard, as long as you are not #1 and are 
			 * trying to do something to #1. 
			 */

			if (ap && (God(player) ||
				   (!God(thing) &&
				    Set_attr(player, player, ap, 0) &&
				    (Wizard(player) ||
				     (aowner == Owner(player)))))) {
				aflags |= AF_LOCK;
				atr_set_flags(thing, atr, aflags);
				if (!Quiet(player) && !Quiet(thing))
					notify_quiet(player,
						     "Attribute locked.");
			} else {
				notify_quiet(player, NOPERM_MESSAGE);
			}
			return;
		}
	}
	init_match(player, name, NOTYPE);
	match_everything(MAT_EXIT_PARENTS);
	thing = match_result();

	switch (thing) {
	case NOTHING:
		notify_quiet(player,
			     "I don't see what you want to lock!");
		return;
	case AMBIGUOUS:
		notify_quiet(player,
			     "I don't know which one you want to lock!");
		return;
	default:
		if (!controls(player, thing)) {
			notify_quiet(player, "You can't lock that!");
			return;
		}
	}

	/* Don't allow funky characters in locks. */

	for (p = keytext; *p; p++) {
	    if ((*p == '\t') || (*p == '\r') || (*p == '\n') ||
		(*p == ESC_CHAR)) {
		notify_quiet(player, "That is not a valid lock.");
		return;
	    }
	}

	okey = parse_boolexp(player, keytext, 0);
	if (okey == TRUE_BOOLEXP) {
		notify_quiet(player, "I don't understand that key.");
	} else {

		/*
		 * everything ok, do it 
		 */

		if (!key)
			key = A_LOCK;
		atr_add_raw(thing, key, unparse_boolexp_quiet(player, okey));
		if (!Quiet(player) && !Quiet(thing))
			notify_quiet(player, "Locked.");
	}
	free_boolexp(okey);
}

/*
 * ---------------------------------------------------------------------------
 * * Remove a lock from an object of attribute.
 */

void do_unlock(player, cause, key, name)
dbref player, cause;
int key;
char *name;
{
	dbref thing, aowner;
	int atr, aflags;
	ATTR *ap;

	if (parse_attrib(player, name, &thing, &atr)) {
		if (atr != NOTHING) {
			if (!atr_get_info(thing, atr, &aowner, &aflags)) {
				notify_quiet(player,
					"Attribute not present on object.");
				return;
			}
			ap = atr_num(atr);

			/*
			 * You may unlock an attribute if:
			 * you could write the attribute if it were stored on
			 * yourself --and-- you own the attribute
			 * or are a wizard, as long as you are not #1 
			 * and are trying to do something to #1. 
			 */

			if (ap && (God(player) ||
				   (!God(thing) &&
				    Set_attr(player, player, ap, 0) &&
				    (Wizard(player) ||
				     (aowner == Owner(player)))))) {
				aflags &= ~AF_LOCK;
				atr_set_flags(thing, atr, aflags);
				if (Owner(player != Owner(thing)))
					if (!Quiet(player) && !Quiet(thing))
						notify_quiet(player,
						     "Attribute unlocked.");
			} else {
				notify_quiet(player, NOPERM_MESSAGE);
			}
			return;
		}
	}
	if (!key)
		key = A_LOCK;
	if ((thing = match_controlled(player, name)) != NOTHING) {
		atr_clr(thing, key);
		if (!Quiet(player) && !Quiet(thing))
			notify_quiet(player, "Unlocked.");
	}
}

/*
 * ---------------------------------------------------------------------------
 * * do_unlink: Unlink an exit from its destination or remove a dropto.
 */

void do_unlink(player, cause, key, name)
dbref player, cause;
int key;
char *name;
{
	dbref exit;

	init_match(player, name, TYPE_EXIT);
	match_everything(0);
	exit = match_result();

	switch (exit) {
	case NOTHING:
		notify_quiet(player, "Unlink what?");
		break;
	case AMBIGUOUS:
		notify_quiet(player, AMBIGUOUS_MESSAGE);
		break;
	default:
		if (!controls(player, exit)) {
			notify_quiet(player, NOPERM_MESSAGE);
		} else {
			switch (Typeof(exit)) {
			case TYPE_EXIT:
				s_Location(exit, NOTHING);
				if (!Quiet(player))
					notify_quiet(player, "Unlinked.");
				break;
			case TYPE_ROOM:
				s_Dropto(exit, NOTHING);
				if (!Quiet(player))
					notify_quiet(player,
						     "Dropto removed.");
				break;
			default:
				notify_quiet(player, "You can't unlink that!");
				break;
			}
		}
	}
}

/* ---------------------------------------------------------------------------
 * do_chown: Change ownership of an object or attribute.
 */

void do_chown(player, cause, key, name, newown)
dbref player, cause;
int key;
char *name, *newown;
{
	dbref thing, owner, aowner;
	int atr, aflags, do_it, cost, quota;
	ATTR *ap;

	if (parse_attrib(player, name, &thing, &atr)) {
		if (atr != NOTHING) {
			if (!*newown) {
				owner = Owner(thing);
			} else if (!(string_compare(newown, "me"))) {
				owner = Owner(player);
			} else {
				owner = lookup_player(player, newown, 1);
			}

			/*
			 * You may chown an attr to yourself if you own the * 
			 * 
			 * *  * *  * * object and the attr is not locked. *
			 * You * may * chown  * an attr to the owner of the
			 * object * if * * you own * the attribute. * To do
			 * anything * else you  * must be a  * wizard. * Only 
			 * #1 can * chown * attributes on #1. 
			 */

			if (!atr_get_info(thing, atr, &aowner, &aflags)) {
				notify_quiet(player,
					"Attribute not present on object.");
				return;
			}
			do_it = 0;
			if (owner == NOTHING) {
				notify_quiet(player,
					     "I couldn't find that player.");
			} else if (God(thing) && !God(player)) {
				notify_quiet(player, NOPERM_MESSAGE);
			} else if (Wizard(player)) {
				do_it = 1;
			} else if (owner == Owner(player)) {

				/*
				 * chown to me: only if I own the obj and * * 
				 * 
				 * *  * * !locked 
				 */

				if (!Controls(player, thing) ||
				    (aflags & AF_LOCK)) {
					notify_quiet(player,
						     NOPERM_MESSAGE);
				} else {
					do_it = 1;
				}
			} else if (owner == Owner(thing)) {

				/*
				 * chown to obj owner: only if I own attr * * 
				 * 
				 * *  * * and !locked 
				 */

				if ((Owner(player) != aowner) ||
				    (aflags & AF_LOCK)) {
					notify_quiet(player,
						     NOPERM_MESSAGE);
				} else {
					do_it = 1;
				}
			} else {
				notify_quiet(player, NOPERM_MESSAGE);
			}

			if (!do_it)
				return;

			ap = atr_num(atr);
			if (!ap || !Set_attr(player, player, ap, aflags)) {
				notify_quiet(player, NOPERM_MESSAGE);
				return;
			}
			atr_set_owner(thing, atr, owner);
			if (!Quiet(player))
				notify_quiet(player,
					     "Attribute owner changed.");
			s_Modified(thing);
			return;
		}
	}
	init_match(player, name, TYPE_THING);
	match_possession();
	match_here();
	match_exit();
	match_me();
	if (Chown_Any(player)) {
		match_player();
		match_absolute();
	}
	switch (thing = match_result()) {
	case NOTHING:
		notify_quiet(player, "You don't have that!");
		return;
	case AMBIGUOUS:
		notify_quiet(player, "I don't know which you mean!");
		return;
	}

	if (!*newown || !(string_compare(newown, "me"))) {
		owner = Owner(player);
	} else {
		owner = lookup_player(player, newown, 1);
	}

	cost = 1;
	quota = 1;
	switch (Typeof(thing)) {
	case TYPE_ROOM:
		cost = mudconf.digcost;
		quota = mudconf.room_quota;
		break;
	case TYPE_THING:
		cost = OBJECT_DEPOSIT(Pennies(thing));
		quota = mudconf.thing_quota;
		break;
	case TYPE_EXIT:
		cost = mudconf.opencost;
		quota = mudconf.exit_quota;
		break;
	case TYPE_PLAYER:
		cost = mudconf.robotcost;
		quota = mudconf.player_quota;
	}

	if (owner == NOTHING) {
		notify_quiet(player, "I couldn't find that player.");
	} else if (isPlayer(thing) && !God(player)) {
		notify_quiet(player, "Players always own themselves.");
	} else if (((!controls(player, thing) && !Chown_Any(player) &&
		     !(Chown_ok(thing) &&
		       could_doit(player, thing, A_LCHOWN))) ||
		    (isThing(thing) && (Location(thing) != player) &&
		     !Chown_Any(player))) ||
		   (!controls(player, owner) && !Chown_Any(player))) {
		notify_quiet(player, NOPERM_MESSAGE);
	} else if (canpayfees(player, owner, cost, quota, Typeof(thing))) {
	    payfees(owner, cost, quota, Typeof(thing));
	    payfees(Owner(thing), -cost, -quota, Typeof(thing));
		
	    if (God(player)) {
		s_Owner(thing, owner);
	    } else {
		s_Owner(thing, Owner(owner));
	    }
	    atr_chown(thing);

	    /* If we're not stripping flags, and we're God, don't strip the
	     * WIZARD flag. Otherwise, do that, at least.
	     */
	    if (key & CHOWN_NOSTRIP) {
		if (God(player))
		    s_Flags(thing, (Flags(thing) & ~CHOWN_OK) | HALT);
		else
		    s_Flags(thing,
			    (Flags(thing) & ~(CHOWN_OK | WIZARD)) | HALT);
	    } else {
		s_Flags(thing,
			(Flags(thing) &
			 ~(CHOWN_OK | mudconf.stripped_flags.word1)) | HALT);
		s_Flags2(thing,
			 (Flags2(thing) & ~(mudconf.stripped_flags.word2)));
		s_Flags3(thing,
			 (Flags3(thing) & ~(mudconf.stripped_flags.word3)));
	    }

	    /* Powers are only preserved by God with nostrip */

	    if (!(key & CHOWN_NOSTRIP) || !God(player)) { 
		s_Powers(thing, 0);
		s_Powers2(thing, 0);
	    }

	    halt_que(NOTHING, thing);
	    if (!Quiet(player))
		notify_quiet(player, "Owner changed.");
	    s_Modified(thing);
	}
}

/*
 * ---------------------------------------------------------------------------
 * * do_set: Set flags or attributes on objects, or flags on attributes.
 */

static void set_attr_internal(player, thing, attrnum, attrtext, key)
dbref player, thing;
int attrnum, key;
char *attrtext;
{
	dbref aowner;
	int aflags, could_hear;
	ATTR *attr;

	attr = atr_num(attrnum);
	atr_pget_info(thing, attrnum, &aowner, &aflags);
	if (attr && Set_attr(player, thing, attr, aflags)) {
		if ((attr->check != NULL) &&
		    (!(*attr->check) (0, player, thing, attrnum, attrtext)))
			return;
		could_hear = Hearer(thing);
		atr_add(thing, attrnum, attrtext, Owner(player), aflags);
		handle_ears(thing, could_hear, Hearer(thing));
		if (!(key & SET_QUIET) && !Quiet(player) && !Quiet(thing))
			notify_quiet(player, "Set.");
	} else {
		notify_quiet(player, NOPERM_MESSAGE);
	}
}

void do_set(player, cause, key, name, flag)
dbref player, cause;
int key;
char *name, *flag;
{
	dbref thing, thing2, aowner;
	char *p, *buff;
	int atr, atr2, aflags, alen, clear, flagvalue, could_hear;
	ATTR *attr, *attr2;

	/*
	 * See if we have the <obj>/<attr> form, which is how you set * * *
	 * attribute * flags. 
	 */

	if (parse_attrib(player, name, &thing, &atr)) {
		if (atr != NOTHING) {

			/*
			 * You must specify a flag name 
			 */

			if (!flag || !*flag) {
				notify_quiet(player,
				      "I don't know what you want to set!");
				return;
			}
			/*
			 * Check for clearing 
			 */

			clear = 0;
			if (*flag == NOT_TOKEN) {
				flag++;
				clear = 1;
			}
			/*
			 * Make sure player specified a valid attribute flag 
			 */

			flagvalue = search_nametab(player,
					    indiv_attraccess_nametab, flag);
			if (flagvalue < 0) {
				notify_quiet(player, "You can't set that!");
				return;
			}
			/*
			 * Make sure the object has the attribute present 
			 */

			if (!atr_get_info(thing, atr, &aowner, &aflags)) {
				notify_quiet(player,
					"Attribute not present on object.");
				return;
			}
			/*
			 * Make sure we can write to the attribute 
			 */

			attr = atr_num(atr);
			if (!attr || !Set_attr(player, thing, attr, aflags)) {
				notify_quiet(player, NOPERM_MESSAGE);
				return;
			}
			/*
			 * Go do it 
			 */

			if (clear)
				aflags &= ~flagvalue;
			else
				aflags |= flagvalue;
			could_hear = Hearer(thing);
			atr_set_flags(thing, atr, aflags);

			/*
			 * Tell the player about it. 
			 */

			handle_ears(thing, could_hear, Hearer(thing));
			if (!(key & SET_QUIET) &&
			    !Quiet(player) && !Quiet(thing)) {
				if (clear)
					notify_quiet(player, "Cleared.");
				else
					notify_quiet(player, "Set.");
			}
			return;
		}
	}
	/*
	 * find thing 
	 */

	if ((thing = match_controlled(player, name)) == NOTHING)
		return;

	/*
	 * check for attribute set first 
	 */
	for (p = flag; *p && (*p != ':'); p++) ;

	if (*p) {
		*p++ = 0;
		atr = mkattr(flag);
		if (atr <= 0) {
			notify_quiet(player, "Couldn't create attribute.");
			return;
		}
		attr = atr_num(atr);
		if (!attr) {
			notify_quiet(player, NOPERM_MESSAGE);
			return;
		}
		atr_get_info(thing, atr, &aowner, &aflags);
		if (!Set_attr(player, thing, attr, aflags)) {
			notify_quiet(player, NOPERM_MESSAGE);
			return;
		}
		buff = alloc_lbuf("do_set");

		/*
		 * check for _ 
		 */
		if (*p == '_') {
			StringCopy(buff, p + 1);
			if (!parse_attrib(player, p + 1, &thing2, &atr2) ||
			    (atr2 == NOTHING)) {
				notify_quiet(player, "No match.");
				free_lbuf(buff);
				return;
			}
			attr2 = atr_num(atr2);
			p = buff;
			atr_pget_str(buff, thing2, atr2, &aowner, &aflags, &alen);

			if (!attr2 ||
			 !See_attr(player, thing2, attr2, aowner, aflags)) {
				notify_quiet(player, NOPERM_MESSAGE);
				free_lbuf(buff);
				return;
			}
		}
		/*
		 * Go set it 
		 */

		set_attr_internal(player, thing, atr, p, key);
		free_lbuf(buff);
		return;
	}
	/*
	 * Set or clear a flag 
	 */

	flag_set(thing, player, flag, key);
}

void do_power(player, cause, key, name, flag)
dbref player, cause;
int key;
char *name, *flag;
{
	dbref thing;

	if (!flag || !*flag) {
		notify_quiet(player,
			     "I don't know what you want to set!");
		return;
	}
	/*
	 * find thing 
	 */

	if ((thing = match_controlled(player, name)) == NOTHING)
		return;

	power_set(thing, player, flag, key);
}

void do_setattr(player, cause, attrnum, name, attrtext)
dbref player, cause;
int attrnum;
char *name, *attrtext;
{
	dbref thing;

	init_match(player, name, NOTYPE);
	match_everything(MAT_EXIT_PARENTS);
	thing = noisy_match_result();

	if (thing == NOTHING)
		return;
	set_attr_internal(player, thing, attrnum, attrtext, 0);
}



void do_cpattr(player, cause, key, oldpair, newpair, nargs)
    dbref player, cause;
    int key, nargs;
    char *oldpair;
    char *newpair[];
{
    int i, ca, got = 0;
    dbref oldthing;
    char **newthings, **newattrs, *tp, oldbuf[LBUF_SIZE];
    ATTR *oldattr;

    if (!*oldpair || !**newpair || !oldpair || !*newpair)
	return;
    if (nargs < 1)
	return;

    /* newpair gets whacked to bits by parse_to(). Do it just once. */

    newthings = (char **) XCALLOC(nargs, sizeof(char *), "do_cpattr.dbrefs");
    newattrs = (char **) XCALLOC(nargs, sizeof(char *), "do_cpattr.attrs");
    for (i = 0; i < nargs; i++) {
	tp = newpair[i];
	newthings[i] = parse_to(&tp, '/', 1);
	newattrs[i] = tp;
    }

    olist_push();
    if (parse_attrib_wild(player,
			  ((strchr(oldpair, '/') == NULL) ?
			   tprintf("me/%s", oldpair) : oldpair),
			  &oldthing, 0, 0, 1)) {
	for (ca = olist_first(); ca != NOTHING; ca = olist_next()) {
	    oldattr = atr_num(ca);
	    if (oldattr) {
		got = 1;
		for (i = 0; i < nargs; i++) {
		    do_set(player, cause, 0, newthings[i],
			   tprintf("%s:_#%d/%s",
				   (newattrs[i] ? newattrs[i] : oldattr->name),
				   oldthing, oldattr->name));
		}
	    }
	}
    }
    if (!got) {
	notify_quiet(player, "No matching attributes found.");
    }
    olist_pop();
    XFREE(newthings, "do_cpattr.dbrefs");
    XFREE(newattrs, "do_cpattr.attrs");
}


void do_mvattr(player, cause, key, what, args, nargs)
dbref player, cause;
int key, nargs;
char *what, *args[];
{
	dbref thing, aowner, axowner;
	ATTR *in_attr, *out_attr;
	int i, anum, in_anum, aflags, alen, axflags, no_delete, num_copied;
	char *astr;

	/*
	 * Make sure we have something to do. 
	 */

	if (nargs < 2) {
		notify_quiet(player, "Nothing to do.");
		return;
	}
	/*
	 * FInd and make sure we control the target object. 
	 */

	thing = match_controlled(player, what);
	if (thing == NOTHING)
		return;

	/*
	 * Look up the source attribute.  If it either doesn't exist or isn't
	 * * * * * readable, use an empty string. 
	 */

	in_anum = -1;
	astr = alloc_lbuf("do_mvattr");
	in_attr = atr_str(args[0]);
	if (in_attr == NULL) {
		*astr = '\0';
	} else {
		atr_get_str(astr, thing, in_attr->number, &aowner, &aflags, &alen);
		if (!See_attr(player, thing, in_attr, aowner, aflags)) {
			*astr = '\0';
		} else {
			in_anum = in_attr->number;
		}
	}

	/*
	 * Copy the attribute to each target in turn. 
	 */

	no_delete = 0;
	num_copied = 0;
	for (i = 1; i < nargs; i++) {
		anum = mkattr(args[i]);
		if (anum <= 0) {
			notify_quiet(player,
				     tprintf("%s: That's not a good name for an attribute.",
					     args[i]));
			continue;
		}
		out_attr = atr_num(anum);
		if (!out_attr) {
			notify_quiet(player,
				tprintf("%s: Permission denied.", args[i]));
		} else if (out_attr->number == in_anum) {
			no_delete = 1;
		} else {
			atr_get_info(thing, out_attr->number, &axowner,
				     &axflags);
			if (!Set_attr(player, thing, out_attr, axflags)) {
				notify_quiet(player,
					   tprintf("%s: Permission denied.",
						   args[i]));
			} else {
				atr_add(thing, out_attr->number, astr,
					Owner(player), aflags);
				num_copied++;
				if (!Quiet(player))
					notify_quiet(player,
						     tprintf("%s: Set.",
							   out_attr->name));
			}
		}
	}

	/*
	 * Remove the source attribute if we can. 
	 */

	if (num_copied < 1) {
	    if (in_attr) {
		notify_quiet(player,
			     tprintf("%s: Not copied anywhere. Not cleared.",
				     in_attr->name));
	    } else {
		notify_quiet(player, "Not copied anywhere. Non-existent attribute.");
	    }
	} else if ((in_anum > 0) && !no_delete) {
		in_attr = atr_num(in_anum);
		if (in_attr && Set_attr(player, thing, in_attr, aflags)) {
			atr_clr(thing, in_attr->number);
			if (!Quiet(player))
				notify_quiet(player,
					     tprintf("%s: Cleared.",
						     in_attr->name));
		} else {
		    if (in_attr) {
			notify_quiet(player,
				     tprintf("%s: Could not remove old attribute.  Permission denied.",
					     in_attr->name));
		    } else {
			notify_quiet(player, "Could not remove old attribute. Non-existent attribute.");
		    }
		}
	}
	free_lbuf(astr);
}

/*
 * ---------------------------------------------------------------------------
 * * parse_attrib, parse_attrib_wild: parse <obj>/<attr> tokens.
 */

int parse_attrib(player, str, thing, atr)
dbref player, *thing;
int *atr;
char *str;
{
	ATTR *attr;
	char *buff;
	dbref aowner;
	int aflags;

	if (!str)
		return 0;

	/*
	 * Break apart string into obj and attr.  Return on failure 
	 */

	buff = alloc_lbuf("parse_attrib");
	StringCopy(buff, str);
	if (!parse_thing_slash(player, buff, &str, thing)) {
		free_lbuf(buff);
		return 0;
	}
	/*
	 * Get the named attribute from the object if we can 
	 */

	attr = atr_str(str);
	free_lbuf(buff);
	if (!attr) {
		*atr = NOTHING;
	} else {
		atr_pget_info(*thing, attr->number, &aowner, &aflags);
		if (!See_attr(player, *thing, attr, aowner, aflags)) {
			*atr = NOTHING;
		} else {
			*atr = attr->number;
		}
	}
	return 1;
}

static void find_wild_attrs(player, thing, str, check_exclude, hash_insert,
			    get_locks)
dbref player, thing;
char *str;
int check_exclude, hash_insert, get_locks;
{
	ATTR *attr;
	char *as;
	dbref aowner;
	int ca, ok, aflags;

	/*
	 * Walk the attribute list of the object 
	 */

	atr_push();
	for (ca = atr_head(thing, &as); ca; ca = atr_next(&as)) {
		attr = atr_num(ca);

		/*
		 * Discard bad attributes and ones we've seen before. 
		 */

		if (!attr)
			continue;

		if (check_exclude &&
		    ((attr->flags & AF_PRIVATE) ||
		     nhashfind(ca, &mudstate.parent_htab)))
			continue;

		/*
		 * If we aren't the top level remember this attr so we * * *
		 * exclude * it in any parents. 
		 */

		atr_get_info(thing, ca, &aowner, &aflags);
		if (check_exclude && (aflags & AF_PRIVATE))
			continue;

		if (get_locks)
			ok = Read_attr(player, thing, attr, aowner, aflags);
		else
			ok = See_attr(player, thing, attr, aowner, aflags);

		/*
		 * Enforce locality restriction on descriptions 
		 */

		if (ok && (attr->number == A_DESC) && !mudconf.read_rem_desc &&
		    !Examinable(player, thing) && !nearby(player, thing))
			ok = 0;

		if (ok && quick_wild(str, (char *)attr->name)) {
			olist_add(ca);
			if (hash_insert) {
				nhashadd(ca, (int *)attr,
					 &mudstate.parent_htab);
			}
		}
	}
	atr_pop();
}

int parse_attrib_wild(player, str, thing, check_parents, get_locks, df_star)
dbref player, *thing;
char *str;
int check_parents, get_locks, df_star;
{
	char *buff;
	dbref parent;
	int check_exclude, hash_insert, lev;

	if (!str)
		return 0;

	buff = alloc_lbuf("parse_attrib_wild");
	StringCopy(buff, str);

	/*
	 * Separate name and attr portions at the first / 
	 */

	if (!parse_thing_slash(player, buff, &str, thing)) {

		/*
		 * Not in obj/attr format, return if not defaulting to * 
		 */

		if (!df_star) {
			free_lbuf(buff);
			return 0;
		}
		/*
		 * Look for the object, return failure if not found 
		 */

		init_match(player, buff, NOTYPE);
		match_everything(MAT_EXIT_PARENTS);
		*thing = match_result();

		if (!Good_obj(*thing)) {
			free_lbuf(buff);
			return 0;
		}
		str = (char *)"*";
	}
	/*
	 * Check the object (and optionally all parents) for attributes 
	 */

	if (check_parents) {
		check_exclude = 0;
		hash_insert = check_parents;
		nhashflush(&mudstate.parent_htab, 0);
		ITER_PARENTS(*thing, parent, lev) {
			if (!Good_obj(Parent(parent)))
				hash_insert = 0;
			find_wild_attrs(player, parent, str, check_exclude,
					hash_insert, get_locks);
			check_exclude = 1;
		}
	} else {
		find_wild_attrs(player, *thing, str, 0, 0, get_locks);
	}
	free_lbuf(buff);
	return 1;
}


/*
 * ---------------------------------------------------------------------------
 * * edit_string, edit_string_ansi, do_edit: Modify attributes.
 */

#define COPY_ANSI_STR \
		    while (*p) { \
			if (*p == ESC_CHAR) { \
			    savep = p; \
			    while (*p && (*p != ANSI_END)) { \
				safe_chr(*p, *dst, &cp); \
				p++; \
			    } \
			    if (*p) { \
				safe_chr(*p, *dst, &cp); \
				p++; \
			    } \
			    if (!strncmp(savep, ANSI_NORMAL, 4)) \
				have_normal = 1; \
			    else \
				have_normal = 0; \
			} else { \
			    safe_chr(*p, *dst, &cp); \
			    p++; \
			} \
		    }

void edit_string(src, dst, from, to)
char *src, **dst, *from, *to;
{
	char *cp, *p, *savep;
	int have_normal;

        /* We may have gotten an ANSI_NORMAL termination to OLD and NEW,
	 * that the user probably didn't intend to be there. (If the
	 * user really did want it there, he simply has to put a double
	 * ANSI_NORMAL in; this is non-intuitive but without it we can't
	 * let users swap one ANSI code for another using this.)  Thus,
	 * we chop off the terminating ANSI_NORMAL on both, if there is
	 * one.
	 */

	p = from + strlen(from) - 4;
	if (p >= from && !strcmp(p, ANSI_NORMAL))
	    *p = '\0';

	p = to + strlen(to) - 4;
	if (p >= to && !strcmp(p, ANSI_NORMAL))
	    *p = '\0';

	/*
	 * Do the substitution.  Idea for prefix/suffix from R'nice@TinyTIM 
	 */

	if (!strcmp(from, "^")) {
		/*
		 * Prepend 'to' to string 
		 */

		*dst = alloc_lbuf("edit_string.^");
		cp = *dst;

		/* If we have an ANSI character, we need to scan the
		 * string, to make sure we don't have to put on a 
		 * trailing ANSI_NORMAL.
		 */

		if (!strchr(to, ESC_CHAR)) {
		    safe_str(to, *dst, &cp);
		    safe_str(src, *dst, &cp);
		} else {
		    have_normal = 1;
		    p = to;
		    COPY_ANSI_STR;
		    p = src;
		    COPY_ANSI_STR;
		    if (!have_normal)
			safe_ansi_normal(*dst, &cp);
		}
		*cp = '\0';
	} else if (!strcmp(from, "$")) {
		/*
		 * Append 'to' to string 
		 */

		*dst = alloc_lbuf("edit_string.$");
		cp = *dst;

		/* If we have an ANSI character, we need to scan the
		 * string, to make sure we don't have to put on a 
		 * trailing ANSI_NORMAL.
		 */

		if (!strchr(to, ESC_CHAR)) {
		    safe_str(src, *dst, &cp);
		    safe_str(to, *dst, &cp);
		} else {
		    have_normal = 1;
		    p = src;
		    COPY_ANSI_STR;
		    p = to;
		    COPY_ANSI_STR;
		    if (!have_normal)
			safe_ansi_normal(*dst, &cp);
		}
		*cp = '\0';
	} else {
		/*
		 * replace all occurances of 'from' with 'to'.  Handle the *
		 * * * * special cases of from = \$ and \^. 
		 */

		if (((from[0] == '\\') || (from[0] == '%')) &&
		    ((from[1] == '$') || (from[1] == '^')) &&
		    (from[2] == '\0'))
			from++;
		*dst = replace_string_ansi(from, to, src);
	}
}

#undef COPY_ANSI_STR

void edit_string_ansi(src, dst, returnstr, from, to)
char *src, **dst, **returnstr, *from, *to;
{
    edit_string(src, dst, from, to);

    if (mudconf.ansi_colors) {
	edit_string(src, returnstr, from,
		    tprintf("%s%s%s%s", ANSI_HILITE, to, ANSI_NORMAL,
			    ANSI_NORMAL));
    } else {
	*returnstr = alloc_lbuf("edit_string_ansi");
	StringCopy(*returnstr, *dst);
    }
}

void do_edit(player, cause, key, it, args, nargs)
dbref player, cause;
int key, nargs;
char *it, *args[];
{
	dbref thing, aowner;
	int attr, got_one, aflags, alen, doit;
	int could_hear;
	char *from, *to, *result, *returnstr, *atext;
	ATTR *ap;

	/*
	 * Make sure we have something to do. 
	 */

	if ((nargs < 1) || !*args[0]) {
		notify_quiet(player, "Nothing to do.");
		return;
	}
	from = args[0];
	to = (nargs >= 2) ? args[1] : (char *)"";

	/*
	 * Look for the object and get the attribute (possibly wildcarded) 
	 */

	olist_push();
	if (!it || !*it || !parse_attrib_wild(player, it, &thing, 0, 0, 0)) {
		notify_quiet(player, "No match.");
		olist_pop();
		return;
	}
	/*
	 * Iterate through matching attributes, performing edit 
	 */

	got_one = 0;
	atext = alloc_lbuf("do_edit.atext");
	could_hear = Hearer(thing);

	for (attr = olist_first(); attr != NOTHING; attr = olist_next()) {
		ap = atr_num(attr);
		if (ap) {

			/*
			 * Get the attr and make sure we can modify it. 
			 */

			atr_get_str(atext, thing, ap->number,
				    &aowner, &aflags, &alen);
			if (Set_attr(player, thing, ap, aflags)) {

				/*
				 * Do the edit and save the result 
				 */

				got_one = 1;
				edit_string_ansi(atext, &result, &returnstr, from, to);
				if (ap->check != NULL) {
					doit = (*ap->check) (0, player, thing,
							ap->number, result);
				} else {
					doit = 1;
				}
				if (doit) {
					atr_add(thing, ap->number, result,
						Owner(player), aflags);
					if (!Quiet(player))
						notify_quiet(player,
						     tprintf("Set - %s: %s",
							     ap->name,
							     returnstr));
				}
				free_lbuf(result);
				free_lbuf(returnstr);
			} else {

				/*
				 * No rights to change the attr 
				 */

				notify_quiet(player,
					   tprintf("%s: Permission denied.",
						   ap->name));
			}

		}
	}

	/*
	 * Clean up 
	 */

	free_lbuf(atext);
	olist_pop();

	if (!got_one) {
		notify_quiet(player, "No matching attributes.");
	} else {
	    	handle_ears(thing, could_hear, Hearer(thing));
	}
}

void do_wipe(player, cause, key, it)
dbref player, cause;
int key;
char *it;
{
	dbref thing, aowner;
	int attr, got_one, aflags, alen;
	int could_hear;
	ATTR *ap;
	char *atext;

	olist_push();
	if (!it || !*it || !parse_attrib_wild(player, it, &thing, 0, 0, 1)) {
		notify_quiet(player, "No match.");
		olist_pop();
		return;
	}
	/*
	 * Iterate through matching attributes, zapping the writable ones 
	 */

	got_one = 0;
	atext = alloc_lbuf("do_wipe.atext");
	could_hear = Hearer(thing);

	for (attr = olist_first(); attr != NOTHING; attr = olist_next()) {
		ap = atr_num(attr);
		if (ap) {

			/*
			 * Get the attr and make sure we can modify it. 
			 */

			atr_get_str(atext, thing, ap->number,
				    &aowner, &aflags, &alen);
			if (Set_attr(player, thing, ap, aflags)) {
				atr_clr(thing, ap->number);
				got_one = 1;
			}
		}
	}
	/*
	 * Clean up 
	 */

	free_lbuf(atext);
	olist_pop();

	if (!got_one) {
		notify_quiet(player, "No matching attributes.");
	} else {
		handle_ears(thing, could_hear, Hearer(thing));
		if (!Quiet(player))
			notify_quiet(player, "Wiped.");
	}
}

void do_trigger(player, cause, key, object, argv, nargs)
dbref player, cause;
int key, nargs;
char *object, *argv[];
{
	dbref thing;
	int attrib;

	if (!((parse_attrib(player, object, &thing, &attrib)
	       && (attrib != NOTHING)) ||
	      (parse_attrib(player, tprintf("me/%s", object), &thing, &attrib)
	       && (attrib != NOTHING)))) {
		notify_quiet(player, "No match.");
		return;
	}
	if (!controls(player, thing)) {
		notify_quiet(player, NOPERM_MESSAGE);
		return;
	}
	did_it(player, thing, A_NULL, NULL, A_NULL, NULL,
	       attrib, key & TRIG_NOW, argv, nargs);

	/*
	 * XXX be more descriptive as to what was triggered? 
	 */
	if (!(key & TRIG_QUIET) && !Quiet(player))
		notify_quiet(player, "Triggered.");

}

void do_use(player, cause, key, object)
dbref player, cause;
int key;
char *object;
{
	char *df_use, *df_ouse, *temp, *bp;
	dbref thing, aowner;
	int aflags, alen, doit;

	init_match(player, object, NOTYPE);
	match_neighbor();
	match_possession();
	if (Wizard(player)) {
		match_absolute();
		match_player();
	}
	match_me();
	match_here();
	thing = noisy_match_result();
	if (thing == NOTHING)
		return;

	/*
	 * Make sure player can use it 
	 */

	if (!could_doit(player, thing, A_LUSE)) {
		did_it(player, thing, A_UFAIL,
		       "You can't figure out how to use that.",
		       A_OUFAIL, NULL, A_AUFAIL, 0, (char **)NULL, 0);
		return;
	}
	temp = alloc_lbuf("do_use");
	doit = 0;
	if (*atr_pget_str(temp, thing, A_USE, &aowner, &aflags, &alen))
		doit = 1;
	else if (*atr_pget_str(temp, thing, A_OUSE, &aowner, &aflags, &alen))
		doit = 1;
	else if (*atr_pget_str(temp, thing, A_AUSE, &aowner, &aflags, &alen))
		doit = 1;
	free_lbuf(temp);

	if (doit) {
		df_use = alloc_lbuf("do_use.use");
		df_ouse = alloc_lbuf("do_use.ouse");
		bp = df_use;
		safe_tprintf_str(df_use, &bp, "You use %s", Name(thing));
		bp = df_ouse;
		safe_tprintf_str(df_ouse, &bp, "uses %s", Name(thing));
		did_it(player, thing, A_USE, df_use, A_OUSE, df_ouse, A_AUSE,
		       1, (char **)NULL, 0);
		free_lbuf(df_use);
		free_lbuf(df_ouse);
	} else {
		notify_quiet(player, "You can't figure out how to use that.");
	}
}

/*
 * ---------------------------------------------------------------------------
 * * do_setvattr: Set a user-named (or possibly a predefined) attribute.
 */

void do_setvattr(player, cause, key, arg1, arg2)
dbref player, cause;
int key;
char *arg1, *arg2;
{
	char *s;
	int anum;

	arg1++;			/*
				 * skip the '&' 
				 */
	for (s = arg1; *s && !isspace(*s); s++) ;	/*
							 * take to the space 
							 */
	if (*s)
		*s++ = '\0';	/*
				 * split it 
				 */

	anum = mkattr(arg1);	/*
				 * Get or make attribute 
				 */
	if (anum <= 0) {
		notify_quiet(player,
			     "That's not a good name for an attribute.");
		return;
	}
	do_setattr(player, cause, anum, s, arg2);
}
