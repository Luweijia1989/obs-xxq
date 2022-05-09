#pragma once

#include <stdint.h>

#define NanoSecondScale 1000000000

#pragma pack(1)
struct CMClock
{
	uint64_t ID;
	uint32_t TimeScale;
	double factor;
	uint64_t startTime;
};
#pragma  pack()

#pragma pack(1)
struct CMTime
{
	uint64_t CMTimeValue;  /*! @field value The value of the CMTime. value/timescale = seconds. */
	uint32_t CMTimeScale;  /*! @field timescale The timescale of the CMTime. value/timescale = seconds.  */
	uint32_t CMTimeFlags;  /*! @field flags The flags, eg. kCMTimeFlags_Valid, kCMTimeFlags_PositiveInfinity, etc. */
	uint64_t CMTimeEpoch;  /*! @field epoch Differentiates between equal timestamps that are actually different because
	of looping, multi-item sequencing, etc.
	Will be used during comparison: greater epochs happen after lesser ones.
	Additions/subtraction is only possible within a single epoch,
	however, since epoch length may be unknown/variable. */
};
#pragma pack()

#ifdef WIN32
uint64_t os_gettime_ns(void);
#endif

#define KCMTimeFlagsValid 0x0
#define KCMTimeFlagsHasBeenRounded 0x1
#define KCMTimeFlagsPositiveInfinity 0x2
#define KCMTimeFlagsNegativeInfinity 0x4
#define KCMTimeFlagsIndefinite 0x8
#define KCMTimeFlagsImpliedValueFlagsMask 0xE
#define CMTimeLengthInBytes 24

struct CMClock NewCMClockWithHostTime(uint64_t id);

struct CMClock NewCMClockWithHostTimeAndScale(uint64_t id, uint32_t timeScale);

struct CMTime GetTime(struct CMClock *c);

uint64_t calcValue(struct CMClock *clock, uint64_t value);

double CalculateSkew(struct CMTime *startTimeClock1 , struct CMTime *endTimeClock1, struct CMTime *startTimeClock2, struct CMTime *endTimeClock2);


double GetTimeForScale(struct CMTime *time, struct CMTime *newScaleToUse);

uint64_t Seconds(struct CMTime *time);

int Serialize(struct CMTime *time, uint8_t *out_buffer, size_t buffer_len);

int NewCMTimeFromBytes(struct CMTime *target, uint8_t *buffer);