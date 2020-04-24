
/* Tests for be.h */

#include <assert.h>
#include <stdint.h>

#include "be.h"

int
main()
{
	assert(swap32(UINT32_C(0x01234567)) ==
		      UINT32_C(0x67452301));
	assert(swap64(UINT64_C(0x0123456789abcdef)) ==
		      UINT64_C(0xefcdab8967452301));

	union {
		unsigned char c[4];
		uint32_t u;
	} v32 = { { 0x01, 0x23, 0x45, 0x67 } };
	assert(be32toh(v32.u) == 0x01234567);

	union {
		unsigned char c[8];
		uint64_t u;
	} v64 = { { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 } };
	assert(be64toh(v64.u) == UINT64_C(0x1122334455667788));
}
