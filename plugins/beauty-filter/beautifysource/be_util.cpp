
#include "be_defines.h"
#include "be_util.h"

#include <memory>
#include <time.h>

bool ENABLE_PROFILE_TIME_COST = false;

bool GetEnableProfileTimeCost()
{
    bool ret = ENABLE_PROFILE_TIME_COST;
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
