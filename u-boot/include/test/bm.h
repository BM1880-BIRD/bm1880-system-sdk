
#ifndef __TEST_BM_H__
#define __TEST_BM_H__

#include <test/test.h>

/* Declare a new bitmain test */
#define BM_TEST(_name, _flags)		UNIT_TEST(_name, _flags, bm_test)

#endif /* __TEST_BM_H__ */
