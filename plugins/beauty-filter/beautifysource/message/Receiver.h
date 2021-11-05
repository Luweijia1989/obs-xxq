#ifndef _BYTED_RENDER_CORE_RECEIVER_H_
#define _BYTED_RENDER_CORE_RECEIVER_H_


class Receiver {

public:
    virtual bool Update(unsigned int msgID, long arg1, long arg2, const char* arg3) = 0 ;
};


#endif //_BYTED_RENDER_CORE_RECEIVER_H_
