#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>

#define DATA_SIZE 4096
int main(int argc, char **argv)
{
	printf("dev: %s\n", argv[1]);
	/**
	 * 1: string to hex
	 * uint64_t offset = strtol(argv[2], NULL, 16);
	 */

	int fd = open(argv[1], O_RDWR | O_DIRECT);
	if(fd <= 0) {
		printf("open failed\n");
		return -1;
	}

	uint8_t *data = aligned_alloc(512, DATA_SIZE);
	uint64_t offset;
	while(1) {
		/**
         * 2: string to hex
         * scanf
         */
		scanf("%lx", &offset);
		printf("offset: 0x%016lx\n", offset);
		ssize_t rlt = pread(fd, data, DATA_SIZE, offset);
		printf("read rlt = %ld\n", rlt);
	}

	close(fd);
	free(data);
	return 0;
}
