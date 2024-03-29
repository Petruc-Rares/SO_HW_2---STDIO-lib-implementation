#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "so_stdio.h"
#include "test_util.h"

#include "hooks.h"

int num_sys_write;
int target_fd;

ssize_t hook_write(int fd, void *buf, size_t len);

struct func_hook hooks[] = {
	[0] = { .name = "write", .addr = (unsigned long)hook_write, .orig_addr = 0 },
};


//this will declare buf[] and buf_len
#include "large_file.h"


ssize_t hook_write(int fd, void *buf, size_t len)
{
	ssize_t (*orig_write)(int, void *, size_t);

	orig_write = (ssize_t (*)(int, void *, size_t))hooks[0].orig_addr;

	if (fd == target_fd)
		num_sys_write++;

	return orig_write(fd, buf, len);
}


int main(int argc, char *argv[])
{
	SO_FILE *f;
	char *tmp;
	int ret;
	char *test_work_dir;
	char fpath[256];

	tmp = malloc(buf_len + 1000);
	FAIL_IF(!tmp, "malloc failed\n");

	install_hooks("libso_stdio.so", hooks, 1);

	if (argc == 2)
		test_work_dir = argv[1];
	else
		test_work_dir = "_test";

	//sprintf(fpath, "%s/large_file", test_work_dir);
	sprintf(fpath, "large file");

	ret = create_file_with_contents(fpath, buf, buf_len);
	FAIL_IF(ret != 0, "Couldn't create file: %s\n", fpath);

	/* --- BEGIN TEST --- */
	f = so_fopen(fpath, "a");
	FAIL_IF(!f, "Couldn't open file: %s\n", fpath);

	target_fd = so_fileno(f);

	num_sys_write = 0;

	ret = so_fwrite(&buf[2000], 1, 1000, f);
	FAIL_IF(ret != 1000, "Incorrect return value for so_fread: got %d, expected %d\n", ret, 1000);
	FAIL_IF(num_sys_write != 0, "Incorrect number of write syscalls: got %d, expected %d\n", num_sys_write, 0);

	ret = so_fclose(f);
	FAIL_IF(ret != 0, "Incorrect return value for so_fclose: got %d, expected %d\n", ret, 0);

	FAIL_IF(num_sys_write != 1, "Incorrect number of write syscalls: got %d, expected %d\n", num_sys_write, 1);

	memcpy(tmp, buf, buf_len);
	memcpy(&tmp[buf_len], &buf[2000], 1000);

	FAIL_IF(!compare_file(fpath, (unsigned char *)tmp, buf_len + 1000), "Incorrect data in file\n");

	free(tmp);

	return 0;
}
