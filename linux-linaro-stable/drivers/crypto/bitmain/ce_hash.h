#ifndef __CE_HASH_H__
#define __CE_HASH_H__

int unregister_all_hash(void);
int register_all_hash(void);
void ce_hash_handle_request(struct ce_inst *inst,
			    struct crypto_async_request *req);

#endif
