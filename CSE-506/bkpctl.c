#include <linux/bkpfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define LIST_FLAG 0x1
#define DELETE_FLAG 0x2
#define VIEW_FLAG 0x4
#define RESTORE_FLAG 0x8

void invalid_option(char *prog_name)
{
	printf("Usage: %s -[ld:v:r:] FILE\n", prog_name);
}

int main(int argc, char **argv)
{
	int err, opt, i, oldest;
	int fd;
	FILE *fp;
	query_arg_t *q;
	char *file_name, *uarg;
	int flag = 0;
	char answer;

	while ((opt = getopt(argc, argv, ":ld:v:r:")) != -1) {
		switch (opt) {
		case 'l':
			if (flag) {
				invalid_option(argv[0]);
				return 0;
			}
			flag |= LIST_FLAG;
			break;
		case 'd':
			if (flag) {
				invalid_option(argv[0]);
				return 0;
			}
			flag |= DELETE_FLAG;
			uarg = optarg;
			break;
		case 'v':
			if (flag) {
				invalid_option(argv[0]);
				return 0;
			}
			flag |= VIEW_FLAG;
			uarg = optarg;
			break;
		case 'r':
			if (flag) {
				invalid_option(argv[0]);
				return 0;
			}
			flag |= RESTORE_FLAG;
			uarg = optarg;
			break;
		case ':':
		default:
			invalid_option(argv[0]);
		}
	}
	if ((argc - optind) != 1) {
		invalid_option(argv[0]);
		return 0;
	}
	file_name = argv[optind];
	fp = fopen(file_name, "r");
	fd = fileno(fp);

	q = (query_arg_t *)malloc(sizeof(query_arg_t));
	if (!q) {
		printf("memory not allocated");
		exit(0);
	}
	if (flag & LIST_FLAG) {
		err = ioctl(fd, QUERY_LIST_VER, q);
		oldest = q->latest_bkp - q->num_bkps + 1;
		printf("Existing backup files are:\n");
		for (i = oldest; i <= q->latest_bkp; i++)
			printf("%s.bkp%03d\n", file_name, i);

	} else if (flag & DELETE_FLAG) {
		q->delete_ver = 0;
		if (strcmp(uarg, "newest") == 0)
			q->delete_ver |=  DEL_LATEST;
		else if (strcmp(uarg, "oldest") == 0)
			q->delete_ver |= DEL_OLDEST;
		else if (strcmp(uarg, "all") == 0)
			q->delete_ver |= DEL_ALL;
		else
			invalid_option(argv[0]);
		err = ioctl(fd, QUERY_DELETE_VER, q);

	} else if (flag & VIEW_FLAG) {
		q->offset = 0;
		if (strcmp(uarg, "newest") == 0) {
			q->version = VIEW_NEW;
		} else if (strcmp(uarg, "oldest") == 0) {
			q->version = VIEW_OLD;
		} else {
			q->version = (int)strtol(uarg, NULL, 10);
			if (q->version < 0 || q->version > 1000) {
				invalid_option(argv[0]);
				goto out;
			}
		}
		do {
			memset(q->buf, '\0', 4096);
			err = ioctl(fd, QUERY_VIEW_VER, q);
			if (err) {
				printf("err in ioctl %d\n", err);
				break;
			}
			if (q->offset == 0) {
				printf("Empty or no file\n");
				break;
			}
			if (strlen(q->buf) == 0)
				break;
			printf("%s\n", q->buf);
			printf("\nMore (y/n): ");
			scanf("%c", &answer);
			getchar();
		} while (answer == 'y');

	} else if (flag & RESTORE_FLAG) {
		if (strcmp(uarg, "newest") == 0) {
			q->version = RESTORE_NEW;
		} else if (strcmp(uarg, "oldest") == 0) {
			q->version = RESTORE_OLD;
		} else {
			q->version = (int)strtol(uarg, NULL, 10);
			if (q->version < 0 || q->version > 1000) {
				invalid_option(argv[0]);
				goto out;
			}
		}
		err = ioctl(fd, QUERY_RESTORE_VER, q);
		if (err)
			printf("Error on restore\n");
	} else {
		invalid_option(argv[0]);
		return 0;
	}
out:
	fclose(fp);
	free(q);
	return 0;
}
