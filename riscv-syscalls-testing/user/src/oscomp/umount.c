#include "unistd.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

//#define MNTPOINT "./mnt"

static char mntpoint[64] = "./mnt";
static char device[64] = "/dev/vda2";
static const char *fs_type = "vfat";

void test_umount() {
	TEST_START(__func__);

	printf("Mounting dev:%s to %s\n", device, mntpoint);
	int ret = mount(device, mntpoint, fs_type, 0, NULL);
	printf("mount return: %d\n", ret);

	if (ret == 0) {
		ret = umount(mntpoint);
		assert(ret == 0);
		printf("umount success.\nreturn: %d\n", ret);
	}

	TEST_END(__func__);
}

int main(int argc,char *argv[]) {
	if(argc >= 2){
		strcpy(device, argv[1]);
	}

	if(argc >= 3){
		strcpy(mntpoint, argv[2]);
	}

	test_umount();
	return 0;
}
