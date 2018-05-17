#include <inttypes.h>
#include <wchar.h>

#include "queue.h"
#include "ravl.h"
#include "obj.h"
#include "out.h"
#include "pmalloc.h"
#include "tx.h"
#include "valgrind_internal.h"


int
pmemobj_tx_add_range_printf(PMEMoid oid, uint64_t hoff, size_t size)
{
	struct tx *tx = get_tx();

	struct tx_range_def args = {
		.offset = oid.off + hoff,//0 //PMEMoid oid 
		.size = size,
		.flags = 0,
	};
	uint64_t data_offset = args.offset;
	uint64_t data_size = args.size;
	void *src = OBJ_OFF_TO_PTR(tx->pop, data_offset);
	printf("pmemobj_tx_add_range_printf_data_size=%d\n",(int)data_size );
	int *p= (int*)(src);
	int ii=0;
	for(;ii<(int)(data_size);ii++) printf("%x\n",*(p+ii) );
		return 0;
}

int
pmemobj_ck_add_range(PMEMoid oid, uint64_t hoff, size_t size)
{
	LOG(3, NULL);
	printf("pmemobj_ck_add_range!\n");
	// struct tx *tx = get_tx();

	// ASSERT_IN_TX(tx);
	// ASSERT_TX_STAGE_WORK(tx);

	// if (oid.pool_uuid_lo != tx->pop->uuid_lo) {
	// 	ERR("invalid pool uuid");
	// 	return obj_tx_abort_err(EINVAL);
	// }
	// ASSERT(OBJ_OID_IS_VALID(tx->pop, oid));

	// struct tx_range_def args = {
	// 	.offset = oid.off + hoff,
	// 	.size = size,
	// 	.flags = 0,
	// };
	return 0;
	//return pmemobj_ck_add_common(tx, &args);
}