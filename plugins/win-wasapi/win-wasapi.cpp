#include "enum-wasapi.hpp"

#include <obs-module.h>
#include <obs.h>
#include <util/platform.h>
#include <util/windows/HRError.hpp>
#include <util/windows/ComPtr.hpp>
#include <util/windows/WinHandle.hpp>
#include <util/windows/CoTaskMemPtr.hpp>
#include <util/threading.h>
#include <util/util_uint64.h>

#include <thread>

#include "xxq-aec.h"

using namespace std;

#define OPT_DEVICE_ID "device_id"
#define OPT_USE_DEVICE_TIMING "use_device_timing"

#define OUTPUT_RTC_SOURCE_CHANNEL MAX_CHANNELS-2

static void GetWASAPIDefaults(obs_data_t *settings);

#define OBS_KSAUDIO_SPEAKER_4POINT1 \
	(KSAUDIO_SPEAKER_SURROUND | SPEAKER_LOW_FREQUENCY)

class XXQAec;
class WASAPISource {
	ComPtr<IMMDevice> device;
	ComPtr<IAudioClient> client;
	ComPtr<IAudioCaptureClient> capture;
	ComPtr<IAudioRenderClient> render;
	ComPtr<IMMDeviceEnumerator> enumerator;
	ComPtr<IMMNotificationClient> notify;

	obs_source_t *source;
	wstring default_id;
	string device_id;
	string device_name;
	string device_sample = "-";
	uint64_t lastNotifyTime = 0;
	bool isInputDevice;
	bool useDeviceTiming = false;
	bool isDefaultDevice = false;
	std::thread defaultThread;

	bool reconnecting = false;
	bool previouslyFailed = false;
	WinHandle reconnectThread;

	bool active = false;
	WinHandle captureThread;

	WinHandle stopSignal;
	WinHandle receiveSignal;

	speaker_layout speakers;
	audio_format format;
	uint32_t sampleRate;

	CRITICAL_SECTION mutex;

	static DWORD WINAPI ReconnectThread(LPVOID param);
	static DWORD WINAPI CaptureThread(LPVOID param);

	bool ProcessCaptureData();

	inline void Start();
	inline void Stop();
	void Reconnect();

	bool InitDevice();
	void InitName();
	void InitClient();
	void InitRender();
	void InitFormat(WAVEFORMATEX *wfex);
	void InitCapture();
	void Initialize();

	bool TryInitialize();

	void UpdateSettings(obs_data_t *settings);

public:
	XXQAec *aec = nullptr;
	obs_source_t *output_rtc_source = nullptr;
	bool force_output = false;
	std::string play_device;

	static void loopback_volume(void *vptr, calldata_t *calldata);
	static void loopback_muted(void *vptr, calldata_t *calldata);

public:
	WASAPISource(obs_data_t *settings, obs_source_t *source_, bool input);
	inline ~WASAPISource();

	void Update(obs_data_t *settings);

	void SetDefaultDevice(EDataFlow flow, ERole role, LPCWSTR id);
	void OutputCaptureFail();
};

class WASAPINotify : public IMMNotificationClient {
	long refs = 0; /* auto-incremented to 1 by ComPtr */
	WASAPISource *source;

public:
	WASAPINotify(WASAPISource *source_) : source(source_) {}

	STDMETHODIMP_(ULONG) AddRef()
	{
		return (ULONG)os_atomic_inc_long(&refs);
	}

	STDMETHODIMP_(ULONG) STDMETHODCALLTYPE Release()
	{
		long val = os_atomic_dec_long(&refs);
		if (val == 0)
			delete this;
		return (ULONG)val;
	}

	STDMETHODIMP QueryInterface(REFIID riid, void **ptr)
	{
		if (riid == IID_IUnknown) {
			*ptr = (IUnknown *)this;
		} else if (riid == __uuidof(IMMNotificationClient)) {
			*ptr = (IMMNotificationClient *)this;
		} else {
			*ptr = nullptr;
			return E_NOINTERFACE;
		}

		os_atomic_inc_long(&refs);
		return S_OK;
	}

	STDMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role,
					    LPCWSTR id)
	{
		source->SetDefaultDevice(flow, role, id);
		return S_OK;
	}

	STDMETHODIMP OnDeviceAdded(LPCWSTR) { return S_OK; }
	STDMETHODIMP OnDeviceRemoved(LPCWSTR) { return S_OK; }
	STDMETHODIMP OnDeviceStateChanged(LPCWSTR, DWORD) { return S_OK; }
	STDMETHODIMP OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY)
	{
		return S_OK;
	}
};

WASAPISource::WASAPISource(obs_data_t *settings, obs_source_t *source_,
			   bool input)
	: source(source_), isInputDevice(input)
{
	if (!isInputDevice) {
		obs_source_set_audio_type(source, OBS_SOURCE_AUDIO_ONLY_RTMP);

		output_rtc_source = obs_source_create("pure_audio_input", "pure_audio", nullptr, nullptr);
		obs_source_set_audio_type(output_rtc_source, OBS_SOURCE_AUDIO_LINK);
		obs_set_output_source(OUTPUT_RTC_SOURCE_CHANNEL, output_rtc_source);
		obs_source_release(output_rtc_source);

		auto sig_handler = obs_source_get_signal_handler(source);
		signal_handler_connect(sig_handler, "volume", loopback_volume, this);
		signal_handler_connect(sig_handler, "mute", loopback_muted, this);
	}

	InitializeCriticalSection(&mutex);

	UpdateSettings(settings);

	stopSignal = CreateEvent(nullptr, true, false, nullptr);
	if (!stopSignal.Valid())
		throw "Could not create stop signal";

	receiveSignal = CreateEvent(nullptr, false, false, nullptr);
	if (!receiveSignal.Valid())
		throw "Could not create receive signal";

	Start();
}

void WASAPISource::loopback_volume(void *vptr, calldata_t *calldata)
{
	WASAPISource *was = (WASAPISource *)vptr;
	float mul = (float)calldata_float(calldata, "volume");
	obs_source_set_volume(was->output_rtc_source, mul);
}

void WASAPISource::loopback_muted(void *vptr, calldata_t *calldata)
{
	WASAPISource *was = (WASAPISource *)vptr;
	bool muted = (bool)calldata_bool(calldata, "muted");
	obs_source_set_muted(was->output_rtc_source, muted);
}

inline void WASAPISource::Start()
{
	if (!TryInitialize()) {
		blog(LOG_INFO,
		     "[WASAPISource::WASAPISource] "
		     "Device '%s' not found.  Waiting for device",
		     device_id.c_str());
		Reconnect();
	}
}

inline void WASAPISource::Stop()
{
	SetEvent(stopSignal);

	if (active) {
		blog(LOG_INFO, "WASAPI: Device '%s' Terminated",
		     device_name.c_str());
		WaitForSingleObject(captureThread, INFINITE);
	}

	if (reconnecting)
		WaitForSingleObject(reconnectThread, INFINITE);

	ResetEvent(stopSignal);
}

inline WASAPISource::~WASAPISource()
{
	if(defaultThread.joinable())
		defaultThread.join();

	enumerator->UnregisterEndpointNotificationCallback(notify);
	Stop();

	DeleteCriticalSection(&mutex);

	if (!isInputDevice) {
		auto sig_handler = obs_source_get_signal_handler(source);
		signal_handler_disconnect(sig_handler, "volume", loopback_volume, this);
		signal_handler_disconnect(sig_handler, "muted", loopback_muted, this);
	}

	if (aec) {
		delete aec;
		aec = nullptr;
	}
	
	if (output_rtc_source) {
		obs_set_output_source(OUTPUT_RTC_SOURCE_CHANNEL, nullptr);
		output_rtc_source = nullptr;
	}
}

void WASAPISource::UpdateSettings(obs_data_t *settings)
{
	device_id = obs_data_get_string(settings, OPT_DEVICE_ID);
	useDeviceTiming = obs_data_get_bool(settings, OPT_USE_DEVICE_TIMING);
	isDefaultDevice = _strcmpi(device_id.c_str(), "default") == 0;
}

void WASAPISource::Update(obs_data_t *settings)
{
	string newDevice = obs_data_get_string(settings, OPT_DEVICE_ID);
	bool restart = newDevice.compare(device_id) != 0;

	if (restart)
		Stop();

	UpdateSettings(settings);

	if (restart)
		Start();
}

bool WASAPISource::InitDevice()
{
	HRESULT res;

	if (isDefaultDevice) {
		res = enumerator->GetDefaultAudioEndpoint(
			isInputDevice ? eCapture : eRender,
			isInputDevice ? eCommunications : eConsole,
			device.Assign());
		if (FAILED(res))
			return false;

		CoTaskMemPtr<wchar_t> id;
		res = device->GetId(&id);
		default_id = id;

	} else {
		wchar_t *w_id;
		os_utf8_to_wcs_ptr(device_id.c_str(), device_id.size(), &w_id);

		res = enumerator->GetDevice(w_id, device.Assign());

		bfree(w_id);
	}

	return SUCCEEDED(res);
}

#define BUFFER_TIME_100NS (5 * 10000000)

void WASAPISource::InitClient()
{
	CoTaskMemPtr<WAVEFORMATEX> wfex;
	HRESULT res;
	DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

	res = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
			       (void **)client.Assign());
	if (FAILED(res))
		throw HRError("Failed to activate client context", res);

	res = client->GetMixFormat(&wfex);
	if (FAILED(res))
		throw HRError("Failed to get mix format", res);

	InitFormat(wfex);

	if (!isInputDevice)
		flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;

	res = client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags,
				 BUFFER_TIME_100NS, 0, wfex, nullptr);
	if (FAILED(res))
		throw HRError("Failed to get initialize audio client", res);
}

void WASAPISource::OutputCaptureFail()
{
	obs_data_t *event = obs_data_create();
	obs_data_set_string(event, "type", "Fail2InitRender");
	struct calldata cd;
	uint8_t stack[512];
	calldata_init_fixed(&cd, stack, sizeof(stack));
	calldata_set_ptr(&cd, "event", event);
	signal_handler_signal(obs_get_signal_handler(), "obs_global_event", &cd);
	obs_data_release(event);
}

void WASAPISource::InitRender()
{
	CoTaskMemPtr<WAVEFORMATEX> wfex;
	HRESULT res;
	LPBYTE buffer;
	UINT32 frames;
	ComPtr<IAudioClient> client;

	res = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
			       (void **)client.Assign());
	if (FAILED(res))
		throw HRError("Failed to activate client context", res);

	res = client->GetMixFormat(&wfex);
	if (FAILED(res))
		throw HRError("Failed to get mix format", res);

	res = client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, BUFFER_TIME_100NS,
				 0, wfex, nullptr);
	if (FAILED(res)) {
		OutputCaptureFail();
		throw HRError("Failed to get initialize audio client", res);
	}

	/* Silent loopback fix. Prevents audio stream from stopping and */
	/* messing up timestamps and other weird glitches during silence */
	/* by playing a silent sample all over again. */

	res = client->GetBufferSize(&frames);
	if (FAILED(res))
		throw HRError("Failed to get buffer size", res);

	res = client->GetService(__uuidof(IAudioRenderClient),
				 (void **)render.Assign());
	if (FAILED(res))
		throw HRError("Failed to get render client", res);

	res = render->GetBuffer(frames, &buffer);
	if (FAILED(res))
		throw HRError("Failed to get buffer", res);

	memset(buffer, 0, frames * wfex->nBlockAlign);

	render->ReleaseBuffer(frames, 0);
}

static speaker_layout ConvertSpeakerLayout(DWORD layout, WORD channels)
{
	switch (layout) {
	case KSAUDIO_SPEAKER_2POINT1:
		return SPEAKERS_2POINT1;
	case KSAUDIO_SPEAKER_SURROUND:
		return SPEAKERS_4POINT0;
	case OBS_KSAUDIO_SPEAKER_4POINT1:
		return SPEAKERS_4POINT1;
	case KSAUDIO_SPEAKER_5POINT1_SURROUND:
		return SPEAKERS_5POINT1;
	case KSAUDIO_SPEAKER_7POINT1_SURROUND:
		return SPEAKERS_7POINT1;
	}

	return (speaker_layout)channels;
}

void WASAPISource::InitFormat(WAVEFORMATEX *wfex)
{
	DWORD layout = 0;

	if (wfex->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		WAVEFORMATEXTENSIBLE *ext = (WAVEFORMATEXTENSIBLE *)wfex;
		layout = ext->dwChannelMask;
	}

	/* WASAPI is always float */
	sampleRate = wfex->nSamplesPerSec;
	format = AUDIO_FORMAT_FLOAT;
	speakers = ConvertSpeakerLayout(layout, wfex->nChannels);
}

void WASAPISource::InitCapture()
{
	HRESULT res = client->GetService(__uuidof(IAudioCaptureClient),
					 (void **)capture.Assign());
	if (FAILED(res))
		throw HRError("Failed to create capture context", res);

	res = client->SetEventHandle(receiveSignal);
	if (FAILED(res))
		throw HRError("Failed to set event handle", res);

	captureThread = CreateThread(nullptr, 0, WASAPISource::CaptureThread,
				     this, 0, nullptr);
	if (!captureThread.Valid())
		throw "Failed to create capture thread";
	
	if (!isInputDevice) {
		if (aec) {
			delete aec;
			aec = nullptr;
		}
		aec = new XXQAec;
		aec->initResamplers(sampleRate, format, speakers);
	}

	client->Start();
	active = true;

	blog(LOG_INFO, "WASAPI: Device '%s' [%s Hz] initialized",
	     device_name.c_str(), device_sample.c_str());
}

void WASAPISource::Initialize()
{
	HRESULT res;

	res = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
			       CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
			       (void **)enumerator.Assign());
	if (FAILED(res))
		throw HRError("Failed to create enumerator", res);

	if (!InitDevice())
		return;

	device_name = GetDeviceName(device);

	if (!notify) {
		notify = new WASAPINotify(this);
		enumerator->RegisterEndpointNotificationCallback(notify);
	}

	HRESULT resSample;
	IPropertyStore *store = nullptr;
	PWAVEFORMATEX deviceFormatProperties;
	PROPVARIANT prop;
	resSample = device->OpenPropertyStore(STGM_READ, &store);
	if (!FAILED(resSample)) {
		resSample =
			store->GetValue(PKEY_AudioEngine_DeviceFormat, &prop);
		if (!FAILED(resSample)) {
			if (prop.vt != VT_EMPTY && prop.blob.pBlobData) {
				deviceFormatProperties =
					(PWAVEFORMATEX)prop.blob.pBlobData;
				device_sample = std::to_string(
					deviceFormatProperties->nSamplesPerSec);
			}
		}

		store->Release();
	}

	InitClient();
	if (!isInputDevice)
		InitRender();
	InitCapture();
}

bool WASAPISource::TryInitialize()
{
	try {
		Initialize();

	} catch (HRError &error) {
		if (previouslyFailed)
			return active;

		blog(LOG_WARNING, "[WASAPISource::TryInitialize]:[%s] %s: %lX",
		     device_name.empty() ? device_id.c_str()
					 : device_name.c_str(),
		     error.str, error.hr);

	} catch (const char *error) {
		if (previouslyFailed)
			return active;

		blog(LOG_WARNING, "[WASAPISource::TryInitialize]:[%s] %s",
		     device_name.empty() ? device_id.c_str()
					 : device_name.c_str(),
		     error);
	}

	previouslyFailed = !active;
	return active;
}

void WASAPISource::Reconnect()
{
	reconnecting = true;
	reconnectThread = CreateThread(
		nullptr, 0, WASAPISource::ReconnectThread, this, 0, nullptr);

	if (!reconnectThread.Valid())
		blog(LOG_WARNING,
		     "[WASAPISource::Reconnect] "
		     "Failed to initialize reconnect thread: %lu",
		     GetLastError());
}

static inline bool WaitForSignal(HANDLE handle, DWORD time)
{
	return WaitForSingleObject(handle, time) != WAIT_TIMEOUT;
}

#define RECONNECT_INTERVAL 3000

DWORD WINAPI WASAPISource::ReconnectThread(LPVOID param)
{
	WASAPISource *source = (WASAPISource *)param;

	os_set_thread_name("win-wasapi: reconnect thread");

	const HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	const bool com_initialized = SUCCEEDED(hr);
	if (!com_initialized) {
		blog(LOG_ERROR,
		     "[WASAPISource::ReconnectThread]"
		     " CoInitializeEx failed: 0x%08X",
		     hr);
	}

	obs_monitoring_type type =
		obs_source_get_monitoring_type(source->source);
	obs_source_set_monitoring_type(source->source,
				       OBS_MONITORING_TYPE_NONE);

	while (!WaitForSignal(source->stopSignal, RECONNECT_INTERVAL)) {
		if (source->TryInitialize())
			break;
	}

	obs_source_set_monitoring_type(source->source, type);

	if (com_initialized)
		CoUninitialize();

	source->reconnectThread = nullptr;
	source->reconnecting = false;
	return 0;
}

bool WASAPISource::ProcessCaptureData()
{
	HRESULT res;
	LPBYTE buffer;
	UINT32 frames;
	DWORD flags;
	UINT64 pos, ts;
	UINT captureSize = 0;

	while (true) {
		res = capture->GetNextPacketSize(&captureSize);

		if (FAILED(res)) {
			if (res != AUDCLNT_E_DEVICE_INVALIDATED)
				blog(LOG_WARNING,
				     "[WASAPISource::GetCaptureData]"
				     " capture->GetNextPacketSize"
				     " failed: %lX",
				     res);
			return false;
		}

		if (!captureSize)
			break;

		res = capture->GetBuffer(&buffer, &frames, &flags, &pos, &ts);
		if (FAILED(res)) {
			if (res != AUDCLNT_E_DEVICE_INVALIDATED)
				blog(LOG_WARNING,
				     "[WASAPISource::GetCaptureData]"
				     " capture->GetBuffer"
				     " failed: %lX",
				     res);
			return false;
		}

		obs_source_audio data = {};
		data.data[0] = buffer;
		data.frames = (uint32_t)frames;
		data.speakers = speakers;
		data.samples_per_sec = sampleRate;
		data.format = format;
		data.timestamp = useDeviceTiming ? ts * 100 : os_gettime_ns();

		if (!useDeviceTiming)
			data.timestamp -= util_mul_div64(frames, 1000000000ULL,
							 sampleRate);

		obs_source_output_audio(source, &data);

		if (!isInputDevice) {
			std::string did = device_id;
			if (device_id == "default") {
				char *buf = NULL;
				os_wcs_to_utf8_ptr(default_id.c_str(), default_id.size(), &buf);
				did = buf;
				bfree(buf);
			}

			uint8_t *audio_data = nullptr;
			bool processed = aec->processData(did == play_device, buffer, frames, &audio_data);
			if (processed) {				
				data.data[0] = audio_data;
				obs_source_output_audio(output_rtc_source, &data);
			} else {
				if (force_output)
					obs_source_output_audio(output_rtc_source, &data);
			}
		} 

		capture->ReleaseBuffer(frames);
	}

	return true;
}

static inline bool WaitForCaptureSignal(DWORD numSignals, const HANDLE *signals,
					DWORD duration)
{
	DWORD ret;
	ret = WaitForMultipleObjects(numSignals, signals, false, duration);

	return ret == WAIT_OBJECT_0 || ret == WAIT_TIMEOUT;
}

DWORD WINAPI WASAPISource::CaptureThread(LPVOID param)
{
	WASAPISource *source = (WASAPISource *)param;
	bool reconnect = false;

	/* Output devices don't signal, so just make it check every 10 ms */
	DWORD dur = source->isInputDevice ? RECONNECT_INTERVAL : 10;

	HANDLE sigs[2] = {source->receiveSignal, source->stopSignal};

	os_set_thread_name("win-wasapi: capture thread");

	while (WaitForCaptureSignal(2, sigs, dur)) {
		if (!source->ProcessCaptureData()) {
			reconnect = true;
			break;
		}
	}

	source->client->Stop();

	source->captureThread = nullptr;
	source->active = false;

	if (reconnect) {
		blog(LOG_INFO, "Device '%s' invalidated.  Retrying",
		     source->device_name.c_str());
		source->Reconnect();
	}

	return 0;
}

void WASAPISource::SetDefaultDevice(EDataFlow flow, ERole role, LPCWSTR id)
{
	if (!isDefaultDevice)
		return;

	EDataFlow expectedFlow = isInputDevice ? eCapture : eRender;
	ERole expectedRole = isInputDevice ? eCommunications : eConsole;

	if (flow != expectedFlow || role != expectedRole)
		return;
	if (id && default_id.compare(id) == 0)
		return;

	blog(LOG_INFO, "WASAPI: Default %s device changed",
	     isInputDevice ? "input" : "output");

	/* reset device only once every 300ms */
	uint64_t t = os_gettime_ns();
	if (t - lastNotifyTime < 300000000)
		return;

	if (defaultThread.joinable())
		defaultThread.join();

	defaultThread = std::move(std::thread([this]() {
		EnterCriticalSection(&mutex);
		Stop();
		Start();
		LeaveCriticalSection(&mutex);
	}));

	lastNotifyTime = t;
}

/* ------------------------------------------------------------------------- */

static const char *GetWASAPIInputName(void *)
{
	return obs_module_text("AudioInput");
}

static const char *GetWASAPIOutputName(void *)
{
	return obs_module_text("AudioOutput");
}

static void GetWASAPIDefaultsInput(obs_data_t *settings)
{
	obs_data_set_default_string(settings, OPT_DEVICE_ID, "default");
	obs_data_set_default_bool(settings, OPT_USE_DEVICE_TIMING, false);
}

static void GetWASAPIDefaultsOutput(obs_data_t *settings)
{
	obs_data_set_default_string(settings, OPT_DEVICE_ID, "default");
	obs_data_set_default_bool(settings, OPT_USE_DEVICE_TIMING, true);
}

static void *CreateWASAPISource(obs_data_t *settings, obs_source_t *source,
				bool input)
{
	try {
		return new WASAPISource(settings, source, input);
	} catch (const char *error) {
		blog(LOG_ERROR, "[CreateWASAPISource] %s", error);
	}

	return nullptr;
}

static void *CreateWASAPIInput(obs_data_t *settings, obs_source_t *source)
{
	return CreateWASAPISource(settings, source, true);
}

static void *CreateWASAPIOutput(obs_data_t *settings, obs_source_t *source)
{
	return CreateWASAPISource(settings, source, false);
}

static void DestroyWASAPISource(void *obj)
{
	delete static_cast<WASAPISource *>(obj);
}

static void UpdateWASAPISource(void *obj, obs_data_t *settings)
{
	static_cast<WASAPISource *>(obj)->Update(settings);
}

static obs_properties_t *GetWASAPIProperties(bool input)
{
	obs_properties_t *props = obs_properties_create();
	vector<AudioDeviceInfo> devices;

	obs_property_t *device_prop = obs_properties_add_list(
		props, OPT_DEVICE_ID, obs_module_text("Device"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	GetWASAPIAudioDevices(devices, input);

	if (devices.size())
		obs_property_list_add_string(
			device_prop, obs_module_text("Default"), "default");

	for (size_t i = 0; i < devices.size(); i++) {
		AudioDeviceInfo &device = devices[i];
		obs_property_list_add_string(device_prop, device.name.c_str(),
					     device.id.c_str());
	}

	obs_properties_add_bool(props, OPT_USE_DEVICE_TIMING,
				obs_module_text("UseDeviceTiming"));

	return props;
}

static obs_properties_t *GetWASAPIPropertiesInput(void *)
{
	return GetWASAPIProperties(true);
}

static obs_properties_t *GetWASAPIPropertiesOutput(void *)
{
	return GetWASAPIProperties(false);
}

static void MakeWASAPICommand(void *param, obs_data_t *command)
{
	WASAPISource *was = (WASAPISource *)param;
	int type = obs_data_get_int(command, "type");
	if (type == 0)
		was->force_output = obs_data_get_bool(command, "force_output");
	else if (type == 1) {
		was->play_device = obs_data_get_string(command, "play_device");
	}
}

void RegisterWASAPIInput()
{
	obs_source_info info = {};
	info.id = "wasapi_input_capture";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;
	info.get_name = GetWASAPIInputName;
	info.create = CreateWASAPIInput;
	info.destroy = DestroyWASAPISource;
	info.update = UpdateWASAPISource;
	info.get_defaults = GetWASAPIDefaultsInput;
	info.get_properties = GetWASAPIPropertiesInput;
	obs_register_source(&info);
}

void RegisterWASAPIOutput()
{
	obs_source_info info = {};
	info.id = "wasapi_output_capture";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE |
			    OBS_SOURCE_DO_NOT_SELF_MONITOR;
	info.get_name = GetWASAPIOutputName;
	info.create = CreateWASAPIOutput;
	info.destroy = DestroyWASAPISource;
	info.update = UpdateWASAPISource;
	info.get_defaults = GetWASAPIDefaultsOutput;
	info.get_properties = GetWASAPIPropertiesOutput;
	info.make_command = MakeWASAPICommand;
	obs_register_source(&info);
}
