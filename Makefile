
# This makefile is only for testing the qcow2.h header (make check)
# and is not for making a library object or building a package.
#
# Please prefer to copy qcow2.c and qcow2.h (and maybe be.h) directly into your
# project as they are public domain for that reason.

CFLAGS = -Wall -pedantic -g

default: check

OBJS = catqcow2.o qcow2.o
catqcow2: $(OBJS)
	$(LINK.c) $(OBJS) $(LDLIBS) -o $@

# Uncomment the next line if <endian.h> is unavailable or bad and
# you need to use be.h.
#qcow2.o: CFLAGS += -DNO_ENDIAN_H

clean:	
	rm -f catqcow2 $(OBJS)

check: test.sh catqcow2 be-t
	sh test.sh
	./be-t
	@echo PASS

.PHONY: check clean
