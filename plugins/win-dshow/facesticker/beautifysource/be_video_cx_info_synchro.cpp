#include "be_video_cx_info_synchro.h"
#include <QString>

static const char* C1_TYPES[] = {
            "Baby",   "Beach", "Building",   "Car",    "Cartoon", "Cat",    "Dog",    "Flower", "Food", "Group", "Hill",
            "Indoor", "Lake",  "Nightscape", "Selfie", "Sky",     "Statue", "Street", "Sunset", "Text", "Tree",  "Other"};

static const char* C2_TYPES[] = {
            "car_c1", "cat_c1", "bus_station", "Giraffe", "vegetables", "Cave", "jiehunzhao",
            "sr_camera", "yinger", "zhongcan", "lawn", "soccer_field", "selfie_c1", "GaoTie",
            "broadleaf_forest", "Americal_Fast_Food", "islet", "dining_hall", "formal_garden",
            "carrousel", "food_court", "Pc_game", "card_data", "Pig", "golf_course", "mianshi",
            "Peacock", "ertong", "supermarket", "sushi", "badminton_indoor", "Panda", "ruin", "TingYuan",
            "Graduation", "ZhanDao", "tennis_outdoor", "snacks", "DuoRouZhiWu", "ocean_liner", "Snooker",
            "hill_c1", "GongDian", "beach_c1", "QuanJiaFu", "dog_c1", "bankcard", "Swimming", "godfish",
            "grill", "LuYing", "fountain", "flower_c1", "rabbit", "Hamster", "desert", "banquet_hall",
            "farm", "Tortoise", "Ski", "downtown", "moutain_path", "street_c1", "HaiXian", "Bonfire",
            "movie_theater", "PaoBu", "park", "mobilephone", "Monument", "Elepant", "hotpot", "KingPenguin",
            "natural_river", "text_c1", "tv", "cartoon_c1", "swimming_pool_indoor", "lake_c1", "mountain_snowy",
            "fruit", "gymnasimum", "skyscraper", "amusement_arcade", "Soccerball", "big_neg", "statue_c1",
            "xiangcunjianzhu", "shineijiudianjucan", "group_c1", "cake", "creek", "ShaLa", "nightscape_c1",
            "bridge", "ocean", "DiTie", "shiwaiyoulechagn", "train_station", "tree_c1", "baseball", "Tiger",
            "temple", "gujianzhu", "Card_game", "throne_room", "xuejing", "tower", "bazaar_outdoor",
            "basketball_court_indoor", "QinZiSheYing", "drink", "ktv", "athletic_field", "screen",
            "note_labels", "pure_text", "puzzle", "qrcode", "scrawl_mark", "solidcolor_bg", "tv_screen"};

 static const char* VIDEO_CLS_TYPES[] = {"human", "baby", "kid", "half-selfie", "full-selfie", "multi-person", "kid-parent", "kid-show",
        "crowd", "dinner", "wedding", "avatar", "homelife", "yard,balcony", "furniture,appliance",
        "urbanlife", "supermarket", "streets", "parks", "campus", "building", "bridges", "station",
        "transportation", "car", "train", "plane", "entertainment", "ktv,bar", "mahjomg", "amusement-park",
        "drama,cross-talk,cinema", "concert,music-festival", "museum", "food", "food-show", "snacks",
        "home-foods", "fruit", "drinks", "japanese-food", "hot-pot", "barbecue", "noodles", "cake",
        "cooking", "natural-scene", "mountain", "rivers,lakes,sea", "road", "grass", "waterfull",
        "desert", "forest", "snow", "fileworks", "sky", "cultural-scene", "animals", "pets", "livestock",
        "plants", "flowers", "pets-plants", "ball", "soccer", "basketball", "badminton", "tennis",
        "pingpong", "billiard", "keep-fit", "yoga", "track-and-field", "extreme-sports", "water-sports",
        "swim", "dance", "singing", "game", "cartoon", "cosplay", "texts", "papers", "cards", "sensitive",
        "sex", "synthesis"};

BEVideoCXInfoSynchro::BEVideoCXInfoSynchro() {
    
}

BEVideoCXInfoSynchro::~BEVideoCXInfoSynchro() {

}

void BEVideoCXInfoSynchro::syncVideoClsInfo(bef_ai_video_cls_ret* videoClsInfo) {
    if (videoClsInfo == nullptr) return;

    int max = -1;
    for (int i = 0; i < videoClsInfo->n_classes; i++) {
        if (videoClsInfo->classes[i].confidence > videoClsInfo->classes[i].thres && 
            (max < 0 || videoClsInfo->classes[i].confidence > videoClsInfo->classes[max].confidence)) {
            max = i;
        }
    }

    if (max < 0) {
        QString title = QString::fromStdWString(L"视频分类");
        QString value = QString::fromStdWString(L"无结果");

        emit videoClsInfoChanged(title, value);
    }
    else {
        bef_ai_video_cls_type c = videoClsInfo->classes[max];
        QString title = QString(VIDEO_CLS_TYPES[c.id]);
        QString value = QString::number(c.confidence, 'f', 2);
       
        emit videoClsInfoChanged(title, value);
    }
}

void BEVideoCXInfoSynchro::syncC1Info(bef_ai_c1_output* c1Info) {
    if (c1Info == nullptr)  return;
    
    int max = -1;
    for (int i = 0; i < BEF_AI_C1_NUM_CLASSES; i++) {
        if (c1Info->items[i].satisfied && (max < 0 || c1Info->items[i].prob > c1Info->items[max].prob)) {
            max = i;
        }
    }
    if (max < 0) {
        QString title = QString("C1");
        QString value = QString::fromStdWString(L"无结果");
        emit c1Changed(title, value);
    }
    else {
        bef_ai_c1_category c = c1Info->items[max];

        QString title = QString(C1_TYPES[max]);
        QString value = QString::number(c.prob, 'f', 2);
      
        emit c1Changed(title, value);
    }
}

void BEVideoCXInfoSynchro::syncC2Info(bef_ai_c2_ret* c2Info) {
    int max = -1;
    for (int i = 0; i < c2Info->n_classes; i++) {
        if (c2Info->items[i].satisfied && (max < 0 || c2Info->items[i].confidence > c2Info->items[max].confidence)) {
            max = i;
        }
    }
    if (max < 0) {
        QString title = QString("C2");
        QString value = QString::fromStdWString(L"无结果");
        emit c2Changed(title, value);
    }
    else {
        bef_ai_c2_category_item c = c2Info->items[max];

        QString title = QString(C2_TYPES[c.id]);
        QString value = QString::number(c.confidence, 'f', 2);

        emit c2Changed(title, value);
    }
}

void BEVideoCXInfoSynchro::registerC1Type() {
	
}