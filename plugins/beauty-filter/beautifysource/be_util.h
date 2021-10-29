#ifndef BE_UTIL_H
#define BE_UTIL_H


#include <stdio.h>
#include <string>

#define CHECK_BEF_AI_RET_SUCCESS(ret, mes) \
if(ret != 0){\
    printf("error id = %d,  %s !\n",ret, mes); \
    return ret;\
}

#define CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, mes)\
if (ret != 0)\
{\
    printf("error id = %d,  %s !\n",ret, mes); \
}

#define BE_DELETE_POINTER(ptr)\
if (ptr != nullptr)\
{\
    delete ptr;\
}

#define BE_DELETE_POINTER_ARRAY(ptr)\
if (ptr != nullptr)\
{\
delete[] ptr; \
}

long GetCurrentMillieconds();
bool GetEnableProfileTimeCost();



class ScopeTimer{
public:
    ScopeTimer(const char* msg);
    ~ScopeTimer();
    void reset(const char* msg);
    void finish();
private:
    long m_time = 0;
    std::string m_msg = "";
    bool m_finished = false;
};

#endif // !BE_UTIL_H
