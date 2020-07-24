#pragma once

#include <stdint.h>
#include "audio_stream_basic_description.h"
#include "list.h"
#include "cmclock.h"

#define  PingPacketMagic 0x70696E67
#define  SyncPacketMagic 0x73796E63
#define  ReplyPacketMagic 0x72706C79
#define  TIME 0x74696D65
#define  CWPA 0x63777061
#define  AFMT 0x61666D74
#define  CVRP 0x63767270
#define  CLOK 0x636C6F6B
#define	 OG 0x676F2120
#define  SKEW 0x736B6577
#define  STOP 0x73746F70

typedef uint64_t CFTypeID;

//EmptyCFType is a CFTypeId of 0x1
#define EmptyCFType 1

#pragma pack(1)
struct SyncCwpaPacket {
	CFTypeID ClockRef;
	uint64_t CorrelationID;
	CFTypeID DeviceClockRef;
};
#pragma pack()

#pragma pack(1)
struct SyncAfmtPacket {
	CFTypeID ClockRef;
	uint64_t CorrelationID;
	struct AudioStreamBasicDescription AudioStreamInfo;
};
#pragma pack()

#pragma pack(1)
struct SyncCvrpPacket {
	CFTypeID ClockRef;
	uint64_t CorrelationID;
	CFTypeID DeviceClockRef;
	list_t *Payload;
};
#pragma pack()

#pragma pack(1)
struct SyncOgPacket {
	CFTypeID	ClockRef;
	uint64_t	CorrelationID;
	uint32_t	Unknown;
};
#pragma pack()

#pragma pack(1)
struct SyncSkewPacket {
	CFTypeID ClockRef;
	uint64_t CorrelationID;
};
#pragma pack()

#pragma pack(1)
struct SyncClokPacket {
	CFTypeID ClockRef;
	uint64_t CorrelationID;
};
#pragma pack()

#pragma pack(1)
struct SyncTimePacket {
	CFTypeID ClockRef;
	uint64_t CorrelationID;
};
#pragma pack()

#pragma pack(1)
struct SyncStopPacket {
	CFTypeID ClockRef;
	uint64_t CorrelationID;
};
#pragma pack()

int parseAsynHeader(uint8_t *data, uint32_t messagemagic, uint8_t **out, CFTypeID *typeId);

int parseHeader(uint8_t *data, uint32_t packetmagic, uint32_t messagemagic, uint8_t **out, CFTypeID *typeId);

int parseSyncHeader(uint8_t *data, uint32_t messagemagic, uint8_t **out, CFTypeID *typeId, uint64_t *correlationID);

int newSyncCwpaPacketFromBytes(uint8_t *data, struct SyncCwpaPacket *packet);

void clockRefReply(uint64_t clockRef, uint64_t correlationID, uint8_t **out, size_t *out_len);

void CwpaPacketNewReply(struct SyncCwpaPacket *sp, CFTypeID clockRef, uint8_t **out, size_t *out_len);

int NewSyncAfmtPacketFromBytes(uint8_t *data, size_t data_len, struct SyncAfmtPacket *packet);

void AfmtPacketNewReply(struct SyncAfmtPacket *sap, uint8_t **out, size_t *out_len);

int NewSyncCvrpPacketFromBytes(uint8_t *data, size_t data_len, struct SyncCvrpPacket *packet);

void SyncCvrpPacketNewReply(struct SyncCvrpPacket *svp, CFTypeID clockRef, uint8_t **out, size_t *out_len);

int NewSyncOgPacketFromBytes(uint8_t *data, size_t data_len, struct SyncOgPacket *packet);

void SyncOgPacketNewReply(struct SyncOgPacket *svp, uint8_t **out, size_t *out_len);

int NewSyncSkewPacketFromBytes(uint8_t *data, size_t data_len, struct SyncSkewPacket *packet);

void SyncSkewPacketNewReply(struct SyncSkewPacket *svp, double skew, uint8_t **out, size_t *out_len);

int NewSyncClokPacketFromBytes(uint8_t *data, size_t data_len, struct SyncClokPacket *packet);

void SyncClokPacketNewReply(struct SyncClokPacket *scp, CFTypeID clockRef, uint8_t **out, size_t *out_len);

int NewSyncTimePacketFromBytes(uint8_t *data, size_t data_len, struct SyncTimePacket *packet);

void SyncTimePacketNewReply(struct SyncTimePacket *packet, struct CMTime *time, uint8_t **out, size_t *out_len);

int NewSyncStopPacketFromBytes(uint8_t *data, size_t data_len, struct SyncStopPacket *packet);

void SyncStopPacketNewReply(struct SyncStopPacket *packet, uint8_t **out, size_t *out_len);

void clearSyncCvrpPacket(struct SyncCvrpPacket *packet);