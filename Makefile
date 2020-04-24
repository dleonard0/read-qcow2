
CFLAGS = -Wall -pedantic -g
OBJS = catqcow2.o qcow2.o

catqcow2: $(OBJS)
	$(LINK.c) $(OBJS) $(LDLIBS) -o $@

# Uncomment the next line if <endian.h> is unavailable or bad
#qcow2.o: CFLAGS += -DNO_ENDIAN_H

clean:	
	rm -f catqcow2 $(OBJS)

check: test.sh catqcow2 be-t
	sh test.sh
	./be-t
	@echo PASS
