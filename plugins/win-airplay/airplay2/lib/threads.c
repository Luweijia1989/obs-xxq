#include "threads.h"

const __int64 DELTA_EPOCH_IN_MICROSECS = 11644473600000000;

#if defined(WIN32)
int gettimeofday(struct timeval* tv/*in*/, struct timezone* tz/*in*/)
{
	FILETIME ft;
	__int64 tmpres = 0;
	TIME_ZONE_INFORMATION tz_winapi;
	int rez = 0;

	ZeroMemory(&ft, sizeof(ft));
	ZeroMemory(&tz_winapi, sizeof(tz_winapi));

	GetSystemTimeAsFileTime(&ft);

	tmpres = ft.dwHighDateTime;
	tmpres <<= 32;
	tmpres |= ft.dwLowDateTime;

	/*converting file time to unix epoch*/
	tmpres /= 10;  /*convert into microseconds*/
	tmpres -= DELTA_EPOCH_IN_MICROSECS;
	tv->tv_sec = (__int32)(tmpres * 0.000001);
	tv->tv_usec = (tmpres % 1000000);


	//_tzset(),don't work properly, so we use GetTimeZoneInformation
	rez = GetTimeZoneInformation(&tz_winapi);
	if (tz != NULL) {
		tz->tz_dsttime = (rez == 2) ? TRUE : FALSE;
		tz->tz_minuteswest = tz_winapi.Bias + ((rez == 2) ? tz_winapi.DaylightBias : 0);
	}

	return 0;
}

static DWORD timespec_to_ms(const struct timespec *abstime)
{
	DWORD t;

	if (abstime == NULL)
		return INFINITE;

	t = ((abstime->tv_sec - time(NULL)) * 1000) + (abstime->tv_nsec / 1000000);
	if (t < 0)
		t = 1;
	return t;
}

int pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *attr)
{
	(void)attr;

	if (mutex == NULL)
		return 1;

	InitializeCriticalSection(mutex);
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	if (mutex == NULL)
		return 1;
	DeleteCriticalSection(mutex);
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	if (mutex == NULL)
		return 1;
	EnterCriticalSection(mutex);
	return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	if (mutex == NULL)
		return 1;
	LeaveCriticalSection(mutex);
	return 0;
}

int pthread_cond_init(thread_cond_t *cond, pthread_condattr_t *attr)
{
	(void)attr;
	if (cond == NULL)
		return 1;
	InitializeConditionVariable(cond);
	return 0;
}

int pthread_cond_destroy(thread_cond_t *cond)
{
	/* Windows does not have a destroy for conditionals */
	(void)cond;
	return 0;
}

int pthread_cond_wait(thread_cond_t *cond, pthread_mutex_t *mutex)
{
	if (cond == NULL || mutex == NULL)
		return 1;
	return pthread_cond_timedwait(cond, mutex, NULL);
}

int pthread_cond_timedwait(thread_cond_t *cond, pthread_mutex_t *mutex,
	const struct timespec *abstime)
{
	if (cond == NULL || mutex == NULL)
		return 1;
	if (!SleepConditionVariableCS(cond, mutex, timespec_to_ms(abstime)))
		return 1;
	return 0;
}

int pthread_cond_signal(thread_cond_t *cond)
{
	if (cond == NULL)
		return 1;
	WakeConditionVariable(cond);
	return 0;
}

int pthread_cond_broadcast(thread_cond_t *cond)
{
	if (cond == NULL)
		return 1;
	WakeAllConditionVariable(cond);
	return 0;
}

#endif // WIN32
