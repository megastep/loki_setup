/* Generic uninstaller for products.
   Parses the product INI file in ~/.loki/installed/ and uninstalls the software.
*/

/* $Id: uninstall.h,v 1.2 2002-01-28 01:13:31 megastep Exp $ */

extern product_t *prod;

/* See if we have permissions to uninstall a product */
extern int check_permissions(product_info_t *info, int verbose);

/* Uninstall a single component */
extern void uninstall_component(product_component_t *comp, product_info_t *info);

/* Uninstall an entire product */
extern int perform_uninstall(product_t *prod, product_info_t *info);
