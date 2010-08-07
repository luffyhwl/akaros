#include <rstdio.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arch/arch.h>
#include <unistd.h>
#include <errno.h>

int main() 
{ 
	FILE *file; 
	file = fopen("/dir1/f1.txt","w+b");
	if (file == NULL)
		printf ("Failed to open file \n");
	fprintf(file,"%s","hello, world\n"); 
	fclose(file); 

	int fd = open("/bin/test.txt", O_RDWR | O_CREAT );
	char rbuf[256] = {0}, wbuf[256] = {0};
	int retval;
	retval = read(fd, rbuf, 16);
	printf("Tried to read, got %d bytes of buf: %s\n", retval, rbuf);
	strcpy(wbuf, "paul <3's the new 61c");
	retval = write(fd, wbuf, 22);
	printf("Tried to write, wrote %d bytes\n", retval);
	printf("Trying to seek to 0\n");
	lseek(fd, 0, SEEK_SET);
	retval = read(fd, rbuf, 64);
	printf("Tried to read again, got %d bytes of buf: %s\n", retval, rbuf);

	retval = access("/bin/laden", X_OK);
	if (errno != ENOENT)
		printf("WARNING! Access error for Osama!\n");
	retval = access("/dir1/f1.txt", R_OK);
	if (retval < 0)
		printf("WARNING! Access error for f1.txt!\n");
}
