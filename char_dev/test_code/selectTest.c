#include<stdio.h>
#include<sys/select.h>
#include<fcntl.h>
#include<stdlib.h>
#include<unistd.h>
#include<stddef.h>
#include<string.h>
#include<errno.h>

#define FIFO_CLEAR 0x01
#define BUFFER_LEN 20
int main(void)
{
	int fd, num;
	char rd_ch[BUFFER_LEN];
	fd_set rfds, wfds;

	fd = open("/dev/globalmem", O_RDONLY | O_NONBLOCK);
	
	if (fd != -1) {
		if (ioctl(fd, FIFO_CLEAR, 0) < 0) {
			perror("ioctl failed");
		}

		while (1) {
			FD_ZERO(&rfds);
			FD_ZERO(&wfds);
			FD_SET(fd, &rfds);
			FD_SET(fd, &wfds);
			
			switch (select(fd + 1, &rfds, &wfds, NULL, NULL)) {
			case 0:
			case -1:
				perror("select failed");
				exit(1);
				break;
			default:
				if (FD_ISSET(fd, &rfds)) {
					if (read(fd, rd_ch, BUFFER_LEN) == -1) {
						perror("read failed");
						exit(1);
					}
					printf("read success:%s\n", rd_ch);
				} else if (FD_ISSET(fd, &wfds)) {
				//	printf("can write\n");
				}
			}
		}
	} else {
		perror("open failed\n");
	}
	return 0;
}
