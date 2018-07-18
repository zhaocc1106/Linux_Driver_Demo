#include<unistd.h>
#include<stdio.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<errno.h>
#include<signal.h>

static void signal_handle(int signal)
{
	printf("the signal is %d\n", signal);
}

void main(void)
{
	int fd, oflags;
	fd = open("/dev/globalmem", O_RDWR, S_IRUSR | S_IWUSR);
	if (fd != -1) {
		signal(SIGIO, signal_handle);
		fcntl(fd, F_SETOWN, getpid());
		oflags = fcntl(fd, F_GETFL);
		fcntl(fd, F_SETFL, oflags | FASYNC);
		while (1) {
			sleep(100);
		}
	} else {
		perror("open failed");
	}
}
