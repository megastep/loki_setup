#include "carbondebug.h"
#include "install_log.h"

void carbon_debug(const char *str)
{
 	/* Just use the standard Setup debug mechanism, i.e. go to
	   stderr when debugging is enabled */   
	log_debug("%s", str);
}
