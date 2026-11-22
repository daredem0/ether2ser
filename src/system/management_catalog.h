#ifndef MANAGEMENT_CATALOG_H
#define MANAGEMENT_CATALOG_H

#include <stddef.h>
#include <stdint.h>

#include "system/management.h"

#define MANAGEMENT_METHOD_MASK(method) ((uint8_t)(1U << (uint8_t)(method)))

typedef struct
{
    management_resource_t   resource;
    const char*             path;
    uint8_t                 method_mask;
    management_value_kind_t value_kind;
    const char*             help;
} management_resource_descriptor_t;

size_t management_catalog_count(void);
const management_resource_descriptor_t* management_catalog_at(size_t index);
const management_resource_descriptor_t* management_catalog_find(const char* path);

#endif /* MANAGEMENT_CATALOG_H */
