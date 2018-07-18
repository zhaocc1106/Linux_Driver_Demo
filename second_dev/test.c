#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>

void main(void)
{
	int fd;
	int counter = 0;
	int old_counter = 0;

	fd = open("/dev/second", O_RDONLY);

	if (fd != -1) {
		while (1) { 
			read(fd, &counter, sizeof(signed int));
			if (counter != old_counter) {
				printf("counter:%d\n", counter);
				old_counter = counter;
			}
		}
	} else {
		perror("open failed");
		exit(1);
	}
}
