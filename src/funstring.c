/* funstring.c - string functions */
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

#include "functions.h"	/* required by code */
#include "powers.h"	/* required by code */
#include "ansi.h"	/* required by code */

/* ---------------------------------------------------------------------------
 * isword: is every character in the argument a letter?
 */
  
FUNCTION(fun_isword)
{
char *p;
      
	for (p = fargs[0]; *p; p++) {
		if (!isalpha(*p)) {
			safe_chr('0', buff, bufc);
			return;
		}
	}
	safe_chr('1', buff, bufc);
}
                                                          

/* ---------------------------------------------------------------------------
 * isnum: is the argument a number?
 */

FUNCTION(fun_isnum)
{
	safe_chr((is_number(fargs[0]) ? '1' : '0'), buff, bufc);
}

/* ---------------------------------------------------------------------------
 * isdbref: is the argument a valid dbref?
 */

FUNCTION(fun_isdbref)
{
	char *p;
	dbref dbitem;

	p = fargs[0];
	if (*p++ == NUMBER_TOKEN) {
	    if (*p) {		/* just the string '#' won't do! */
		dbitem = parse_dbref(p);
		if (Good_obj(dbitem)) {
			safe_chr('1', buff, bufc);
			return;
		}
	    }
	}
	safe_chr('0', buff, bufc);
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
	char *tp, *bp;
	Delim isep;

	if (nfargs == 0) {
		return;
	}

	VaChk_Only_InPure("SQUISH", 2);

	bp = tp = fargs[0];

	while (*tp) {

		/* Move over and copy the non-sep characters */

		while (*tp && *tp != isep.c) {
			if (*tp == ESC_CHAR) {
				copy_esccode(tp, bp);
			} else {
				*bp++ = *tp++;
			}
		}

		/* If we've reached the end of the string, leave the loop. */

		if (!*tp)
			break;

		/* Otherwise, we've hit a sep char. Move over it, and then
		 * move on to the next non-separator. Note that we're
		 * overwriting our own string as we do this. However, the
		 * other pointer will always be ahead of our current copy
		 * pointer.
		 */

		*bp++ = *tp++;
		while (*tp && (*tp == isep.c))
			tp++;
	}

	/* Must terminate the string */

	*bp = '\0';
    
	safe_str(fargs[0], buff, bufc);
}

/* ---------------------------------------------------------------------------
 * trim: trim off unwanted white space.
 */
#define TRIM_L 0x1
#define TRIM_R 0x2

FUNCTION(fun_trim)
{
	char *p, *q, *endchar;
	Delim isep;
	int trim;

	if (nfargs == 0) {
		return;
	}
	VaChk_InPure("TRIM", 1, 3);
	if (nfargs >= 2) {
		switch (tolower(*fargs[1])) {
		case 'l':
			trim = TRIM_L;
			break;
		case 'r':
			trim = TRIM_R;
			break;
		default:
			trim = TRIM_L | TRIM_R;
			break;
		}
	} else {
		trim = TRIM_L | TRIM_R;
	}

	p = fargs[0];
	if (trim & TRIM_L) {
		while (*p == isep.c) {
			p++;
		}
	}
	if (trim & TRIM_R) {
		q = endchar = p;
		while (*q != '\0') {
			if (*q == ESC_CHAR) {
				skip_esccode(q);
				endchar = q;
			} else if (*q++ != isep.c) {
				endchar = q;
			}
		}
		*endchar = '\0';
	}
	safe_str(p, buff, bufc);
}

/* ---------------------------------------------------------------------------
 * fun_after, fun_before: Return substring after or before a specified string.
 */

FUNCTION(fun_after)
{
	char *bp, *cp, *mp, *np;
	int ansi_needle, ansi_needle2, ansi_haystack, ansi_haystack2;

	if (nfargs == 0) {
		return;
	}
	if (!fn_range_check("AFTER", nfargs, 1, 2, buff, bufc))
		return;
	bp = fargs[0];	/* haystack */
	mp = fargs[1];	/* needle */

	/* Sanity-check arg1 and arg2 */

	if (bp == NULL)
		bp = "";
	if (mp == NULL)
		mp = " ";
	if (!mp || !*mp)
		mp = (char *)" ";
	if ((mp[0] == ' ') && (mp[1] == '\0'))
		bp = Eat_Spaces(bp);

	/* Get ansi state of the first needle char */
	ansi_needle = ANST_NONE;
	while (*mp == ESC_CHAR) {
		track_esccode(mp, ansi_needle);
		if (!*mp)
			mp = (char *)" ";
	}
	ansi_haystack = ANST_NORMAL;

	/* Look for the needle string */

	while (*bp) {
		while (*bp == ESC_CHAR) {
			track_esccode(bp, ansi_haystack);
		}

		if ((*bp == *mp) &&
		    (ansi_needle == ANST_NONE ||
		     ansi_haystack == ansi_needle)) {

			/* See if what follows is what we are looking for */
			ansi_needle2 = ansi_needle;
			ansi_haystack2 = ansi_haystack;

			cp = bp;
			np = mp;

			while (1) {
				while (*cp == ESC_CHAR) {
					track_esccode(cp, ansi_haystack2);
				}
				while (*np == ESC_CHAR) {
					track_esccode(np, ansi_needle2);
				}
				if ((*cp != *np) ||
				    (ansi_needle2 != ANST_NONE &&
				     ansi_haystack2 != ansi_needle2) ||
				    !*cp || !*np)
					break;
				++cp, ++np;
			}

			if (!*np) {
				/* Yup, return what follows */
				safe_str(ansi_transition_esccode(ANST_NORMAL, ansi_haystack2), buff, bufc);
				safe_str(cp, buff, bufc);
				return;
			}
		}

		/* Nope, continue searching */
		if (*bp)
			++bp;
	}

	/* Ran off the end without finding it */
	return;
}

FUNCTION(fun_before)
{
	char *haystack, *bp, *cp, *mp, *np;
	int ansi_needle, ansi_needle2, ansi_haystack, ansi_haystack2;

	if (nfargs == 0) {
		return;
	}
	if (!fn_range_check("BEFORE", nfargs, 1, 2, buff, bufc))
		return;
	haystack = fargs[0];	/* haystack */
	mp = fargs[1];	/* needle */

	/* Sanity-check arg1 and arg2 */

	if (haystack == NULL)
		haystack = "";
	if (mp == NULL)
		mp = " ";
	if (!mp || !*mp)
		mp = (char *)" ";
	if ((mp[0] == ' ') && (mp[1] == '\0'))
		haystack = Eat_Spaces(haystack);
	bp = haystack;

	/* Get ansi state of the first needle char */
	ansi_needle = ANST_NONE;
	while (*mp == ESC_CHAR) {
		track_esccode(mp, ansi_needle);
		if (!*mp)
			mp = (char *)" ";
	}
	ansi_haystack = ANST_NORMAL;

	/* Look for the needle string */

	while (*bp) {
		/* See if what follows is what we are looking for */
		ansi_needle2 = ansi_needle;
		ansi_haystack2 = ansi_haystack;

		cp = bp;
		np = mp;

		while (1) {
			while (*cp == ESC_CHAR) {
				track_esccode(cp, ansi_haystack2);
			}
			while (*np == ESC_CHAR) {
				track_esccode(np, ansi_needle2);
			}
			if ((*cp != *np) ||
			    (ansi_needle2 != ANST_NONE &&
			     ansi_haystack2 != ansi_needle2) ||
			    !*cp || !*np)
				break;
			++cp, ++np;
		}

		if (!*np) {
			/* Yup, return what came before this */
			*bp = '\0';
			safe_str(haystack, buff, bufc);
			safe_str(ansi_transition_esccode(ansi_haystack, ANST_NORMAL), buff, bufc);
			return;
		}

		/* Nope, continue searching */
		while (*bp == ESC_CHAR) {
			track_esccode(bp, ansi_haystack);
		}

		if (*bp)
			++bp;
	}

	/* Ran off the end without finding it */
	safe_str(haystack, buff, bufc);
	return;
}

/* ---------------------------------------------------------------------------
 * fun_lcstr, fun_ucstr, fun_capstr: Lowercase, uppercase, or capitalize str.
 */

FUNCTION(fun_lcstr)
{
	char *ap;

	ap = *bufc;
	safe_str(fargs[0], buff, bufc);

	while (*ap) {
		if (*ap == ESC_CHAR) {
			skip_esccode(ap);
		} else {
			*ap = tolower(*ap);
			ap++;
		}
	}
}

FUNCTION(fun_ucstr)
{
	char *ap;

	ap = *bufc;
	safe_str(fargs[0], buff, bufc);

	while (*ap) {
		if (*ap == ESC_CHAR) {
			skip_esccode(ap);
		} else {
			*ap = toupper(*ap);
			ap++;
		}
	}
}

FUNCTION(fun_capstr)
{
	char *ap;

	ap = *bufc;
	safe_str(fargs[0], buff, bufc);

	while (*ap == ESC_CHAR) {
		skip_esccode(ap);
	}

	*ap = toupper(*ap);
}

/* ---------------------------------------------------------------------------
 * fun_space: Make spaces.
 */

FUNCTION(fun_space)
{
	int num, max;

	if (!fargs[0] || !(*fargs[0])) {
		num = 1;
	} else {
		num = atoi(fargs[0]);
	}

	if (num < 1) {
		/* If negative or zero spaces return a single space,
		 * -except- allow 'space(0)' to return "" for calculated
		 * padding 
		 */

		if (!is_integer(fargs[0]) || (num != 0)) {
			num = 1;
		}
	}

	max = LBUF_SIZE - 1 - (*bufc - buff);
	num = (num > max) ? max : num;
	memset(*bufc, ' ', num);
	*bufc += num;
	**bufc = '\0';
}

/* ---------------------------------------------------------------------------
 * rjust, ljust, center: Justify or center text, specifying fill character
 */

FUNCTION(fun_ljust)
{
	int spaces, max, i, slen;
	char *tp, *fillchars;

	VaChk_Range("LJUST", 2, 3);
	spaces = atoi(fargs[1]) - strlen((char *)strip_ansi(fargs[0]));

	safe_str(fargs[0], buff, bufc);

	/* Sanitize number of spaces */
	if (spaces <= 0)
		return;	/* no padding needed, just return string */

	tp = *bufc;
	max = LBUF_SIZE - 1 - (tp - buff); /* chars left in buffer */
	spaces = (spaces > max) ? max : spaces;

	if (fargs[2]) {
		fillchars = strip_ansi(fargs[2]);
		slen = strlen(fillchars);
		slen = (slen > spaces) ? spaces : slen;
		if (slen == 0) {
			/* NULL character fill */
			memset(tp, ' ', spaces);
			tp += spaces;
		}
		else if (slen == 1) {
			/* single character fill */
		   memset(tp, *fillchars, spaces);
			tp += spaces;
		}
		else {
			/* multi character fill */
			for (i=spaces; i >= slen; i -= slen) {
				memcpy(tp, fillchars, slen);
				tp += slen;
			}
			if (i) {
				/* we have a remainder here */
			   memcpy(tp, fillchars, i);
				tp += i;
			}
		}
	}
	else {
		/* no fill character specified */
		memset(tp, ' ', spaces);
		tp += spaces;
	}

	*tp = '\0';
	*bufc = tp;
}

FUNCTION(fun_rjust)
{
	int spaces, max, i, slen;
	char *tp, *fillchars;

	VaChk_Range("RJUST", 2, 3);
	spaces = atoi(fargs[1]) - strlen((char *)strip_ansi(fargs[0]));

	/* Sanitize number of spaces */

	if (spaces <= 0) {
		/* no padding needed, just return string */
		safe_str(fargs[0], buff, bufc);
		return;
	}

	tp = *bufc;
	max = LBUF_SIZE - 1 - (tp - buff); /* chars left in buffer */
	spaces = (spaces > max) ? max : spaces;

	if (fargs[2]) {
		fillchars = strip_ansi(fargs[2]);
		slen = strlen(fillchars);
		slen = (slen > spaces) ? spaces : slen;
		if (slen == 0) {
			/* NULL character fill */
			memset(tp, ' ', spaces);
			tp += spaces;
		}
		else if (slen == 1) {
			/* single character fill */
		   memset(tp, *fillchars, spaces);
			tp += spaces;
		}
		else {
			/* multi character fill */
			for (i=spaces; i >= slen; i -= slen) {
				memcpy(tp, fillchars, slen);
				tp += slen;
			}
			if (i) {
				/* we have a remainder here */
			   memcpy(tp, fillchars, i);
				tp += i;
			}
		}
	}
	else {
		/* no fill character specified */
		memset(tp, ' ', spaces);
		tp += spaces;
	}

	*bufc = tp;
	safe_str(fargs[0], buff, bufc);
}

FUNCTION(fun_center)
{
	char *tp, *fillchars;
	int len, lead_chrs, trail_chrs, width, max, i, slen;

	VaChk_Range("CENTER", 2, 3);

	width = atoi(fargs[1]);
	len = strlen((char *)strip_ansi(fargs[0]));

	width = (width > LBUF_SIZE - 1) ? LBUF_SIZE - 1 : width;
	if (len >= width) {
		safe_str(fargs[0], buff, bufc);
		return;
	}

	lead_chrs = (int)((width / 2) - (len / 2) + .5);
	tp = *bufc;
	max = LBUF_SIZE - 1 - (tp - buff); /* chars left in buffer */
	lead_chrs = (lead_chrs > max) ? max : lead_chrs;

	if (fargs[2]) {
		fillchars = (char *)strip_ansi(fargs[2]);
		slen = strlen(fillchars);
		slen = (slen > lead_chrs) ? lead_chrs : slen;
		if (slen == 0) {
			/* NULL character fill */
			memset(tp, ' ', lead_chrs);
			tp += lead_chrs;
		}
		else if (slen == 1) {
			/* single character fill */
			memset(tp, *fillchars, lead_chrs);
			tp += lead_chrs;
		}
		else {
			/* multi character fill */
			for (i=lead_chrs; i >= slen; i -= slen) {
				memcpy(tp, fillchars, slen);
				tp += slen;
			}
			if (i) {
				/* we have a remainder here */
				memcpy(tp, fillchars, i);
				tp += i;
			}
		}
	}
	else {
		/* no fill character specified */
		memset(tp, ' ', lead_chrs);
		tp += lead_chrs;
	}

	*bufc = tp;

	safe_str(fargs[0], buff, bufc);

	trail_chrs = width - lead_chrs - len;
	tp = *bufc;
	max = LBUF_SIZE - 1 - (tp - buff);
	trail_chrs = (trail_chrs > max) ? max : trail_chrs;

	if (fargs[2]) {
		if (slen == 0) {
			/* NULL character fill */
			memset(tp, ' ', trail_chrs);
			tp += trail_chrs;
		}
		else if (slen == 1) {
			/* single character fill */
			memset(tp, *fillchars, trail_chrs);
			tp += trail_chrs;
		}
		else {
			/* multi character fill */
			for (i=trail_chrs; i >= slen; i -= slen) {
				memcpy(tp, fillchars, slen);
				tp += slen;
			}
			if (i) {
				/* we have a remainder here */
				memcpy(tp, fillchars, i);
				tp += i;
			}
		}
	}
	else {
		/* no fill character specified */
		memset(tp, ' ', trail_chrs);
		tp += trail_chrs;
	}

	*tp = '\0';
	*bufc = tp;
}

/* ---------------------------------------------------------------------------
 * fun_left: Returns first n characters in a string
 * fun_right: Returns last n characters in a string
 * fun_strtrunc: Truncates string to n characters
 */

FUNCTION(fun_left)
{
    char *s;
    int count, nchars;
    int ansi_state = ANST_NORMAL;

    s = fargs[0];
    nchars = atoi(fargs[1]);

    if (nchars <= 0)
	return;

    for (count = 0; (count < nchars) && *s; count++) {
	while (*s == ESC_CHAR) {
	    track_esccode(s, ansi_state);
	}

	if (*s) {
	    ++s;
	}
    }

    safe_copy_known_str(fargs[0], s - fargs[0], buff, bufc, (LBUF_SIZE - 1));

    safe_str(ansi_transition_esccode(ansi_state, ANST_NORMAL), buff, bufc);
}

FUNCTION(fun_right)
{
    char *s;
    int count, start, nchars;
    int ansi_state = ANST_NORMAL;

    s = fargs[0];
    nchars = atoi(fargs[1]);
    start = strip_ansi_len(s) - nchars;

    if (nchars <= 0)
	return;

    if (start < 0) {
	nchars += start;
	if (nchars <= 0)
	    return;
	start = 0;
    }

    while (*s == ESC_CHAR) {
	track_esccode(s, ansi_state);
    }

    for (count = 0; (count < start) && *s; count++) {
	++s;

	while (*s == ESC_CHAR) {
	    track_esccode(s, ansi_state);
	}
    }

    if (*s) {
	safe_str(ansi_transition_esccode(ANST_NORMAL, ansi_state), buff, bufc);
    }

    safe_str(s, buff, bufc);
}

/* ---------------------------------------------------------------------------
 * fun_chomp: If the line ends with CRLF, CR, or LF, chop it off.
 */

FUNCTION(fun_chomp)
{
	char *bb_p = *bufc;

	safe_str(fargs[0], buff, bufc);
	if (*bufc != bb_p && (*bufc)[-1] == '\n')
		(*bufc)--;
	if (*bufc != bb_p && (*bufc)[-1] == '\r')
		(*bufc)--;
}

/* ---------------------------------------------------------------------------
 * fun_comp: exact-string compare.
 * fun_streq: non-case-sensitive string compare.
 * fun_strmatch: wildcard string compare.
 */

FUNCTION(fun_comp)
{
	int x;

	x = strcmp(fargs[0], fargs[1]);
	if (x > 0) {
		safe_chr('1', buff, bufc);
	} else if (x < 0) {
		safe_str("-1", buff, bufc);
	} else {
		safe_chr('0', buff, bufc);
	}
}

FUNCTION(fun_streq)
{
    safe_bool(buff, bufc, !string_compare(fargs[0], fargs[1]));
}

FUNCTION(fun_strmatch)
{
	/* Check if we match the whole string.  If so, return 1 */

	safe_bool(buff, bufc, quick_wild(fargs[1], fargs[0]));
}

/* ---------------------------------------------------------------------------
 * fun_edit: Edit text.
 */

FUNCTION(fun_edit)
{
    char *tstr;

    edit_string(fargs[0], &tstr, fargs[1], fargs[2]);
    safe_str(tstr, buff, bufc);
    free_lbuf(tstr);
}

/* ---------------------------------------------------------------------------
 * fun_merge:  given two strings and a character, merge the two strings
 *   by replacing characters in string1 that are the same as the given 
 *   character by the corresponding character in string2 (by position).
 *   The strings must be of the same length.
 */

FUNCTION(fun_merge)
{
	char *str, *rep;
	char c;

	/* do length checks first */

	if (strlen(fargs[0]) != strlen(fargs[1])) {
		safe_str("#-1 STRING LENGTHS MUST BE EQUAL", buff, bufc);
		return;
	}
	if (strlen(fargs[2]) > 1) {
		safe_str("#-1 TOO MANY CHARACTERS", buff, bufc);
		return;
	}
	/* find the character to look for. null character is considered a
	 * space 
	 */

	if (!*fargs[2])
		c = ' ';
	else
		c = *fargs[2];

	/* walk strings, copy from the appropriate string */

	for (str = fargs[0], rep = fargs[1];
	     *str && *rep && ((*bufc - buff) < (LBUF_SIZE - 1));
	     str++, rep++, (*bufc)++) {
		if (*str == c)
			**bufc = *rep;
		else
			**bufc = *str;
	}

	return;
}

/* ---------------------------------------------------------------------------
 * fun_secure, fun_escape: escape [, ], %, \, and the beginning of the string.
 */

FUNCTION(fun_secure)
{
	char *s;

	s = fargs[0];
	while (*s) {
		switch (*s) {
		case ESC_CHAR:
			while (*s && (*s != ANSI_END)) {
				safe_chr(*s, buff, bufc);
				s++;
			}
			if (*(s-1) != ESC_CHAR)
				s--;
			break;
		case '%':
		case '$':
		case '\\':
		case '[':
		case ']':
		case '(':
		case ')':
		case '{':
		case '}':
		case ',':
		case ';':
			safe_chr(' ', buff, bufc);
			break;
		default:
			safe_chr(*s, buff, bufc);
		}
		s++;
	}
}

FUNCTION(fun_escape)
{
	char *s, *d;

	d = *bufc;
	s = fargs[0];
	while (*s) {
		switch (*s) {
		case ESC_CHAR:
			if (*bufc == d)
				safe_chr('\\', buff, bufc);
			safe_chr(*s, buff, bufc);
			s++;
			while (*s && (*s != ANSI_END)) {
				safe_chr(*s, buff, bufc);
				s++;
			}
			if (*(s-1) != ESC_CHAR)
				s--;
			break;
		case '%':
		case '\\':
		case '[':
		case ']':
		case '{':
		case '}':
		case ';':
			safe_chr('\\', buff, bufc);
			/* FALLTHRU */
		default:
			if (*bufc == d)
				safe_chr('\\', buff, bufc);
			safe_chr(*s, buff, bufc);
		}
		s++;
	}
}

/* ---------------------------------------------------------------------------
 * ANSI handlers.
 */

FUNCTION(fun_ansi)
{
	char *s, *bb_p;

	if (!mudconf.ansi_colors) {
	    safe_str(fargs[1], buff, bufc);
	    return;
	}

	if (!fargs[0] || !*fargs[0]) {
	    safe_str(fargs[1], buff, bufc);
	    return;
	}

	/* Favor truncating the string over truncating the ANSI codes,
	 * but make sure to leave room for ANSI_NORMAL (4 characters).
	 * That means that we need a minimum of 9 (maybe 10) characters
	 * left in the buffer (8 or 9 in ANSI codes, plus at least one
	 * character worth of string to hilite). We do 10 just to be safe.
	 *
	 * This means that in MOST cases, we are not going to drop the
	 * trailing ANSI code for lack of space in the buffer. However,
	 * because of the possibility of an extended buffer created by
	 * exec(), this is not a guarantee (because extending the buffer
	 * gives us a fresh new buff, rather than having us continue to
	 * copy through the new buffer). Sadly, the times when we extend
	 * are also to be the times we're most likely to run out of space
	 * in the buffer. There's nothing we can do about that, though.
	 */

	if (strlen(buff) > LBUF_SIZE - 11)
	    return;

	s = fargs[0];
	bb_p = *bufc;

	while (*s) {
	    if (ansi_nchartab[(unsigned char) *s]) {
		if (*bufc != bb_p) {
		    safe_copy_chr(';', buff, bufc, LBUF_SIZE - 5);
		} else {
		    safe_copy_known_str(ANSI_BEGIN, 2, buff, bufc, LBUF_SIZE - 5);
		}
		safe_copy_str(ansi_nchartab[(unsigned char) *s],
			      buff, bufc, LBUF_SIZE - 5);
	    }
	    s++;
	}
	if (*bufc != bb_p) {
	    safe_copy_chr(ANSI_END, buff, bufc, LBUF_SIZE - 5);
	}
	safe_copy_str(fargs[1], buff, bufc, LBUF_SIZE - 5);
	safe_ansi_normal(buff, bufc);
}

FUNCTION(fun_stripansi)
{
	safe_str((char *)strip_ansi(fargs[0]), buff, bufc);
}

/*---------------------------------------------------------------------------
 * encrypt() and decrypt(): From DarkZone.
 */

/*
 * Copy over only alphanumeric chars 
 */
#define CRYPTCODE_LO   32	/* space */
#define CRYPTCODE_HI  126	/* tilde */
#define CRYPTCODE_MOD  95	/* count of printable ascii chars */

static void crunch_code(code)
char *code;
{
	char *in, *out;
	
	in = out = code;
	while (*in && (out == in)) {
		if ((*in >= CRYPTCODE_LO) && (*in <= CRYPTCODE_HI)) {
			out++; in++;
		} else if (*in == ESC_CHAR) {
			while (*in && (*in != ANSI_END)) {
				in++;
			}
			if (*in) {
				in++;
			}
		} else {
			in++;
		}
	}
	while (*in) {
		if ((*in >= CRYPTCODE_LO) && (*in <= CRYPTCODE_HI)) {
			*out++ = *in++;
		} else if (*in == ESC_CHAR) {
			while (*in && (*in != ANSI_END)) {
				in++;
			}
			if (*in) {
				in++;
			}
		} else {
			in++;
		}
	}
	*out = '\0';
}

static void crypt_code(buff, bufc, code, text, type)
char *buff, **bufc, *code, *text;
int type;
{
	char *q;

	if (!*text)
		return;
	crunch_code(code);
	if (!*code) {
		safe_str(text, buff, bufc);
		return;
	}

	q = code;

	/*
	 * Encryption: Simply go through each character of the text, get its
	 * ascii value, subtract LO, add the ascii value (less
	 * LO) of the code, mod the result, add LO. Continue
	 */
	while (*text) {
		if ((*text >= CRYPTCODE_LO) && (*text <= CRYPTCODE_HI)) {
			if (type) {
				safe_chr(((((*text - CRYPTCODE_LO) + (*q - CRYPTCODE_LO)) % CRYPTCODE_MOD) + CRYPTCODE_LO), buff, bufc);
			} else {
				safe_chr(((((*text - *q) + 2 * CRYPTCODE_MOD) % CRYPTCODE_MOD) + CRYPTCODE_LO), buff, bufc);
			}
			text++;
			q++;
			if (!*q) {
				q = code;
			}
		} else if (*text == ESC_CHAR) {
			while (*text && (*text != ANSI_END)) {
				safe_chr(*text++, buff, bufc);
			}
			if (*text) {
				safe_chr(*text++, buff, bufc);
			}
		} else {
			safe_chr(*text++, buff, bufc);
		}
	}
}

FUNCTION(fun_encrypt)
{
	crypt_code(buff, bufc, fargs[1], fargs[0], 1);
}

FUNCTION(fun_decrypt)
{
	crypt_code(buff, bufc, fargs[1], fargs[0], 0);
}

/* ---------------------------------------------------------------------------
 * fun_scramble:  randomizes the letters in a string.
 */

/* Borrowed from PennMUSH 1.50 */
FUNCTION(fun_scramble)
{
	int n, i, j;
	char c;

	if (!fargs[0] || !*fargs[0]) {
	    return;
	}

	n = strlen(fargs[0]);
	for (i = 0; i < n; i++) {
	    j = (random() % (n - i)) + i;
	    c = fargs[0][i];
	    fargs[0][i] = fargs[0][j];
	    fargs[0][j] = c;
	}

	safe_str(fargs[0], buff, bufc);
}

/* ---------------------------------------------------------------------------
 * fun_reverse: reverse a string
 */

FUNCTION(fun_reverse)
{
    /* Nasty bounds checking */

    if ((int)strlen(fargs[0]) >= LBUF_SIZE - (*bufc - buff) - 1) {
	*(fargs[0] + (LBUF_SIZE - (*bufc - buff) - 1)) = '\0';
    }
    do_reverse(fargs[0], *bufc);
    *bufc += strlen(fargs[0]);
}

/* ---------------------------------------------------------------------------
 * fun_mid: mid(foobar,2,3) returns oba
 */

FUNCTION(fun_mid)
{
    char *s, *savep;
    int count, start, nchars;
    int ansi_state = ANST_NORMAL;

    s = fargs[0];
    start = atoi(fargs[1]);
    nchars = atoi(fargs[2]);

    if (nchars <= 0)
	return;

    if (start < 0) {
	nchars += start;
	if (nchars <= 0)
	    return;
	start = 0;
    }

    while (*s == ESC_CHAR) {
	track_esccode(s, ansi_state);
    }

    for (count = 0; (count < start) && *s; count++) {
	++s;

	while (*s == ESC_CHAR) {
	    track_esccode(s, ansi_state);
	}
    }

    if (*s) {
	safe_str(ansi_transition_esccode(ANST_NORMAL, ansi_state), buff, bufc);
    }

    savep = s;
    for (count = 0; (count < nchars) && *s; count++) {
	while (*s == ESC_CHAR) {
	    track_esccode(s, ansi_state);
	}

	if (*s) {
	    ++s;
	}
    }

    safe_copy_known_str(savep, s - savep, buff, bufc, (LBUF_SIZE - 1));

    safe_str(ansi_transition_esccode(ansi_state, ANST_NORMAL), buff, bufc);
}

/* ---------------------------------------------------------------------------
 * fun_translate: Takes a string and a second argument. If the second argument
 * is 0 or s, control characters are converted to spaces. If it's 1 or p,
 * they're converted to percent substitutions.
 */

FUNCTION(fun_translate)
{
    if (*fargs[0] && *fargs[1]) {

	/* Strictly speaking, we're just checking the first char */

	if (fargs[1][0] == 's')
	    safe_str(translate_string(fargs[0], 0), buff, bufc);
	else if (fargs[1][0] == 'p')
	    safe_str(translate_string(fargs[0], 1), buff, bufc);
	else
	    safe_str(translate_string(fargs[0], atoi(fargs[1])), buff, bufc);
    }
}

/* ---------------------------------------------------------------------------
 * fun_pos: Find a word in a string 
 */

FUNCTION(fun_pos)
{
	int i = 1;
	char *s, *t, *u;

	i = 1;
	s = strip_ansi(fargs[1]);
	while (*s) {
		u = s;
		t = fargs[0];
		while (*t && *t == *u)
			++t, ++u;
		if (*t == '\0') {
			safe_ltos(buff, bufc, i);
			return;
		}
		++i, ++s;
	}
	safe_nothing(buff, bufc);
	return;
}

/* ---------------------------------------------------------------------------
 * fun_lpos: Find all occurrences of a character in a string, and return
 * a space-separated list of the positions, starting at 0. i.e.,
 * lpos(a-bc-def-g,-) ==> 1 4 8
 */

FUNCTION(fun_lpos)
{
    char *s, *bb_p, c;
    int i;

    if (!fargs[0] || !*fargs[0])
	return;

    c = (char) *(fargs[1]);
    if (!c)
	c = ' ';

    bb_p = *bufc;

    for (i = 0, s = strip_ansi(fargs[0]); *s; i++, s++) {
	if (*s == c) {
	    if (*bufc != bb_p) {
		safe_chr(' ', buff, bufc);
	    }
	    safe_ltos(buff, bufc, i);
	}
    }

}

/* ---------------------------------------------------------------------------
 * Take a character position and return which word that char is in.
 * wordpos(<string>, <charpos>)
 */

FUNCTION(fun_wordpos)
{
	int charpos, i, isep_len;
	char *cp, *tp, *xp;
	Delim isep;

	VaChk_Only_In("WORDPOS", 3);

	charpos = atoi(fargs[1]);
	cp = fargs[0];
	if ((charpos > 0) && (charpos <= (int)strlen(cp))) {
		tp = &(cp[charpos - 1]);
		cp = trim_space_sep(cp, isep, isep_len);
		xp = split_token(&cp, isep, isep_len);
		for (i = 1; xp; i++) {
			if (tp < (xp + strlen(xp)))
				break;
			xp = split_token(&cp, isep, isep_len);
		}
		safe_ltos(buff, bufc, i);
		return;
	}
	safe_nothing(buff, bufc);
	return;
}

/* ---------------------------------------------------------------------------
 * fun_repeat: repeats a string
 */

FUNCTION(fun_repeat)
{
    int times, len, i, maxtimes;

    times = atoi(fargs[1]);
    if ((times < 1) || (fargs[0] == NULL) || (!*fargs[0])) {
	return;
    } else if (times == 1) {
	safe_str(fargs[0], buff, bufc);
    } else {
	len = strlen(fargs[0]);
	maxtimes = (LBUF_SIZE - 1 - (*bufc - buff)) / len;
	maxtimes = (times > maxtimes) ? maxtimes : times;

	for (i = 0; i < maxtimes; i++) {
		memcpy(*bufc, fargs[0], len);
		*bufc += len;
	}
	if (times > maxtimes) {
		safe_known_str(fargs[0], len, buff, bufc);
	}
    }
}

/* ---------------------------------------------------------------------------
 * fun_border: Turn a string of words into a bordered paragraph.
 * border(<words>,<width without margins>[,<L margin fill>[,<R margin fill>]])
 */

#define BORDER_LJUST	0
#define BORDER_RJUST	1
#define BORDER_CENTER	2

static void border_helper(player, str, last_state,
			  tlen, l_fill, r_fill, buff, bufc, key)
    dbref player;
    char *str, *last_state;
    int tlen;
    char *l_fill, *r_fill, *buff, **bufc;
    int key;
{
    int i, nwords, nstates, over, copied, nleft, max, lens[LBUF_SIZE / 2];
    int start, end, out, lead_chrs;
    char *words[LBUF_SIZE / 2], *states[LBUF_SIZE / 2], tbuf[LBUF_SIZE];
    char *p;

    /* Carve up the list and find the ANSI state at the end of each word. */

    strcpy(tbuf, str);
    nstates = list2ansi(states, last_state, LBUF_SIZE / 2, tbuf, SPACE_DELIM);
    nwords = list2arr(words, LBUF_SIZE / 2, str, SPACE_DELIM, 1);
    if (nstates != nwords) {
	for (i = 0; i < nstates; i++) {
	    XFREE(states[i], "list2ansi");
	}
	notify_quiet(player, "Strange ANSI parsing error.");
	return;
    }
    for (i = 0; i < nwords; i++)
	lens[i] = strip_ansi_len(words[i]);

    /* For every line, figure out how many words we can copy, then
     * do the borders appropriately. We do it this way in order to
     * avoid repeating a lot of code for left vs. right vs. center
     * justification.
     */

    end = over = 0;
    while (!over && (end < nwords)) {

	/* We're at the beginning of a line. */

	out = copied = 0;
	start = end; 

	/* If this isn't the first thing we're writing, insert a newline.
	 * (We do this here rather than when we write the right margin,
	 * to avoid an extra carriage return at the very end.)
	 */

	if (start != 0) 
	    safe_crlf(buff, bufc);

	/* Figure out how many words we can get on this line.
	 * We are always guaranteed the first one, even if it's wider
	 * than our buffer. If this word exceeds the buffer, don't 
	 * increment the end counter. Note that words after the first
	 * are an extra 1 character long, to account for the leading
	 * space. Multiple spaces have gotten eaten by the list-chomper.
	 */

	while (!out && (end < nwords)) {
	    if (copied == 0) {
		copied += lens[end];
		end++;
	    } else {
		nleft = tlen - copied;
		if (lens[end] + 1 > nleft) {
		    out = 1;
		} else {
		    copied += lens[end] + 1;
		    end++;
		}
	    }
	}

	/* Write the left margin. */

	over = safe_str(l_fill, buff, bufc);

	if (!over) {

	    /* Write spaces if we need them. */

	    if (key == BORDER_RJUST) {
		nleft = tlen - copied;
		print_padding(nleft, max, ' ');
	    } else if (key == BORDER_CENTER) {
		lead_chrs = (int)((tlen / 2) - (copied / 2) + .5);
		print_padding(lead_chrs, max, ' ');
	    }

	    /* Write the first word. If we have a non-null ANSI state from
	     * the previous position, we have to restore that, too.
	     * We must also check the ending last state.
	     */
	    if ((start > 0) && *states[start - 1]) {
		print_ansi_state(p, states[start - 1]);
	    } else if ((start == 0) && *last_state) {
		print_ansi_state(p, last_state);
	    }
	    over = safe_str(words[start], buff, bufc);
	}

	/* Write the rest of the words, including a leading space. */

	for (i = start + 1; (i < end) && !over; i++) {
	    safe_chr(' ', buff, bufc);
	    over = safe_str(words[i], buff, bufc);
	}

	if (!over) {

	    /* Insert an ANSI normal if we need to. */

	    if (*states[end - 1]) {
		safe_ansi_normal(buff, bufc);
	    }

	    /* Write right-padding spaces if we need them. */

	    if (key == BORDER_LJUST) {
		nleft = tlen - copied;
		print_padding(nleft, max, ' ');
	    } else if (key == BORDER_CENTER) {
		nleft = tlen - lead_chrs - copied;
		print_padding(nleft, max, ' ');
	    }

	    /* Write the right margin. */

	    if (!over)
		over = safe_str(r_fill, buff, bufc);
	}
    }

    /* Save the ANSI state of the last word. */

    strcpy(last_state, states[nstates - 1]);

    /* Clean up. */

    for (i = 0; i < nstates; i++) {
	XFREE(states[i], "list2ansi");
    }
}

static void perform_border(player, buff, bufc, fargs, nfargs, key)
    dbref player;
    char *buff, **bufc, *fargs[];
    int nfargs, key;
{
    int width, l_width, r_width;
    char *l_fill, *r_fill, *savep, *p, *bb_p;
    char last_state[LBUF_SIZE];

    if (!fargs[0] || !*fargs[0])
	return;

    width = atoi(fargs[1]);
    if (nfargs > 2) {
	l_fill = fargs[2];
	l_width = strip_ansi_len(l_fill);
    } else {
	l_fill = NULL;
	l_width = 0;
    }
    if (nfargs > 3) {
	r_fill = fargs[3];
	r_width = strip_ansi_len(r_fill);
    } else {
	r_fill = NULL;
	r_width = 0;
    }

    /* The width must be at least one character. */

    if (width < 1)
	width = 1;

    /* Now we have to go do this by line breaks. Life is made more
     * difficult by the fact we have to preserve the ANSI state across
     * line breaks.
     */

    bb_p = *bufc;
    savep = fargs[0];
    last_state[0] = '\0';
    p = strchr(fargs[0], '\r');
    while (p) {
	*p = '\0';
	if (*bufc != bb_p)
	    safe_crlf(buff, bufc);
	border_helper(player, savep, last_state,
		      width, l_fill, r_fill, buff, bufc, key);
	savep = p + 2;	/* must skip '\n' too */
	p = strchr(savep, '\r'); 
    }
    if (*bufc != bb_p)
	safe_crlf(buff, bufc);
    border_helper(player, savep, last_state,
		  width, l_fill, r_fill, buff, bufc, key);
}

FUNCTION(fun_border)
{
    VaChk_Range("BORDER", 2, 4);
    perform_border(player, buff, bufc, fargs, nfargs, BORDER_LJUST);
}

FUNCTION(fun_rborder)
{
    VaChk_Range("RBORDER", 2, 4);
    perform_border(player, buff, bufc, fargs, nfargs, BORDER_RJUST);
}

FUNCTION(fun_cborder)
{
    VaChk_Range("CBORDER", 2, 4);
    perform_border(player, buff, bufc, fargs, nfargs, BORDER_CENTER);
}

/* ---------------------------------------------------------------------------
 * Misc functions.
 */

FUNCTION(fun_cat)
{
	int i;

	safe_str(fargs[0], buff, bufc);
	for (i = 1; i < nfargs; i++) {
		safe_chr(' ', buff, bufc);
		safe_str(fargs[i], buff, bufc);
	}
}

FUNCTION(fun_strcat)
{
	int i;
	
	safe_str(fargs[0], buff, bufc);
	for (i = 1; i < nfargs; i++) {
		safe_str(fargs[i], buff, bufc);
	}
}

FUNCTION(fun_strlen)
{
	safe_ltos(buff, bufc, (int)strlen((char *)strip_ansi(fargs[0])));
}

FUNCTION(fun_delete)
{
    char *s, *savep;
    int count, start, nchars, len, have_normal;

    s = fargs[0];
    start = atoi(fargs[1]);
    nchars = atoi(fargs[2]);
    len = strip_ansi_len(s);

    if ((start >= len) || (nchars <= 0)) {
	safe_str(s, buff, bufc);
	return;
    }

    have_normal = 1;
    for (count = 0; *s && (count < len); ) {
	if (*s == ESC_CHAR) {
	    Skip_Ansi_Code(s, buff, bufc);
	} else {
	    if ((count >= start) && (count < start + nchars)) {
		s++;
	    } else {
		safe_chr(*s, buff, bufc);
		s++;
	    }
	    count++;
	}
    }

    if (!have_normal)
	safe_ansi_normal(buff, bufc);
}

/* ---------------------------------------------------------------------------
 * Misc PennMUSH-derived functions.
 */

FUNCTION(fun_lit)
{
	/* Just returns the argument, literally */
	safe_str(fargs[0], buff, bufc);
}

FUNCTION(fun_art)
{
/* checks a word and returns the appropriate article, "a" or "an" */
	char c = tolower(*fargs[0]);

	if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u')
		safe_known_str("an", 2, buff, bufc);
	else
		safe_chr('a', buff, bufc);
}

FUNCTION(fun_alphamax)
{
	char *amax;
	int i = 1;

	if (!fargs[0]) {
		safe_str("#-1 TOO FEW ARGUMENTS", buff, bufc);
		return;
	} else
		amax = fargs[0];

	while ((i < nfargs) && fargs[i]) {
		amax = (strcmp(amax, fargs[i]) > 0) ? amax : fargs[i];
		i++;
	}

	safe_str(amax, buff, bufc);
}

FUNCTION(fun_alphamin)
{
	char *amin;
	int i = 1;

	if (!fargs[0]) {
		safe_str("#-1 TOO FEW ARGUMENTS", buff, bufc);
		return;
	} else
		amin = fargs[0];

	while ((i < nfargs) && fargs[i]) {
		amin = (strcmp(amin, fargs[i]) < 0) ? amin : fargs[i];
		i++;
	}

	safe_str(amin, buff, bufc);
}

FUNCTION(fun_valid)
{
/* Checks to see if a given <something> is valid as a parameter of a
 * given type (such as an object name).
 */

	if (!fargs[0] || !*fargs[0] || !fargs[1] || !*fargs[1]) {
		safe_chr('0', buff, bufc);
	} else if (!strcasecmp(fargs[0], "name")) {
		safe_bool(buff, bufc, ok_name(fargs[1]));
	} else {
		safe_nothing(buff, bufc);
	}
}

FUNCTION(fun_beep)
{
	safe_chr(BEEP_CHAR, buff, bufc);
}
