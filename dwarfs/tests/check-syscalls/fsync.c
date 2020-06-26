#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>

/* 
 * This program runs fsync tests. Note that, in order to ensure that the buffers are dirty, tiny writes
 * are made before calling fsync. The write() latency can be tested with write.c, and should be
 * removed from the latency of fsync().
 */


int main (void) {
	const char *path = "/mnt/dwarfs/file";
	int fd[500000];
	int i;
	struct timeval starttime, endtime;
	unsigned long long starttimeus, endtimeus;
	struct stat statbuf;

	for(i = 0; i < 100000; i++) {
		fd[i] = open(path, O_RDWR);
	}

	for(i = 0; i < 100000; i++) {
		if(fd[i] < 0) {
			printf("Invalid fd at %d\n", i);
			return fd[i];
		}
	}

	printf("Starting fsync loop\n");
	gettimeofday(&starttime, NULL);
	for(i = 0; i < 100000; i++) {
		write(fd[i], "l", 2);
		fsync(fd[i]);
	}
	gettimeofday(&endtime, NULL);

	starttimeus = 1000000*starttime.tv_sec + starttime.tv_usec;
	endtimeus = 1000000*endtime.tv_sec + endtime.tv_usec;
	printf("Wrote and synced %d file descriptors in %lld microseconds.\n", i, endtimeus - starttimeus);
	printf("%f\n", (endtimeus - starttimeus) / 100000.0f);
	
	for(i = 0; i < 100000; i++) {
		close(fd[i]);
	}

	return 0;
}
