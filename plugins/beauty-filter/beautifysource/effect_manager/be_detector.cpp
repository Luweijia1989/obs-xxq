#include "be_util.h"
#include "be_resource_context.h"
#include "be_detector.h"
#include "be_defines.h"

static int HEADSEG_INPUT_WIDTH = 128;
static int HEADSEG_INPUT_HEIGHT = 128;

static const int FRAME_INTERVAL = 5;

BEDectector::BEDectector()
{
}


BEDectector::~BEDectector()
{
}

void BEDectector::setWidthAndHeight(int width, int height, int bytePerRow) {
    m_width = width;
    m_height = height;
    m_bytePerRow = bytePerRow;
}

void BEDectector::initializeDetector(bool imageMode) {
    m_featureContext = getResourceContext()->getFeatureContext();
    /*initializeSkeletonHandle();
    initializeFaceDetectHandle(imageMode);
    initializeFaceAttrDetectHandle();
    initializeHandDetectHandle();
    initializeHeadSegHandle();
    initializeHairParseHandle();
    initializePortraitMattingHandle();
    initializeLightClassifyHandle(imageMode);
    initializeC1DetectHandle();
    initializeC2DetectHandle();
    initializeVideoDetectHandle();
    initializeGazeEstimationHandle();*/
}

void BEDectector::releaseDetector() {
    bef_effect_ai_skeleton_destroy(m_skeletonHandle);
    bef_effect_ai_face_detect_destroy(m_faceDetectHandle);
    bef_effect_ai_face_attribute_destroy(m_faceAttributeHandle);
    bef_effect_ai_hand_detect_destroy(m_handDetectHandle);
    bef_effect_ai_hairparser_destroy(m_hairParseHandle);
    bef_effect_ai_portrait_matting_destroy(m_portraitMattingHandle);
    bef_effect_ai_lightcls_release(m_lightClassifyHandle);
    BEF_AI_HSeg_ReleaseHandle(m_headSegHandle);
       
    bef_effect_ai_c1_release(m_c1Handle);
    if (m_c2Info) {
        bef_effect_ai_c2_release_ret(m_c2Handle, m_c2Info);
        m_c2Info = nullptr;
    }
    bef_effect_ai_c2_release(m_c2Handle);
    bef_effect_ai_video_cls_release(m_videoClsHandle);
    bef_effect_ai_gaze_estimation_destroy(m_gazeEstimationHandle);
}

bef_effect_result_t BEDectector::detectFace(bef_ai_face_info *faceInfo, unsigned char *buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori, unsigned long long config, bool imageMode) {
    if(!m_bInitFace) {
        initializeFaceDetectHandle(imageMode);
        m_bInitFace = true;
    }
    ScopeTimer t(__FUNCTION__);
    bef_effect_result_t ret = bef_effect_ai_face_detect(m_faceDetectHandle, buffer, format, m_width, m_height, m_bytePerRow, ori, config, faceInfo);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::detectFace bef_effect_ai_face_detect");
    return ret;
}

bef_effect_result_t BEDectector::detectFace280(bef_ai_face_info *faceInfo, unsigned char *buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori, unsigned long long config, bool imageMode) {
    if (!m_bInitFace) {
        initializeFaceDetectHandle(imageMode);
        m_bInitFace = true;
    }
    ScopeTimer t(__FUNCTION__);
    bef_effect_result_t ret = bef_effect_ai_face_detect(m_faceDetectHandle, buffer, format, m_width, m_height, m_bytePerRow, ori, config, faceInfo);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::detectFace bef_effect_ai_face_detect");
    return ret;
}

bef_effect_result_t BEDectector::detectFaceMask(void* maskInfo, unsigned long long config, bef_face_mask_type maskType) {
    ScopeTimer t(__FUNCTION__);
    bef_effect_result_t ret = bef_effect_ai_face_mask_detect(m_faceDetectHandle, config, maskType, maskInfo);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::detectFace bef_effect_ai_face_mask_detect");
    return ret;
}

bef_effect_result_t BEDectector::detectFaceAttribute(bef_ai_face_attribute_result *faceAttrResult, unsigned char *buffer, bef_ai_face_106 *faceInfo, int faceCount, bef_ai_pixel_format format) {
    if (!m_bInitFaceAttr) {
        initializeFaceAttrDetectHandle();
        m_bInitFaceAttr = true;
    }
    ScopeTimer t(__FUNCTION__);
    bef_effect_result_t ret;
    unsigned long long attriConfig = BEF_FACE_ATTRIBUTE_AGE | BEF_FACE_ATTRIBUTE_HAPPINESS | BEF_FACE_ATTRIBUTE_EXPRESSION | BEF_FACE_ATTRIBUTE_GENDER
        | BEF_FACE_ATTRIBUTE_ATTRACTIVE | BEF_FACE_ATTRIBUTE_CONFUSE;
    if (faceCount == 1) {
        ret = bef_effect_ai_face_attribute_detect(m_faceAttributeHandle, buffer, format, m_width, m_height, m_bytePerRow, faceInfo, attriConfig, &faceAttrResult->attr_info[0]);
        CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::detectFaceAttribute bef_effect_ai_face_attribute_detect");
    }
    else
    {
        ret = bef_effect_ai_face_attribute_detect_batch(m_faceAttributeHandle, buffer, format, m_width, m_height, m_bytePerRow, faceInfo, faceCount, attriConfig, faceAttrResult);
        CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::detectFaceAttribute bef_effect_ai_face_attribute_detect_batch");
    }

    return ret;
}

bef_effect_result_t BEDectector::detectSkeleton(bef_ai_skeleton_info * skeletonInfo, unsigned char* buffer, int *validCount, bef_ai_pixel_format format, bef_ai_rotate_type ori) {
    if (!m_bInitSkeleton) {
        initializeSkeletonHandle();
        m_bInitSkeleton = true;
    }
    ScopeTimer t(__FUNCTION__);
    bef_effect_result_t ret = bef_effect_ai_skeleton_detect(m_skeletonHandle, buffer, format, m_width, m_height, m_bytePerRow, ori, validCount, &skeletonInfo);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::detectSkeleton bef_effect_ai_skeleton_detect");
    return ret;
}


bef_effect_result_t BEDectector::detectHand(bef_ai_hand_info *handInfo, unsigned char *buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori) {
    if (!m_bInitHand) {
        initializeHandDetectHandle();
        m_bInitHand = true;
    }
    ScopeTimer t(__FUNCTION__);
    bef_effect_result_t ret;
    ret = bef_effect_ai_hand_detect(m_handDetectHandle, buffer, format, m_width, m_height, m_bytePerRow, ori,
        BEF_AI_HAND_MODEL_DETECT | BEF_AI_HAND_MODEL_BOX_REG | BEF_AI_HAND_MODEL_GESTURE_CLS | BEF_AI_HAND_MODEL_KEY_POINT, handInfo, 0);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::detectHand bef_effect_ai_hand_detect");
    return ret;
}

bef_effect_result_t BEDectector::detectHeadSeg(bef_ai_face_info* inputInfo, bef_ai_headseg_output* outputInfo, unsigned char* buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori) {
    if (!m_bInitHeadSeg) {
        initializeHeadSegHandle();
        m_bInitHeadSeg = true;
    }
    ScopeTimer t(__FUNCTION__);
    bef_effect_result_t ret;

    bef_ai_headseg_input input;
    input.face_count = inputInfo->face_count;
    input.image = const_cast<unsigned char*>(buffer);
    input.image_height = m_height;
    input.image_width = m_width;
    input.image_stride = m_bytePerRow;
    input.orient = (bef_ai_rotate_type)ori;
    input.pixel_format = (bef_ai_pixel_format)format;

    memset(outputInfo, 0, sizeof(bef_ai_headseg_output));
    if (input.face_count > 0) {
        bef_ai_headseg_faceinfo* headFaceInfo = new bef_ai_headseg_faceinfo[input.face_count];
        for (int i = 0; i < input.face_count; i++) {
            headFaceInfo[i].face_id = inputInfo->base_infos[i].ID;
            memcpy(headFaceInfo[i].points, inputInfo->base_infos[i].points_array, sizeof(float) * 2 * 106);
        }

        input.face_info = headFaceInfo;
        ret = BEF_AI_HSeg_DoHeadSeg(m_headSegHandle, &input, outputInfo);
        delete[] headFaceInfo;

        CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::detectHeadSeg BEF_AI_HSeg_DoHeadSeg");
    }
    else {
        ret = BEF_RESULT_FAIL;
    }

    return ret;
}

unsigned char* BEDectector::detectHairParse(unsigned char* buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori, int* size) {
    if (!m_bInitHairParse) {
        initializeHairParseHandle();
        m_bInitHairParse = true;
    }
    ScopeTimer t(__FUNCTION__);
    bef_effect_result_t ret;
    ret = bef_effect_ai_hairparser_get_output_shape(m_hairParseHandle, size, size + 1, size + 2);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::detectHairParse bef_effect_ai_hairparser_get_output_shape");

    int largeWidth = m_width;
    int largeHeight = m_height;

    int smallWidth = size[0];
    int smallHeight = size[1];

    int maskSize = 0;
    if (((float)largeHeight / smallHeight) != ((float)largeWidth / smallWidth)) {
        int net_input_w = 0;
        int net_input_h = 0;
        bef_effect_ai_hairparser_get_output_shape_with_input(m_hairParseHandle, largeWidth, largeHeight, &net_input_w, &net_input_h);
        maskSize = net_input_w * net_input_h;
        size[0] = net_input_w;
        size[1] = net_input_h;
    }
    else {
        maskSize = smallHeight * smallWidth;
    }

    unsigned char* dstAlpha = new unsigned char[maskSize];
    memset(dstAlpha, 0, maskSize);
    ret = bef_effect_ai_hairparser_do_detect(m_hairParseHandle, buffer, format, m_width, m_height, m_bytePerRow, ori, dstAlpha, false);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::detectHairParse bef_effect_ai_hairparser_do_detect");
    return dstAlpha;
}

bef_effect_result_t BEDectector::detectPortraitMatting(unsigned char *buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori, unsigned char **mask, int *size) {
    if (!m_bInitMatting) {
        initializePortraitMattingHandle();
        m_bInitMatting = true;
    }
    bef_effect_result_t ret;
    ScopeTimer t(__FUNCTION__);
    int out_w = m_width;
    int out_h = m_height;
    bef_effect_ai_portrait_get_output_shape(m_portraitMattingHandle, m_width, m_height, &out_w, &out_h);
    *mask = new unsigned char[out_w * out_h];
    bef_ai_matting_ret mattingRet = { *mask, m_width, m_height };

    ret = bef_effect_ai_portrait_matting_do_detect(m_portraitMattingHandle, buffer, format, m_width, m_height, m_bytePerRow, ori, false, &mattingRet);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::detectPortraitMatting bef_effect_ai_portrait_matting_do_detect");

    *size = mattingRet.width;
    *(size + 1) = mattingRet.height;
    *(size + 2) = 1;
    return ret;
}

bef_effect_result_t BEDectector::classifyLight(unsigned char *buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori, bef_ai_light_cls_result*lightClassifyInfo, bool imageMode) {
    if (!m_bInitLightClass) {
        initializeLightClassifyHandle(imageMode);
        m_bInitLightClass = true;
    }
    ScopeTimer t(__FUNCTION__);
    if (lightClassifyInfo == nullptr)
    {
        return -1;
    }
    bef_effect_result_t ret;
    ret = bef_effect_ai_lightcls_detect(m_lightClassifyHandle, buffer, format, m_width, m_height, m_bytePerRow, ori, lightClassifyInfo);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::classifyLight bef_effect_ai_light_classify");
    return ret;
}

bef_effect_result_t BEDectector::detectC1(bef_ai_c1_output* result, unsigned char* buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori) {
    if (!m_bInitC1) {
        initializeC1DetectHandle();
        m_bInitC1 = true;
    }
    ScopeTimer t(__FUNCTION__);

    bef_effect_result_t ret = bef_effect_ai_c1_detect(m_c1Handle, buffer, format, m_width, m_height, m_bytePerRow, ori, result);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "bef_effect_ai_c1_detect");
    return ret;
}

bef_effect_result_t BEDectector::detectC2(bef_ai_c2_ret** result, unsigned char* buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori) {
    if (!m_bInitC2) {
        initializeC2DetectHandle();
        m_bInitC2 = true;
    }
    ScopeTimer t(__FUNCTION__);

    bef_effect_result_t ret = bef_effect_ai_c2_detect(m_c2Handle, buffer, format, m_width, m_height, m_bytePerRow, ori, m_c2Info);
    *result = m_c2Info;
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "bef_effect_ai_c1_detect");
    return ret;
}

bef_effect_result_t BEDectector::detectVideoCls(bef_ai_video_cls_ret* videoRet, unsigned char* buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori, bool &isLast) {
    if (!m_bInitVideo) {
        initializeVideoDetectHandle();
        m_bInitVideo = true;
    }
    ScopeTimer t(__FUNCTION__);

    bef_ai_video_cls_args args;
    bef_ai_base_args base;
    base.image = buffer;
    base.image_width = m_width;
    base.image_height = m_height;
    base.pixel_fmt = format;
    base.orient = ori;
    base.image_stride = m_bytePerRow;
    args.bases = &base;
    args.is_last = (++m_frameCount % FRAME_INTERVAL) == 0;
    
    bef_effect_result_t ret;
    // bef_ai_video_cls_ret cls_ret;
    ret = bef_effect_ai_video_cls_detect(m_videoClsHandle, &args, videoRet);
    isLast = args.is_last;
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "bef_effect_ai_video_cls_detect");
    return ret;
}

bef_effect_result_t BEDectector::detectGazeEstimation(bef_ai_face_info* inputInfo, 
                                                      bef_ai_gaze_estimation_info* gazeInfo, 
                                                      unsigned char* buffer, 
                                                      bef_ai_pixel_format format, 
                                                      bef_ai_rotate_type ori) {
    if (!m_bInitGaze) {
        initializeGazeEstimationHandle();
        m_bInitGaze = true;
    }
    ScopeTimer t(__FUNCTION__);
    bef_effect_result_t ret;
    ret = bef_effect_ai_gaze_estimation_detect(m_gazeEstimationHandle, buffer, format, m_width, m_height, m_bytePerRow, ori, inputInfo, 0, gazeInfo);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "bef_effect_ai_gaze_estimation_detect");
    return ret;
}

bef_effect_result_t BEDectector::detectPetFace(unsigned char* buffer, bef_ai_pixel_format format, bef_ai_rotate_type ori, bef_ai_pet_face_result* petInfo) {
    if (!m_bInitPetFace) {
        initializePetFaceHandle();
        m_bInitPetFace = true;
    }
    
    bef_effect_result_t ret;
    ScopeTimer t(__FUNCTION__);
    ret = bef_effect_ai_pet_face_detect(m_petFaceHandle, buffer, format, m_width, m_height, m_bytePerRow, ori, petInfo);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::detectPetFace bef_effect_ai_pet_face_detect");
    return ret;
}

bef_effect_result_t BEDectector::detectFaceVerify(const BEImage* inputImage, const BEImage* inputImageOther, float* curFaceFeatures, bool& isVerify, double* score) {
    ScopeTimer t(__FUNCTION__);
    bef_effect_result_t ret = BEF_RESULT_FAIL;
    if (inputImage != nullptr && inputImageOther != nullptr)
    {
        if (isVerify) {
            bool valid = false;
            ret = detectFaceVerifyExtractFeaturesSingle(curFaceFeatures,
                &valid,
                inputImageOther->buffer,
                inputImageOther->width,
                inputImageOther->height,
                inputImageOther->channel,
                inputImageOther->format,
                inputImageOther->ori);

            isVerify = false;
        }
        else
        {
            float featutre[128];
            bool valid = false;
            ret = detectFaceVerifyExtractFeaturesSingle(featutre,
                &valid,
                inputImage->buffer,
                inputImage->width,
                inputImage->height,
                inputImage->channel,
                inputImage->format,
                inputImage->ori);
            if (ret == 0 && valid)
            {
                double norm = bef_effect_ai_face_verify(curFaceFeatures, featutre, 128);
                *score = bef_effect_ai__dist2score(norm);
            }
        }
    }
    return ret;
}

bef_effect_result_t BEDectector::detectFaceSingle(bef_ai_face_info* faceInfo, unsigned char* buffer, int width, int height, int channel, bef_ai_pixel_format format, bef_ai_rotate_type ori) {
    if (!m_bInitFaceVerify) {
        initializeFaceVerifyHandle();
        m_bInitFaceVerify = true;
    }
    bef_effect_result_t ret = bef_effect_ai_face_detect(m_faceDetectForFaceVerifyHandle, buffer, format, width, height, width * channel, ori, BEF_DETECT_MODE_IMAGE_SLOW | BEF_DETECT_SMALL_MODEL | BEF_FACE_DETECT, faceInfo);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::detectFaceSingle bef_effect_ai_face_detect");
    return ret;
}

bef_effect_result_t BEDectector::detectFaceVerifyExtractFeaturesSingle(float* features, bool* valid, const unsigned char* buffer, int width, int height, int channel, bef_ai_pixel_format format, bef_ai_rotate_type ori) {
    if (!m_bInitFaceVerify) {
        initializeFaceVerifyHandle();
        m_bInitFaceVerify = true;
    }
    bef_effect_result_t ret;
    bef_ai_face_info faceInfo;
    *valid = false;
    ret = bef_effect_ai_face_detect(m_faceDetectForFaceVerifyHandle,
        buffer,
        format,
        width,
        height,
        width * channel,
        ori,
        BEF_DETECT_MODE_IMAGE_SLOW | BEF_DETECT_SMALL_MODEL | BEF_FACE_DETECT,
        &faceInfo);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::detectFaceVerifyExtractFeatures bef_effect_ai_face_detect");
    if (faceInfo.face_count > 0)
    {
        ret = bef_effect_ai_face_extract_feature_single(m_faceVerifyHandle,
            buffer,
            format,
            width,
            height,
            width * channel,
            ori,
            &faceInfo.base_infos[0],
            features);
        CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::detectFaceVerifyExtractFeatures bef_effect_ai_face_extract_feature_single");
        if (ret == 0)
        {
            *valid = true;
        }
    }
    return ret;
}

void BEDectector::initializeFaceDetectHandle(bool imageMode) {
    bef_effect_result_t ret = 0;
    //load face detect model
    unsigned long long config = BEF_DETECT_SMALL_MODEL | BEF_DETECT_FULL;
    if (imageMode) {
        config |= BEF_DETECT_MODE_IMAGE_SLOW;
    }
    else {
        config |= BEF_DETECT_MODE_VIDEO;
    }
    ret = bef_effect_ai_face_detect_create(config, m_featureContext->getFeatureModelPath(BE_FEATURE_FACE).c_str(), &m_faceDetectHandle);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeFaceDetectHandle bef_effect_ai_face_detect_create");
    ret = bef_effect_ai_face_check_license(m_faceDetectHandle, m_featureContext->getLicensePath().c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeFaceDetectHandle bef_effect_ai_face_check_license");
    ret = bef_effect_ai_face_detect_setparam(m_faceDetectHandle, BEF_FACE_PARAM_FACE_DETECT_INTERVAL, 15);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeFaceDetectHandle bef_effect_ai_face_detect_setparam");
    ret = bef_effect_ai_face_detect_setparam(m_faceDetectHandle, BEF_FACE_PARAM_MAX_FACE_NUM, 15);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeFaceDetectHandle bef_effect_ai_face_detect_setparam");

    //load face detect model with 280 keypoints
    ret = bef_effect_ai_face_detect_add_extra_model(m_faceDetectHandle, TT_MOBILE_FACE_280_DETECT, m_featureContext->getFeatureModelPath(BE_FEATURE_FACE_EXTRA).c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeFaceDetectHandle bef_effect_ai_face_detect_add_extra_model");
}

void BEDectector::initializeFaceAttrDetectHandle() {
    bef_effect_result_t ret = 0;
    // load face attribute detect model
    ret = bef_effect_ai_face_attribute_create(0, m_featureContext->getFeatureModelPath(BE_FEATURE_FACE_ATTRIBUTE).c_str(), &m_faceAttributeHandle);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeFaceDetectHandle bef_effect_ai_face_attribute_create");
    ret = bef_effect_ai_face_attribute_check_license(m_faceAttributeHandle, m_featureContext->getLicensePath().c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeFaceDetectHandle bef_effect_ai_face_attribute_check_license");
}

void BEDectector::initializeSkeletonHandle() {
    bef_effect_result_t ret;
    ret = bef_effect_ai_skeleton_create(getResourceContext()->getFeatureContext()->getFeatureModelPath(BE_FEATURE_SKELETON).c_str(), &m_skeletonHandle);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initialSkeletonHandle bef_effect_ai_skeleton_create");
    
    ret = bef_effect_ai_skeleton_check_license(m_skeletonHandle, getResourceContext()->getFeatureContext()->getLicensePath().c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initialSkeletonHandle bef_effect_ai_skeleton_check_license");
    ret = bef_effect_ai_skeleton_set_targetnum(m_skeletonHandle, 1);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initialSkeletonHandle bef_effect_ai_skeleton_set_targetnum");
}


void BEDectector::initializeHandDetectHandle() {
    bef_effect_result_t ret;

    // create hand detect handle
    ret = bef_effect_ai_hand_detect_create(&m_handDetectHandle, 0);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeHandDetectHandle bef_effect_ai_hand_detect_create");

    // check license for hand model
    ret = bef_effect_ai_hand_check_license(m_handDetectHandle, m_featureContext->getLicensePath().c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeHandDetectHandle bef_effect_ai_hand_check_license");

    //init hand detecting  with model
    ret = bef_effect_ai_hand_detect_setmodel(m_handDetectHandle, BEF_AI_HAND_MODEL_DETECT, m_featureContext->getFeatureModelPath(BE_FEATURE_HAND_DETECT).c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeHandDetectHandle bef_effect_ai_hand_detect_setmodel");

    //init hand box detecting  with model
    ret = bef_effect_ai_hand_detect_setmodel(m_handDetectHandle, BEF_AI_HAND_MODEL_BOX_REG, m_featureContext->getFeatureModelPath(BE_FEATURE_HAND_BOX).c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeHandDetectHandle bef_effect_ai_hand_detect_setmodel");

    //init hand gesture detecting with model
    ret = bef_effect_ai_hand_detect_setmodel(m_handDetectHandle, BEF_AI_HAND_MODEL_GESTURE_CLS, m_featureContext->getFeatureModelPath(BE_FEATURE_HAND_GESTURE).c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeHandDetectHandle bef_effect_ai_hand_detect_setmodel");

    // init hand key point detecting with model
    ret = bef_effect_ai_hand_detect_setmodel(m_handDetectHandle, BEF_AI_HAND_MODEL_KEY_POINT, m_featureContext->getFeatureModelPath(BE_FEATURE_HAND_KEY_POINT).c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeHandDetectHandle bef_effect_ai_hand_detect_setmodel");

    // set max hand number for detect
    ret = bef_effect_ai_hand_detect_setparam(m_handDetectHandle, BEF_HAND_MAX_HAND_NUM, 1);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeHandDetectHandle bef_effect_ai_hand_detect_setparam");


    ret = bef_effect_ai_hand_detect_setparam(m_handDetectHandle, BEF_HAND_NARUTO_GESTURE, 1);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeHandDetectHandle bef_effect_ai_hand_detect_setparam");

    //set the enlarge ratio of initial rect for regression model
    ret = bef_effect_ai_hand_detect_setparam(m_handDetectHandle, BEF_HNAD_ENLARGE_FACTOR_REG, 2.0);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeHandDetectHandle bef_effect_ai_hand_detect_setparam");


}

void BEDectector::initializeHeadSegHandle() {
    bef_ai_headseg_config conf;
    conf.net_input_height = HEADSEG_INPUT_HEIGHT;
    conf.net_input_width = HEADSEG_INPUT_WIDTH;

    bef_effect_result_t ret = BEF_AI_HSeg_CreateHandler(&m_headSegHandle);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeHeadSegHandle BEF_AI_HSeg_CreateHandler");

    ret = BEF_AI_HSeg_CheckLicense(m_headSegHandle, m_featureContext->getLicensePath().c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeHeadSegHandle BEF_AI_HSeg_CheckLicense");

    ret = BEF_AI_HSeg_SetConfig(m_headSegHandle, &conf);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeHeadSegHandle BEF_AI_HSeg_SetConfig");

    ret = BEF_AI_HSeg_SetParam(m_headSegHandle, BEF_AI_HS_ENABLE_TRACKING, 1.0f);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeHeadSegHandle BEF_AI_HS_ENABLE_TRACKING");

    ret = BEF_AI_HSeg_SetParam(m_headSegHandle, BEF_AI_HS_MAX_FACE, 2);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeHeadSegHandle BEF_AI_HS_MAX_FACE");

    ret = BEF_AI_HSeg_InitModel(m_headSegHandle, m_featureContext->getFeatureModelPath(BE_FEATURE_HEAD_SEG).c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeHeadSegHandle BEF_AI_HSeg_InitModel");
}

void BEDectector::initializeHairParseHandle() {
    bef_effect_result_t ret;
    //create hair parse handle
    ret = bef_effect_ai_hairparser_create(&m_hairParseHandle);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeHairParseHandle bef_effect_ai_hairparser_create");

    //check license for hair parse
    ret = bef_effect_ai_hairparser_check_license(m_hairParseHandle, m_featureContext->getLicensePath().c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeHairParseHandle bef_effect_ai_hairparser_check_license");

    //init the handle with hair parse model
    ret = bef_effect_ai_hairparser_init_model(m_hairParseHandle, m_featureContext->getFeatureModelPath(BE_FEATURE_HAIRPARSE).c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeHairParseHandle bef_effect_ai_hairparser_init_model");

    //set param width = 128, height = 224, the others set true
    ret = bef_effect_ai_hairparser_set_param(m_hairParseHandle, 128, 224, true, true);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeHairParseHandle bef_effect_ai_hairparser_set_param");
}

void BEDectector::initializePortraitMattingHandle() {
    bef_effect_result_t ret;
    // create portrait matting handle
    ret = bef_effect_ai_portrait_matting_create(&m_portraitMattingHandle);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializePortraitMattingHandle bef_effect_ai_portrait_matting_create");

    // check the  license of portrait matting
    ret = bef_effect_ai_matting_check_license(m_portraitMattingHandle, m_featureContext->getLicensePath().c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializePortraitMattingHandle bef_effect_ai_matting_check_license");

    // initialize the handle with portrait matting model
    ret = bef_effect_ai_portrait_matting_init_model(m_portraitMattingHandle, BEF_MP_SMALL_MODEL, m_featureContext->getFeatureModelPath(BE_FEATURE_PORTRAIT_MATTING).c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializePortraitMattingHandle bef_effect_ai_portrait_matting_init_model");

    //set param of model
    ret = bef_effect_ai_portrait_matting_set_param(m_portraitMattingHandle, BEF_MP_EdgeMode, 1);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializePortraitMattingHandle bef_effect_ai_portrait_matting_set_param");

    //set param of model
    ret = bef_effect_ai_portrait_matting_set_param(m_portraitMattingHandle, BEF_MP_FrashEvery, 15);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializePortraitMattingHandle bef_effect_ai_portrait_matting_set_param");
}


void BEDectector::initializeLightClassifyHandle(bool imageMode) {

    bef_effect_result_t ret;
    //create light classify handle
    int fps = 5;
    if (imageMode) {
        fps = 1;
    }
    ret = bef_effect_ai_lightcls_create(&m_lightClassifyHandle, m_featureContext->getFeatureModelPath(BE_FEATURE_LIGHT_CLASSIFY).c_str(), fps);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeLightClassifyHandle bef_effect_ai_light_classify_create_handle");

    //check license for light classify
    ret = bef_effect_ai_lightcls_check_license(m_lightClassifyHandle, m_featureContext->getLicensePath().c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeLightClassifyHandle bef_effect_ai_light_classify_check_license");
}

void BEDectector::initializeC1DetectHandle() {
    bef_effect_result_t ret = bef_effect_ai_c1_create(&m_c1Handle, BEF_AI_C1_MODEL_SMALL, m_featureContext->getFeatureModelPath(BE_FEATURE_C1).c_str());
    
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeC1DetectHandle bef_effect_ai_c1_create");
    ret = bef_effect_ai_c1_check_license(m_c1Handle, m_featureContext->getLicensePath().c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeC1DetectHandle bef_effect_ai_c1_check_license");
    ret = bef_effect_ai_c1_set_param(m_c1Handle, BEF_AI_C1_USE_MultiLabels, 1);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeC1DetectHandle bef_effect_ai_c1_set_param");
}

void BEDectector::initializeC2DetectHandle() {
    bef_effect_result_t ret = bef_effect_ai_c2_create(&m_c2Handle);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeC2DetectHandle bef_effect_ai_c2_create");
    ret = bef_effect_ai_c2_check_license(m_c2Handle, m_featureContext->getLicensePath().c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeC2DetectHandle bef_effect_ai_c2_check_license");
    m_c2Info = bef_effect_ai_c2_init_ret(m_c2Handle);
    ret = bef_effect_ai_c2_set_model(m_c2Handle, BEF_AI_kC2Model1, m_featureContext->getFeatureModelPath(BE_FEATURE_C2).c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeC2DetectHandle bef_effect_ai_c2_set_model");
    ret = bef_effect_ai_c2_set_paramF(m_c2Handle, BEF_AI_C2_USE_VIDEO_MODE, 1);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeC2DetectHandle set_paramF BEF_AI_C2_USE_VIDEO_MODE");
    ret = bef_effect_ai_c2_set_paramF(m_c2Handle, BEF_AI_C2_USE_MultiLabels, 1);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeC2DetectHandle set_paramF BEF_AI_C2_USE_MultiLabels");
}

void BEDectector::initializeVideoDetectHandle() {
    bef_effect_result_t ret = bef_effect_ai_video_cls_create(&m_videoClsHandle);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "bef_effect_ai_video_cls_create");
    ret = bef_effect_ai_video_cls_check_license(m_videoClsHandle, m_featureContext->getLicensePath().c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "bef_effect_ai_video_cls_check_license");
    ret = bef_effect_ai_video_cls_set_model(m_videoClsHandle, BEF_AI_kVideoClsModel1, m_featureContext->getFeatureModelPath(BE_FEATURE_VIDEO).c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "bef_effect_ai_video_cls_set_model");
}

void BEDectector::initializeGazeEstimationHandle() {
    bef_effect_result_t ret;

    ret = bef_effect_ai_gaze_estimation_create_handle(&m_gazeEstimationHandle);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "bef_effect_ai_gaze_estimation_create_handle");
    ret = bef_effect_ai_gaze_estimation_check_license(m_gazeEstimationHandle, m_featureContext->getLicensePath().c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "bef_effect_ai_gaze_estimation_check_license");
    ret = bef_effect_ai_gaze_estimation_init_model(m_gazeEstimationHandle, BEF_GAZE_ESTIMATION_MODEL1, m_featureContext->getFeatureModelPath(BE_FEATURE_GAZE_ESTIMATION).c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "bef_effect_ai_gaze_estimation_init_model");
}

void BEDectector::initializePetFaceHandle() {
    bef_effect_result_t ret;
    ret = bef_effect_ai_pet_face_create(m_featureContext->getFeatureModelPath(BE_FEATURE_PET_FACE).c_str(), BEF_DetCat | BEF_DetDog, AI_MAX_PET_NUM, &m_petFaceHandle);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializePetFaceHandle bef_effect_ai_pet_face_create");
    ret = bef_effect_ai_pet_face_check_license(m_petFaceHandle, m_featureContext->getLicensePath().c_str());
}

void BEDectector::initializeFaceVerifyHandle() {
    bef_effect_result_t ret;

    ret = bef_effect_ai_face_detect_create(BEF_DETECT_MODE_IMAGE_SLOW | BEF_DETECT_SMALL_MODEL | BEF_FACE_DETECT, m_featureContext->getFeatureModelPath(BE_FEATURE_FACE).c_str(), &m_faceDetectForFaceVerifyHandle);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeFaceVerifyHandle bef_effect_ai_face_detect_create");
    ret = bef_effect_ai_face_check_license(m_faceDetectForFaceVerifyHandle, m_featureContext->getLicensePath().c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeFaceVerifyHandle bef_effect_ai_face_check_license");

    ret = bef_effect_ai_face_verify_create(getResourceContext()->getFeatureContext()->getFeatureModelPath(BE_FEATURE_FACE_VERIFY).c_str(), 1, &m_faceVerifyHandle);
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeFaceVerifyHandle bef_effect_ai_face_verify_create");
    ret = bef_effect_ai_face_verify_check_license(m_faceVerifyHandle, getResourceContext()->getFeatureContext()->getLicensePath().c_str());
    CHECK_BEF_AI_RET_SUCCESS_NO_RETURN(ret, "BEDectector::initializeFaceVerifyHandle bef_effect_ai_face_verify_check_license");
}
