#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <memory.h>
#include "dict.h"

#pragma pack(1)
struct AsynSprpPacket {
	CFTypeID ClockRef;
	struct StringKeyEntry Property;
};
#pragma pack()

int NewAsynSprpPacketFromBytes(uint8_t *data, size_t data_len, struct AsynSprpPacket *as_packet);