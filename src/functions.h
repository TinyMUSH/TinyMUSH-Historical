/* functions.h - declarations for functions & function processing */
/* $Id$ */

#include "copyright.h"

#ifndef __FUNCTIONS_H
#define __FUNCTIONS_H

/* ---------------------------------------------------------------------------
 * Type definitions.
 */

typedef struct fun {
	const char *name;	/* function name */
	void	(*fun)();	/* handler */
	int	nargs;		/* Number of args needed or expected */
	int	flags;		/* Function flags */
	int	perms;		/* Access to function */
} FUN;

typedef struct ufun {
	char *name;	/* function name */
	dbref	obj;		/* Object ID */
	int	atr;		/* Attribute ID */
	int	flags;		/* Function flags */
	int	perms;		/* Access to function */
	struct ufun *next;	/* Next ufun in chain */
} UFUN;

typedef union char_or_string {
    char c;
    char str[LBUF_SIZE];
} Delim;

typedef struct var_entry VARENT;
struct var_entry {
    char *text;			/* variable text */
};

#ifdef FLOATING_POINTS
#ifndef linux                   /* linux defines atof as a macro */
double atof();
#endif                          /* ! linux */
#define aton atof
typedef double NVAL;
#else
#define aton atoi
typedef int NVAL;
#endif                          /* FLOATING_POINTS */

/* ---------------------------------------------------------------------------
 * Constants.
 */

#define DELIM_EVAL	0x001	/* Must eval delimiter. */
#define DELIM_NULL	0x002	/* Null delimiter okay. */
#define DELIM_CRLF	0x004	/* '%r' delimiter okay. */
#define DELIM_STRING	0x008	/* Multi-character delimiter okay. */

/* ---------------------------------------------------------------------------
 * Function declarations.
 */

extern char *FDECL(trim_space_sep, (char *, char));
extern char *FDECL(next_token, (char *, char));
extern char *FDECL(split_token, (char **, char));
extern int FDECL(countwords, (char *, char));
extern int FDECL(list2arr, (char **, int, char *, char));
extern void FDECL(arr2list, (char **, int, char *, char **, Delim, int));
extern int FDECL(list2ansi, (char **, char *, int, char *, char));
extern double NDECL(makerandom);
extern int FDECL(fn_range_check, (const char *, int, int, int, char *, char **));
extern int FDECL(delim_check, (char **, int, int, Delim *, char *, char **,
			       dbref, dbref, dbref, char **, int, int));
extern INLINE void FDECL(do_reverse, (char *, char *));

extern char *ansi_nchartab[];	/* from fnhelper.c */

/* ---------------------------------------------------------------------------
 * Function prototype macro.
 */

#define	FUNCTION(x)	\
    void x(buff, bufc, player, caller, cause, fargs, nfargs, cargs, ncargs) \
	char *buff, **bufc; \
	dbref player, caller, cause; \
	char *fargs[], *cargs[]; \
	int nfargs, ncargs;

/* ---------------------------------------------------------------------------
 * Delimiter macros for functions that take an optional delimiter character.
 */

/* Helper macros:
 *   VaChkHelp_Args(min_args, max_args): Check number of args.
 *   VaChkHelp_InSep(arg_number, flags): Use arg_number as input sep.
 *   VaChkHelp_OutDef(arg_number): If nfargs less than arg_number,
 *     use the input separator. DO NOT PUT A SEMI-COLON AFTER THIS MACRO.
 *   VaChkHelp_OutSep(arg_number, flags): Use arg_number as output sep.
 */

#define VaChkHelp_Args(xname, xmin, xmax) \
if (!fn_range_check(xname, nfargs, xmin, xmax, buff, bufc)) \
    return;

#define VaChkHelp_InSep(xargnum, xflags) \
if (!delim_check(fargs, nfargs, xargnum, &isep, buff, bufc, \
    player, caller, cause, cargs, ncargs, xflags)) \
    return;

#define VaChkHelp_OutDef(xargnum) \
if (nfargs < xargnum) { \
    osep.c = isep.c; \
    osep_len = 1; \
} else

#define VaChkHelp_OutSep(xargnum, xflags) \
if (!(osep_len = delim_check(fargs, nfargs, xargnum, &osep, \
           buff, bufc, player, caller, cause, cargs, ncargs, \
           (xflags)|DELIM_NULL|DELIM_CRLF))) \
    return;

/*
 * VaChk_Range("FUNCTION", min_args, max_args): Functions which take
 *   between min_args and max_args. Don't check for delimiters.
 * 
 * VaChk_Only_In("FUNCTION", max_args): Functions which take max_args - 1 args
 *   or, with a delimiter, max_args args.
 *
 * VaChk_In("FUNCTION", min_args, max_args): Functions which take
 *   between min_args and max_args, with max_args as a delimiter.
 *
 * VaChk_Only_In_Out("FUNCTION", max_args): Functions which take at least
 *   max_args - 2, with max_args - 1 as an input delimiter, and max_args as
 *   an output delimiter.
 *
 * VaChk_In_Out("FUNCTION", min_args, max_args): Functions which take at
 *   least min_args, with max_args - 1 as an input delimiter, and max_args
 *   as an output delimiter.
 *
 * VaChk_InEval_OutEval("FUNCTION", min_args, max_args): Functions which
 *   take at least min_args, with max_args - 1 as an input delimiter that
 *   must be evaluated, and max_args as an output delimiter which must
 *   be evaluated.
 */

#define VaChk_Range(xname,xminargs,xnargs) \
  VaChkHelp_Args(xname, xminargs, xnargs);

#define VaChk_Only_In(xname,xnargs) \
  VaChkHelp_Args(xname, xnargs-1, xnargs); \
  VaChkHelp_InSep(xnargs, 0);

#define VaChk_In(xname,xminargs,xnargs) \
  VaChkHelp_Args(xname, xminargs, xnargs); \
  VaChkHelp_InSep(xnargs, 0);

#define VaChk_Only_In_Out(xname,xnargs) \
  VaChkHelp_Args(xname, xnargs-2, xnargs); \
  VaChkHelp_InSep(xnargs-1, 0); \
  VaChkHelp_OutDef(xnargs) \
    VaChkHelp_OutSep(xnargs, 0);

#define VaChk_In_Out(xname,xminargs,xnargs) \
  VaChkHelp_Args(xname, xminargs, xnargs); \
  VaChkHelp_InSep(xnargs-1, 0); \
  VaChkHelp_OutDef(xnargs) \
    VaChkHelp_OutSep(xnargs, 0);

#define VaChk_InEval_OutEval(xname, xminargs, xnargs) \
  VaChkHelp_Args(xname, xminargs, xnargs); \
  VaChkHelp_InSep(xnargs-1, DELIM_EVAL); \
  VaChkHelp_OutSep(xnargs, DELIM_EVAL);

/* ---------------------------------------------------------------------------
 * Miscellaneous macros.
 */

/* Special handling of separators. */

#define print_sep(s,l,b,p) \
if ((l) == 1) { \
    if ((s).c == '\r') { \
	safe_crlf((b),(p)); \
    } else if ((s).c != '\0') { \
	safe_chr((s).c,(b),(p)); \
    } \
} else { \
    safe_known_str((s).str, (l), (b), (p)); \
}

/* Macro for finding an <attr> or <obj>/<attr>
 */

#define Parse_Uattr(p,s,t,n,a)				\
    if (parse_attrib((p), (s), &(t), &(n), 0)) {	\
	if (((n) == NOTHING) || !(Good_obj(t)))		\
	    (a) = NULL;					\
	else						\
	    (a) = atr_num(n);				\
    } else {						\
        (t) = (p);					\
	(a) = atr_str(s);				\
    }

/* Macro for obtaining an attrib. */

#define Get_Uattr(p,t,a,b,o,f,l)				\
    if (!(a)) {							\
	return;							\
    }								\
    (b) = atr_pget(t, (a)->number, &(o), &(f), &(l));		\
    if (!*(b) || !(See_attr((p), (t), (a), (o), (f)))) {	\
	free_lbuf(b);						\
	return;							\
    }

/* Macro for skipping to the end of an ANSI code, if we're at the
 * beginning of one. 
 */
#define Skip_Ansi_Code(s) \
	    savep = s;				\
	    while (*s && (*s != ANSI_END)) {	\
		safe_chr(*s, buff, bufc);	\
		s++;				\
	    }					\
	    if (*s) {				\
		safe_chr(*s, buff, bufc);	\
		s++;				\
	    }					\
	    if (!strncmp(savep, ANSI_NORMAL, 4)) {	\
		have_normal = 1;		\
	    } else {				\
		have_normal = 0;		\
            }

/* Macro for writing a certain amount of padding into a buffer.
 * l is the number of characters left to write.
 * m is a throwaway integer for holding the maximum.
 * c is the separator character to use.
 */
#define print_padding(l,m,c) \
if ((l) > 0) { \
    (m) = LBUF_SIZE - 1 - (*bufc - buff); \
    (l) = ((l) > (m)) ? (m) : (l); \
    memset(*bufc, (c), (l)); \
    *bufc += (l); \
    **bufc = '\0'; \
}

/* Macro for turning an ANSI state back into ANSI codes.
 * x is a throwaway character pointer.
 * w is a pointer to the ANSI state of the previous word.
 */
#define print_ansi_state(x,w) \
safe_copy_known_str(ANSI_BEGIN, 2, buff, bufc, LBUF_SIZE - 1); \
for ((x) = (w); *(x); (x)++) { \
    if ((x) != (w)) { \
	safe_chr(';', buff, bufc); \
    } \
    safe_str(ansi_nchartab[(unsigned char) *(x)], buff, bufc); \
} \
safe_chr(ANSI_END, buff, bufc);

#endif /* __FUNCTIONS_H */
