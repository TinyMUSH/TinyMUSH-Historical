/* funiter.c - functions for user-defined iterations over lists */
/* $Id$ */

#include "copyright.h"
#include "autoconf.h"
#include "config.h"

#include "alloc.h"	/* required by mudconf */
#include "flags.h"	/* required by mudconf */
#include "htab.h"	/* required by mudconf */
#include "mail.h"	/* required by mudconf */
#include "mudconf.h"	/* required by code */

#include "db.h"		/* required by externs */
#include "externs.h"	/* required by code */

#include "functions.h"	/* required by code */
#include "attrs.h"	/* required by code */
#include "powers.h"	/* required by code */

/* ---------------------------------------------------------------------------
 * fun_loop and fun_parse exist for reasons of backwards compatibility.
 * See notes on fun_iter for the explanation.
 */

static void perform_loop(buff, bufc, player, cause, list, exprstr,
			 cargs, ncargs, sep, osep, flag)
    char *buff, **bufc;
    dbref player, cause;
    char *list, *exprstr;
    char *cargs[];
    int ncargs;
    char sep, osep;
    int flag;			/* 0 is parse(), 1 is loop() */
{
    char *curr, *objstring, *buff2, *buff3, *cp, *dp, *str, *result, *bb_p;
    char tbuf[8];
    int number = 0;

    dp = cp = curr = alloc_lbuf("perform_loop.1");
    str = list;
    exec(curr, &dp, 0, player, cause, EV_STRIP | EV_FCHECK | EV_EVAL, &str,
	 cargs, ncargs);
    *dp = '\0';
    cp = trim_space_sep(cp, sep);
    if (!*cp) {
	free_lbuf(curr);
	return;
    }

    bb_p = *bufc;

    while (cp && (mudstate.func_invk_ctr < mudconf.func_invk_lim)) {
	if (!flag && (*bufc != bb_p)) {
	    print_sep(osep, buff, bufc);
	}
	number++;
	objstring = split_token(&cp, sep);
	buff2 = replace_string(BOUND_VAR, objstring, exprstr);
	ltos(tbuf, number);
	buff3 = replace_string(LISTPLACE_VAR, tbuf, buff2);
	str = buff3;
	if (!flag) {
	    exec(buff, bufc, 0, player, cause,
		 EV_STRIP | EV_FCHECK | EV_EVAL, &str, cargs, ncargs);
	} else {
	    dp = result = alloc_lbuf("perform_loop.2");
	    exec(result, &dp, 0, player, cause,
		 EV_STRIP | EV_FCHECK | EV_EVAL, &str, cargs, ncargs);
	    *dp = '\0';
	    notify(cause, result);
	    free_lbuf(result);
	}
	free_lbuf(buff2);
	free_lbuf(buff3);
    }
    free_lbuf(curr);
}

FUNCTION(fun_parse)
{
    char sep, osep;

    evarargs_preamble("PARSE", 2, 4);
    perform_loop(buff, bufc, player, cause, fargs[0], fargs[1],
		 cargs, ncargs, sep, osep, 0);
}

FUNCTION(fun_loop)
{
    char sep;

    varargs_preamble("LOOP", 3);
    perform_loop(buff, bufc, player, cause, fargs[0], fargs[1],
		 cargs, ncargs, sep, ' ', 1);
}

/* ---------------------------------------------------------------------------
 * fun_iter() and fun_list() parse an expression, substitute elements of
 * a list, one at a time, using the '##' replacement token. Uses of these
 * functions can be nested.
 * In older versions of MUSH, these functions could not be nested.
 * parse() and loop() exist for reasons of backwards compatibility,
 * since the peculiarities of the way substitutions were done in the string
 * replacements make it necessary to provide some way of doing backwards
 * compatibility, in order to avoid breaking a lot of code that relies upon
 * particular patterns of necessary escaping.
 *
 * fun_whentrue() and fun_whenfalse() work similarly to iter(). 
 * whentrue() loops as long as the expression evaluates to true.
 * whenfalse() loops as long as the expression evaluates to false.
 */

#define BOOL_COND_NONE -1
#define BOOL_COND_FALSE 0
#define BOOL_COND_TRUE 1 

static void perform_iter(buff, bufc, player, cause, list, exprstr,
			 cargs, ncargs, sep, osep, flag, bool_flag)
    char *buff, **bufc;
    dbref player, cause;
    char *list, *exprstr;
    char *cargs[];
    int ncargs;
    char sep, osep;
    int flag;			/* 0 is iter(), 1 is list() */
    int bool_flag;
{
    char *list_str, *lp, *str, *input_p, *bb_p, *work_buf;
    char *savep, *dp, *result;
    int is_true, cur_lev, elen;
    
    /* Enforce maximum nesting level. */

    if (mudstate.in_loop >= MAX_ITER_NESTING - 1) {
	notify_quiet(player, "Exceeded maximum iteration nesting.");
	return;
    }

    /* The list argument is unevaluated. Go evaluate it. */

    input_p = lp = list_str = alloc_lbuf("perform_iter.list");
    str = list;
    exec(list_str, &lp, 0, player, cause, EV_STRIP | EV_FCHECK | EV_EVAL, &str,
	 cargs, ncargs);
    *lp = '\0';
    input_p = trim_space_sep(input_p, sep);
    if (!*input_p) {
	free_lbuf(list_str);
	return;
    }

    cur_lev = mudstate.in_loop++;
    mudstate.loop_token[cur_lev] = NULL;
    mudstate.loop_number[cur_lev] = 0;

    bb_p = *bufc;
    elen = strlen(exprstr);

    while (input_p && (mudstate.func_invk_ctr < mudconf.func_invk_lim)) {
	if (!flag && (*bufc != bb_p)) {
	    print_sep(osep, buff, bufc);
	}
	mudstate.loop_token[cur_lev] = split_token(&input_p, sep);
	mudstate.loop_number[cur_lev] += 1;
	work_buf = alloc_lbuf("perform_iter.eval");
	StrCopyKnown(work_buf, exprstr, elen); /* we might nibble this */
	str = work_buf;
	if (!flag) {
	    savep = *bufc;
	    exec(buff, bufc, 0, player, cause, EV_STRIP | EV_FCHECK | EV_EVAL,
		 &str, cargs, ncargs);
	} else {
	    dp = result = alloc_lbuf("perform_iter.out");
	    exec(result, &dp, 0, player, cause, EV_STRIP | EV_FCHECK | EV_EVAL,
		 &str, cargs, ncargs);
	    *dp = '\0';
	    notify(cause, result);
	    free_lbuf(result);
	}
	free_lbuf(work_buf);
	if (bool_flag != BOOL_COND_NONE) {
	    is_true = xlate(savep);
	    if (!is_true && (bool_flag == BOOL_COND_TRUE))
		break;
	    if (is_true && (bool_flag == BOOL_COND_FALSE))
		break;
	} 
    }

    free_lbuf(list_str);
    mudstate.in_loop--;
}

FUNCTION(fun_iter)
{
    char sep, osep;

    evarargs_preamble("ITER", 2, 4);
    perform_iter(buff, bufc, player, cause, fargs[0], fargs[1],
		 cargs, ncargs, sep, osep, 0, BOOL_COND_NONE);
}

FUNCTION(fun_whenfalse)
{
    char sep, osep;

    evarargs_preamble("WHENFALSE", 2, 4);
    perform_iter(buff, bufc, player, cause, fargs[0], fargs[1],
		 cargs, ncargs, sep, osep, 0, BOOL_COND_FALSE);
}

FUNCTION(fun_whentrue)
{
    char sep, osep;

    evarargs_preamble("WHENTRUE", 2, 4);
    perform_iter(buff, bufc, player, cause, fargs[0], fargs[1],
		 cargs, ncargs, sep, osep, 0, BOOL_COND_TRUE);
}

FUNCTION(fun_list)
{
    char sep;

    varargs_preamble("LIST", 3);
    perform_iter(buff, bufc, player, cause, fargs[0], fargs[1],
		 cargs, ncargs, sep, ' ', 1, BOOL_COND_NONE);
}

/* ---------------------------------------------------------------------------
 * itext(), inum(), ilev(): Obtain nested iter tokens (##, #@, #!).
 */

FUNCTION(fun_ilev)
{
    safe_ltos(buff, bufc, mudstate.in_loop - 1);
}

FUNCTION(fun_itext)
{
    int lev;

    lev = atoi(fargs[0]);
    if ((lev > mudstate.in_loop - 1) || (lev < 0))
	return;

    safe_str(mudstate.loop_token[lev], buff, bufc);
}

FUNCTION(fun_inum)
{
    int lev;

    lev = atoi(fargs[0]);
    if ((lev > mudstate.in_loop - 1) || (lev < 0)) {
	safe_chr('0', buff, bufc);
	return;
    }
    
    safe_ltos(buff, bufc, mudstate.loop_number[lev]);
}

/* ---------------------------------------------------------------------------
 * fun_fold: iteratively eval an attrib with a list of arguments
 *        and an optional base case.  With no base case, the first list element
 *    is passed as %0 and the second is %1.  The attrib is then evaluated
 *    with these args, the result is then used as %0 and the next arg is
 *    %1 and so it goes as there are elements left in the list.  The
 *    optinal base case gives the user a nice starting point.
 *
 *    > &REP_NUM object=[%0][repeat(%1,%1)]
 *    > say fold(OBJECT/REP_NUM,1 2 3 4 5,->)
 *    You say "->122333444455555"
 *
 *      NOTE: To use added list separator, you must use base case!
 */

FUNCTION(fun_fold)
{
	dbref aowner, thing;
	int aflags, alen, anum;
	ATTR *ap;
	char *atext, *result, *curr, *bp, *str, *cp, *atextbuf, *clist[2],
	*rstore, sep;

	/* We need two to four arguements only */

	mvarargs_preamble("FOLD", 2, 4);

	/* Two possibilities for the first arg: <obj>/<attr> and <attr>. */

	Parse_Uattr(player, fargs[0], thing, anum, ap);
	Get_Uattr(player, thing, ap, atext, aowner, aflags, alen);

	/* Evaluate it using the rest of the passed function args */

	cp = curr = fargs[1];
	atextbuf = alloc_lbuf("fun_fold");
	StrCopyKnown(atextbuf, atext, alen);

	/* may as well handle first case now */

	if ((nfargs >= 3) && (fargs[2])) {
		clist[0] = fargs[2];
		clist[1] = split_token(&cp, sep);
		result = bp = alloc_lbuf("fun_fold");
		str = atextbuf;
		exec(result, &bp, 0, player, cause, EV_STRIP | EV_FCHECK | EV_EVAL,
		     &str, clist, 2);
		*bp = '\0';
	} else {
		clist[0] = split_token(&cp, sep);
		clist[1] = split_token(&cp, sep);
		result = bp = alloc_lbuf("fun_fold");
		str = atextbuf;
		exec(result, &bp, 0, player, cause, EV_STRIP | EV_FCHECK | EV_EVAL,
		     &str, clist, 2);
		*bp = '\0';
	}

	rstore = result;
	result = NULL;

	while (cp && (mudstate.func_invk_ctr < mudconf.func_invk_lim)) {
		clist[0] = rstore;
		clist[1] = split_token(&cp, sep);
		StrCopyKnown(atextbuf, atext, alen);
		result = bp = alloc_lbuf("fun_fold");
		str = atextbuf;
		exec(result, &bp, 0, player, cause,
		     EV_STRIP | EV_FCHECK | EV_EVAL, &str, clist, 2);
		*bp = '\0';
		strcpy(rstore, result);
		free_lbuf(result);
	}
	safe_str(rstore, buff, bufc);
	free_lbuf(rstore);
	free_lbuf(atext);
	free_lbuf(atextbuf);
}

/* ---------------------------------------------------------------------------
 * fun_filter: iteratively perform a function with a list of arguments
 *             and return the arg, if the function evaluates to TRUE using the 
 *      arg.
 *
 *      > &IS_ODD object=mod(%0,2)
 *      > say filter(object/is_odd,1 2 3 4 5)
 *      You say "1 3 5"
 *      > say filter(object/is_odd,1-2-3-4-5,-)
 *      You say "1-3-5"
 *
 *  NOTE:  If you specify a separator it is used to delimit returned list
 */

static void handle_filter(player, cause, arg_func, arg_list, buff, bufc,
			  sep, osep, flag)
    dbref player, cause;
    char *arg_func, *arg_list;
    char *buff;
    char **bufc;
    char sep, osep;
    int flag;			/* 0 is filter(), 1 is filterbool() */
{
	dbref aowner, thing;
	int aflags, alen, anum;
	ATTR *ap;
	char *atext, *result, *curr, *objstring, *bp, *str, *cp, *atextbuf;
	char *bb_p;

	/* Two possibilities for the first arg: <obj>/<attr> and <attr>. */

	Parse_Uattr(player, arg_func, thing, anum, ap);
	Get_Uattr(player, thing, ap, atext, aowner, aflags, alen);

	/* Now iteratively eval the attrib with the argument list */

	cp = curr = trim_space_sep(arg_list, sep);
	atextbuf = alloc_lbuf("fun_filter");
	bb_p = *bufc;
	while (cp) {
		objstring = split_token(&cp, sep);
		StrCopyKnown(atextbuf, atext, alen);
		result = bp = alloc_lbuf("fun_filter");
		str = atextbuf;
		exec(result, &bp, 0, player, cause,
		     EV_STRIP | EV_FCHECK | EV_EVAL, &str, &objstring, 1);
		*bp = '\0';
		if ((!flag && (*result == '1')) || (flag && xlate(result))) {
		        if (*bufc != bb_p) {
			    print_sep(osep, buff, bufc);
			}
			safe_str(objstring, buff, bufc);
		}
		free_lbuf(result);
	}
	free_lbuf(atext);
	free_lbuf(atextbuf);
}

FUNCTION(fun_filter)
{
	char sep, osep;

	svarargs_preamble("FILTER", 4);
	handle_filter(player, cause, fargs[0], fargs[1], buff, bufc,
		      sep, osep, 0);
}

FUNCTION(fun_filterbool)
{
	char sep, osep;

	svarargs_preamble("FILTERBOOL", 4);
	handle_filter(player, cause, fargs[0], fargs[1], buff, bufc,
		      sep, osep, 1);
}

/* ---------------------------------------------------------------------------
 * fun_map: iteratively evaluate an attribute with a list of arguments.
 *  > &DIV_TWO object=fdiv(%0,2)
 *  > say map(1 2 3 4 5,object/div_two)
 *  You say "0.5 1 1.5 2 2.5"
 *  > say map(object/div_two,1-2-3-4-5,-)
 *  You say "0.5-1-1.5-2-2.5"
 *
 */

FUNCTION(fun_map)
{
	dbref aowner, thing;
	int aflags, alen, anum;
	ATTR *ap;
	char *atext, *objstring, *str, *cp, *atextbuf, *bb_p, sep, osep;

	svarargs_preamble("MAP", 4);

	/* If we don't have anything for a second arg, don't bother. */
	if (!fargs[1] || !*fargs[1])
	        return;

	/* Two possibilities for the second arg: <obj>/<attr> and <attr>. */

	Parse_Uattr(player, fargs[0], thing, anum, ap);
	Get_Uattr(player, thing, ap, atext, aowner, aflags, alen);

	/* now process the list one element at a time */

	cp = trim_space_sep(fargs[1], sep);
	atextbuf = alloc_lbuf("fun_map");
	bb_p = *bufc;
	while (cp && (mudstate.func_invk_ctr < mudconf.func_invk_lim)) {
	        if (*bufc != bb_p) {
		    print_sep(osep, buff, bufc);
		}
		objstring = split_token(&cp, sep);
		StrCopyKnown(atextbuf, atext, alen);
		str = atextbuf;
		exec(buff, bufc, 0, player, cause,
		     EV_STRIP | EV_FCHECK | EV_EVAL, &str, &objstring, 1);
	}
	free_lbuf(atext);
	free_lbuf(atextbuf);
}

/* ---------------------------------------------------------------------------
 * fun_mix: Like map, but operates on two lists or more lists simultaneously,
 * passing the elements as %0, %1, %2, etc.
 */

FUNCTION(fun_mix)
{
    dbref aowner, thing;
    int aflags, alen, anum, i, lastn, nwords, wc;
    ATTR *ap;
    char *str, *atext, *os[10], *atextbuf, *bb_p, sep;
    char *cp[10];
    int count[LBUF_SIZE / 2];
    char tmpbuf[2];

    /* Check to see if we have an appropriate number of arguments.
     * If there are more than three arguments, the last argument is
     * ALWAYS assumed to be a delimiter.
     */

    if (!fn_range_check("MIX", nfargs, 3, 12, buff, bufc)) {
	return;
    }
    if (nfargs < 4) {
	sep = ' ';
	lastn = nfargs - 1;
    } else if (!delim_check(fargs, nfargs, nfargs, &sep, buff, bufc, 0,
			    player, cause, cargs, ncargs, 0)) {
	return;
    } else {
	lastn = nfargs - 2;
    }

    /* Get the attribute, check the permissions. */

    Parse_Uattr(player, fargs[0], thing, anum, ap);
    Get_Uattr(player, thing, ap, atext, aowner, aflags, alen);

    for (i = 0; i < 10; i++)
	cp[i] = NULL;

    bb_p = *bufc;

    /* process the lists, one element at a time. */

    for (i = 1; i <= lastn; i++) {
	cp[i] = trim_space_sep(fargs[i], sep);
    }
    nwords = count[1] = countwords(cp[1], sep);
    for (i = 2; i<= lastn; i++) {
	count[i] = countwords(cp[i], sep);
	if (count[i] > nwords)
	    nwords = count[i];
    }
    atextbuf = alloc_lbuf("fun_mix");

    for (wc = 0;
	 (wc < nwords) && (mudstate.func_invk_ctr < mudconf.func_invk_lim);
	 wc++) {
	for (i = 1; i <= lastn; i++) {
	    if (count[i]) {
		os[i - 1] = split_token(&cp[i], sep);
	    } else {
		tmpbuf[0] = '\0';
		os[i - 1] = tmpbuf;
	    }
	}
	StrCopyKnown(atextbuf, atext, alen);

	if (*bufc != bb_p)
	    safe_chr(sep, buff, bufc);

	str = atextbuf;
	
	exec(buff, bufc, 0, player, cause, EV_STRIP | EV_FCHECK | EV_EVAL,
		      &str, &(os[0]), lastn);
    }
    free_lbuf(atext);
    free_lbuf(atextbuf);
}

/* ---------------------------------------------------------------------------
 * fun_step: A little like a fusion of iter() and mix(), it takes elements
 * of a list X at a time and passes them into a single function as %0, %1,
 * etc.   step(<attribute>,<list>,<step size>,<delim>,<outdelim>)
 */

FUNCTION(fun_step)
{
    ATTR *ap;
    dbref aowner, thing;
    int aflags, alen, anum;
    char *atext, *str, *cp, *atextbuf, *bb_p, *os[10];
    char sep, osep;
    int step_size, i;

    svarargs_preamble("STEP", 5);

    step_size = atoi(fargs[2]);
    if ((step_size < 1) || (step_size > NUM_ENV_VARS)) {
	notify(player, "Illegal step size.");
	return;
    }

    /* Get attribute. Check permissions. */

    Parse_Uattr(player, fargs[0], thing, anum, ap);
    Get_Uattr(player, thing, ap, atext, aowner, aflags, alen);

    cp = trim_space_sep(fargs[1], sep);
    atextbuf = alloc_lbuf("fun_step");
    bb_p = *bufc;
    while (cp && (mudstate.func_invk_ctr < mudconf.func_invk_lim)) {
	if (*bufc != bb_p) {
	    print_sep(osep, buff, bufc);
	}
	for (i = 0; cp && (i < step_size); i++)
	    os[i] = split_token(&cp, sep);
	StrCopyKnown(atextbuf, atext, alen);
	str = atextbuf;
	exec(buff, bufc, 0, player, cause, EV_STRIP | EV_FCHECK | EV_EVAL,
	     &str, &(os[0]), i);
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
    int aflags, alen, anum;
    ATTR *ap;
    char *str, *atext, *atextbuf, *cp, *cbuf;
    char start_token, end_token;
    int in_string = 1;

    if (!fn_range_check("FOREACH", nfargs, 2, 4, buff, bufc))
	return;

    Parse_Uattr(player, fargs[0], thing, anum, ap);
    Get_Uattr(player, thing, ap, atext, aowner, aflags, alen);

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
	StrCopyKnown(atextbuf, atext, alen);
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

FUNCTION(fun_munge)
{
	dbref aowner, thing;
	int aflags, alen, anum, nptrs1, nptrs2, nresults, i, j;
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

	Parse_Uattr(player, fargs[0], thing, anum, ap);
	Get_Uattr(player, thing, ap, atext, aowner, aflags, alen);

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
			        if (*bufc != oldp) {
				    print_sep(osep, buff, bufc);
				}
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

/* ---------------------------------------------------------------------------
 * fun_while: Evaluate a list until a termination condition is met:
 * while(EVAL_FN,CONDITION_FN,foo|flibble|baz|meep,1,|,-)
 * where EVAL_FN is "[strlen(%0)]" and CONDITION_FN is "[strmatch(%0,baz)]"
 * would result in '3-7-3' being returned.
 * The termination condition is an EXACT not wild match.
 */

FUNCTION(fun_while)
{
    char sep, osep;
    dbref aowner1, thing1, aowner2, thing2;
    int aflags1, aflags2, anum1, anum2, alen1, alen2;
    int is_same, is_exact_same;
    ATTR *ap, *ap2;
    char *atext1, *atext2, *atextbuf, *condbuf;
    char *objstring, *cp, *str, *dp, *savep, *bb_p;

    svarargs_preamble("WHILE", 6);

    /* If our third arg is null (empty list), don't bother. */

    if (!fargs[2] || !*fargs[2])
	return;

    /* Our first and second args can be <obj>/<attr> or just <attr>.
     * Use them if we can access them, otherwise return an empty string.
     *
     * Note that for user-defined attributes, atr_str() returns a pointer
     * to a static, and that therefore we have to be careful about what
     * we're doing.
     */

    Parse_Uattr(player, fargs[0], thing1, anum1, ap);
    Get_Uattr(player, thing1, ap, atext1, aowner1, aflags1, alen1);
    Parse_Uattr(player, fargs[1], thing2, anum2, ap2);
    if (!ap2) {
	free_lbuf(atext1);	/* we allocated this, remember? */
	return;
    }

    /* If our evaluation and condition are the same, we can save ourselves
     * some time later. There are two possibilities: we have the exact
     * same obj/attr pair, or the attributes contain identical text.
     */

    if ((thing1 == thing2) && (ap->number == ap2->number)) {
	is_same = 1;
	is_exact_same = 1;
	atext2 = atext1;
    } else {
	is_exact_same = 0; 
	atext2 = atr_pget(thing2, ap2->number, &aowner2, &aflags2, &alen2);
	if (!*atext2 || !See_attr(player, thing2, ap2, aowner2, aflags2)) {
	    free_lbuf(atext1);
	    free_lbuf(atext2);
	    return;
	}
	if (!strcmp(atext1, atext2))
	    is_same = 1;
	else 
	    is_same = 0;
    }

    /* Process the list one element at a time. */

    cp = trim_space_sep(fargs[2], sep);
    atextbuf = alloc_lbuf("fun_while.eval");
    if (!is_same)
	condbuf = alloc_lbuf("fun_while.cond");
    bb_p = *bufc;
    while (cp && (mudstate.func_invk_ctr < mudconf.func_invk_lim)) {
	if (*bufc != bb_p) {
	    print_sep(osep, buff, bufc);
	}
	objstring = split_token(&cp, sep);
	StrCopyKnown(atextbuf, atext1, alen1);
	str = atextbuf;
	savep = *bufc;
	exec(buff, bufc, 0, player, cause, EV_STRIP | EV_FCHECK | EV_EVAL,
	     &str, &objstring, 1);
	if (is_same) {
	    if (!strcmp(savep, fargs[3]))
		break;
	} else {
	    StrCopyKnown(condbuf, atext2, alen2);
	    dp = str = savep = condbuf;
	    exec(condbuf, &dp, 0, player, cause,
		 EV_STRIP | EV_FCHECK | EV_EVAL, &str, &objstring, 1);
	    if (!strcmp(savep, fargs[3]))
		break;
	}
    }
    free_lbuf(atext1);
    if (!is_exact_same)
	free_lbuf(atext2);
    free_lbuf(atextbuf);
    if (!is_same)
	free_lbuf(condbuf);
}
