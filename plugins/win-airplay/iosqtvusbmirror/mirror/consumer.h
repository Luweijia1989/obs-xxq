#pragma once

#include "cmsamplebuf.h"
#include <stdio.h>

struct Consumer
{
	void *consumer_ctx;
	void (*consume)(struct CMSampleBuffer *buf, void *c);
	void (*stop)(void *c);
};
void clearConsumer(struct Consumer *consumer);
struct Consumer AVFileWriter(char *h264_path, char *wav_path);


struct AVFileWriterConsumer
{
	FILE *h264File;
	FILE *wavFile;
	uint8_t start_code[4];
};

void avfileConsume(struct CMSampleBuffer *buf, void *c);
void avfileStop(void *c);
void avfileConsumeVideo(struct CMSampleBuffer *buf, struct AVFileWriterConsumer *avfw);
void writeNalu(uint8_t *nalu_bytes, size_t nalu_bytes_len, struct AVFileWriterConsumer *avfw);
void writeNalus(uint8_t *nalu_bytes, size_t nalu_bytes_len, struct AVFileWriterConsumer *avfw);
