/* Copyright (C) 2009 Red Hat, Inc. All Rights Reserved.
 * Written by Ivana Hutarova Varekova <varekova@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <libcgroup.h>
#include <libcgroup-internal.h>
#include "tools-common.h"

enum flag{
	/*
	 * the flag set if there is a cgroup on output
	 * if there is no one we want to display all cgroups
	 */
	FL_LIST = 1
};

static inline void trim_filepath(char *path)
{
	int len;
	len = strlen(path) - 1;
	while (path[len] == '/')
		len--;

	path[len + 1] = '\0';
}

void usage(int status, char *program_name)
{
	if (status != 0) {
		fprintf(stderr, "Wrong input parameters,"
			" try %s -h' for more information.\n",
			program_name);
	} else {
		fprintf(stdout, "Usage: %s [<controllers>:<path>] [...]\n",
			program_name);
		fprintf(stdout, "list all cgroups\n");
	}
}

/*
 * if the info about controller "name" should be printed,
 *  then the function returns nonzero value
 */
int is_ctlr_on_list(struct cgroup_group_spec *cgroup_list,
			char name[FILENAME_MAX])
{
	int j;

	for (j = 0; cgroup_list->controllers[j] != NULL; j++)
		if (strcmp(cgroup_list->controllers[j], name) == 0)
			return 1;

	return 0;
}

void print_info(struct cgroup_file_info *info, char *name, int pref)
{
	if (info->type == CGROUP_FILE_TYPE_DIR) {
		if (info->full_path[pref] ==  '/')
			printf("%s:%s\n", name, &info->full_path[pref]);
		else
			printf("%s:/%s\n", name, &info->full_path[pref]);
	}
}

/* display controller:/input_path cgroups */
int display_controller_data(char *input_path, char *controller, char *name)
{
	int ret;
	void *handle;
	struct cgroup_file_info info;
	char cgroup_dir_path[FILENAME_MAX];
	char input_dir_path[FILENAME_MAX];
	char *end;
	int lvl;
	int len;

	ret = cgroup_walk_tree_begin(controller, input_path, 0,
		&handle, &info, &lvl);
	if (ret != 0)
		return ret;

	strncpy(cgroup_dir_path, info.full_path, FILENAME_MAX);
	/* remove problematic  '/' characters from cgroup directory path*/
	trim_filepath(cgroup_dir_path);

	strncpy(input_dir_path, input_path, FILENAME_MAX);
	/* remove problematic  '/' characters from input path*/
	trim_filepath(cgroup_dir_path);

	len  = strlen(cgroup_dir_path) - strlen(input_dir_path) + 1;
	print_info(&info, name, len);
	while ((ret = cgroup_walk_tree_next(0, &handle, &info, lvl)) == 0)
		print_info(&info, name, len);

	cgroup_walk_tree_end(&handle);
	if (ret == ECGEOF)
		ret = 0;

	return ret;
}

/*
 * print data about input cgroup_list cgroups
 * if FL_LIST flag is set then if the function does not find
 * the cgroup it returns ECGEOF
 */

int print_cgroup(struct cgroup_group_spec *cgroup_spec, int flags)
{
	int ret = 0;
	void *handle;
	struct cgroup_mount_point controller;
	char path[FILENAME_MAX];
	char con_name[FILENAME_MAX];
	char all_conts[FILENAME_MAX];
	int output = 0;

	path[0] = '\0';
	con_name[0] = '\0';
	all_conts[0] = '\0';

	ret = cgroup_get_controller_begin(&handle, &controller);

	/* go through the list of controllers/mount point pairs */
	while (ret == 0) {
		if (strcmp(path, controller.path) == 0) {
			/* if it is still the same mount point */
			strncat(all_conts, ",",
				FILENAME_MAX-strlen(all_conts)-1);
			strncat(all_conts, controller.name,
				FILENAME_MAX-strlen(all_conts)-1);
		} else {
			/* we got new mount point, print it if needed */
			if (output) {
				ret = display_controller_data(
					cgroup_spec->path,
					con_name, all_conts);
				if (ret)
					return ret;
				if ((flags & FL_LIST) != 0) {
					/* we succesfully finish printing */
					output = 0;
					break;
				}
			}

			output = 0;
			strncpy(all_conts, controller.name, FILENAME_MAX);
			all_conts[FILENAME_MAX-1] = '\0';
			strncpy(con_name, controller.name, FILENAME_MAX);
			con_name[FILENAME_MAX-1] = '\0';
			strncpy(path, controller.path, FILENAME_MAX);
			path[FILENAME_MAX-1] = '\0';
		}

		/* set output flag */
		if ((output == 0) && (!(flags & FL_LIST) ||
			(is_ctlr_on_list(cgroup_spec, controller.name))))
			output = 1;

		/* the actual controller should not be printed */
		ret = cgroup_get_controller_next(&handle, &controller);
	}

	cgroup_get_controller_end(&handle);
	if (ret != ECGEOF)
		return ret;

	if (output) {
		ret = display_controller_data(
			cgroup_spec->path,
			con_name, all_conts);
	}

	return ret;
}


int cgroup_list_cgroups(char *tname,
	struct cgroup_group_spec *cgroup_list[],
	int flags)
{
	int ret = 0;
	int final_ret = 0;
	int i = 0;

	/* initialize libcgroup */
	ret = cgroup_init();
	if (ret) {
		fprintf(stderr, "cgroups can't be listed: %s\n",
			cgroup_strerror(ret));
		return ret;
	}

	if ((flags & FL_LIST) == 0) {
		struct cgroup_group_spec *cgroup_spec;
		cgroup_spec = calloc(1, sizeof(struct cgroup_group_spec));
		/* we have to print all cgroups */
		ret = print_cgroup(cgroup_spec,  flags);
		free(cgroup_spec);
		if (ret == 0) {
			final_ret = 0;
		} else {
			final_ret = ret;
			fprintf(stderr, "cgroups can't be listed: %s\n",
				cgroup_strerror(ret));
		}
	} else {
		/* we have he list of controllers which should be print */
		while ((cgroup_list[i] != NULL)
			&& ((ret == ECGEOF) || (ret == 0))) {
			ret = print_cgroup(cgroup_list[i], flags);
			if (ret != 0) {
				if (ret == ECGEOF) {
					/* controller was not found */
					final_ret = ECGFAIL;
				} else {
					/* other problem */
					final_ret = ret;
				}
				fprintf(stderr,
					"%s: cannot find group %s..:%s: %s\n",
					tname, cgroup_list[i]->controllers[0],
					cgroup_list[i]->path,
					cgroup_strerror(final_ret));
			}
			i++;
		}
	}
	return final_ret;
}

int main(int argc, char *argv[])
{

	int ret = 0;
	int c;

	int flags = 0;

	struct cgroup_group_spec *cgroup_list[CG_HIER_MAX];

	static struct option options[] = {
		{"help", 0, 0, 'h'},
		{0, 0, 0, 0}
	};

	/* parse arguments */
	while ((c = getopt_long(argc, argv, "h", options, NULL)) > 0) {
		switch (c) {
		case 'h':
			usage(0, argv[0]);
			return 0;
		default:
			usage(1, argv[0]);
			return -1;
		}
	}

	memset(cgroup_list, 0, sizeof(cgroup_list));

	/* no cgroup on input */
	if (optind < argc)
		flags |= FL_LIST;

	/* read the list of controllers */
	while (optind < argc) {
		ret = parse_cgroup_spec(cgroup_list, optarg,
				CG_HIER_MAX);
		if (ret) {
			fprintf(stderr, "%s: cgroup controller"
				" and path parsing failed (%s)\n",
				argv[0], argv[optind]);
			return -1;
		}
		optind++;
	}

	/* print the information, based on list of input cgroups and flags */
	ret = cgroup_list_cgroups(argv[0], cgroup_list, flags);

	return ret;
}
