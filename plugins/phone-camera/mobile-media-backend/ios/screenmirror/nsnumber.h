#pragma once

#include <stdint.h>

#pragma pack(1)
struct NSNumber
{
	uint8_t typeSpecifier;
	uint32_t IntValue;
	uint64_t LongValue;
	double FloatValue;
};
#pragma pack()

struct NSNumber NewNSNumberFromUInt32(uint32_t intValue);

struct NSNumber NewNSNumberFromUInt64(uint64_t longValue);

struct NSNumber NewNSNumberFromUFloat64(double floatValue);

int NewNSNumber(uint8_t *bytes, size_t bytes_len, struct NSNumber *out);

/*
remember to free the out the buffer
*/
void ToBytes(struct NSNumber *n, uint8_t **out, size_t *out_len);