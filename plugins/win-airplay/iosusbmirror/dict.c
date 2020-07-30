#include "dict.h"
#include "nsnumber.h"
#include "byteutils.h"
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdbool.h>
#include "parse_header.h"
#include "audio_stream_basic_description.h"
#include "formatdescriptor.h"

#define KeyValuePairMagic 0x6B657976
#define StringKey 0x7374726B
#define IntKey 0x6964786B
#define BooleanValueMagic 0x62756C76
#define DictionaryMagic 0x64696374
#define DataValueMagic 0x64617476
#define StringValueMagic 0x73747276
#define NumberValueMagic 0x6E6D6276

void WriteLengthAndMagic(uint8_t *dst_buffer, int length, uint32_t magic)
{
	byteutils_put_int(dst_buffer, 0, length);
	byteutils_put_int(dst_buffer, 4, magic);
}

int ParseLengthAndMagic(uint8_t *bytes, size_t bytes_len,
			uint32_t exptectedMagic, uint8_t **next_data_pointer,
			size_t *key_len)
{
	uint32_t length = byteutils_get_int(bytes, 0);
	uint32_t magic = byteutils_get_int(bytes, 4);

	if (length > bytes_len)
		return -1;

	if (magic != exptectedMagic)
		return -1;

	*key_len = length;
	*next_data_pointer = bytes + 8;

	return 0;
}

void PutStringKeyBool(struct StringKeyEntry *entry, const char *key, int value)
{
	PutStringKeyBool2(entry, key, strlen(key), value);
}

void PutStringKeyNSNumber(struct StringKeyEntry *entry, const char *key,
			  struct NSNumber *value)
{
	PutStringKeyNSNumber2(entry, key, strlen(key), value);
}

void PutStringKeyString(struct StringKeyEntry *entry, const char *key,
			const char *value)
{
	PutStringKeyString2(entry, key, strlen(key), value, strlen(value));
}

void PutStringKeyBytes(struct StringKeyEntry *entry, const char *key,
		       uint8_t *bytes, size_t bytes_len)
{
	PutStringKeyBytes2(entry, key, strlen(key), bytes, bytes_len);
}

void PutStringKeyStringKeyDict(struct StringKeyEntry *entry, const char *key,
			       list_t *list)
{
	PutStringKeyStringKeyDict2(entry, key, strlen(key), list);
}

void PutStringKeyBool2(struct StringKeyEntry *entry, uint8_t *key,
		       size_t key_len, int value)
{
	if (!entry)
		return;

	entry->typeMagic = bool_type;
	memcpy(entry->Key, key, key_len);
	entry->data_len = 9;
	entry->serialized_data = calloc(1, entry->data_len);
	WriteLengthAndMagic(entry->serialized_data, 9, BooleanValueMagic);
	entry->serialized_data[8] = value != 0 ? 1 : 0;
}

void PutStringKeyNSNumber2(struct StringKeyEntry *entry, uint8_t *key,
			   size_t key_len, struct NSNumber *value)
{
	if (!entry)
		return;

	entry->typeMagic = nsnumber_type;
	memcpy(entry->Key, key, key_len);
	uint8_t *out = NULL;
	size_t len = 0;
	ToBytes(value, &out, &len);
	entry->data_len = len + 8;
	entry->serialized_data = calloc(1, entry->data_len);
	WriteLengthAndMagic(entry->serialized_data, entry->data_len,
			    NumberValueMagic);
	memcpy(entry->serialized_data + 8, out, len);
	free(out);
}

void PutStringKeyString2(struct StringKeyEntry *entry, uint8_t *key,
			 size_t key_len, uint8_t *value, size_t value_len)
{
	if (!entry)
		return;

	entry->typeMagic = string_type;
	memcpy(entry->Key, key, key_len);
	entry->data_len = value_len + 8;
	entry->serialized_data = calloc(1, entry->data_len);
	WriteLengthAndMagic(entry->serialized_data, entry->data_len,
			    StringValueMagic);
	memcpy(entry->serialized_data + 8, value, value_len);
}

void PutStringKeyBytes2(struct StringKeyEntry *entry, uint8_t *key,
			size_t key_len, uint8_t *bytes, size_t bytes_len)
{
	if (!entry)
		return;

	entry->typeMagic = byte_type;
	memcpy(entry->Key, key, key_len);
	entry->data_len = bytes_len + 8;
	entry->serialized_data = calloc(1, entry->data_len);
	WriteLengthAndMagic(entry->serialized_data, entry->data_len,
			    DataValueMagic);
	memcpy(entry->serialized_data + 8, bytes, bytes_len);
}

void PutStringKeyStringKeyDict2(struct StringKeyEntry *entry, uint8_t *key,
				size_t key_len, list_t *list)
{
	if (!entry)
		return;

	entry->typeMagic = string_keydict_type;
	memcpy(entry->Key, key, key_len);
	entry->children = list;
}

void PutIntKeyBool(struct IndexKeyEntry *entry, uint16_t key, int value)
{
	if (!entry)
		return;

	entry->typeMagic = bool_type;
	entry->Key = key;
	entry->data_len = 9;
	entry->serialized_data = calloc(1, entry->data_len);
	WriteLengthAndMagic(entry->serialized_data, 9, BooleanValueMagic);
	entry->serialized_data[8] = value != 0 ? 1 : 0;
}

void PutIntKeyNSNumber(struct IndexKeyEntry *entry, uint16_t key,
		       struct NSNumber *value)
{
	if (!entry)
		return;

	entry->typeMagic = nsnumber_type;
	entry->Key = key;
	uint8_t *out = NULL;
	size_t len = 0;
	ToBytes(value, &out, &len);
	entry->data_len = len + 8;
	entry->serialized_data = calloc(1, entry->data_len);
	WriteLengthAndMagic(entry->serialized_data, entry->data_len,
			    NumberValueMagic);
	memcpy(entry->serialized_data + 8, out, len);
	free(out);
}
void PutIntKeyString(struct IndexKeyEntry *entry, uint16_t key, uint8_t *value,
		     size_t value_len)
{
	if (!entry)
		return;

	entry->typeMagic = string_type;
	entry->Key = key;
	entry->data_len = value_len + 8;
	entry->serialized_data = calloc(1, entry->data_len);
	WriteLengthAndMagic(entry->serialized_data, entry->data_len,
			    StringValueMagic);
	memcpy(entry->serialized_data + 8, value, value_len);
}

void PutIntKeyBytes(struct IndexKeyEntry *entry, uint16_t key, uint8_t *bytes,
		    size_t bytes_len)
{
	if (!entry)
		return;

	entry->typeMagic = byte_type;
	entry->Key = key;
	entry->data_len = bytes_len + 8;
	entry->serialized_data = calloc(1, entry->data_len);
	WriteLengthAndMagic(entry->serialized_data, entry->data_len,
			    DataValueMagic);
	memcpy(entry->serialized_data + 8, bytes, bytes_len);
}
void PutIntKeyStringKeyDict(struct IndexKeyEntry *entry, uint16_t key,
			    list_t *list)
{
	if (!entry)
		return;

	entry->typeMagic = string_keydict_type;
	entry->Key = key;
	entry->children = list;
}

void SerializeKey(const char *key, uint8_t *out, size_t *out_len)
{
	size_t keyLength = strlen(key) + 8;
	*out_len = keyLength;
	WriteLengthAndMagic(out, keyLength, StringKey);
	memcpy(out + 8, key, strlen(key));
}

list_t *NewStringDictFromBytes(uint8_t *bytes, size_t bytes_len)
{
	uint8_t *remaining_bytes = NULL;
	size_t value_len;
	if (ParseLengthAndMagic(bytes, bytes_len, DictionaryMagic,
				&remaining_bytes, &value_len) < 0)
		return NULL;

	list_t *res = list_new();
	uint8_t *slice = remaining_bytes;

	bool has_error = false;
	while (slice < bytes + bytes_len) {
		size_t keyValuePairLength;
		uint8_t *p;
		if (ParseLengthAndMagic(slice, bytes_len - (slice - bytes),
					KeyValuePairMagic, &p,
					&keyValuePairLength) < 0) {
			has_error = true;
			break;
		}

		struct list_node *node =
			parseEntry(slice + 8, keyValuePairLength - 8);
		if (!node) {
			has_error = true;
			break;
		}
		list_rpush(res, node);
		slice = slice + keyValuePairLength;
	}

	if (has_error) {
		list_destroy(res);
		return NULL;
	}

	return res;
}

list_t *NewIndexDictFromBytes(uint8_t *bytes, size_t bytes_len)
{
	return NewIndexDictFromBytesWithCustomMarker(bytes, bytes_len,
						     DictionaryMagic);
}

list_t *NewIndexDictFromBytesWithCustomMarker(uint8_t *bytes, size_t bytes_len,
					      uint32_t magic)
{
	uint8_t *remaining_bytes = NULL;
	size_t value_len;
	if (ParseLengthAndMagic(bytes, bytes_len, magic, &remaining_bytes,
				&value_len) < 0)
		return NULL;

	list_t *res = list_new();
	uint8_t *slice = remaining_bytes;

	bool has_error = false;
	while (slice < bytes + bytes_len) {
		size_t keyValuePairLength;
		uint8_t *p;
		if (ParseLengthAndMagic(slice, bytes_len - (slice - bytes),
					KeyValuePairMagic, &p,
					&keyValuePairLength) < 0) {
			has_error = true;
			break;
		}

		struct list_node *node =
			parseIntDictEntry(slice + 8, keyValuePairLength - 8);
		if (!node) {
			has_error = true;
			break;
		}
		list_rpush(res, node);
		slice = slice + keyValuePairLength;
	}

	if (has_error) {
		list_destroy(res);
		return NULL;
	}

	return res;
}

int parseIntKey(uint8_t *data, size_t data_len, uint16_t *key,
		uint8_t **next_data_pointer)
{
	uint8_t *re;
	size_t key_len;
	if (ParseLengthAndMagic(data, data_len, IntKey, &re, &key_len) < 0)
		return -1;

	*key = byteutils_get_short(data, 8);
	*next_data_pointer = data + key_len;
	return 0;
}

struct list_node *parseIntDictEntry(uint8_t *data, size_t data_len)
{
	uint16_t key;
	uint8_t *remaining_bytes;
	if (parseIntKey(data, data_len, &key, &remaining_bytes) < 0)
		return NULL;

	struct IndexKeyEntry *entry = calloc(1, sizeof(struct IndexKeyEntry));
	if (parseIntValue(remaining_bytes, data_len - (remaining_bytes - data),
			  key, entry) < 0) {
		free(entry);
		return NULL;
	}

	list_node_t *node = list_node_new_index_entry(entry);
	return node;
}

struct IndexKeyEntry *getIndexValue(list_t *list, uint16_t key)
{
	if (!list)
		return NULL;

	struct IndexKeyEntry *res = NULL;
	list_node_t *node;
	list_iterator_t *it = list_iterator_new(list, LIST_HEAD);
	while ((node = list_iterator_next(it))) {
		struct IndexKeyEntry *entry = node->val;
		if (entry->Key == key) {
			res = entry;
			break;
		}
	}
	list_iterator_destroy(it);
	return res;
}

int parseKey(uint8_t *data, size_t data_len, uint8_t **string_key,
	     size_t *string_key_len, uint8_t **remaining_data)
{
	uint8_t *temp;
	size_t key_len;

	if (ParseLengthAndMagic(data, data_len, StringKey, &temp, &key_len) < 0)
		return -1;

	*string_key = data + 8;
	*string_key_len = key_len - 8;
	*remaining_data = data + key_len;
	return 0;
}

int parseValue(uint8_t *data, size_t data_len, uint8_t *key, size_t key_len,
	       struct StringKeyEntry *entry)
{
	size_t value_len = byteutils_get_int(data, 0);
	if (value_len > data_len)
		return -1;

	uint32_t magic = byteutils_get_int(data, 4);
	switch (magic) {
	case StringValueMagic:
		PutStringKeyString2(entry, key, key_len, data + 8,
				    value_len - 8);
		break;
	case DataValueMagic:
		PutStringKeyBytes2(entry, key, key_len, data + 8,
				   value_len - 8);
		break;
	case BooleanValueMagic:
		PutStringKeyBool2(entry, key, key_len, *(data + 8) == 1);
		break;
	case NumberValueMagic: {
		struct NSNumber ns = {0};
		NewNSNumber(data + 8, value_len - 8, &ns);
		PutStringKeyNSNumber2(entry, key, key_len, &ns);
	} break;
	case DictionaryMagic: {
		list_t *sub = NewStringDictFromBytes(data, data_len);
		PutStringKeyStringKeyDict2(entry, key, key_len, sub);
	} break;
	case FormatDescriptorMagic: {
		struct FormatDescriptor *fd =
			calloc(1, sizeof(struct FormatDescriptor));
		int res = NewFormatDescriptorFromBytes(data, data_len, fd);
		if (res < 0)
			free(fd);
		else {
			memcpy(entry->Key, key, key_len);
			entry->children = fd;
			entry->typeMagic = format_descriptor_type;
		}
	} break;
	default:
		break;
	}

	return 0;
}

struct list_node *parseValueEntry(uint8_t *data, size_t data_len)
{
	uint8_t *next_data_pointer;
	size_t key_len;
	if (ParseLengthAndMagic(data, data_len, KeyValuePairMagic,
				&next_data_pointer, &key_len) < 0)
		return NULL;

	uint8_t *keyValuePairData = data + 8;
	return parseEntry(keyValuePairData, key_len - 8);
}

int parseIntValue(uint8_t *data, size_t data_len, uint16_t key,
		  struct IndexKeyEntry *entry)
{
	size_t value_len = byteutils_get_int(data, 0);
	if (value_len > data_len)
		return -1;

	uint32_t magic = byteutils_get_int(data, 4);
	switch (magic) {
	case StringValueMagic:
		PutIntKeyString(entry, key, data + 8, value_len - 8);
		break;
	case DataValueMagic:
		PutIntKeyBytes(entry, key, data + 8, value_len - 8);
		break;
	case BooleanValueMagic:
		PutIntKeyBool(entry, key, *(data + 8) == 1);
		break;
	case NumberValueMagic: {
		struct NSNumber ns = {0};
		NewNSNumber(data + 8, value_len - 8, &ns);
		PutIntKeyNSNumber(entry, key, &ns);
	} break;
	case DictionaryMagic: {
		list_t *sub = NewIndexDictFromBytes(data, data_len);
		PutIntKeyStringKeyDict(entry, key, sub);
	} break;
	case FormatDescriptorMagic: {
		struct FormatDescriptor *fd =
			calloc(1, sizeof(struct FormatDescriptor));
		int res = NewFormatDescriptorFromBytes(data, data_len, fd);
		if (res < 0)
			free(fd);
		else {
			entry->Key = key;
			entry->children = fd;
			entry->typeMagic = format_descriptor_type;
		}
	} break;
	default:
		break;
	}

	return 0;
}

struct list_node *parseEntry(uint8_t *data, size_t data_len)
{
	uint8_t *key;
	size_t key_len;
	uint8_t *remaining_data;
	if (parseKey(data, data_len, &key, &key_len, &remaining_data) < 0)
		return NULL;

	struct StringKeyEntry *entry = calloc(1, sizeof(struct StringKeyEntry));
	if (parseValue(remaining_data, data_len - (remaining_data - data), key,
		       key_len, entry) < 0) {
		free(entry);
		return NULL;
	}

	list_node_t *node = list_node_new_string_entry(entry);
	return node;
}

void SerializeStringKeyDict(list_t *stringKeyDict, uint8_t **out,
			    size_t *out_len)
{
	uint8_t temp_buf[1024 * 100] = {0};
	uint8_t *slice = temp_buf + 8;
	size_t index = 0;

	list_node_t *node;
	list_iterator_t *it = list_iterator_new(stringKeyDict, LIST_HEAD);
	while ((node = list_iterator_next(it))) {
		struct StringKeyEntry *entry = node->val;
		uint8_t *keyvaluePair = slice + index + 8;
		size_t keyLength = 0;
		SerializeKey(entry->Key, keyvaluePair, &keyLength);

		if (entry->typeMagic != format_descriptor_type) {
			if (entry->typeMagic == string_keydict_type) {
				SerializeStringKeyDict(entry->children,
						       &entry->serialized_data,
						       &entry->data_len);
				memcpy(keyvaluePair + keyLength,
				       entry->serialized_data, entry->data_len);
				free(entry->serialized_data);
				entry->serialized_data = NULL;
			} else
				memcpy(keyvaluePair + keyLength,
				       entry->serialized_data, entry->data_len);
		}

		WriteLengthAndMagic(slice + index,
				    keyLength + entry->data_len + 8,
				    KeyValuePairMagic);
		index = index + 8 + entry->data_len + keyLength;
	}
	list_iterator_destroy(it);

	size_t dictSizePlusHeaderAndLength = index + 4 + 4;
	*out_len = dictSizePlusHeaderAndLength;
	WriteLengthAndMagic(temp_buf, dictSizePlusHeaderAndLength,
			    DictionaryMagic);

	uint8_t *ret_buf = calloc(1, dictSizePlusHeaderAndLength);
	memcpy(ret_buf, temp_buf, dictSizePlusHeaderAndLength);
	*out = ret_buf;
}

void NewAsynHpd1Packet(list_t *stringKeyDict, uint8_t **out, size_t *out_len)
{
	NewAsynDictPacket(stringKeyDict, HPD1, EmptyCFType, out, out_len);
}

void NewAsynHpa1Packet(list_t *stringKeyDict, CFTypeID clockRef, uint8_t **out,
		       size_t *out_len)
{
	NewAsynDictPacket(stringKeyDict, HPA1, clockRef, out, out_len);
}

void NewAsynDictPacket(list_t *stringKeyDict, uint32_t subtypeMarker,
		       uint64_t asynTypeHeader, uint8_t **out, size_t *out_len)
{
	uint8_t *serialize_data = NULL;
	size_t serialize_len = 0;
	SerializeStringKeyDict(stringKeyDict, &serialize_data, &serialize_len);

	*out_len = serialize_len + 20;

	*out = calloc(1, *out_len);
	byteutils_put_int(*out, 0, *out_len);
	byteutils_put_int(*out, 4, AsynPacketMagic);
	byteutils_put_long(*out, 8, asynTypeHeader);
	byteutils_put_int(*out, 16, subtypeMarker);
	memcpy(*out + 20, serialize_data, serialize_len);
	free(serialize_data);
}

list_t *CreateHpd1DeviceInfoDict()
{
	list_t *res = list_new();
	struct StringKeyEntry *one = calloc(1, sizeof(struct StringKeyEntry));
	PutStringKeyBool(one, "Valeria", 1);
	list_node_t *nodeOne = list_node_new_string_entry(one);
	list_rpush(res, nodeOne);

	struct StringKeyEntry *two = calloc(1, sizeof(struct StringKeyEntry));
	PutStringKeyBool(two, "HEVCDecoderSupports444", 1);
	list_node_t *nodeTwo = list_node_new_string_entry(two);
	list_rpush(res, nodeTwo);

	list_t *sub = list_new();
	{
		struct StringKeyEntry *subone =
			calloc(1, sizeof(struct StringKeyEntry));
		struct NSNumber width = NewNSNumberFromUFloat64(1920);
		PutStringKeyNSNumber(subone, "Width", &width);
		list_node_t *nodeSubone = list_node_new_string_entry(subone);
		list_rpush(sub, nodeSubone);

		struct StringKeyEntry *subtwo =
			calloc(1, sizeof(struct StringKeyEntry));
		struct NSNumber height = NewNSNumberFromUFloat64(1200);
		PutStringKeyNSNumber(subtwo, "Height", &height);
		list_node_t *nodeSubonetwo = list_node_new_string_entry(subtwo);
		list_rpush(sub, nodeSubonetwo);
	}

	struct StringKeyEntry *three = calloc(1, sizeof(struct StringKeyEntry));
	PutStringKeyStringKeyDict(three, "DisplaySize", sub);
	list_node_t *nodeThree = list_node_new_string_entry(three);
	list_rpush(res, nodeThree);

	return res;
}

list_t *CreateHpa1DeviceInfoDict()
{
	uint8_t *asbd_bytes = NULL;
	size_t asbd_bytes_len = 0;
	struct AudioStreamBasicDescription asbd =
		DefaultAudioStreamBasicDescription();
	SerializeAudioStreamBasicDescription(&asbd, &asbd_bytes,
					     &asbd_bytes_len);

	list_t *res = list_new();
	struct StringKeyEntry *one = calloc(1, sizeof(struct StringKeyEntry));
	struct NSNumber interval = NewNSNumberFromUFloat64(0.07300000000000001);
	PutStringKeyNSNumber(one, "BufferAheadInterval", &interval);
	list_node_t *nodeOne = list_node_new_string_entry(one);
	list_rpush(res, nodeOne);

	struct StringKeyEntry *two = calloc(1, sizeof(struct StringKeyEntry));
	PutStringKeyString(two, "deviceUID", "Valeria");
	list_node_t *nodeTwo = list_node_new_string_entry(two);
	list_rpush(res, nodeTwo);

	struct StringKeyEntry *three = calloc(1, sizeof(struct StringKeyEntry));
	struct NSNumber Latency = NewNSNumberFromUFloat64(0.04);
	PutStringKeyNSNumber(three, "ScreenLatency", &Latency);
	list_node_t *nodethree = list_node_new_string_entry(three);
	list_rpush(res, nodethree);

	struct StringKeyEntry *four = calloc(1, sizeof(struct StringKeyEntry));
	PutStringKeyBytes(four, "formats", asbd_bytes, asbd_bytes_len);
	list_node_t *nodefour = list_node_new_string_entry(four);
	list_rpush(res, nodefour);

	struct StringKeyEntry *five = calloc(1, sizeof(struct StringKeyEntry));
	struct NSNumber Support = NewNSNumberFromUInt32(0);
	PutStringKeyNSNumber(five, "EDIDAC3Support", &Support);
	list_node_t *nodefive = list_node_new_string_entry(five);
	list_rpush(res, nodefive);

	struct StringKeyEntry *six = calloc(1, sizeof(struct StringKeyEntry));
	PutStringKeyString(six, "deviceName", "Valeria");
	list_node_t *nodesix = list_node_new_string_entry(six);
	list_rpush(res, nodesix);

	free(asbd_bytes);
	return res;
}

void AsynNeedPacketBytes(CFTypeID clockRef, uint8_t **out, size_t *out_len)
{
	*out_len = 20;
	*out = calloc(1, *out_len);

	byteutils_put_int(*out, 0, *out_len);
	byteutils_put_int(*out, 4, AsynPacketMagic);
	byteutils_put_long(*out, 8, clockRef);
	byteutils_put_int(*out, 16, NEED);
}

void free_string_entry(struct StringKeyEntry *entry)
{
	if (entry->typeMagic == format_descriptor_type)
		free(entry->children);
	else if (entry->typeMagic == string_keydict_type)
		list_destroy(entry->children);
	else
		free(entry->serialized_data);

	free(entry);
}

void free_index_entry(struct IndexKeyEntry *entry)
{
	if (entry->typeMagic == format_descriptor_type)
		free(entry->children);
	else if (entry->typeMagic == string_keydict_type)
		list_destroy(entry->children);
	else
		free(entry->serialized_data);

	free(entry);
}

void NewAsynHPD0(uint8_t **out, size_t *out_len)
{
	*out_len = 20;
	*out = calloc(1, *out_len);

	byteutils_put_int(*out, 0, *out_len);
	byteutils_put_int(*out, 4, AsynPacketMagic);
	byteutils_put_long(*out, 8, EmptyCFType);
	byteutils_put_int(*out, 16, HPD0);
}

void NewAsynHPA0(uint64_t clockRef, uint8_t **out, size_t *out_len)
{
	*out_len = 20;
	*out = calloc(1, *out_len);

	byteutils_put_int(*out, 0, *out_len);
	byteutils_put_int(*out, 4, AsynPacketMagic);
	byteutils_put_long(*out, 8, clockRef);
	byteutils_put_int(*out, 16, HPA0);
}
