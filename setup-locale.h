/*
 * Isolate the macros related to locale
 * $Id: setup-locale.h,v 1.6 2003-02-27 06:16:01 megastep Exp $
 */

#ifndef _setup_locale_h_
#define _setup_locale_h_

#include <libintl.h>
#define _(String) gettext (String)
#define gettext_noop(String) (String)

/* The prefix for all our setup data files */
#define SETUP_BASE          "setup.data/"

#define SETUP_CONFIG  SETUP_BASE "setup.xml"

#define PACKAGE "setup"
#define LOCALEDIR SETUP_BASE "locale"

#endif

