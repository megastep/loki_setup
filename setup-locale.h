/*
 * Isolate the macros related to locale
 * $Id: setup-locale.h,v 1.8 2004-02-28 13:48:26 icculus Exp $
 */

#ifndef _setup_locale_h_
#define _setup_locale_h_

#ifdef HAVE_LIBINTL_H
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

