#include <obs-module.h>
#include <util/circlebuf.h>
#include <thread>
#include <util/windows/win-version.h>

#if defined(__APPLE__)
#include <onnxruntime/core/session/onnxruntime_cxx_api.h>
#include <onnxruntime/core/providers/cpu/cpu_provider_factory.h>
#else
#include <onnxruntime_cxx_api.h>
#include <cpu_provider_factory.h>
#endif
#ifdef _WIN32
#include <dml_provider_factory.h>
#include <wchar.h>
#endif

#include <opencv2/imgproc.hpp>

#include <numeric>
#include <memory>
#include <exception>
#include <fstream>

#include "plugin.h"
#include "Model.h"

const char *MODEL_RVM = "rvm_mobilenetv3_fp32.onnx";

struct background_removal_filter {
	background_removal_filter() {}

	obs_source_t *source = nullptr;

	std::unique_ptr<Ort::Session> session;
	std::unique_ptr<Ort::Env> env;
	std::vector<const char *> inputNames;
	std::vector<const char *> outputNames;
	std::vector<Ort::Value> inputTensor;
	std::vector<Ort::Value> outputTensor;
	std::vector<std::vector<int64_t>> inputDims;
	std::vector<std::vector<int64_t>> outputDims;
	std::vector<std::vector<float>> outputTensorValues;
	std::vector<std::vector<float>> inputTensorValues;
	float threshold = 0.5f;
	float smoothContour = 0.5f;
	bool useGPU = true;
	std::unique_ptr<Model> model;

	cv::Mat backgroundMaskCache;
	cv::Mat backgroundMask;
	int maskEveryXFrames = 1;
	int maskEveryXFramesCount = 0;

	bool running = true;
	std::thread process_th;
	std::mutex mutex;
	std::condition_variable cv;

	size_t cache_input_width = 0, cache_input_height = 0;
	std::vector<uint8_t> cache_input_buffer;
	size_t cache_output_width = 0, cache_output_height = 0;
	std::vector<uint8_t> cache_output_buffer;
	bool input_available = false;
	bool out_available = false;

#if _WIN32
	const wchar_t *modelFilepath = nullptr;
#else
	const char *modelFilepath = nullptr;
#endif
};

static const char *filter_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Background Removal";
}

/**                   PROPERTIES                     */

static obs_properties_t *filter_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_property_t *p_threshold = obs_properties_add_float_slider(props, "threshold", obs_module_text("Threshold"), 0.0, 1.0, 0.025);

	obs_property_t *p_smooth_contour = obs_properties_add_float_slider(props, "smooth_contour", obs_module_text("Smooth silhouette"), 0.0, 1.0, 0.05);

	obs_properties_add_bool(props, "useGPU", "useGPU");

	obs_property_t *p_mask_every_x_frames =
		obs_properties_add_int(props, "mask_every_x_frames", obs_module_text("Calculate mask every X frame"), 1, 300, 1);

	UNUSED_PARAMETER(data);
	return props;
}

static void filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, "threshold", 0.5);
	obs_data_set_default_double(settings, "smooth_contour", 0.5);
	obs_data_set_default_bool(settings, "useGPU", true);
	obs_data_set_default_int(settings, "mask_every_x_frames", 1);
}

static bool createOrtSession(struct background_removal_filter *tf)
{
	Ort::SessionOptions sessionOptions;

	sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
	if (tf->useGPU) {
		sessionOptions.DisableMemPattern();
		sessionOptions.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
	}

	char *modelFilepath_rawPtr = obs_module_file(MODEL_RVM);

	if (modelFilepath_rawPtr == nullptr) {
		blog(LOG_ERROR, "Unable to get model filename %s from plugin.", MODEL_RVM);
		return false;
	}

	std::string modelFilepath_s(modelFilepath_rawPtr);
	bfree(modelFilepath_rawPtr);

#if _WIN32
	std::wstring modelFilepath_ws(modelFilepath_s.size(), L' ');
	std::copy(modelFilepath_s.begin(), modelFilepath_s.end(), modelFilepath_ws.begin());
	tf->modelFilepath = modelFilepath_ws.c_str();
#else
	tf->modelFilepath = modelFilepath_s.c_str();
#endif

	try {
		if (tf->useGPU) {
			Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_DML(sessionOptions, 0));
		}
		tf->session.reset(new Ort::Session(*tf->env, tf->modelFilepath, sessionOptions));
	} catch (const std::exception &e) {
		blog(LOG_ERROR, "%s", e.what());
		return false;
	}

	Ort::AllocatorWithDefaultOptions allocator;

	tf->model->populateInputOutputNames(tf->session, tf->inputNames, tf->outputNames);

	if (!tf->model->populateInputOutputShapes(tf->session, tf->inputDims, tf->outputDims)) {
		blog(LOG_ERROR, "Unable to get model input and output shapes");
		return false;
	}

	for (size_t i = 0; i < tf->inputNames.size(); i++) {
		blog(LOG_INFO, "Model %s input %d: name %s shape (%d dim) %d x %d x %d x %d", MODEL_RVM, (int)i, tf->inputNames[i],
		     (int)tf->inputDims[i].size(), (int)tf->inputDims[i][0], ((int)tf->inputDims[i].size() > 1) ? (int)tf->inputDims[i][1] : 0,
		     ((int)tf->inputDims[i].size() > 2) ? (int)tf->inputDims[i][2] : 0, ((int)tf->inputDims[i].size() > 3) ? (int)tf->inputDims[i][3] : 0);
	}
	for (size_t i = 0; i < tf->outputNames.size(); i++) {
		blog(LOG_INFO, "Model %s output %d: name %s shape (%d dim) %d x %d x %d x %d", MODEL_RVM, (int)i, tf->outputNames[i],
		     (int)tf->outputDims[i].size(), (int)tf->outputDims[i][0], ((int)tf->outputDims[i].size() > 1) ? (int)tf->outputDims[i][1] : 0,
		     ((int)tf->outputDims[i].size() > 2) ? (int)tf->outputDims[i][2] : 0, ((int)tf->outputDims[i].size() > 3) ? (int)tf->outputDims[i][3] : 0);
	}

	// Allocate buffers
	tf->model->allocateTensorBuffers(tf->inputDims, tf->outputDims, tf->outputTensorValues, tf->inputTensorValues, tf->inputTensor, tf->outputTensor);
	return true;
}

static void filter_update(void *data, obs_data_t *settings)
{
	struct background_removal_filter *tf = reinterpret_cast<background_removal_filter *>(data);

	std::unique_lock<std::mutex> lk1(tf->mutex);

	tf->threshold = (float)obs_data_get_double(settings, "threshold");

	tf->smoothContour = (float)obs_data_get_double(settings, "smooth_contour");
	tf->maskEveryXFrames = (int)obs_data_get_int(settings, "mask_every_x_frames");
	tf->maskEveryXFramesCount = (int)(0);
}

/**                   FILTER CORE                     */

static void processImageForBackground(struct background_removal_filter *tf, const cv::Mat &imageRGBA, cv::Mat &backgroundMask)
{
	if (tf->session.get() == nullptr) {
		// Onnx runtime session is not initialized. Problem in initialization
		return;
	}
	try {
		// Resize to network input size
		uint32_t inputWidth, inputHeight;
		tf->model->getNetworkInputSize(tf->inputDims, inputWidth, inputHeight);

		cv::Mat resizedImageRGB;
		cv::resize(imageRGBA, resizedImageRGB, cv::Size(inputWidth, inputHeight));

		// Prepare input to nework
		cv::Mat resizedImage, preprocessedImage;
		resizedImageRGB.convertTo(resizedImage, CV_32F);

		tf->model->prepareInputToNetwork(resizedImage, preprocessedImage);

		tf->model->loadInputToTensor(preprocessedImage, inputWidth, inputHeight, tf->inputTensorValues);

		// Run network inference
		tf->model->runNetworkInference(tf->session, tf->inputNames, tf->outputNames, tf->inputTensor, tf->outputTensor);

		// Get output
		// Map network output mask to cv::Mat
		cv::Mat outputImage = tf->model->getNetworkOutput(tf->outputDims, tf->outputTensorValues, tf->inputDims, tf->inputTensorValues);

		backgroundMask = outputImage < tf->threshold;

		// Resize the size of the mask back to the size of the original input.
		cv::resize(backgroundMask, backgroundMask, imageRGBA.size());

		// Smooth mask with a fast filter (box).
		if (tf->smoothContour > 0.0) {
			int k_size = (int)(100 * tf->smoothContour);
			cv::boxFilter(backgroundMask, backgroundMask, backgroundMask.depth(), cv::Size(k_size, k_size));
			backgroundMask = backgroundMask > 128;
		}
	} catch (const std::exception &e) {
		blog(LOG_ERROR, "%s", e.what());
	}
}

static void process_frame(void *data)
{
	struct background_removal_filter *tf = reinterpret_cast<background_removal_filter *>(data);
	while (tf->running) {
		uint8_t *buffer = nullptr;
		size_t width = 0, height = 0;
		size_t size = 0;
		{
			std::unique_lock<std::mutex> lk(tf->mutex);
			tf->cv.wait(lk);
			if (!tf->running)
				break;

			if (!tf->input_available)
				continue;

			width = tf->cache_input_width;
			height = tf->cache_input_height;
			size = width * height * 4;
			buffer = (uint8_t *)bmalloc(size);

			memcpy(buffer, tf->cache_input_buffer.data(), size);
			tf->input_available = false;
		}

		if (!buffer)
			continue;

		auto settings = obs_source_get_settings(tf->source);
		bool newUseGpu = obs_data_get_bool(settings, "useGPU");
		obs_data_release(settings);

		if (!tf->model || tf->useGPU != newUseGpu) {
			// Re-initialize model if it's not already the selected one or switching inference device
			tf->useGPU = newUseGpu;
			tf->model.reset(new ModelRVM);

			if (!createOrtSession(tf)) {
				bfree(buffer);
				break;
			}
		}

		auto ts = os_gettime_ns();
		struct background_removal_filter *tf = reinterpret_cast<background_removal_filter *>(data);

		cv::Mat imageRGBA(height, width, CV_8UC4, buffer);

		tf->maskEveryXFramesCount = ++(tf->maskEveryXFramesCount) % tf->maskEveryXFrames;
		if (tf->maskEveryXFramesCount != 0 && !tf->backgroundMaskCache.empty()) {
			// We are skipping processing of the mask for this frame.
			// Get the background mask previously generated.
			tf->backgroundMaskCache.copyTo(tf->backgroundMask);
		} else {
			// Process the image to find the mask.
			processImageForBackground(tf, imageRGBA, tf->backgroundMask);

			// Now that the mask is completed, save it off so it can be used on a later frame
			// if we've chosen to only process the mask every X frames.
			tf->backgroundMask.copyTo(tf->backgroundMaskCache);
		}

		// Apply the mask back to the main image.
		try {
			cv::Mat maskFloat;
			int k_size = (int)(40 * 0.5);
			cv::boxFilter(tf->backgroundMask, maskFloat, tf->backgroundMask.depth(), cv::Size(k_size, k_size));
			unsigned char *maskFloatData = maskFloat.data;
			imageRGBA.forEach<cv::Vec4b>([maskFloatData, width](cv::Vec4b &pixel, const int *position) -> void {
				pixel[3] = 255 - *(maskFloatData + position[0] * width + position[1]);
			});
		} catch (const std::exception &e) {
			blog(LOG_ERROR, "%s", e.what());
		}

		std::unique_lock<std::mutex> lk(tf->mutex);
		if (tf->cache_output_buffer.size() < size) {
			tf->cache_output_buffer.resize(size);
			memset(tf->cache_output_buffer.data(), 0, size);
		}
		tf->cache_output_width = width;
		tf->cache_output_height = height;
		memcpy(tf->cache_output_buffer.data(), buffer, size);
		bfree(buffer);
		tf->out_available = true;
	}
}

uint32_t GetWindowsVersion()
{
    static uint32_t ver = 0;

    if(ver == 0)
    {
        struct win_version_info ver_info;

        get_win_ver(&ver_info);
        ver = (ver_info.major << 8) | ver_info.minor;
    }

    return ver;
}

static void *filter_create(obs_data_t *settings, obs_source_t *source)
{
	uint32_t winVer = GetWindowsVersion();
	if (winVer > 0 && winVer < 0x602) {
		return nullptr;
	}

	struct background_removal_filter *tf = new struct background_removal_filter;

	tf->source = source;

	std::string instanceName{"background-removal-inference"};
	tf->env.reset(new Ort::Env(OrtLoggingLevel::ORT_LOGGING_LEVEL_ERROR, instanceName.c_str()));

	filter_update(tf, settings);

	tf->process_th = std::thread{process_frame, tf};

	return tf;
}

static struct obs_source_frame *filter_render(void *data, struct obs_source_frame *frame)
{
	if (frame->format != VIDEO_FORMAT_RGBA)
		return frame;

	struct background_removal_filter *tf = reinterpret_cast<background_removal_filter *>(data);
	std::unique_lock<std::mutex> lk1(tf->mutex);
	auto size = frame->width * frame->height * 4;
	if (tf->cache_input_buffer.size() < size)
		tf->cache_input_buffer.resize(size);

	memcpy(tf->cache_input_buffer.data(), frame->data[0], size);
	tf->input_available = true;
	tf->cache_input_width = frame->width;
	tf->cache_input_height = frame->height;
	tf->cv.notify_all();

	if (tf->out_available && tf->cache_output_width == frame->width && tf->cache_output_height == frame->height) {
		memcpy(frame->data[0], tf->cache_output_buffer.data(), size);
		tf->out_available = false;
		return frame;
	} else {
		obs_source_release_frame(obs_filter_get_parent(tf->source), frame);
		return nullptr;
	}
}

static void filter_destroy(void *data)
{
	struct background_removal_filter *tf = reinterpret_cast<background_removal_filter *>(data);

	if (tf) {
		tf->running = false;
		tf->cv.notify_all();
		if (tf->process_th.joinable())
			tf->process_th.join();

		delete tf;
	}
}

void register_filter()
{
	struct obs_source_info background_removal_filter_info = {0};
	background_removal_filter_info.id = "background_removal";
	background_removal_filter_info.type = OBS_SOURCE_TYPE_FILTER;
	background_removal_filter_info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_ASYNC;
	background_removal_filter_info.get_name = filter_getname;
	background_removal_filter_info.create = filter_create;
	background_removal_filter_info.destroy = filter_destroy;
	background_removal_filter_info.get_defaults = filter_defaults;
	background_removal_filter_info.get_properties = filter_properties;
	background_removal_filter_info.update = filter_update;
	background_removal_filter_info.filter_video = filter_render;
	obs_register_source(&background_removal_filter_info);
}
