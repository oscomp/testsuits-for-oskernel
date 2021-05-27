#include "unistd.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

/*
 * SYS_mount系统调用, 此处是为检测自开发的OS是否能成功支持Fat32的拓展SDCard，并正常挂载到系统当中；
 * 此处"./mnt"和"/dev/vda2"是作为mount的默认参数，指的参考测试系统中的一个挂载点，和一个Fat32存储设备；
 * 在实际测试时，可以通过命令行传参的方式，替换掉默认参数；
 * 例如可以手动指定自开发的OS中的Fat32拓展SDCard存储设备/dev/sdc1，挂载到/mnt/point:
 * "mount /dev/sdc1 /mnt/point"
 */

static char mntpoint[64] = "./mnt";
static char device[64] = "/dev/vda2";
static const char *fs_type = "vfat";

void test_mount() {
	TEST_START(__func__);

	printf("Mounting dev:%s to %s\n", device, mntpoint);
	int ret = mount(device, mntpoint, fs_type, 0, NULL);
	printf("mount return: %d\n", ret);
	assert(ret == 0);

	if (ret == 0) {
		printf("mount successfully\n");
		ret = umount(mntpoint);
		printf("umount return: %d\n", ret);
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

	test_mount();
	return 0;
}
