
This is a tiny standalone C library for reading qcow2 disk image files.

Unallocated image data is read as all zeros.
It does not know about encryption or compression.
It can only access the active data, not the snapshots.
It cannot make changes to the image file.
The qcow2 file's cluster size must be at least the system page size.

But it is small and standalone.
I know I said "library" above, but
you only need the files [qcow2.c](qcow2.c) and [qcow2.h](qcow2.h)
(and perhaps [be.h](be.h))
to access the content of a qcow2 file from C.

Open a qcow2 image file
-

    struct qcow2 *qcow2_open(int fd, const char **error_ret);

This function checks that `fd` is an uncompressed qcow2 image file, and
returns a context structure to access it.

On error, this function returns NULL, closes the file `fd`, sets `errno` and
puts an error message in the optional `error_ret`.

On success, returns a pointer to a context structure, and leaves
the `*error_ret` unchanged.
The caller should eventually release the context structure with
`qcow2_close()`.

This API is designed for use with this style of error handling:

        struct qcow2 *q = NULL;
    
        int fd = open(path, O_RDONLY), &error);
        if (fd == -1) {
            warn("%s", path);
            goto cleanup;
        }
    
        const char *error;
        q = qcow2_open(fd, &error);
        if (!q) {
            warn("%s: %s", path, error);
            goto cleanup;
        }
    
        ... qcow2_get_size(q, ...) ...
        ... qcow2_read(q, ...) ...
    
    cleanup:
        qcow2_close(q);


Close the qcow2 image file
-

    void qcow2_close(struct qcow2 *q);

This function closes the file, and frees `q`.

If `q` is NULL, this function has no effect.

Query the image's virtual size
-

    uint64_t qcow2_get_size(struct qcow2 *q);

Returns the image's virtual size, in bytes.

Read data from the image
-

    int qcow2_read(struct qcow2 *q, void *dest, size_t len, uint64_t offset);

This function copies to `dest` at most `len` bytes of data
from the image's virtual content starting at `offset`.
On success, returns the number of bytes copied out to `dest`.
This is the same as `len` unless the copy would exceed the image size.

In particular, this function will not return a "short" read whenever there
is more data available.

On error returns -1, sets `errno` and the content of `dest` are unspecified.

References
=

 * [qcow2.txt](https://git.qemu.org/?p=qemu.git;a=blob;f=docs/interop/qcow2.txt) spec
