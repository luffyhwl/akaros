#include <stdio.h> 
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arch/arch.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <ros/syscall.h>
#include <virtio.h>

unsigned long long stack[1024];
volatile int shared = 0;
int mcp = 1;
#define V(x, t) (*((volatile t*)(x)))
// NOTE: p is both our virtual and guest physical.
void *p;

struct virtqueue *head, *consin, *consout;
pthread_t *my_threads;
void **my_retvals;
int nr_threads = 2;
	char line[128], consline[128], outline[128];
	struct scatterlist iov[32];
	unsigned int inlen, outlen, conslen;
	/* unlike Linux, this shared struct is for both host and guest. */
//	struct virtqueue *constoguest = 
//		vring_new_virtqueue(0, 512, 8192, 0, inpages, NULL, NULL, "test");
struct virtqueue *guesttocons;
	struct scatterlist out[] = { {outline, sizeof(outline)}, };
	struct scatterlist in[] = { {line, sizeof(line)}, };
	int iter = 1;



static void *fail(void *arg)
{

		uint16_t head;
		int i, ret;
		for(i = 0; i < 8; i++) {
			/* guest: make a line available to host */
			ret = virtqueue_add_inbuf_avail(guesttocons, in, 1, line, 0);
			
			add_used(guesttocons, head, outlen+inlen);
			/* guest code. Get all your buffers back */
			char *cp;
			while ((cp = virtqueue_get_buf_used(guesttocons, &conslen))) {
				if (cp != line)
					continue;
				//fprintf(stderr, "guest: from host: %s\n", cp);
				/* guest: push some buffers into the channel for the host to use */
				sprintf(outline, "guest: outline %d:%s:\n", iter, line);
				ret = virtqueue_add_outbuf_avail(guesttocons, out, 1, outline, 0);
			}
		}

	__asm__ __volatile__("vmcall\n");
	__asm__ __volatile__("mov $0xdeadbeef, %rbx; mov 5, %rax\n");
}

unsigned long long *p512, *p1, *p2m;

void *talk_thread(void *arg)
{
	fprintf(stderr, "talk thread ..\n");
	uint16_t head;
	int i;
	while (1) {
		/* host: use any buffers we should have been sent. */
		head = wait_for_vq_desc(guesttocons, iov, &outlen, &inlen);
		
		/* host: if we got an output buffer, just output it. */
		for(i = 0; i < outlen; i++) {
			printf("Host:%s:\n", (char *)iov[i].v);
		}
		
		/* host: fill in the writeable buffers. */
		for (i = outlen; i < outlen + inlen; i++) {
			/* host: read a line. */
			memset(consline, 0, 128);
			if (fgets(consline, sizeof(consline), stdin) == NULL) {
				exit(0);
			}
			memmove(iov[i].v, consline, strlen(consline)+ 1);
			iov[i].length = strlen(consline) + 1;
		}
		
		/* host: now ack that we used them all. */
		add_used(guesttocons, head, outlen+inlen);
	}
	fprintf(stderr, "All done\n");
	return NULL;
}

int main(int argc, char **argv)
{
	int fd = open("#c/sysctl", O_RDWR), ret;
	void * x;
	static char cmd[512];
	if (fd < 0) {
		perror("#c/sysctl");
		exit(1);
	}

	void *inpages, *outpages;
	ret = posix_memalign(&inpages, 8192, 1048576);
	if (ret) {
		perror("inpages");
		exit(1);
	}
	ret = posix_memalign(&outpages, 8192, 1048576);
	if (ret) {
		perror("outpages");
		exit(1);
	}

	my_threads = calloc(sizeof(pthread_t) , nr_threads);
	my_retvals = calloc(sizeof(void *) , nr_threads);
	if (!(my_retvals && my_threads))
		perror("Init threads/malloc");

	pthread_lib_init();	/* gives us one vcore */
	vcore_request(nr_threads - 1);	/* ghetto incremental interface */
	for (int i = 0; i < nr_threads; i++) {
		x = __procinfo.vcoremap;
		fprintf(stderr, "%p\n", __procinfo.vcoremap);
		fprintf(stderr, "Vcore %d mapped to pcore %d\n", i,
			   __procinfo.vcoremap[i].pcoreid);
	}

	guesttocons = vring_new_virtqueue(0, 512, 8192, 0, outpages, NULL, NULL, "test");
	
	if (mcp) {
		if (pthread_create(&my_threads[0], NULL, &talk_thread, NULL))
			perror("pth_create failed");
//      if (pthread_create(&my_threads[1], NULL, &fail, NULL))
//          perror("pth_create failed");
	}
	fprintf(stderr, "threads started\n");
	
	if (0)
		for (int i = 0; i < nr_threads - 1; i++) {
			int ret;
			if (pthread_join(my_threads[i], &my_retvals[i]))
				perror("pth_join failed");
			fprintf(stderr, "%d %d\n", i, ret);
		}
	
	ret = syscall(33, 1);
	if (ret < 0) {
		perror("vm setup");
		exit(1);
	}
	ret = posix_memalign((void **)&p512, 4096, 3 * 4096);
	if (ret) {
		perror("ptp alloc");
		exit(1);
	}
	p1 = &p512[512];
	p2m = &p512[1024];
	p512[0] = (unsigned long long)p1 | 7;
	p1[0] = /*0x87; */ (unsigned long long)p2m | 7;
	p2m[0] = 0x87;
	p2m[1] = 0x200000 | 0x87;
	p2m[2] = 0x400000 | 0x87;
	p2m[3] = 0x600000 | 0x87;
	
	fprintf(stderr, "p512 %p p512[0] is 0x%lx p1 %p p1[0] is 0x%x\n", p512, p512[0], p1,
	       p1[0]);
	sprintf(cmd, "V 0x%x 0x%x 0x%x", (unsigned long long)fail,
		(unsigned long long)&stack[1024], (unsigned long long)p512);
	fprintf(stderr, "Writing command :%s:\n", cmd);
	ret = write(fd, cmd, strlen(cmd));
	if (ret != strlen(cmd)) {
		perror(cmd);
	}
	fprintf(stderr, "shared is %d\n", shared);
	
#if 0
// This code works. You can always uncomment to test.
	while (iter++) {
		uint16_t head;
		int i;
		/* guest: make a line available to host */
		ret = virtqueue_add_inbuf_avail(guesttocons, in, 1, line, 0);
		
		/* host: use any buffers we should have been sent. */
		head = wait_for_vq_desc(guesttocons, iov, &outlen, &inlen);

		/* host: if we got an output buffer, just output it. */
		for(i = 0; i < outlen; i++) {
			printf("Host:%s:\n", (char *)iov[i].v);
		}

		/* host: fill in the writeable buffers. */
		for (i = outlen; i < outlen + inlen; i++) {
			/* host: read a line. */
			memset(consline, 0, 128);
			if (fgets(consline, sizeof(consline), stdin) == NULL) {
				exit(0);
			}
			memmove(iov[i].v, consline, strlen(consline)+ 1);
			iov[i].length = strlen(consline) + 1;
		}

		/* host: now ack that we used them all. */
		add_used(guesttocons, head, outlen+inlen);
		/* guest code. Get all your buffers back */
		char *cp;
		while ((cp = virtqueue_get_buf_used(guesttocons, &conslen))) {
			if (cp != line)
				continue;
			fprintf(stderr, "guest: from host: %s\n", cp);
			/* guest: push some buffers into the channel for the host to use */
			sprintf(outline, "guest: outline %d:%s:\n", iter, line);
			ret = virtqueue_add_outbuf_avail(guesttocons, out, 1, outline, 0);
		}

	}
#endif
	return 0;
}
