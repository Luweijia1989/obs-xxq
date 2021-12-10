#pragma once
#include "cmclock.h"
#include "byteutils.h"

#ifdef WIN32
#include <Windows.h>
#endif // WIN32



#ifdef WIN32
static inline uint64_t get_clockfreq(void)
{
	static int have_clockfreq = 0;
	static LARGE_INTEGER clock_freq;

	if (have_clockfreq == 0) {
		QueryPerformanceFrequency(&clock_freq);
		have_clockfreq = 1;
	}

	return clock_freq.QuadPart;
}

uint64_t os_gettime_ns(void)
{
	LARGE_INTEGER current_time;
	double time_val;

	QueryPerformanceCounter(&current_time);
	time_val = (double)current_time.QuadPart;
	time_val *= 1000000000.0;
	time_val /= (double)get_clockfreq();

	return (uint64_t)time_val;
}
#endif

// startTime remain
struct CMClock NewCMClockWithHostTime(uint64_t id)
{
	struct CMClock cm = { .ID = id,.TimeScale = NanoSecondScale,.factor = 1,.startTime = os_gettime_ns() };
	return cm;
}

struct CMClock NewCMClockWithHostTimeAndScale(uint64_t id, uint32_t timeScale)
{
	struct CMClock cm = { .ID = id,.TimeScale = timeScale,.factor = 1,.startTime = os_gettime_ns() };
	return cm;
}

struct CMTime GetTime(struct CMClock *c)
{
	struct CMTime cmt = { .CMTimeValue = calcValue(c, os_gettime_ns() - c->startTime),
						.CMTimeScale = c->TimeScale,
						.CMTimeFlags = KCMTimeFlagsHasBeenRounded,
						.CMTimeEpoch = 0 };
	return cmt;
}

uint64_t calcValue(struct CMClock *clock, uint64_t value)
{
	if (clock->TimeScale == NanoSecondScale)
		return value;

	return (uint64_t)(clock->factor*(double)value);
}

double CalculateSkew(struct CMTime *startTimeClock1, struct CMTime *endTimeClock1, struct CMTime *startTimeClock2, struct CMTime *endTimeClock2)
{
	uint64_t timeDiffClock1 = endTimeClock1->CMTimeValue - startTimeClock1->CMTimeValue;
	uint64_t timeDiffClock2 = endTimeClock2->CMTimeValue - startTimeClock2->CMTimeValue;

	struct CMTime diffTime = {.CMTimeValue =timeDiffClock1, .CMTimeScale = startTimeClock1->CMTimeScale};
	double scaledDiff = GetTimeForScale(&diffTime, startTimeClock2);
	return (double)startTimeClock2->CMTimeScale * scaledDiff / (double)timeDiffClock2;
}
 

double GetTimeForScale(struct CMTime *time, struct CMTime *newScaleToUse)
{
	double scalingFactor = (double)newScaleToUse->CMTimeScale / (double)time->CMTimeScale;
	return (double)time->CMTimeValue * scalingFactor;
}

uint64_t Seconds(struct CMTime *time)
{
	if (time->CMTimeValue == 0)
		return 0;

	return time->CMTimeValue / time->CMTimeScale;
}

int Serialize(struct CMTime *time, uint8_t *out_buffer, size_t buffer_len)
{
	if (buffer_len < CMTimeLengthInBytes)
		return -1;

	byteutils_put_long(out_buffer, 0, time->CMTimeValue);
	byteutils_put_int(out_buffer, 8, time->CMTimeScale);
	byteutils_put_int(out_buffer, 12, time->CMTimeFlags);
	byteutils_put_long(out_buffer, 16, time->CMTimeEpoch);
	return 0;
}

int NewCMTimeFromBytes(struct CMTime *target, uint8_t *buffer)
{
	if (!target)
		return -1;

	target->CMTimeValue = byteutils_get_long(buffer, 0);
	target->CMTimeScale = byteutils_get_int(buffer, 8);
	target->CMTimeFlags = byteutils_get_int(buffer, 12);
	target->CMTimeEpoch = byteutils_get_long(buffer, 16);
	return 0;
}

