#pragma once

struct media_header
{
	uint8_t type;
	int64_t timestamp;
	uint32_t payload_size;
};
