/*
 * Isolate the macros related to locale
 * $Id: setup-locale.h,v 1.10 2004-03-31 16:33:14 icculus Exp $
 */

#ifndef _setup_locale_h_
#define _setup_locale_h_

/*#define HARDCODE_TRANSLATION 1*/

#if HARDCODE_TRANSLATION
const char *translation_lookup_table(const char *str);
#define _(String) translation_lookup_table(String)
#define bindtextdomain(x, y)
#define textdomain(x)
#elif defined HAVE_LIBINTL_H
#include <libintl.h>
#define _(String) gettext (String)
#else
#define _(String) (String)
#define bindtextdomain(x, y)
#define textdomain(x)
#endif

#define gettext_noop(String) (String)

/* The prefix for all our setup data files */
#define SETUP_BASE          "setup.data/"

#define SETUP_CONFIG  SETUP_BASE "setup.xml"

#define PACKAGE "setup"
#ifndef LOCALEDIR
#define LOCALEDIR SETUP_BASE "locale"
#endif

#endif

