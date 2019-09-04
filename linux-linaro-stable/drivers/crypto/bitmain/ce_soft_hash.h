#ifndef __CE_SOFT_HASH_H__
#define __CE_SOFT_HASH_H__

void ce_sha256_transform(u32 *state, const u8 *input);
void ce_sha1_transform(u32 *state, const u8 *input);

#endif
