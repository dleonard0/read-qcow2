#pragma once

#include <inttypes.h>
#include <stdlib.h>

struct qcow2;

struct qcow2 *qcow2_open(int fd, const char **error_ret);
void qcow2_close(struct qcow2 *q);
uint64_t qcow2_get_size(struct qcow2 *q);
int qcow2_read(struct qcow2 *q, void *dest, size_t len, uint64_t offset);

