#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/sysctl.h>
#include <sys/stat.h>

#define PROGRAM_NAME    "freebsd-info"
#define VERSION         "@@#REVISION#@@"

/* Constants */
enum {
	OPT_OSTYPE     = 1,
	OPT_HOSTNAME   = 2,
	OPT_HOSTUUID   = 3,
	OPT_RKERNEL    = 4,
	OPT_IKERNEL    = 5,
	OPT_ARCH       = 6,
	OPT_MODEL      = 7,
	OPT_LOADAVG    = 8,
	OPT_PHYSMEM    = 9,
	OPT_USERMEM    = 10,
	OPT_REALMEM    = 11,
	OPT_SINCEBOOT  = 12,
	OPT_HELP       = 'h',
};

static char *retrive_kernel_version(void)
{
	FILE *fp;
	int c;
	char *p;

	fp = fopen("/boot/kernel/kernel", "rb");
	if (fp == NULL)
		/*
		  If kernel isn't located where it should be
		  (e.g. reason: a different varient of FreeBSD)
		  we can assume, this value is unknown for us.
		*/
	        return strdup("Unknown");

	p = calloc(128, sizeof(char));
	if (p == NULL) {
		fclose(fp);
		perror("calloc()");
	        return NULL;
	}

	/* Start looking for string starts with "@(#)". */
start_loop:
        if (getc_unlocked(fp) != '@')
		goto start_loop;
	if (getc_unlocked(fp) != '(')
		goto start_loop;
	if (getc_unlocked(fp) != '#')
		goto start_loop;
	if (getc_unlocked(fp) != ')')
		goto start_loop;

	while ((c = getc_unlocked(fp)) != EOF && c != ' ')
		;

        while ((c = getc_unlocked(fp)) != EOF && c != ' ')
	        memcpy(p + strlen(p), (const char *)&c, 1);

	fclose(fp);
        return p;
}

static char *collect_boot_timespan(void)
{
	int mib[2];
	long upt, days, hrs, mins;
	struct timeval tv;
	size_t tv_sz;
	time_t tm;
	char *p;

	mib[0] = CTL_KERN;
	mib[1] = KERN_BOOTTIME;
	tv_sz = sizeof(tv);

	/* Retrive the system boot time */
	if (sysctl(mib, 2, &tv, &tv_sz, NULL, 0) == -1) {
		perror("sysctl()");
		abort();
	}

	tm = time(NULL);
	if (tm == (time_t)-1)
		/* No worth of calculations */
		abort();

	upt = (long)(tm - tv.tv_sec + 30);
	days = upt / (60 * 60 * 24);
	upt %= 60 * 60 * 24;
	hrs = upt / (60 * 60);
	upt %= (60 * 60);
	mins = upt / 60;

	p = calloc(128, sizeof(char));
	if (p == NULL)
	        return NULL;

	snprintf(p, (size_t)128, "%ldd, %ldh, %ldm", days, hrs, mins);
	return p;
}

static char *collect_string_ctl_info(int id0, int id1)
{
	int mib[2];
	size_t sz;
	char *p;

	mib[0] = id0;
	mib[1] = id1;

	if (sysctl(mib, 2, NULL, &sz, NULL, 0) == -1) {
		perror("sysctl()");
		return NULL;
	}

	p = calloc(sz, sizeof(char));
	if (p == NULL) {
		perror("calloc()");
		return NULL;
	}

	if (sysctl(mib, 2, p, &sz, NULL, 0) == -1) {
		perror("sysctl()");
	        return NULL;
	}

	return p;
}

static uint64_t collect_number_ctl_info(int id0, int id1)
{
	int mib[2];
	size_t sz;
        uint64_t ret;

	mib[0] = id0;
	mib[1] = id1;

	sz = sizeof(ret);
	if (sysctl(mib, 2, &ret, &sz, NULL, 0) == -1) {
		perror("sysctl()");
		return (uint64_t)-1;
	}

	return ret;
}

static void print_ctl_string(int id0, int id1)
{
	char *str;

	str = collect_string_ctl_info(id0, id1);
	if (str == NULL)
		abort();

	fprintf(stdout, "%s\n", str);
	free(str);
}

static void print_ctl_number(int id0, int id1)
{
        uint64_t val;

	val = collect_number_ctl_info(id0, id1);
	if (val == (uint64_t)-1)
		abort();

        fprintf(stdout, "%lu\n", val);
}

static void print_installed_kernel(void)
{
	char *str;

	str = retrive_kernel_version();
	if (str == NULL)
		abort();

	fprintf(stdout, "%s\n", str);
	free(str);
}

static void print_system_loadavg(void)
{
	double loadavg[3];

	if (getloadavg(loadavg, 3) == -1)
		abort();

	fprintf(stdout,
		"%.2f %.2f %.2f\n",
		loadavg[0], loadavg[1], loadavg[2]);
}

static void print_boot_timespan(void)
{
        char *str;

        str = collect_boot_timespan();
	if (str == NULL)
		abort();

	fprintf(stdout, "%s\n", str);
	free(str);
}

static void print_all(void)
{
	/* OS */
	fputs("OS: ", stdout);
        print_ctl_string(CTL_KERN, KERN_OSTYPE);

	/* Hostname */
	fputs("Hostname: ", stdout);
	print_ctl_string(CTL_KERN, KERN_HOSTNAME);

	/* HostUUID */
	fputs("HostUUID: ", stdout);
	print_ctl_string(CTL_KERN, KERN_HOSTUUID);

	/* Running kernel */
	fputs("Running: ", stdout);
        print_ctl_string(CTL_KERN, KERN_OSRELEASE);

	/* Installed kernel */
	fputs("Installed: ", stdout);
	print_installed_kernel();

	/* Architecture */
	fputs("Architecture: ", stdout);
	print_ctl_string(CTL_HW, HW_MACHINE);

	/* Model */
	fputs("Model: ", stdout);
	print_ctl_string(CTL_HW, HW_MODEL);

	/* Loadavg */
	fputs("Loadavg: ", stdout);
	print_system_loadavg();

	/* Physical memory */
	fputs("Physmem: ", stdout);
        print_ctl_number(CTL_HW, HW_PHYSMEM);

	/* User memory */
	fputs("Usermem: ", stdout);
	print_ctl_number(CTL_HW, HW_USERMEM);

	/* Real memory */
	fputs("Realmem: ", stdout);
	print_ctl_number(CTL_HW, HW_REALMEM);

	/* Since booted */
	fputs("Since: ", stdout);
        print_boot_timespan();
}

static void usage(void)
{
	fputs(
		"Usage: "PROGRAM_NAME" [OPTION]\n\n"
		"Options:\n"
		"    --ostype\tType of the OS (FreeBSD)\n"
		"    --hostname\tComputer hostname\n"
		"    --hostuuid\tDefault installation UUID\n"
		"    --rkernel\tRunning kernel version\n"
		"    --ikernel\tInstalled/updated kernel version\n"
		"    --arch\tCPU architecture\n"
		"    --model\tBoard model\n"
		"    --loadavg\tHost system average load\n"
		"    --physmem\tAvailable physical memory\n"
		"    --usermem\tUnwired memory\n"
		"    --realmem\tTotal addressable memory\n"
		"    --sinceboot\tTime spent since the last boot\n"
		"    --help\tThis menu\n"
		, stdout);
}

int main(int argc, char **argv)
{
	int opt;
        struct option lopts[] = {
		{ "ostype",    no_argument, NULL, OPT_OSTYPE },
		{ "hostname",  no_argument, NULL, OPT_HOSTNAME },
		{ "hostuuid",  no_argument, NULL, OPT_HOSTUUID },
		{ "rkernel",   no_argument, NULL, OPT_RKERNEL },
		{ "ikernel",   no_argument, NULL, OPT_IKERNEL },
		{ "arch",      no_argument, NULL, OPT_ARCH },
		{ "model",     no_argument, NULL, OPT_MODEL },
		{ "loadavg",   no_argument, NULL, OPT_LOADAVG },
		{ "physmem",   no_argument, NULL, OPT_PHYSMEM },
		{ "usermem",   no_argument, NULL, OPT_USERMEM },
		{ "realmem",   no_argument, NULL, OPT_REALMEM },
		{ "sinceboot", no_argument, NULL, OPT_SINCEBOOT },
		{ "help",      no_argument, NULL, OPT_HELP },
		{ NULL,        0,           NULL, 0 },
	};

	if (argc < 2 || argv[1][0] != '-') {
		print_all();
		exit(EXIT_SUCCESS);
	}

	while ((opt = getopt_long(argc, argv, "h", lopts, NULL)) != -1) {
		switch (opt) {
		case OPT_OSTYPE:
			/* sysctl entry: kern.ostype */
			print_ctl_string(CTL_KERN, KERN_OSTYPE);
			break;

		case OPT_HOSTNAME:
			/* sysctl entry: kern.hostname */
			print_ctl_string(CTL_KERN, KERN_HOSTNAME);
			break;

		case OPT_HOSTUUID:
			/* sysctl entry: kern.hostuuid */
		        print_ctl_string(CTL_KERN, KERN_HOSTUUID);
			break;

		case OPT_RKERNEL:
			/* sysctl entry: kern.osrelease */
		        print_ctl_string(CTL_KERN, KERN_OSRELEASE);
			break;

		case OPT_IKERNEL:
		        print_installed_kernel();
			break;

		case OPT_ARCH:
			/* sysctl entry: hw.machine_arch */
		        print_ctl_string(CTL_HW, HW_MACHINE_ARCH);
			break;

		case OPT_MODEL:
			/* sysctl entry: hw.model */
		        print_ctl_string(CTL_HW, HW_MODEL);
			break;

		case OPT_LOADAVG:
		        print_system_loadavg();
			break;

		case OPT_PHYSMEM:
			/* sysctl entry: hw.physmem */
			print_ctl_number(CTL_HW, HW_PHYSMEM);
			break;

		case OPT_USERMEM:
			/* sysctl entry: hw.usermem */
			print_ctl_number(CTL_HW, HW_USERMEM);
			break;

		case OPT_REALMEM:
			/* sysctl entry: kern.realmem */
			print_ctl_number(CTL_HW, HW_REALMEM);
			break;

		case OPT_SINCEBOOT:
		        print_boot_timespan();
			break;

		case OPT_HELP:
			usage();
			exit(EXIT_SUCCESS);

		default:
		        exit(EXIT_FAILURE);
		}
	}

	/* Program has ended */
	exit(EXIT_SUCCESS);
}
