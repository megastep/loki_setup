/*
 * Isolate the macros related to locale
 * $Id: setup-locale.h,v 1.9 2004-03-31 16:30:39 icculus Exp $
 */

#ifndef _setup_locale_h_
#define _setup_locale_h_

/*#define HARDCODE_TRANSLATION 1*/

#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#define _(String) gettext (String)
#elif HARDCODE_TRANSLATION
const char *translation_lookup_table(const char *str);
#define _(String) translation_lookup_table(String)
#define bindtextdomain(x, y)
#define textdomain(x)
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

