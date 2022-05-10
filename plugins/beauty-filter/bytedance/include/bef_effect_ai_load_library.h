#ifndef BEF_EFFECT_AI_LOAD_LIBRARY_H
#define BEF_EFFECT_AI_LOAD_LIBRARY_H

#include "bef_effect_ai_public_define.h"

typedef void (*bef_ai_generic_proc)();
#if defined(_WIN32)
/* Win32 but not WinCE */
typedef bef_ai_generic_proc(__stdcall* bef_ai_get_proc_func)(const char*);
#else
typedef bef_ai_generic_proc (*bef_ai_get_proc_func)(const char*);
#endif

/**
 * @brief Load EGL function address through function pointer
 * @param proc_func 
 * @return If succeed return BYTED_EFFECT_RESULT_SUC, other value please see byted_effect_define.h
 */
BEF_SDK_API bef_effect_result_t bef_effect_ai_load_egl_library_with_func(bef_ai_get_proc_func proc_func);

/**
 * @brief Load GLESv2 function address through function pointer
 * @param proc_func 
 * @return If succeed return BYTED_EFFECT_RESULT_SUC, other value please see byted_effect_define.h
 */
BEF_SDK_API bef_effect_result_t bef_effect_ai_load_glesv2_library_with_func(bef_ai_get_proc_func proc_func);

/**
 * @brief Load EGL dynamic library through path
 * @param fileName 
 * @param searchType 
 * @return If succeed return BYTED_EFFECT_RESULT_SUC, other value please see byted_effect_define.h
 */
BEF_SDK_API bef_effect_result_t bef_effect_ai_load_egl_library_with_path(const char* filename, int type);

/**
 * @brief Load GLESv2 dynamic library through path
 * @param fileName 
 * @param searchType 
 * @return If succeed return BYTED_EFFECT_RESULT_SUC, other value please see byted_effect_define.h
 */
BEF_SDK_API bef_effect_result_t bef_effect_ai_load_glesv2_library_with_path(const char* filename, int type);

#endif //EFFECT_SDK_BEF_EFFECT_DYNAMIC_LIBRARY_H
