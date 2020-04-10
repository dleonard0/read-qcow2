
/*
 * Open and read from a qcow2 image file.
 * David Leonard, 2020.
 */

#include <unistd.h>
#include <errno.h>
#include <endian.h>
#include <string.h>
#include <sys/mman.h>

#include "qcow2.h"

/* Cluster kinds, used for the cache */
#define KIND_L1		0
#define KIND_L2		1
#define KIND_DATA	2
#define KIND_MAX	3

/* Context structure for an opened qcow2 image file*/
struct qcow2 {
	int fd;

	/* extracted from file header */
	uint64_t size;
	uint64_t l1_table_offset;
	size_t cluster_size;

	/* cluster cache */
	struct cluster {
		void *base;
		uint64_t offset;	/* 0 when unused */
	} cluster[KIND_MAX];
};

/* Endian conversion with unaligned pointers */

static uint32_t
be32tohp(const void *p) {
	uint32_t v;
	memcpy(&v, p, sizeof v);
	return be32toh(v);
}

static uint64_t
be64tohp(const void *p) {
	uint64_t v;
	memcpy(&v, p, sizeof v);
	return be64toh(v);
}

/* Maps a cluster from the file into process virtual memory.
 * Each cluster kind is cached independently */
static const void *
load_cluster(struct qcow2 *q, uint64_t offset, unsigned kind)
{
	if (!offset) {
		errno = EINVAL; /* There is no cluster 0 */
		return NULL;
	}

	struct cluster *c = &q->cluster[kind];
	if (c->offset == offset)
		return c->base;	/* Cache hit */

	/* Cache miss; try to map */
	void *base = mmap(NULL, q->cluster_size, PROT_READ, MAP_SHARED,
	    q->fd, offset);
	if (base == MAP_FAILED)
		return NULL;

	if (c->offset)
		munmap(c->base, q->cluster_size);
	c->base = base;
	c->offset = offset;
	return base;
}

struct qcow2 *
qcow2_open(int fd, const char **error_ret)
{
	struct qcow2 *q = NULL;

#define FAIL(err, msg)				\
	do {					\
		if (error_ret)			\
			*error_ret = msg;	\
		errno = err;			\
		goto fail;			\
	} while (0)

	/* Check the header */
	char hdr[104];
	ssize_t n = pread(fd, hdr, sizeof hdr, 0);
	if (n == -1)
		FAIL(errno, "read");
	if (n != sizeof hdr)
		FAIL(ENOTSUP, "too short");

	if (memcmp(hdr, "QFI\xfb", 4) != 0)
		FAIL(ENOTSUP, "not qcow2");

	q = calloc(1, sizeof *q);
	if (!q)
		FAIL(errno, "calloc");

	uint32_t version = be32tohp(&hdr[4]);
	if (version < 2)
		FAIL(ENOTSUP, "too old");

	uint32_t crypt_method = be32tohp(&hdr[32]);
	if (crypt_method != 0)
		FAIL(ENOTSUP, "encrypted");

	if (version >= 3) {
		uint64_t incompatible_features = be64tohp(&hdr[72]);
		if ((incompatible_features >> 3) & 1)
			FAIL(ENOTSUP, "compressed");
	}

	q->fd = fd;
	q->size = be64tohp(&hdr[24]);
	q->l1_table_offset = be64tohp(&hdr[40]);

	uint32_t cluster_bits = be32tohp(&hdr[20]);
	q->cluster_size = (size_t)1 << cluster_bits;
	if (!q->cluster_size)
		FAIL(ENOTSUP, "too big");

	return q;

fail:;
	int saved_errno = errno;
	close(fd);
	free(q);
	errno = saved_errno;

	return NULL;
}

void
qcow2_close(struct qcow2 *q)
{
	unsigned kind;

	if (!q)
		return;
	for (kind = 0; kind < KIND_MAX; kind++)
		if (q->cluster[kind].offset)
			munmap(q->cluster[kind].base, q->cluster_size);
	close(q->fd);
	free(q);
}

uint64_t
qcow2_get_size(struct qcow2 *q)
{
	return q->size;
}

/* Reads virtual image data into memory */
int
qcow2_read(struct qcow2 *q, void *dest, size_t len, uint64_t offset)
{
	unsigned int l2_entries = q->cluster_size / sizeof (uint64_t);
	unsigned int l2_index = (offset / q->cluster_size) % l2_entries;
	unsigned int l1_index = (offset / q->cluster_size) / l2_entries;
	int ret = 0;

	/* Ensure read within bounds */
	if (offset >= q->size)
		len = 0;
	else if (len > q->size - offset)
		len = q->size - offset;

	while (len) {
		const uint64_t *l1_table = load_cluster(q,
		    q->l1_table_offset, KIND_L1);
		if (!l1_table)
			return -1;

		uint64_t data_offset;
		uint64_t l1_val = be64toh(l1_table[l1_index]);
		uint64_t l2_offset = l1_val & UINT64_C(0x00fffffffffffe00);

		if (!l2_offset) {
			data_offset = 0;
		} else {
			const uint64_t *l2_table = load_cluster(q, l2_offset,
			    KIND_L2);
			if (!l2_table)
				return -1;

			uint64_t l2_val = be64toh(l2_table[l2_index]);
			if ((l2_val >> 62) & 1) {
				/* Compressed cluster not supported */
				errno = ENOTSUP;
				return -1;
			}
			if ((l2_val & 1))
				data_offset = 0;
			else
				data_offset = l2_val &
				   UINT64_C(0x00fffffffffffe00);
		}

		/* Be careful not to read beyond the end of the cluster */
		size_t cluster_offset = offset % q->cluster_size;
		size_t rlen = len;
		if (rlen + cluster_offset > q->cluster_size)
			rlen = q->cluster_size - cluster_offset;

		if (!data_offset) {
			memset(dest, '\0', rlen);
		} else {
			ssize_t n = pread(q->fd, dest, rlen,
			    data_offset + cluster_offset);
			if (n == -1)
				return -1;
			rlen = n;
		}

		if (rlen == 0)
			break; /* EOF */
		ret += rlen;
		len -= rlen;
		dest = (char *)dest + rlen;
		offset += rlen;
	}
	return ret;
}
