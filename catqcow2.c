
/*
 * An example program that copies the content of a qcow2 image to stdout.
 * It doubles as a tool to exercise the functions in qcow2.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <err.h>

#include "qcow2.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))

static int
to_uint64(const char *s, uint64_t *ret)
{
	int64_t i64;
	if (sscanf(s, "%" SCNi64, &i64) != 1)
		return -1;
	*ret = i64;
	return 0;
}

int
main(int argc, char *argv[])
{
	const char *path;
	int ch;
	uint64_t seek = 0;
	uint64_t len = UINT64_MAX;
	int error = 0;

	while ((ch = getopt(argc, argv, "n:s:")) != -1)
		switch (ch) {
		case 'n':
			if (to_uint64(optarg, &len) == -1) {
				warnx("bad length");
				error = 1;
			}
			break;
		case 's':
			if (to_uint64(optarg, &seek) == -1) {
				warnx("bad seek");
				error = 1;
			}
			break;
		default:
			error = 1;
		}

	if (optind < argc)
		path = argv[optind++];
	else
		path = "-";

	if (optind < argc)
		error = 1;

	if (error) {
		fprintf(stderr,
		    "usage: %s [-n len] [-s skip] [file.qcow2|-]\n",
		    argv[0]);
		exit(2);
	}

	int fd;
	if (strcmp(path, "-") == 0) {
		path = "<stdin>";
		fd = STDIN_FILENO;
	} else {
		fd = open(path, O_RDONLY);
		if (fd == -1)
			err(1, "%s", path);
	}

	/* Open the image */
	errno = 0;
	const char *error_msg = NULL;
	struct qcow2 *q = qcow2_open(fd, &error_msg);
	if (!q)
		err(1, "%s: %s", path, error_msg);

	/* Restrict the seek/len parameters to the file size */
	uint64_t file_len = qcow2_get_size(q);
	if (seek > file_len)
		errx(1, "invalid seek");
	if (len > file_len - seek)
		len = file_len - seek;

	/* Copy out the range */
	char buf[BUFSIZ];
	while (len) {
		int rlen = qcow2_read(q, buf, MIN(len, BUFSIZ), seek);
		if (rlen == -1)
			err(1, "%s", path);
		if (rlen == 0)
			break; /* EOF */
		int n = write(STDOUT_FILENO, buf, rlen);
		if (n == -1)
			err(1, "write");
		len -= n;
		seek += n;
	}

	qcow2_close(q);

	exit(0);
}
