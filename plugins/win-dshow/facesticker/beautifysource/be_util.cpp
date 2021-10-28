
#include "be_defines.h"
#include "be_util.h"

#include <memory>
#include <time.h>
#include <QMutex>


static QMutex PROFILE_MUTEX;
bool ENABLE_PROFILE_TIME_COST = false;


void SetEnableProfileTimeCost(bool status) {
    PROFILE_MUTEX.lock();
    ENABLE_PROFILE_TIME_COST = status;
    PROFILE_MUTEX.unlock();
}

bool GetEnableProfileTimeCost()
{
    bool ret = false;
    PROFILE_MUTEX.lock();
    ret = ENABLE_PROFILE_TIME_COST;
    PROFILE_MUTEX.unlock();
    return ret;
}

long GetCurrentMillieconds() {      
    return clock();
}




ScopeTimer::ScopeTimer(const char* msg) {
    m_msg = std::string(msg);
    m_time = GetCurrentMillieconds();
    
}
ScopeTimer::~ScopeTimer() {
    if (GetEnableProfileTimeCost() && !m_finished )
    {
        long interval = GetCurrentMillieconds() - m_time;
        BYTE_TIME_COST_PRINT(m_msg.c_str(), interval);
    }    
}

void ScopeTimer::finish() {
    if (GetEnableProfileTimeCost() && !m_finished)
    {
        m_finished = true;
        long interval = GetCurrentMillieconds() - m_time;
        BYTE_TIME_COST_PRINT(m_msg.c_str(), interval);
    }    
}

void ScopeTimer::reset(const char* msg) {
    if (GetEnableProfileTimeCost() && !m_finished)
    {
        long interval = GetCurrentMillieconds() - m_time;
        BYTE_TIME_COST_PRINT(m_msg.c_str(), interval);
    }
    m_finished = false;
    m_msg = std::string(msg);
    m_time = GetCurrentMillieconds() - m_time;
}
