/* $Id: bools.h,v 1.1 2006-03-29 23:38:28 megastep Exp $ */

/*
  Manage global installer booleans.
  
  Author: Stephane Peter
*/

#ifndef __LOKI_BOOLS_H__
#define __LOKI_BOOLS_H__

/* Type definitions */
typedef struct _setup_bool {
	char *name;  /* Symbolic name */
	char *script; /* A script to be run to determine the value, or NULL */
	unsigned value : 1; /* Boolean value */
	unsigned once : 1; /* Whether the script has to be run every time the bool is evaluated, or just upon init */
	unsigned inited : 1; /* Value was filled in */
	struct _setup_bool *next;
} setup_bool;


/* Private type */
struct _setup_expression;
typedef struct _setup_expression setup_expression;


/**** API functions *******/

/* Fill up with standard booleans */
void setup_init_bools(install_info *);

/* Create and free booleans */
setup_bool *setup_new_bool(const char *name);
void        setup_free_bool(setup_bool *);

/* Easy add of simple new bools */
setup_bool *setup_add_bool(const char *name, unsigned value);

/* Manipulate booleans */
setup_bool *setup_find_bool(const char *name);
int         setup_get_bool(const setup_bool *b);
void        setup_set_bool(setup_bool *b, unsigned value);


/* Handle expressions */
setup_expression *setup_parse_expression(const char *expr);
void              setup_free_expression(setup_expression *);
/* Evaluate the expression - returns TRUE or FALSE */
int               setup_evaluate(setup_expression *);

/* Easy shortcut to evaluate an expression string */
int   match_condition(const char *expr);

/* Free up all memory */
void        setup_exit_bools(void);

#endif
