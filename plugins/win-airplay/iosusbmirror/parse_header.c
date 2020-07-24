#pragma once
#include "parse_header.h"
#include "byteutils.h"
#include "dict.h"
#include "nsnumber.h"
#include <memory.h>
#include <stdlib.h>

int parseAsynHeader(uint8_t *data, uint32_t messagemagic, uint8_t **out, CFTypeID *typeId)
{
	return parseHeader(data, AsynPacketMagic, messagemagic, out, typeId);
}

int parseHeader(uint8_t *data, uint32_t packetmagic, uint32_t messagemagic, uint8_t **out, CFTypeID *typeId)
{
	uint32_t magic = byteutils_get_int(data, 0);
	if (magic != packetmagic)
	{
		*out = NULL;
		*typeId = 0;
		return -1;
	}

	uint64_t clockRef = byteutils_get_long(data, 4);
	uint32_t messageType = byteutils_get_int(data, 12);

	if (messageType != messagemagic)
	{
		*out = NULL;
		*typeId = 0;
		return -1;
	}

	*out = data+16;
	*typeId = clockRef;
	return 0;
}

int parseSyncHeader(uint8_t *data, uint32_t messagemagic, uint8_t **out, CFTypeID *typeId, uint64_t *correlationID)
{
	uint8_t *remainingBytes = NULL;
	CFTypeID clockRef;
	int res = parseHeader(data, SyncPacketMagic, messagemagic, &remainingBytes, &clockRef);

	if (res != 0)
	{
		*out = data;
		*typeId = 0;
		*correlationID = 0;
		return res;
	}

	*out = remainingBytes + 8;
	*typeId = clockRef;
	*correlationID = byteutils_get_long(remainingBytes, 0);
	return 0;
}

int newSyncCwpaPacketFromBytes(uint8_t *data, struct SyncCwpaPacket *packet)
{
	uint8_t *remainingBytes = NULL;
	CFTypeID clockRef;
	uint64_t correlationID;
	int res = parseSyncHeader(data, CWPA, &remainingBytes, &clockRef, &correlationID);
	if (res != 0)
		return -1;

	packet->ClockRef = clockRef;
	packet->CorrelationID = correlationID;
	packet->ClockRef = byteutils_get_long(data, 4);
	if (packet->ClockRef != EmptyCFType)
	{
		return -2;
	}

	packet->DeviceClockRef = byteutils_get_long(remainingBytes, 0);
	return 0;
}

void clockRefReply(uint64_t clockRef, uint64_t correlationID, uint8_t **out, size_t *out_len)
{
	*out_len = 28;
	*out = calloc(1, *out_len);
	byteutils_put_int(*out, 0, *out_len);
	byteutils_put_int(*out, 4, ReplyPacketMagic);
	byteutils_put_long(*out, 8, correlationID);
	byteutils_put_int(*out, 16, 0);
	byteutils_put_long(*out, 20, clockRef);
}

void CwpaPacketNewReply(struct SyncCwpaPacket *sp, CFTypeID clockRef, uint8_t **out, size_t *out_len)
{
	clockRefReply(clockRef, sp->CorrelationID, out, out_len);
}

int NewSyncAfmtPacketFromBytes(uint8_t *data, size_t data_len, struct SyncAfmtPacket *packet)
{
	uint8_t *remainingBytes = NULL;
	CFTypeID clockRef;
	uint64_t correlationID;
	int res = parseSyncHeader(data, AFMT, &remainingBytes, &clockRef, &correlationID);
	if (res != 0)
		return -1;

	packet->ClockRef = clockRef;
	packet->CorrelationID = correlationID;
	packet->AudioStreamInfo = NewAudioStreamBasicDescriptionFromBytes(remainingBytes, data_len-(remainingBytes-data));

	return 0;
}

void AfmtPacketNewReply(struct SyncAfmtPacket *sap, uint8_t **out, size_t *out_len)
{
	list_t *res = list_new();
	struct StringKeyEntry *one = calloc(1, sizeof(struct StringKeyEntry));
	struct NSNumber err = NewNSNumberFromUInt32(0);
	PutStringKeyNSNumber(one, "Error", &err);
	list_node_t *nodeOne = list_node_new_string_entry(one);
	list_rpush(res, nodeOne);

	uint8_t *dict_buf = NULL;
	size_t dict_len = 0;
	SerializeStringKeyDict(res, &dict_buf, &dict_len);

	int len = dict_len+20;
	*out = calloc(1, len);
	*out_len = len;

	uint8_t *out_p = *out;

	byteutils_put_int(out_p, 0, len);
	byteutils_put_int(out_p, 4, ReplyPacketMagic);
	byteutils_put_long(out_p, 8, sap->CorrelationID);
	byteutils_put_int(out_p, 16, 0);
	memcpy(out_p+20, dict_buf, dict_len);

	free(dict_buf);
}

int NewSyncCvrpPacketFromBytes(uint8_t *data, size_t data_len, struct SyncCvrpPacket *packet)
{
	uint8_t *remainingBytes = NULL;
	CFTypeID clockRef;
	uint64_t correlationID;
	int res = parseSyncHeader(data, CVRP, &remainingBytes, &clockRef, &correlationID);
	if (res != 0)
		return -1;

	packet->ClockRef = clockRef;
	packet->CorrelationID = correlationID;
	packet->ClockRef = byteutils_get_long(data, 4); 

	if (packet->ClockRef != EmptyCFType)
		return -1;

	packet->DeviceClockRef = byteutils_get_long(remainingBytes, 0);

	list_t *dict = NewStringDictFromBytes(remainingBytes+8, data_len-(remainingBytes-data)-8);
	if (!dict)
		return -1;

	packet->Payload = dict;
	return 0;
}

void SyncCvrpPacketNewReply(struct SyncCvrpPacket *svp, CFTypeID clockRef, uint8_t **out, size_t *out_len)
{
	clockRefReply(clockRef, svp->CorrelationID, out, out_len);
}

int NewSyncOgPacketFromBytes(uint8_t *data, size_t data_len, struct SyncOgPacket *packet)
{
	uint8_t *remainingBytes = NULL;
	CFTypeID clockRef;
	uint64_t correlationID;
	int res = parseSyncHeader(data, OG, &remainingBytes, &clockRef, &correlationID);
	if (res < 0)
		return -1;

	packet->ClockRef = clockRef;
	packet->CorrelationID = correlationID;
	packet->Unknown = byteutils_get_int(remainingBytes, 0);

	return 0;
}

void SyncOgPacketNewReply(struct SyncOgPacket *svp, uint8_t **out, size_t *out_len)
{
	*out_len = 24;
	*out = calloc(1, 24);

	byteutils_put_int(*out, 0, *out_len);
	byteutils_put_int(*out, 4, ReplyPacketMagic);
	byteutils_put_long(*out, 8, svp->CorrelationID);
	byteutils_put_long(*out, 16, 0);
}

int NewSyncSkewPacketFromBytes(uint8_t *data, size_t data_len, struct SyncSkewPacket *packet)
{
	uint8_t *remainingBytes = NULL;
	CFTypeID clockRef;
	uint64_t correlationID;
	int res = parseSyncHeader(data, SKEW, &remainingBytes, &clockRef, &correlationID);
	if (res < 0)
		return -1;

	packet->ClockRef = clockRef;
	packet->CorrelationID = correlationID;
	return 0;
}

void SyncSkewPacketNewReply(struct SyncSkewPacket *svp, double skew, uint8_t **out, size_t *out_len)
{
	*out_len = 28;
	*out = calloc(1, *out_len);
	byteutils_put_int(*out, 0, *out_len);
	byteutils_put_int(*out, 4, ReplyPacketMagic);
	byteutils_put_long(*out, 8, svp->CorrelationID);
	byteutils_put_int(*out, 16, 0);
	memcpy(*out+20, &skew, 8);
}

int NewSyncClokPacketFromBytes(uint8_t *data, size_t data_len, struct SyncClokPacket *packet)
{
	uint8_t *next_data_pointer = NULL;
	CFTypeID clockRef;
	uint64_t correlationID;
	int res = parseSyncHeader(data, CLOK, &next_data_pointer, &clockRef, &correlationID);
	if (res != 0)
		return -1;

	packet->ClockRef = clockRef;
	packet->CorrelationID = correlationID;
	return 0;
}

void SyncClokPacketNewReply(struct SyncClokPacket *scp, CFTypeID clockRef, uint8_t **out, size_t *out_len)
{
	clockRefReply(clockRef, scp->CorrelationID, out, out_len);
}

int NewSyncTimePacketFromBytes(uint8_t *data, size_t data_len, struct SyncTimePacket *packet)
{
	uint8_t *next_data_pointer = NULL;
	CFTypeID clockRef;
	uint64_t correlationID;
	int res = parseSyncHeader(data, TIME, &next_data_pointer, &clockRef, &correlationID);
	if (res != 0)
		return -1;

	packet->ClockRef = clockRef;
	packet->CorrelationID = correlationID;
	return 0;
}

void SyncTimePacketNewReply(struct SyncTimePacket *packet, struct CMTime *time, uint8_t **out, size_t *out_len)
{
	*out_len = 44;
	*out = calloc(1, *out_len);
	
	byteutils_put_int(*out, 0, *out_len);
	byteutils_put_int(*out, 4, ReplyPacketMagic);
	byteutils_put_long(*out, 8, packet->CorrelationID);
	byteutils_put_int(*out, 16, 0);

	Serialize(time, *out+20, 24);
}

int NewSyncStopPacketFromBytes(uint8_t *data, size_t data_len, struct SyncStopPacket *packet)
{
	uint8_t *next_data_pointer = NULL;
	CFTypeID clockRef;
	uint64_t correlationID;
	int res = parseSyncHeader(data, STOP, &next_data_pointer, &clockRef, &correlationID);
	if (res != 0)
		return -1;

	packet->ClockRef = clockRef;
	packet->CorrelationID = correlationID;
	return 0;
}

void SyncStopPacketNewReply(struct SyncStopPacket *packet, uint8_t **out, size_t *out_len)
{
	*out_len = 24;
	*out = calloc(1, *out_len);

	byteutils_put_int(*out, 0, *out_len);
	byteutils_put_int(*out, 4, ReplyPacketMagic);
	byteutils_put_long(*out, 8, packet->CorrelationID);
	byteutils_put_int(*out, 16, 0);
	byteutils_put_int(*out, 20, 0);
}

void clearSyncCvrpPacket(struct SyncCvrpPacket *packet)
{
	if (packet && packet->Payload)
		list_destroy(packet->Payload);
}

