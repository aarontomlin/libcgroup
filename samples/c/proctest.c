// SPDX-License-Identifier: LGPL-2.1-only
/*
 * Copyright NEC Soft Ltd. 2009
 *
 * Author:	Ken'ichi Ohmichi <oomichi@mxs.nes.nec.co.jp>
 */

#include <stdio.h>
#include <stdlib.h>
#include "../src/libcgroup-internal.h"

int main(int argc, char *argv[])
{
	int i;
	int ret;
	pid_t pid;
	uid_t uid;
	gid_t gid;
	char *procname;

	if (argc < 2) {
		printf("Specify process-id.\n");
		return 1;
	}
	printf("  Pid  |        Process name              |  Uid  |  Gid  \n");
	printf("-------+----------------------------------+-------+-------\n");

	for (i = 1; i < argc; i++) {
		pid = atoi(argv[i]);

		ret = cgroup_get_uid_gid_from_procfs(pid, &uid, &gid);
		if (ret) {
			printf("%6d | ret = %d\n", pid, ret);
			continue;
		}
		ret = cgroup_get_procname_from_procfs(pid, &procname);
		if (ret) {
			printf("%6d | ret = %d\n", pid, ret);
			continue;
		}
		printf("%6d | %32s | %5d | %5d\n", pid, procname, uid, gid);
		free(procname);
	}
	return 0;
}
