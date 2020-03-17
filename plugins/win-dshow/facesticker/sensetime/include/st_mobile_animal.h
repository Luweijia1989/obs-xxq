#ifndef INCLUDE_STMOBILE_ST_MOBILE_ANIMAL_H_
#define INCLUDE_STMOBILE_ST_MOBILE_ANIMAL_H_

#include "st_mobile_common.h"

/// 该文件中的API不保证线程安全.多线程调用时,需要确保安全调用.例如在 create handle 没有执行完就执行 process 可能造成crash;在 process 执行过程中调用 destroy 函数可能会造成crash.

/// @defgroup st_mobile_tracker_cat
/// @brief cat keypoints tracking interfaces
/// This set of interfaces processing cat keypoints tracking routines
///
/// @{

#define ST_MOBILE_CAT_DETECT		0x00000001  ///< 猫脸检测

typedef struct st_mobile_animal_face_t {
	int id;                 ///<  每个检测到的脸拥有唯一的ID.跟踪丢失以后重新被检测到,会有一个新的ID
	st_rect_t rect;         ///< 代表面部的矩形区域
	float score;            ///< 置信度
	st_pointf_t *p_key_points;  ///< 关键点
	int key_points_count;       ///< 关键点个数
	float yaw;              ///< 水平转角,真实度量的左负右正
	float pitch;            ///< 俯仰角,真实度量的上负下正
	float roll;             ///< 旋转角,真实度量的左负右正
} st_mobile_animal_face_t, *p_st_mobile_animal_face_t;


#endif // INCLUDE_STMOBILE_ST_MOBILE_ANIMAL_H_
