#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

/*
 * This program tests open, stat and close system calls. Each test looks like:
 * 
 * GET_START_TIME
 * RUN TEST
 * GET_END_TIME
 * CALCULATE AVG TIME
 *
 * In order to run only one test (e.g. to make a file with average times for one syscall), comment out
 * the tests you do not want to use. If commenting out open(), only remove the print statements, as the  * other tests cannot run without open fd's.
 */



int main (void) {
	const char *path = "/mnt/xfs/file";
	int fd[500000];
	int i;
	struct timeval starttime, endtime;
	unsigned long long starttimeus, endtimeus;
	struct stat statbuf;

	gettimeofday(&starttime, NULL);
	for(i = 0; i < 500000; i++) {
		fd[i] = open(path, O_RDWR);
	}
	gettimeofday(&endtime, NULL);

	for(i = 0; i < 500000; i++) {
		if(fd[i] < 0) {
			printf("Encountered an error on fd %d: %d\n", i, errno);
			return -1;
		}
	}

	starttimeus = 1000000*starttime.tv_sec + starttime.tv_usec;
	endtimeus = 1000000*endtime.tv_sec + endtime.tv_usec;
	printf("%f\n", (endtimeus - starttimeus) / 500000.0f);

	gettimeofday(&starttime, NULL);
	for(i = 0; i < 500000; i++) {
		fstat(fd[i], &statbuf);
	}
	gettimeofday(&endtime, NULL);

	starttimeus = 1000000*starttime.tv_sec + starttime.tv_usec;
	endtimeus = 1000000*endtime.tv_sec + endtime.tv_usec;
	printf("Got stat of %d files in %lld microseconds.\n", i, endtimeus - starttimeus);
	printf("%f\n", (endtimeus - starttimeus) / 500000.0f);

	gettimeofday(&starttime, NULL);
	for(i = 0; i < 500000; i++) {
		close(fd[i]);
	}
	gettimeofday(&endtime, NULL);

	starttimeus = 1000000*starttime.tv_sec + starttime.tv_usec;
	endtimeus = 1000000*endtime.tv_sec + endtime.tv_usec;
	printf("Closed %d files in %lld microseconds.\n", i, endtimeus - starttimeus);
	printf("%f\n", (endtimeus - starttimeus) / 500000.0f);

	return 0;
}
