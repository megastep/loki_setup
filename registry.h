#ifndef LOKI_REGISTRY_H
#define LOKI_REGISTRY_H

#include "setupdb.h"

product_t *open_registered_product(const char *name);
int trusted_registry(void);
int trusted_product_files(const product_info_t *info);

#endif
