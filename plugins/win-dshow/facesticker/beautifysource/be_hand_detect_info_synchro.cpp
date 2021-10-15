#include "be_hand_detect_info_synchro.h"

BEHandDetectInfoSynchro::BEHandDetectInfoSynchro() {
    registerHandGestureType();
}

void BEHandDetectInfoSynchro::syncHandDetectInfo(bef_ai_hand_info * handInfo) {    
    if (handInfo != nullptr && handInfo->hand_count > 0)
    {
        auto itr = m_handGestureMap.find(handInfo->p_hands[0].action);
        if (itr != m_handGestureMap.end())
        {
            emit handGestureChanged(itr->second);
        }
        else
        {
            emit handGestureChanged(QString(""));
        }
    }
    else
    {
        emit handGestureChanged(QString(""));
    }
}

void BEHandDetectInfoSynchro::registerHandGestureType() {
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_HEART_A, QString("heart_a")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_HEART_B, QString("heart_b")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_HEART_C, QString("heart_c")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_HEART_D, QString("heart_d")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_OK, QString("ok")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_HAND_OPEN, QString("hand_open")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_THUMB_UP, QString("thumb_up")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_THUMB_DOWN, QString("thumb_down")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_ROCK, QString("rock")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_NAMASTE, QString("namaste")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_PLAM_UP, QString("palm_up")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_FIST, QString("fist")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_INDEX_FINGER_UP, QString("index_finger_up")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_DOUBLE_FINGER_UP, QString("double_finger_up")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_VICTORY, QString("victory")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_BIG_V, QString("big_v")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_PHONECALL, QString("phonecall")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_BEG, QString("beg")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_THANKS, QString("thanks")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_UNKNOWN, QString("unknown")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_CABBAGE, QString("cabbage")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_THREE, QString("three")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_FOUR, QString("four ")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_PISTOL, QString("pistol")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_ROCK2, QString("rock2")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_SWEAR, QString("swear")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_HOLDFACE, QString("holdface")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_SALUTE, QString("salute")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_SPREAD, QString("spread")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_PRAY, QString("pray")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_QIGONG, QString("qigong")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_SLIDE, QString("slide")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_PALM_DOWN, QString("palm_down")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_PISTOL2, QString("pistol2")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_NARUTO1, QString("naruto1")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_NARUTO2, QString("naruto2")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_NARUTO3, QString("naruto3")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_NARUTO4, QString("naruto4")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_NARUTO5, QString("naruto5")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_NARUTO7, QString("naruto7")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_NARUTO8, QString("naruto8")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_NARUTO9, QString("naruto9")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_NARUTO10, QString("naruto10")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_NARUTO11, QString("naruto11")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_NARUTO12, QString("naruto12")));
    m_handGestureMap.insert(std::make_pair(BEF_TT_HAND_GESTURE_RAISE, QString("raise")));
}