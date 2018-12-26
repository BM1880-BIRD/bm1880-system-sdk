#ifndef _BM_RUNTIME_BMKERNEL_H_
#define _BM_RUNTIME_BMKERNEL_H_

#include <libbmruntime/bmruntime.h>

#ifdef __cplusplus
extern "C" {
#endif

bmerr_t bmruntime_bmkernel_create(bmctx_t ctx, void **p_bk_ctx);
void bmruntime_bmkernel_destroy(bmctx_t ctx);
void bmruntime_bmkernel_submit(bmctx_t ctx);
void bmruntime_bmkernel_submit_pio(bmctx_t ctx);

#ifdef __cplusplus
}
#endif

#endif /* _BM_RUNTIME_BMKERNEL_H_ */
