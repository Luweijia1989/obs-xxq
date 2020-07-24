#pragma once

#include <stdint.h>
#include "list.h"
#include "parse_header.h"

#define	AsynPacketMagic   0x6173796E
#define	FEED              0x66656564 //These contain CMSampleBufs which contain raw h264 Nalus
#define	TJMP              0x746A6D70
#define	SRAT              0x73726174 //CMTimebaseSetRateAndAnchorTime https://developer.apple.com/documentation/coremedia/cmtimebase?language=objc
#define	SPRP              0x73707270 // Set Property
#define	TBAS              0x74626173 //TimeBase https://developer.apple.com/library/archive/qa/qa1643/_index.html
#define	RELS              0x72656C73
#define	HPD1              0x68706431 //hpd1 - 1dph | For specifying/requesting the video format
#define	HPA1              0x68706131 //hpa1 - 1aph | For specifying/requesting the audio format
#define	NEED              0x6E656564 //need - deen
#define	EAT               0x65617421 //contains audio sbufs
#define	HPD0              0x68706430
#define	HPA0              0x68706130


#pragma pack(1)
struct StringKeyEntry
{
	char Key[256];
	uint8_t *serialized_data;
	size_t data_len;
	uint32_t typeMagic;
	void *children;
};
#pragma pack()

#pragma pack(1)
struct IndexKeyEntry
{
	uint16_t Key;
	uint8_t *serialized_data;
	size_t data_len;
	uint32_t typeMagic;
	void *children;
};
#pragma pack()

void WriteLengthAndMagic(uint8_t *dst_buffer, int length, uint32_t magic);

int ParseLengthAndMagic(uint8_t *bytes, size_t bytes_len, uint32_t exptectedMagic, uint8_t **next_data_pointer, size_t *key_len);

void PutStringKeyBool(struct StringKeyEntry *entry, const char *key, int value);
void PutStringKeyNSNumber(struct StringKeyEntry *entry, const char *key, struct NSNumber *value);
void PutStringKeyString(struct StringKeyEntry *entry, const char *key, const char *value);
void PutStringKeyBytes(struct StringKeyEntry *entry, const char *key, uint8_t *bytes, size_t bytes_len);
void PutStringKeyStringKeyDict(struct StringKeyEntry *entry, const char *key, list_t *list);

void PutStringKeyBool2(struct StringKeyEntry *entry, uint8_t *key, size_t key_len, int value);
void PutStringKeyNSNumber2(struct StringKeyEntry *entry, uint8_t *key, size_t key_len, struct NSNumber *value);
void PutStringKeyString2(struct StringKeyEntry *entry, uint8_t *key, size_t key_len, uint8_t *value, size_t value_len);
void PutStringKeyBytes2(struct StringKeyEntry *entry, uint8_t *key, size_t key_len, uint8_t *bytes, size_t bytes_len);
void PutStringKeyStringKeyDict2(struct StringKeyEntry *entry, uint8_t *key, size_t key_len, list_t *list);

void PutIntKeyBool(struct IndexKeyEntry *entry, uint16_t key, int value);
void PutIntKeyNSNumber(struct IndexKeyEntry *entry, uint16_t key, struct NSNumber *value);
void PutIntKeyString(struct IndexKeyEntry *entry, uint16_t key, uint8_t *value, size_t value_len);
void PutIntKeyBytes(struct IndexKeyEntry *entry, uint16_t key, uint8_t *bytes, size_t bytes_len);
void PutIntKeyStringKeyDict(struct IndexKeyEntry *entry, uint16_t key, list_t *list);

void SerializeKey(const char *key, uint8_t *out, size_t *out_len);
list_t *NewStringDictFromBytes(uint8_t *bytes, size_t bytes_len);
list_t *NewIndexDictFromBytes(uint8_t *bytes, size_t bytes_len);
list_t *NewIndexDictFromBytesWithCustomMarker(uint8_t *bytes, size_t bytes_len, uint32_t magic);
				   
int parseIntKey(uint8_t *data, size_t data_len, uint16_t *key, uint8_t **next_data_pointer);
int parseIntValue(uint8_t *data, size_t data_len, uint16_t key, struct IndexKeyEntry *entry);
struct list_node *parseIntDictEntry(uint8_t *data, size_t data_len);
struct IndexKeyEntry *getIndexValue(list_t *list, uint16_t key);

int parseKey(uint8_t *data, size_t data_len, uint8_t **string_key, size_t *string_key_len, uint8_t **remaining_data);
int parseValue(uint8_t *data, size_t data_len, uint8_t *key, size_t key_len, struct StringKeyEntry *entry);
struct list_node *parseValueEntry(uint8_t *data, size_t data_len);
struct list_node *parseEntry(uint8_t *data, size_t data_len);

/*
after serialize, the list will be destroyed
*/
void SerializeStringKeyDict(list_t *stringKeyDict, uint8_t **out, size_t *out_len);

void NewAsynHpd1Packet(list_t *stringKeyDict, uint8_t **out, size_t *out_len);
void NewAsynHpa1Packet(list_t *stringKeyDict, CFTypeID clockRef, uint8_t **out, size_t *out_len);

void NewAsynDictPacket(list_t *stringKeyDict, uint32_t subtypeMarker, uint64_t asynTypeHeader, uint8_t **out, size_t *out_len);

list_t *CreateHpd1DeviceInfoDict();
list_t *CreateHpa1DeviceInfoDict();

void AsynNeedPacketBytes(CFTypeID clockRef, uint8_t **out, size_t *out_len);

void free_string_entry(struct StringKeyEntry *entry);
void free_index_entry(struct IndexKeyEntry *entry);

void NewAsynHPD0(uint8_t **out, size_t *out_len);
void NewAsynHPA0(uint64_t clockRef, uint8_t **out, size_t *out_len);
