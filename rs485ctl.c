// gcc -Wall -Wextra -Werror -O2 -std=gnu11 -o rs485ctl rs485ctl.c

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <linux/serial.h>
#include <sys/ioctl.h>

static int opt_delay_before_send = -1;
static int opt_delay_after_send = -1;
static __u32 opt_flags_on;
static __u32 opt_flags_off;
static int opt_quiet;

static const char *progname = "rs485ctl";
static const char *progver  = "1.0";

enum {
	OPT_DELAY_BEFORE_SEND = 256,
	OPT_DELAY_AFTER_SEND,
	OPT_RTS_ON_SEND,
	OPT_NO_RTS_ON_SEND,
	OPT_RTS_AFTER_SEND,
	OPT_NO_RTS_AFTER_SEND,
	OPT_RX_DURING_TX,
	OPT_NO_RX_DURING_TX,
};

enum {
	ACTION_ON,
	ACTION_OFF,
	ACTION_SHOW,
};

static const struct option long_options[] = {
	{"delay-before-send", required_argument, 0, OPT_DELAY_BEFORE_SEND },
	{"delay-after-send",  required_argument, 0, OPT_DELAY_AFTER_SEND },
	{"rts-on-send",       optional_argument, 0, OPT_RTS_ON_SEND },
	{"no-rts-on-send",    no_argument,       0, OPT_NO_RTS_ON_SEND },
	{"rts-after-send",    optional_argument, 0, OPT_RTS_AFTER_SEND },
	{"no-rts-after-send", no_argument,       0, OPT_NO_RTS_AFTER_SEND },
	{"rx-during-tx",      optional_argument, 0, OPT_RX_DURING_TX },
	{"no-rx-during-tx",   no_argument,       0, OPT_NO_RX_DURING_TX },
	{"quiet",             no_argument,       0, 'q' },
	{"help",              no_argument,       0, 'h' },
	{"version",           no_argument,       0, 'v' },
	{NULL,                0,                 0,  0 }
};

static void help(void)
{
	printf("\
usage: %s [options] <on|off|show> <device>\n\
\n\
Actions:\n\
	on			Set SER_RS485_ENABLED and other options\n\
	off			Clear SER_RS485_ENABLED\n\
	show			Print current settings\n\
\n\
Options:\n\
	--delay-before-send=<delay>\n\
	--delay-after-send=<delay>\n\
	--rts-on-send[=<0|1>]\n\
	--rts-after-send[=<0|1>]\n\
	--rx-during-tx[=<0|1>]\n\
\n\
	-h, --help		Print this help and exit\n\
	-v, --version		Print version and exit\n\
	-q, --quiet		Do not print effective configuration\n\
\n\
For the flag options, omitting the optional argument is equivalent to\n\
passing 1.  They also have --no- variants, e.g. --no-rts-on-send,\n\
which is equivalent to --rts-on-send=0.\n\
\n\
Note that --rts-on-send and --rts-after-send are mutually exclusive.\n\
So --rts-on-send implies --no-rts-after-send and vice versa. Whichever\n\
option is passed last takes precedence.\n\
\n\
Settings which are not explicitly given are preserved as-is, as returned\n\
by the TIOCGRS485 ioctl.\n\
", progname);
	exit(0);
}
static void version(void)
{
	printf("%s v%s\n", progname, progver);
	exit(0);
}

static int parse_delay(const char *arg, int idx)
{
	char *endptr;
	int delay = strtol(arg, &endptr, 0);
	if (endptr == arg || *endptr || delay < 0 || delay > 100)
		errx(1, "invalid argument to --%s (must be integer in [0, 100])",
			long_options[idx].name);
	return delay;
}

static void set_flag(__u32 flag)
{
	opt_flags_on |= flag;
	opt_flags_off &= ~flag;
}

static void clear_flag(__u32 flag)
{
	opt_flags_off |= flag;
	opt_flags_on &= ~flag;
}

static int parse_flag_optarg(int opt_index)
{
	if (!optarg)
		return 1;
	if (!strcmp(optarg, "0"))
		return 0;
	if (!strcmp(optarg, "1"))
		return 1;
	errx(1, "invalid argument to --%s (must be 0 or 1)", long_options[opt_index].name);
}

static void parse_options(int argc, char *argv[])
{
	int opt, opt_index;

	while (1) {
		opt = getopt_long(argc, argv, "hvq", long_options, &opt_index);

		if (opt == -1)
			break;

		switch (opt) {
		case 'h':
			help();
			break;
		case 'v':
			version();
			break;

		case 'q':
			opt_quiet = 1;
			break;

		case OPT_DELAY_BEFORE_SEND:
			opt_delay_before_send = parse_delay(optarg, opt_index);
			break;
		case OPT_DELAY_AFTER_SEND:
			opt_delay_after_send = parse_delay(optarg, opt_index);
			break;

		case OPT_RTS_ON_SEND:
			if (parse_flag_optarg(opt_index)) {
				set_flag(SER_RS485_RTS_ON_SEND);
				clear_flag(SER_RS485_RTS_AFTER_SEND);
				break;
			}
			/* fall-through */
		case OPT_NO_RTS_ON_SEND:
			set_flag(SER_RS485_RTS_AFTER_SEND);
			clear_flag(SER_RS485_RTS_ON_SEND);
			break;
			
		case OPT_RTS_AFTER_SEND:
			if (parse_flag_optarg(opt_index)) {
				set_flag(SER_RS485_RTS_AFTER_SEND);
				clear_flag(SER_RS485_RTS_ON_SEND);
				break;
			}
			/* fall-through */
		case OPT_NO_RTS_AFTER_SEND:
			set_flag(SER_RS485_RTS_ON_SEND);
			clear_flag(SER_RS485_RTS_AFTER_SEND);
			break;

		case OPT_RX_DURING_TX:
			if (parse_flag_optarg(opt_index)) {
				set_flag(SER_RS485_RX_DURING_TX);
				break;
			}
			/* fall-through */
		case OPT_NO_RX_DURING_TX:
			clear_flag(SER_RS485_RX_DURING_TX);
			break;
			
		case '?':
			exit(1);
		}
	}
}

static void show_rs485(const char *dev, const struct serial_rs485 *conf)
{
	printf("%s: rs485 %s\n", dev, (conf->flags & SER_RS485_ENABLED) ? "on" : "off");

	if (!(conf->flags & SER_RS485_ENABLED))
		return;

	printf("delay-before-send: %d\n", conf->delay_rts_before_send);
	printf("delay-after-send: %d\n",  conf->delay_rts_after_send);
	printf("rts-on-send: %d\n",       !!(conf->flags & SER_RS485_RTS_ON_SEND));
	printf("rts-after-send: %d\n",    !!(conf->flags & SER_RS485_RTS_AFTER_SEND));
	printf("rx-during-tx: %d\n",      !!(conf->flags & SER_RS485_RX_DURING_TX));
}


int main(int argc, char *argv[])
{
	int fd, ret, action = ACTION_SHOW;
	const char *device;
	struct serial_rs485 conf, wanted_conf;
	bool rts_on_send_hack = false;
	
	parse_options(argc, argv);
	argc -= optind;
	argv += optind;
	assert(!(opt_flags_on & opt_flags_off));

	if (!argc)
		errx(1, "missing device");
	if (argc > 2)
		errx(1, "too many positional arguments");

	device = argv[argc-1];
	if (argc == 2) {
		if (!strcmp(argv[0], "on"))
			action = ACTION_ON;
		else if (!strcmp(argv[0], "off"))
			action = ACTION_OFF;
		else if (!strcmp(argv[0], "show"))
			action = ACTION_SHOW;
		else
			errx(1, "invalid action %s", argv[0]);
	}

	/*
	 * Regardless of what action we should take, first order of
	 * business is of course to open the device, and then to read
	 * the current settings.
	 */
	fd = open(device, O_RDWR | O_NOCTTY | O_CLOEXEC | O_NONBLOCK);
	if (fd < 0)
		err(1, "cannot open %s", device);
	memset(&conf, 0, sizeof(conf));
	ret = ioctl(fd, TIOCGRS485, &conf);
	if (ret < 0)
		err(1, "cannot get rs485 configuration for %s", device);

	if (action == ACTION_ON) {
		if (opt_delay_before_send >= 0)
			conf.delay_rts_before_send = opt_delay_before_send;
		if (opt_delay_after_send >= 0)
			conf.delay_rts_after_send = opt_delay_after_send;
		conf.flags &= ~opt_flags_off;
		conf.flags |= opt_flags_on;
		conf.flags |= SER_RS485_ENABLED;

		/*
		 * Hack: If the driver has at least one of the SER_RS485_RTS_*_SEND flags in its
		 * rs485_supported.flags, but the conf.flags passed to TIOCSRS485 have neither (#),
		 * the kernel will issue a warning and implicitly set SER_RS485_RTS_ON_SEND. See
		 * uart_sanitize_serial_rs485() in serial_core.c.
		 *
		 * In order to avoid that warning and make this DTRT/DWIM, we proactively set
		 * SER_RS485_RTS_ON_SEND, and if the driver then doesn't support either flag, we
		 * just ignore the lack of that bit in the returned effective configuration.
		 * 
		 * (#) This is the case when 
		 *
		 * - the user didn't pass any of the relevant flags, and
		 *
		 * - the current configuration read via TIOCGRS485 also doesn't have any of those
		 *
		 * and the latter shouldn't happen when rs485 was already enabled (because of this
		 * very same logic), but if rs485 was disabled, the configuration read from the
		 * kernel is all zeroes.
		 */
		if (!(conf.flags & (SER_RS485_RTS_ON_SEND | SER_RS485_RTS_AFTER_SEND))) {
			conf.flags |= SER_RS485_RTS_ON_SEND;
			rts_on_send_hack = true;
		}
	} else if (action == ACTION_OFF) {
		memset(&conf, 0, sizeof(conf));
	}

	if (action != ACTION_SHOW) {
		memcpy(&wanted_conf, &conf, sizeof(conf));
		ret = ioctl(fd, TIOCSRS485, &conf);
		if (ret)
			err(1, "cannot set rs485 configuration for %s", device);
		/*
		 * Sanity check: Did the kernel actually apply all the settings, except perhaps for
		 * the RTS_ON_SEND we added implicitly?
		 */
		if (memcmp(&wanted_conf, &conf, sizeof(conf)) && rts_on_send_hack)
			wanted_conf.flags &= ~SER_RS485_RTS_ON_SEND;
		if (memcmp(&wanted_conf, &conf, sizeof(conf)))
			warnx("not all settings applied by the kernel");
	}
	
	if (action == ACTION_SHOW || !opt_quiet)
		show_rs485(device, &conf);

	return 0;
}
