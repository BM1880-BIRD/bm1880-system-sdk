#ifndef _BM_DEBUG_H_
#define _BM_DEBUG_H_

#include <linux/debugfs.h>

extern int bm_vip_debug;

#define dprintk(level, fmt, arg...) \
	do { \
		if (bm_vip_debug & level) \
			pr_info("%s(): " fmt, __func__, ## arg); \
	} while (0)

enum vip_msg_prio {
	VIP_ERR  = 0x0001,
	VIP_WARN = 0x0002,
	VIP_INFO = 0x0004,
	VIP_DBG  = 0x0008,
	VIP_VB2  = 0x0010,
};

#endif
