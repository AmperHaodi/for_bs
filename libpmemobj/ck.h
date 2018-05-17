#include <stdint.h>
#include "pvector.h"
#ifdef __cplusplus
extern "C" {
#endif

int pmemobj_ck_add_range(PMEMoid oid, uint64_t hoff, size_t size);
int pmemobj_tx_add_range_printf(PMEMoid oid, uint64_t off, size_t size);



#ifdef __cplusplus
}
#endif