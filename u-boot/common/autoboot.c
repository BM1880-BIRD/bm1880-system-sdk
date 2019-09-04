/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <autoboot.h>
#include <bootretry.h>
#include <cli.h>
#include <console.h>
#include <fdtdec.h>
#include <menu.h>
#include <post.h>
#include <u-boot/sha256.h>

DECLARE_GLOBAL_DATA_PTR;

#define MAX_DELAY_STOP_STR 32

#ifndef DEBUG_BOOTKEYS
#define DEBUG_BOOTKEYS 0
#endif
#define debug_bootkeys(fmt, args...)		\
	debug_cond(DEBUG_BOOTKEYS, fmt, ##args)

/* Stored value of bootdelay, used by autoboot_command() */
static int stored_bootdelay;

#if defined(CONFIG_AUTOBOOT_KEYED)
#if defined(CONFIG_AUTOBOOT_STOP_STR_SHA256)

/*
 * Use a "constant-length" time compare function for this
 * hash compare:
 *
 * https://crackstation.net/hashing-security.htm
 */
static int slow_equals(u8 *a, u8 *b, int len)
{
	int diff = 0;
	int i;

	for (i = 0; i < len; i++)
		diff |= a[i] ^ b[i];

	return diff == 0;
}

static int passwd_abort(uint64_t etime)
{
	const char *sha_env_str = getenv("bootstopkeysha256");
	u8 sha_env[SHA256_SUM_LEN];
	u8 sha[SHA256_SUM_LEN];
	char presskey[MAX_DELAY_STOP_STR];
	const char *algo_name = "sha256";
	u_int presskey_len = 0;
	int abort = 0;
	int size;
	int ret;

	if (sha_env_str == NULL)
		sha_env_str = CONFIG_AUTOBOOT_STOP_STR_SHA256;

	/*
	 * Generate the binary value from the environment hash value
	 * so that we can compare this value with the computed hash
	 * from the user input
	 */
	ret = hash_parse_string(algo_name, sha_env_str, sha_env);
	if (ret) {
		printf("Hash %s not supported!\n", algo_name);
		return 0;
	}

	/*
	 * We don't know how long the stop-string is, so we need to
	 * generate the sha256 hash upon each input character and
	 * compare the value with the one saved in the environment
	 */
	do {
		if (tstc()) {
			/* Check for input string overflow */
			if (presskey_len >= MAX_DELAY_STOP_STR)
				return 0;

			presskey[presskey_len++] = getc();

			/* Calculate sha256 upon each new char */
			hash_block(algo_name, (const void *)presskey,
				   presskey_len, sha, &size);

			/* And check if sha matches saved value in env */
			if (slow_equals(sha, sha_env, SHA256_SUM_LEN))
				abort = 1;
		}
	} while (!abort && get_ticks() <= etime);

	return abort;
}
#else
static int passwd_abort(uint64_t etime)
{
	int abort = 0;
	struct {
		char *str;
		u_int len;
		int retry;
	}
	delaykey[] = {
		{ .str = getenv("bootdelaykey"),  .retry = 1 },
		{ .str = getenv("bootstopkey"),   .retry = 0 },
	};

	char presskey[MAX_DELAY_STOP_STR];
	u_int presskey_len = 0;
	u_int presskey_max = 0;
	u_int i;

#  ifdef CONFIG_AUTOBOOT_DELAY_STR
	if (delaykey[0].str == NULL)
		delaykey[0].str = CONFIG_AUTOBOOT_DELAY_STR;
#  endif
#  ifdef CONFIG_AUTOBOOT_STOP_STR
	if (delaykey[1].str == NULL)
		delaykey[1].str = CONFIG_AUTOBOOT_STOP_STR;
#  endif

	for (i = 0; i < sizeof(delaykey) / sizeof(delaykey[0]); i++) {
		delaykey[i].len = delaykey[i].str == NULL ?
				    0 : strlen(delaykey[i].str);
		delaykey[i].len = delaykey[i].len > MAX_DELAY_STOP_STR ?
				    MAX_DELAY_STOP_STR : delaykey[i].len;

		presskey_max = presskey_max > delaykey[i].len ?
				    presskey_max : delaykey[i].len;

		debug_bootkeys("%s key:<%s>\n",
			       delaykey[i].retry ? "delay" : "stop",
			       delaykey[i].str ? delaykey[i].str : "NULL");
	}

	/* In order to keep up with incoming data, check timeout only
	 * when catch up.
	 */
	do {
		if (tstc()) {
			if (presskey_len < presskey_max) {
				presskey[presskey_len++] = getc();
			} else {
				for (i = 0; i < presskey_max - 1; i++)
					presskey[i] = presskey[i + 1];

				presskey[i] = getc();
			}
		}

		for (i = 0; i < sizeof(delaykey) / sizeof(delaykey[0]); i++) {
			if (delaykey[i].len > 0 &&
			    presskey_len >= delaykey[i].len &&
				memcmp(presskey + presskey_len -
					delaykey[i].len, delaykey[i].str,
					delaykey[i].len) == 0) {
					debug_bootkeys("got %skey\n",
						delaykey[i].retry ? "delay" :
						"stop");

				/* don't retry auto boot */
				if (!delaykey[i].retry)
					bootretry_dont_retry();
				abort = 1;
			}
		}
	} while (!abort && get_ticks() <= etime);

	return abort;
}
#endif

/***************************************************************************
 * Watch for 'delay' seconds for autoboot stop or autoboot delay string.
 * returns: 0 -  no key string, allow autoboot 1 - got key string, abort
 */
static int __abortboot(int bootdelay)
{
	int abort;
	uint64_t etime = endtick(bootdelay);

#  ifdef CONFIG_AUTOBOOT_PROMPT
	/*
	 * CONFIG_AUTOBOOT_PROMPT includes the %d for all boards.
	 * To print the bootdelay value upon bootup.
	 */
	printf(CONFIG_AUTOBOOT_PROMPT, bootdelay);
#  endif

	abort = passwd_abort(etime);
	if (!abort)
		debug_bootkeys("key timeout\n");

	return abort;
}

# else	/* !defined(CONFIG_AUTOBOOT_KEYED) */

#ifdef CONFIG_MENUKEY
static int menukey;
#endif

#ifdef CONFIG_TARGET_BITMAIN_BM1682_HDS
#define CLI_HOT_KEY 'j'
#endif

static int __abortboot(int bootdelay)
{
	int abort = 0;
	unsigned long ts;

#ifdef CONFIG_MENUPROMPT
	printf(CONFIG_MENUPROMPT);
#else
	printf("Hit any key to stop autoboot: %2d ", bootdelay);
#endif

	/*
	 * Check if key already pressed
	 */
	if (tstc()) {	/* we got a key press	*/
#ifdef CLI_HOT_KEY
		if (getc() == CLI_HOT_KEY) {	/* consume input	*/
			puts("\b\b\b 0");
			abort = 1;	/* don't auto boot	*/
		}
#else
		(void) getc();  /* consume input	*/
		puts("\b\b\b 0");
		abort = 1;	/* don't auto boot	*/
#endif
	}

	while ((bootdelay > 0) && (!abort)) {
		--bootdelay;
		/* delay 1000 ms */
		ts = get_timer(0);
		do {
			if (tstc()) {	/* we got a key press	*/
#ifdef CLI_HOT_KEY
				if (getc() == CLI_HOT_KEY) {	/* consume input	*/
					abort  = 1;	/* don't auto boot	*/
					bootdelay = 0;	/* no more delay	*/
					break;
				}
#else
				abort  = 1;	/* don't auto boot	*/
				bootdelay = 0;	/* no more delay	*/
# ifdef CONFIG_MENUKEY
				menukey = getc();
# else
				(void) getc();  /* consume input	*/
# endif
				break;
#endif
			}
			udelay(10000);
		} while (!abort && get_timer(ts) < 1000);

		printf("\b\b\b%2d ", bootdelay);
	}

	putc('\n');

	return abort;
}
# endif	/* CONFIG_AUTOBOOT_KEYED */

__weak int board_abortboot(int bootdelay)
{
	return false;
}

static int abortboot(int bootdelay)
{
	int abort = board_abortboot(bootdelay);

	if (bootdelay >= 0 && !abort)
		abort = __abortboot(bootdelay);

#ifdef CONFIG_SILENT_CONSOLE
	if (abort)
		gd->flags &= ~GD_FLG_SILENT;
#endif

	return abort;
}

static void process_fdt_options(const void *blob)
{
#if defined(CONFIG_OF_CONTROL) && defined(CONFIG_SYS_TEXT_BASE)
	ulong addr;

	/* Add an env variable to point to a kernel payload, if available */
	addr = fdtdec_get_config_int(gd->fdt_blob, "kernel-offset", 0);
	if (addr)
		setenv_addr("kernaddr", (void *)(CONFIG_SYS_TEXT_BASE + addr));

	/* Add an env variable to point to a root disk, if available */
	addr = fdtdec_get_config_int(gd->fdt_blob, "rootdisk-offset", 0);
	if (addr)
		setenv_addr("rootaddr", (void *)(CONFIG_SYS_TEXT_BASE + addr));
#endif /* CONFIG_OF_CONTROL && CONFIG_SYS_TEXT_BASE */
}

const char *bootdelay_process(void)
{
	char *s;
	int bootdelay;
#ifdef CONFIG_BOOTCOUNT_LIMIT
	unsigned long bootcount = 0;
	unsigned long bootlimit = 0;
#endif /* CONFIG_BOOTCOUNT_LIMIT */

#ifdef CONFIG_BOOTCOUNT_LIMIT
	bootcount = bootcount_load();
	bootcount++;
	bootcount_store(bootcount);
	setenv_ulong("bootcount", bootcount);
	bootlimit = getenv_ulong("bootlimit", 10, 0);
#endif /* CONFIG_BOOTCOUNT_LIMIT */

	s = getenv("bootdelay");
	bootdelay = s ? (int)simple_strtol(s, NULL, 10) : CONFIG_BOOTDELAY;

#ifdef CONFIG_OF_CONTROL
	bootdelay = fdtdec_get_config_int(gd->fdt_blob, "bootdelay",
			bootdelay);
#endif

	debug("### main_loop entered: bootdelay=%d\n\n", bootdelay);

#if defined(CONFIG_MENU_SHOW)
	bootdelay = menu_show(bootdelay);
#endif
	bootretry_init_cmd_timeout();

#ifdef CONFIG_POST
	if (gd->flags & GD_FLG_POSTFAIL) {
		s = getenv("failbootcmd");
	} else
#endif /* CONFIG_POST */
#ifdef CONFIG_BOOTCOUNT_LIMIT
	if (bootlimit && (bootcount > bootlimit)) {
		printf("Warning: Bootlimit (%u) exceeded. Using altbootcmd.\n",
		       (unsigned)bootlimit);
		s = getenv("altbootcmd");
	} else
#endif /* CONFIG_BOOTCOUNT_LIMIT */
		s = getenv("bootcmd");

	process_fdt_options(gd->fdt_blob);
	stored_bootdelay = bootdelay;

	return s;
}

#ifdef CONFIG_TARGET_BITMAIN_BM1682_HDS
static int uart_recv(void)
{
#define UART_BUFF_SIZE 256
#define CMD_STR_LEN 8
#define DATE_STR_LENGTH 8
#define UART_RECV_WINDOW 1000 // ms

	char buffer[UART_BUFF_SIZE + 1]; // the extra char is just for safety
	char date_str[DATE_STR_LENGTH + 1]; // the extra char is for '\0'
	unsigned int count = 0, hit_pos, hit_start = 0, date_hex;
	int i;
	char *hit, pr;
	unsigned long ts;
	/*
	 * all commands must have 8 chars, leading by a '!',
	 * and its argument should be surrounded by '[]'.
	 * all characters should be in capital form.
	 */
	const char *cmd_recovery = "recovery";
	const char *cmd_date = "DATETIME";

	memset(buffer, 0, sizeof(buffer));
	memset(date_str, 0, sizeof(date_str));
	ts = get_timer(0);

	printf("waiting strings on UART\n");
	while (count < UART_BUFF_SIZE && get_timer(ts) < UART_RECV_WINDOW) {
		if (tstc())
			buffer[count++] = getc();
	}

	if (count < CMD_STR_LEN + 1) {
		printf("no string received\n");
		return 0;
	}
	printf("received %d:", count);
	for (i = 0; i < UART_BUFF_SIZE; i++) {
		if (i % 64 == 0)
			putc('\n');
		pr = *(buffer + i);
		if ((pr >= 'A' && pr <= 'Z') ||
		    (pr >= 'a' && pr <= 'z') ||
		    (pr >= '0' && pr <= '9') ||
		    pr == '!' || pr == '[' || pr == ']')
			putc(pr);
		else
			putc(' ');
	}
	putc('\n');
next:
	hit = NULL; // can't use strchr here as there are several strings in buffer
	for (i = hit_start; i <= count - CMD_STR_LEN - 1; i++) {
		pr = *(buffer + i);
		if (pr == '!') {
			hit = buffer + i;
			break;
		}
	}
	if (!hit) {
		printf("no valid hit in string\n");
		return 0;
	}
	hit_pos = (unsigned int)(hit - buffer);

	if (strncmp(hit + 1, cmd_recovery, strlen(cmd_recovery)) == 0) {
		printf("enter recovery mode\n");
		run_command_list("run recovery", -1, 0);
		return 1;
	} else if (strncmp(hit + 1, cmd_date, strlen(cmd_date)) == 0 &&
		   hit_pos + 1 + CMD_STR_LEN + 1 + DATE_STR_LENGTH < count &&
		   *(hit + 1 + CMD_STR_LEN) == '[' &&
		   *(hit + 1 + CMD_STR_LEN + 1 + DATE_STR_LENGTH) == ']') {
		// get argument, skip '!', command str, and '[']
		strncpy(date_str, hit + 1 + CMD_STR_LEN + 1, DATE_STR_LENGTH);
		date_hex = simple_strtoul(date_str, NULL, 16);
		printf("date string %s @ %d: 0x%x\n", date_str, hit_pos, date_hex);
		writel(date_hex & 0xFFFFFFFF, 0x50008240);
		return 1;
	} else if (hit_start + CMD_STR_LEN + 1 <= count) {
		hit_start++;
		goto next;
	}

	printf("no valid cmd in string\n");
	return 0;
}
#endif

void autoboot_command(const char *s)
{
	debug("### main_loop: bootcmd=\"%s\"\n", s ? s : "<UNDEFINED>");

#ifdef CONFIG_TARGET_BITMAIN_BM1682_HDS
	uart_recv();
#endif

	if (stored_bootdelay != -1 && s && !abortboot(stored_bootdelay)) {
#if defined(CONFIG_AUTOBOOT_KEYED) && !defined(CONFIG_AUTOBOOT_KEYED_CTRLC)
		int prev = disable_ctrlc(1);	/* disable Control C checking */
#endif

		run_command_list(s, -1, 0);

#if defined(CONFIG_AUTOBOOT_KEYED) && !defined(CONFIG_AUTOBOOT_KEYED_CTRLC)
		disable_ctrlc(prev);	/* restore Control C checking */
#endif
	}

#ifdef CONFIG_MENUKEY
	if (menukey == CONFIG_MENUKEY) {
		s = getenv("menucmd");
		if (s)
			run_command_list(s, -1, 0);
	}
#endif /* CONFIG_MENUKEY */
}
