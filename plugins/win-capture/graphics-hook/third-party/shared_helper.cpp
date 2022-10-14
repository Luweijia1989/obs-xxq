#include "shared_helper.h"
#include "shared_mem.h"

namespace qBase {

HANDLE g_danmuShare = nullptr;
HANDLE g_danmuEvent = nullptr;

bool connect(DWORD size, const char* mem_name) {
    g_danmuShare = qBase::SM_Open(mem_name);
    if (!g_danmuShare)
        g_danmuShare = qBase::SM_Create(size, mem_name);

    /*g_danmuEvent = OpenEvent(EVENT_MODIFY_STATE, true, L"SettingUpdatedEvent");
    if (!g_danmuEvent)
        g_danmuEvent = CreateEvent(NULL, NULL, NULL, L"SettingUpdatedEvent");*/
    return true;
}

void disConnect() {
    if(!g_danmuShare)
        qBase::SM_Close(g_danmuShare);
}

void readShare(DWORD size, char* buff) {
    if (!g_danmuShare)
        return;
    qBase::SM_Read(g_danmuShare, 0, size, buff);
}

void writeShare(char* buff, int length) {
    if (!g_danmuShare)
        return;

    qBase::SM_Write(g_danmuShare, 0, length, buff);

    if (g_danmuEvent)
        SetEvent(g_danmuEvent);

    /*LINE_INFO info;
    wmemcpy(info.data, data.c_str(), data.size());

    if (data_verctor.size() >= LINE_COUNT) {
        std::vector<LINE_INFO>::iterator it = data_verctor.begin();
        data_verctor.erase(it);
    }

    data_verctor.push_back(info);

    int size = 0;
    for (auto item : data_verctor) {
        memcpy(buf + sizeof(RECT_SETTING_NUM) + size, &(item), sizeof(info));
        size += sizeof(info);
    }

    qBase::SM_Write(m_share_memory, 0, max_size_, buf);*/
}

}
