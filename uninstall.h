/* Generic uninstaller for products.
   Parses the product INI file in ~/.loki/installed/ and uninstalls the software.
*/

/* $Id: uninstall.h,v 1.7 2002-12-07 00:57:32 megastep Exp $ */

extern product_t *prod;

/* See if we have permissions to uninstall a product */
extern int check_permissions(product_info_t *info, int verbose);

/* Uninstall a single component */
extern void uninstall_component(product_component_t *comp, product_info_t *info);

/* Uninstall an entire product */
extern int perform_uninstall(product_t *prod, product_info_t *info);
