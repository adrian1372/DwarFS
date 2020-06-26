#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>

/*
 * This program tests the latency of write() for tiny writes, for the purpose of removing the added
 * latency of write() from the latency of fsync() in the fsync tests.
 */

int main (void) {
	const char *path = "/mnt/xfs/file";
	int fd[500000];
	int i;
	struct timeval starttime, endtime;
	unsigned long long starttimeus, endtimeus;
	struct stat statbuf;

	for(i = 0; i < 500000; i++) {
		fd[i] = open(path, O_RDWR);
	}
	for(i = 0; i < 500000; i++) {
		if(fd[i] < 0) {
			printf("Encountered error on fd %d\n", fd[i]);
			return -1;
		}
	}

	gettimeofday(&starttime, NULL);
	for(i = 0; i < 500000; i++) {
		write(fd[i], "l", 2);
	}
	gettimeofday(&endtime, NULL);

	starttimeus = 1000000*starttime.tv_sec + starttime.tv_usec;
	endtimeus = 1000000*endtime.tv_sec + endtime.tv_usec;
	printf("%f\n", (endtimeus - starttimeus) / 1000000.0f);
	
	for(i = 0; i < 500000; i++) {
		close(fd[i]);
	}

	return 0;
}
