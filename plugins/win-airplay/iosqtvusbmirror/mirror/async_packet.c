#include "async_packet.h"

int NewAsynSprpPacketFromBytes(uint8_t *data, size_t data_len, struct AsynSprpPacket *as_packet)
{
	uint8_t *next_data_pointer;
	CFTypeID clockRef;
	if (parseAsynHeader(data, SPRP, &next_data_pointer, &clockRef) < 0)
		return -1;

	as_packet->ClockRef = clockRef;

	struct list_node *node = parseValueEntry(next_data_pointer, data_len-(next_data_pointer-data));
	if (!node)
		return -1;

	as_packet->Property = *((struct StringKeyEntry*)node->val);
	as_packet->Property.serialized_data = calloc(1, as_packet->Property.data_len);
	memcpy(as_packet->Property.serialized_data, ((struct StringKeyEntry*)node->val)->serialized_data, as_packet->Property.data_len);
	node->free(node->val);
	free(node);
	return 0;
}

