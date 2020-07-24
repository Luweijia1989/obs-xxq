#include "consumer.h"
#include <memory.h>
#include "byteutils.h"

struct Consumer AVFileWriter(char *h264_path, char *wav_path)
{
	struct AVFileWriterConsumer *afwc = calloc(1, sizeof(struct AVFileWriterConsumer));
	afwc->h264File = fopen(h264_path, "wb");
	afwc->wavFile = fopen(wav_path, "wb");
	uint8_t s[4] = { 00, 00, 00, 01 };
	memcpy(afwc->start_code, s, 4);
	struct Consumer file_write = {
		.consume = avfileConsume,
		.stop = avfileStop,
		.consumer_ctx = afwc
	};
	return file_write;
}

void avfileConsume(struct CMSampleBuffer *buf, void *c)
{
	struct AVFileWriterConsumer *writer = (struct AVFileWriterConsumer *)c;
	if (buf->MediaType == MediaTypeSound)
	{
		if (buf->SampleData_len > 0)
		{
			fwrite(buf->SampleData, 1, buf->SampleData_len, writer->wavFile);
		}
	}
	else
	{
		avfileConsumeVideo(buf, writer);
	}
}

void avfileStop(void *c)
{
	struct AVFileWriterConsumer *writer = (struct AVFileWriterConsumer *)c;
	fclose(writer->h264File);
	fclose(writer->wavFile);
}

void avfileConsumeVideo(struct CMSampleBuffer *buf, struct AVFileWriterConsumer *avfw)
{
	if (buf->HasFormatDescription)
	{
		writeNalu(buf->FormatDescription.PPS, buf->FormatDescription.PPS_len, avfw);
		writeNalu(buf->FormatDescription.SPS, buf->FormatDescription.SPS_len, avfw);
	}
	
	if (buf->SampleData_len == 0)
		return;

	writeNalus(buf->SampleData, buf->SampleData_len, avfw);
}
			
void writeNalu(uint8_t *nalu_bytes, size_t nalu_bytes_len, struct AVFileWriterConsumer *avfw)
{
	fwrite(avfw->start_code, 1, 4, avfw->h264File);
	fwrite(nalu_bytes, 1, nalu_bytes_len, avfw->h264File);
}

void writeNalus(uint8_t *nalu_bytes, size_t nalu_bytes_len, struct AVFileWriterConsumer *avfw)
{
	uint8_t *slice = nalu_bytes;
	while (slice < nalu_bytes + nalu_bytes_len)
	{
		size_t length = byteutils_get_int_be(slice, 0);
		writeNalu(slice+4, length, avfw);
		slice += length + 4;
	}
}

void clearConsumer(struct Consumer *consumer)
{
	if (consumer && consumer->consumer_ctx)
	{
		consumer->stop(consumer->consumer_ctx);
		free(consumer->consumer_ctx);
	}
}

