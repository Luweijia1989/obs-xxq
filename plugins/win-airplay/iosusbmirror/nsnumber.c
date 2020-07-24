#include "nsnumber.h"
#include "byteutils.h"
#include <memory.h>
#include <stdlib.h>

struct NSNumber NewNSNumberFromUInt32(uint32_t intValue)
{
	struct NSNumber n = { 0 };
	n.IntValue = intValue;
	n.typeSpecifier = 03;

	return n;
}

struct NSNumber NewNSNumberFromUInt64(uint64_t longValue)
{
	struct NSNumber n = { 0 };
	n.LongValue = longValue;
	n.typeSpecifier = 04;

	return n;
}

struct NSNumber NewNSNumberFromUFloat64(double floatValue)
{
	struct NSNumber n = { 0 };
	n.FloatValue = floatValue;
	n.typeSpecifier = 06;

	return n;
}

int NewNSNumber(uint8_t *bytes, size_t bytes_len, struct NSNumber *out)
{
	int res = -1;
	uint8_t typeSpecifier = bytes[0];
	switch (typeSpecifier)
	{
	case 6: // double
		if (bytes_len == 9)
		{
			out->typeSpecifier = typeSpecifier;
			memcpy(&out->FloatValue, bytes + 1, 8);
			res = 0;
		}
		break;
	case 5: // uint32
		if (bytes_len == 5)
		{
			out->typeSpecifier = typeSpecifier;
			out->IntValue = byteutils_get_int(bytes, 1);
			res = 0;
		}
		break;
	case 4: // uint64
		if (bytes_len == 9)
		{
			out->typeSpecifier = typeSpecifier;
			out->LongValue = byteutils_get_long(bytes, 1);
			res = 0;
		}
		break;
	case 3: // uint32
		if (bytes_len == 5)
		{
			out->typeSpecifier = typeSpecifier;
			out->IntValue = byteutils_get_int(bytes, 1);
			res = 0;
		}
		break;
	default:
		break;
	}
	return res;
}

void ToBytes(struct NSNumber *n, uint8_t **out, size_t *out_len)
{
	switch (n->typeSpecifier)
	{
	case 6:
		*out = malloc(9 * sizeof(uint8_t));
		memcpy(*out + 1, &n->FloatValue, 8);
		*(*out) = n->typeSpecifier;
		*out_len = 9;
		break;
	case 4:
		*out = malloc(9 * sizeof(uint8_t));
		*(*out) = n->typeSpecifier;
		byteutils_put_long(*out, 1, n->LongValue);
		*out_len = 9;
		break;
	case 3:
		*out = malloc(5 * sizeof(uint8_t));
		*(*out) = n->typeSpecifier;
		byteutils_put_int(*out, 1, n->IntValue);
		*out_len = 5;
		break;
	default:
		break;
	}
}

