/* ${CROSS_COMPILE}gcc -O2 -Wall -Wextra -Werror -o md md.c */
#define _GNU_SOURCE
#include <assert.h>
#include <error.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#define AZ(x) assert((x) == 0)
#define AN(x) assert((x) != 0)

#define PAGE_SIZE 4096

static int width = 4;
static int wmask;
static int wshift;

struct line {
	union {
		uint8_t  u8[16];
		uint16_t u16[8];
		uint32_t u32[4];
		uint64_t u64[2];
	};
	unsigned long base;
	unsigned long offset;
	unsigned long len;
};

static unsigned long page_addr(unsigned long addr)
{
	return addr & ~(PAGE_SIZE - 1);
}
static unsigned long page_offset(unsigned long addr)
{
	return addr & (PAGE_SIZE - 1);	
}

static void usage(void)
{
	error(1, 0, "usage: md [-b|-h|-w|-l|-q] <addr+length>|addr [length]\n");
}

static void print_line(const struct line *line)
{
	unsigned long idx, len;
	int count = 0;

	AZ(line->base & 15);
	AZ(line->offset & wmask);
	AZ(line->len & wmask);
	
	printf("%08lx:%*s", line->base, (int)((1+2*width)*(line->offset >> wshift)), "");
	for (idx = line->offset >> wshift, len = line->len; len; idx++, len -= width) {
		switch (width) {
		case 1: count += printf(" %02"   PRIx8,  line->u8[idx]); break;
		case 2: count += printf(" %04"  PRIx16, line->u16[idx]); break;
		case 4: count += printf(" %08"  PRIx32, line->u32[idx]); break;
		case 8: count += printf(" %016" PRIx64, line->u64[idx]); break;
		}
	}
	while (count++ < 52)
		putchar(' ');
	for (idx = 0; idx < 16; ++idx) {
		uint8_t b = line->u8[idx];
		putchar(0x20 <= b && b < 0x7f ? b : '.');
	}
	printf("\n");
}

static void dump(unsigned long addr, unsigned long size)
{
	unsigned long cur_page = 0;
	void *map = NULL;
	int fd;

	fd = open("/dev/mem", (O_RDONLY | O_SYNC));
	if (fd < 0)
		error(1, errno, "open(/dev/mem)");
	
	while (size) {
		unsigned long page = page_addr(addr);
		unsigned long len, index;
		struct line line;
		
		if (!map || page != cur_page) {
			if (map)
				munmap(map, PAGE_SIZE);
			map = mmap(NULL, PAGE_SIZE, PROT_READ, MAP_SHARED, fd, page);
			if (map == MAP_FAILED)
				error(1, errno, "mmap");
			cur_page = page;
		}
		memset(&line, 0, sizeof(line));

		AZ(addr & wmask);
		AZ(size & wmask);

		line.base = addr & ~15;
		line.offset = addr & 15;
		len = 16 - line.offset;
		if (len > size)
			len = size;
		AZ(len & wmask);
		line.len = len;
		for (index = line.offset >> wshift; len; len -= width, addr += width, ++index) {
			volatile void *p = map + page_offset(addr);
			switch (width) {
			case 1: line.u8[index] = *(volatile uint8_t*)(p); break;
			case 2: line.u16[index] = *(volatile uint16_t*)(p); break;
			case 4: line.u32[index] = *(volatile uint32_t*)(p); break;
			case 8: line.u64[index] = *(volatile uint64_t*)(p); break;
			}
		}
		size -= line.len;

		print_line(&line);
	}
}

int main(int argc, char *argv[])
{
	unsigned long addr, size;
	char *size_arg, *end;
	int opt;

	while ((opt = getopt(argc, argv, "bhwlq")) != -1) {
		switch (opt) {
		case 'b': width = 1; break;
		case 'h': width = 2; break;
		case 'w': width = 4; break;
		case 'l': width = sizeof(long); break;
		case 'q': width = 8; break;
		default: usage();
		}
	}

	switch(width) {
	case 1: wshift = 0; break;
	case 2: wshift = 1; break;
	case 4: wshift = 2; break;
	case 8: wshift = 3; break;
	default: assert(0);
	}
	wmask = width - 1;
	
	argc -= optind;
	argv += optind;
	if (argc < 1 || argc > 2)
		usage();
	addr = strtoul(argv[0], &end, 0);
	if (end == argv[0])
		usage();
	if (*end == '+')
		size_arg = end+1;
	else if (*end)
		usage();
	else if (argc > 1)
		size_arg = argv[1];
	else
		size_arg = NULL;

	if (size_arg) {
		size = strtoul(size_arg, &end, 0);
		if (end == size_arg || *end)
			usage();
	} else {
		size = width;
	}

	if ((addr & (width - 1)) || (size & (width - 1)))
		error(1, 0, "address and size must be aligned to the selected access width (%d)\n", width);
		
	dump(addr, size);
	
	return 0;
}
