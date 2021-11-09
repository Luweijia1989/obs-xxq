#ifndef BE_DETECTOR_H
#define BE_DETECTOR_H

#include "bef_effect_ai_public_define.h"
#include "bef_effect_ai_face_detect.h"
#include "bef_effect_ai_skeleton.h"
#include "bef_effect_ai_face_attribute.h"
#include "bef_effect_ai_hand.h"
#include "bef_effect_ai_portrait_matting.h"
#include "bef_effect_ai_lightcls.h"
#include "bef_effect_ai_headseg.h"
#include "bef_effect_ai_hairparser.h"
#include "bef_effect_ai_c1.h"
#include "bef_effect_ai_c2.h"
#include "bef_effect_ai_video_cls.h"
#include "bef_effect_ai_gaze_estimation.h"
#include "bef_effect_ai_pet_face.h"
#include "bef_effect_ai_face_verify.h"
#include "be_feature_context.h"


typedef struct BEImage
{
    unsigned char* buffer;
    int width;
    int height;
    int stride;
    int channel;
    bef_ai_pixel_format format;
    bef_ai_rotate_type ori;
} BEImage;

class BEDectector
{
public:
    BEDectector();
    ~BEDectector();

    void setWidthAndHeight(int width, int height, int bytePerRow);
    void initializeDetector(bool imageMode);
    void releaseDetector();

    bef_effect_result_t detectFace(bef_ai_face_info *faceInfo, unsigned char *buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori, unsigned long long config, bool imageMode);
    bef_effect_result_t detectFace280(bef_ai_face_info *faceInfo, unsigned char *buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori, unsigned long long config, bool imageMode);
    bef_effect_result_t detectFaceMask(void* maskInfo, unsigned long long config, bef_face_mask_type maskType);
    bef_effect_result_t detectFaceAttribute(bef_ai_face_attribute_result *faceAttrResult, unsigned char *buffer, bef_ai_face_106 *faceInfo, int faceCount, bef_ai_pixel_format format);
    bef_effect_result_t detectSkeleton(bef_ai_skeleton_info * skeletonInfo, unsigned char* buffer, int *validCount,  bef_ai_pixel_format format, bef_ai_rotate_type ori);
    bef_effect_result_t detectHand(bef_ai_hand_info *handInfo, unsigned char *buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori);
    bef_effect_result_t detectHeadSeg(bef_ai_face_info* inputInfo, bef_ai_headseg_output* outputInfo, unsigned char* buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori);
    unsigned char * detectHairParse( unsigned char *buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori, int *size);
    bef_effect_result_t detectPortraitMatting(unsigned char *buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori, unsigned char **mask, int *size);
    bef_effect_result_t detectC1(bef_ai_c1_output* result, unsigned char* buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori);
    bef_effect_result_t detectC2(bef_ai_c2_ret** result, unsigned char* buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori);
    bef_effect_result_t detectVideoCls(bef_ai_video_cls_ret *videoRet, unsigned char* buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori, bool &isLast);
    bef_effect_result_t classifyLight(unsigned char *buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori, bef_ai_light_cls_result*lightClassifyInfo, bool imageMode);
    bef_effect_result_t detectGazeEstimation(bef_ai_face_info* inputInfo, bef_ai_gaze_estimation_info* gazeInfo, unsigned char* buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori);

    bef_effect_result_t detectPetFace(unsigned char* buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori, bef_ai_pet_face_result* petInfo);
    bef_effect_result_t detectFaceVerify(const BEImage* inputImage, const BEImage* inputImageOther, float* curFaceFeatures, bool& isVerify, double* score);

public:
    bef_effect_result_t detectFaceSingle(bef_ai_face_info* faceInfo, unsigned char* buffer, int width, int height, int channel, bef_ai_pixel_format format, bef_ai_rotate_type ori);
    bef_effect_result_t detectFaceVerifyExtractFeaturesSingle(float* features, bool* valid, const unsigned char* buffer, int width, int height, int channel, bef_ai_pixel_format format, bef_ai_rotate_type ori);
private:
    void initializeFaceDetectHandle(bool imageMode);
    void initializeFaceAttrDetectHandle();
    void initializeSkeletonHandle();
    void initializeHandDetectHandle();
    void initializeHeadSegHandle();
    void initializeHairParseHandle();
    void initializePortraitMattingHandle();
	void initializeLightClassifyHandle(bool imageMode);
    void initializeC1DetectHandle();
    void initializeC2DetectHandle();
    void initializeVideoDetectHandle();
    void initializeGazeEstimationHandle();
    void initializePetFaceHandle();
    void initializeFaceVerifyHandle();
private:
    int m_width;
    int m_height;
    int m_bytePerRow;

    int m_frameCount = 0;

    bef_effect_handle_t m_faceDetectHandle = 0;
    bef_effect_handle_t m_faceAttributeHandle = 0;
    bef_effect_handle_t m_skeletonHandle = 0;
    bef_ai_headseg_handle m_headSegHandle = 0;
    bef_effect_handle_t m_hairParseHandle = 0;
    bef_effect_handle_t m_portraitMattingHandle = 0;
    bef_effect_handle_t m_lightClassifyHandle = 0;
    bef_ai_c1_handle m_c1Handle = 0;
    bef_ai_c2_handle m_c2Handle = 0;
    bef_ai_video_cls_handle m_videoClsHandle = 0;
    bef_ai_gaze_handle m_gazeEstimationHandle = 0;
    bef_effect_handle_t m_petFaceHandle = 0;

    bef_effect_handle_t m_faceVerifyHandle = 0;
    bef_effect_handle_t m_faceDetectForFaceVerifyHandle = 0;

    bef_ai_hand_sdk_handle m_handDetectHandle = 0;

    bef_ai_c2_ret* m_c2Info = nullptr;

    BEFeatureContext *m_featureContext = nullptr;

    bool m_bInitFace = false; 
    bool m_bInitFaceAttr = false;
    bool m_bInitSkeleton = false;
    bool m_bInitHeadSeg = false;
    bool m_bInitHairParse = false;
    bool m_bInitMatting = false;
    bool m_bInitLightClass = false;
    bool m_bInitC1 = false;
    bool m_bInitC2 = false;
    bool m_bInitVideo = false;
    bool m_bInitGaze = false;
    bool m_bInitHand = false;
    bool m_bInitPetFace = false;
    bool m_bInitFaceVerify = false;
};

#endif //BE_DETECTOR_H
