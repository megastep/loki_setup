/* $Id: bools.c,v 1.2 2006-03-31 01:29:02 megastep Exp $ */

/*
  Manage global installer booleans.
  
  Author: Stephane Peter
*/

#include "config.h"
#include "install.h"
#include "bools.h"
#include "install_log.h"
#include "detect.h"
#include "arch.h"

#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

/* The maximum length of a boolean variable name */
#define MAX_VARNAME 30

/* Use a N-tree to represent the expressions */
struct _setup_expression
{
	enum { OP_VARIABLE = 1, OP_OR, OP_AND, OP_XOR } type;
	int negate;
	setup_bool *var;
	struct _setup_expression *list, *next;
};

setup_bool *setup_booleans = NULL;

static install_info *cur_info = NULL;
static setup_bool *setup_false = NULL, *setup_true = NULL;

/* Fill up with standard booleans */
void setup_init_bools(install_info *info)
{
	char buf[80];

	cur_info = info;
	setup_booleans = NULL;

	/* Basic bools */
	setup_true = setup_add_bool("true", 1);
	setup_false = setup_add_bool("false", 0);

	setup_add_bool("is-root", geteuid()==0);
	setup_add_bool("reinstalling", info->options.reinstalling);
#ifdef __linux
	setup_add_bool("selinux", have_selinux);
#endif
#ifdef HAVE_BZIP2_SUPPORT
	setup_add_bool("bzip2", 1);
#endif
#ifdef RPM_SUPPORT
	setup_add_bool("rpm-support", 1);
# if RPM_SUPPORT == 3
	setup_add_bool("rpm3-support", 1);
# endif
#endif

	/* Add arch, glibc, and os bools */
	setup_add_bool(detect_os(), 1);
	setup_add_bool(info->arch, 1);
	setup_add_bool(info->libc, 1);
	setup_add_bool(distribution_symbol[info->distro], 1);
	snprintf(buf, sizeof(buf), "distro-major-%d", info->distro_maj);
	setup_add_bool(buf, 1);
	snprintf(buf, sizeof(buf), "distro-minor-%d", info->distro_min);
	setup_add_bool(buf, 1);

	SetLocaleBools();
}

/* Create and free booleans */
setup_bool *setup_new_bool(const char *name)
{
	setup_bool *ret = malloc(sizeof(setup_bool));
	if ( ret ) {
		memset(ret, 0, sizeof(*ret));
		ret->name = strdup(name);
		ret->next = setup_booleans;
		setup_booleans = ret;
	}
	return ret;
}

void setup_free_bool(setup_bool *b)
{
	if (b) {
		free(b->name);
		free(b->script);
		free(b->envvar);
		free(b);
	}
}

/* Easy add of simple new bools */
setup_bool *setup_add_bool(const char *name, unsigned value)
{
	setup_bool *ret = setup_new_bool(name);
	setup_set_bool(ret, value);
	ret->once = 1;
	log_debug("New bool: %s = %s", name, value ? "TRUE" : "FALSE");
	return ret;
}

setup_bool *setup_find_bool(const char *name)
{
	setup_bool *ret;

	for ( ret = setup_booleans; ret; ret = ret->next ) {
		if ( !strcmp(ret->name, name) ) {
			return ret;
		}
	}
	return NULL;
}

int setup_get_bool(setup_bool *b)
{
	if ( b ) {
		if ( b->once ) {
			return b->inited ? b->value : 0;
		} else if (b->script) { /* Run the script to determine the value */
			b->value = (run_script(cur_info, b->script, 0, 0) == 0);
			b->inited = 1; /* Keep track of the last value */
			return b->value;
		}
	}
	return 0;
}

void setup_set_bool(setup_bool *b, unsigned value)
{
	if (b) {
		b->value = value;
		b->inited = 1;
	}
}

/* Return the number of characters parsed */
static setup_expression *parse_token(const char *str, int *len)
{
	if ( *str ) {
		setup_expression *exp = malloc(sizeof(setup_expression));
		if ( exp ) {
			int count = 0, cv;
			char var[MAX_VARNAME+1], *v;

			memset(exp, 0, sizeof(setup_expression));
			if ( *str == '!' ) {
				exp->negate = 1;
				str ++;
				count ++;
			}

			switch (*str) {
			case '+': /* & creates problems with XML parsers */
				log_debug("Parsing AND: %s", str);
				exp->type = OP_AND;
				str ++; count ++;
				break;
			case '|':
				log_debug("Parsing OR: %s", str);
				exp->type = OP_OR;
				str ++; count ++;
				break;
			case '^':
				log_debug("Parsing XOR: %s", str);
				exp->type = OP_XOR;
				str ++; count ++;
				break;
			default: /* Variable ? */
				log_debug("Parsing VARIABLE: %s", str);
				if ( isalnum(*str) ) {
					exp->type = OP_VARIABLE;
					v = var;
					cv = 0;
					for (;;) {
						if (cv == MAX_VARNAME) {
							*v = '\0';
							log_warning("Variable '%s' name too long.", var);
							break;
						}
						if ( *str == ')' || *str=='\0' )  /* Let the parent handle the closing parenthesis */
							break;
						cv ++;
						if ( *str==' ' || *str=='\t' || *str==',' ) {
							break;
						}
						*v ++ = *str ++;
					}
					count += cv;
					*v ++ = '\0';
					exp->var = setup_find_bool(var);
					if ( !exp->var ) {
						log_debug("Boolean '%s' is undefined - assuming false.", var);
						exp->var = setup_false;
					}
				} else {
					log_warning("Syntax error in expression: '%c' is not alnum. (%s)", *str, str);
					free(exp); 
					exp = NULL;
				}
			}

			if ( exp && exp->type!=OP_VARIABLE ) { /* Look for () */
				if ( *str == '(' ) {
					setup_expression *sub;
					int sublen = 0;

					str ++; count ++;
					/* Recurse for sub-expressions */
					for (;;) {
						sub = parse_token(str, &sublen);
						if ( sub ) {
							sub->next = exp->list; /* This keeps items in the reverse order */
							exp->list = sub;
							count += sublen;
							str += sublen;
						} else
							break;
						if ( ! *str ) {
							log_warning("Syntax error in expression: missing ')' before end of string");
						}
						if (*str == ')') {
							str ++; count ++;
							break;
						}
					}

					/* Skip any following extraneous character */
					if ( *str==' ' || *str=='\t' || *str==',' ) {
						str ++; count ++;
					}

					if ( !exp->list ) {
						log_warning("Operator didn't refer to any operands.");
						free(exp);
						exp = NULL;
					}
				} else {
					log_warning("Syntax error in expression: '%c' instead of '('", *str);
					free(exp); 
					exp = NULL;
				}
			}
			
			if ( len )
				*len = count;
			return exp;
		} else {
			log_fatal("Failed to allocate expression token");
			return NULL;
		}
	} else { /* Empty string */
		*len = 0;
		return NULL;
	}
}

/* Handle expressions */
setup_expression *setup_parse_expression(const char *expr)
{
	/* Break down the string into a binary tree */
	return parse_token(expr, NULL);
}

void setup_free_expression(setup_expression *expr)
{
	if (expr) {
		setup_expression *next;

		/* Start by freeing the children */
		while ( expr->list ) {
			next = expr->list->next;
			setup_free_expression(expr->list);
			expr->list = next;
		}

		free(expr);
	}
}

/* Evaluate the expression - returns TRUE or FALSE */
int setup_evaluate(setup_expression *expr)
{
	int ret = 0;
	if ( expr ) {
		setup_expression *sub = expr->list;

		switch (expr->type) {
		case OP_VARIABLE:
			ret = setup_get_bool(expr->var);
			break;
		case OP_AND:
			ret = 1;
			while (sub) {
				ret = ret && setup_evaluate(sub);
				if ( !ret )
					break; /* Exit early */
				sub = sub->next;
			}
			break;
		case OP_OR:
			while (sub) {
				ret = ret || setup_evaluate(sub);
				if ( ret )
					break; /* Exit early */
				sub = sub->next;
			}
			break;
		case OP_XOR: /* Only one of the operands must be true */
			ret = 0;
			while (sub) {
				ret += setup_evaluate(sub);
				sub = sub->next;
			}
			if ( ret > 1 )
				ret = 0;
			break;
		}

		if ( expr->negate )
			ret = !ret;
	}
	return ret;
}

/* Easy shortcut to evaluate an expression string */
int match_condition(const char *expr)
{
	int ret = 1; /* Default to TRUE so that empty strings still match */
	if ( expr ) {
		setup_expression *xp = setup_parse_expression(expr);
		if ( xp ) {
			ret = setup_evaluate(xp);
			setup_free_expression(xp);
		} else {
			ret = 0;
		}
		log_debug("Expression '%s' evaluated to %s", expr, ret ? "TRUE" : "FALSE");
	}
	return ret;
}

/* Free up all memory */
void setup_exit_bools(void)
{
	setup_bool *b;
	while ( setup_booleans ) {
		b = setup_booleans->next;
		setup_free_bool(setup_booleans);
		setup_booleans = b;
	}
	cur_info = NULL;
	setup_booleans = NULL;
}
