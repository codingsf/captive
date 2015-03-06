#include <define.h>
#include <verify.h>
#include <string.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

static struct verify_t {
	uint8_t barrier_enter[32];
	uint8_t barrier_exit[32];
	uint8_t data[256];
} *verify;

static int verify_tid;

int verify_prepare(int id)
{
	int shmfd = open("./captive.shm", O_RDWR);
	if (shmfd < 0)
		return -1;

	verify = (struct verify_t *)mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
	close(shmfd);

	if (verify == MAP_FAILED) {
		return -1;
	}

	if (id == 0) {
		bzero(verify, 0x1000);
	}
	
	verify_tid = id;

	return 0;
}

void *verify_get_shared_data()
{
	return verify;
}

int verify_get_tid()
{
	return verify_tid;
}