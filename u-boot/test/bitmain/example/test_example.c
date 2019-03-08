#include <common.h>
#include <command.h>
#include <test/ut.h>
#include <test/bm.h>

static int bm_test_example(struct unit_test_state *uts)
{
	printf("Hello, Bitmain unit test\n");

	return 0;
}
BM_TEST(bm_test_example, 0);
