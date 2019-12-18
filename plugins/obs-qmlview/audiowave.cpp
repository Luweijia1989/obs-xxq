#include "audiowave.h"
#include "renderer.h"
#include <QDebug>


const int AudioRectCount = 74;

AudioInfoModel::AudioInfoModel(AudioWave *w, QObject *parent)
	: QAbstractListModel(parent),
	audiowave(w) {
	audioInfos.resize(AudioRectCount);
	QLinearGradient linearGrad(QPointF(AudioRectCount, 0), QPointF(0, 0));
	linearGrad.setColorAt(0, QColor(50, 197, 255));
	linearGrad.setColorAt(0.5, QColor(182, 32, 224));
	linearGrad.setColorAt(1, QColor(247, 181, 0));
	QBrush brush(linearGrad);
	brush.setStyle(Qt::LinearGradientPattern);

	QImage image(AudioRectCount, 10, QImage::Format_RGB32);
	QPainter painter(&image);
	painter.setPen(Qt::NoPen);
	painter.setBrush(brush);
	painter.drawRect(0, 0, AudioRectCount, 10);

	for (int i = 0; i < AudioRectCount; i++) {
		AudioInfo &info = audioInfos[i];
		info.color = image.pixelColor(i, 1).name(QColor::HexRgb);
	}

	updateTimer.setInterval(100);
	connect(&updateTimer, &QTimer::timeout, this, &AudioInfoModel::onTimerUpdate);
}

QVariant AudioInfoModel::data(const QModelIndex &index, int role) const {
	if (index.row() < 0 || index.row() >= audioInfos.count())
		return QVariant();

	const AudioInfo &d = audioInfos[index.row()];
	switch (role) {
	case RoleColor:
		return d.color;
	case RoleAudioLevel:
		return d.audioLevel;
	default:
		return QVariant();
	}
}

int AudioInfoModel::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent)
		return audioInfos.count();
}

QHash<int, QByteArray> AudioInfoModel::roleNames() const {
	QHash<int, QByteArray> roles;
	roles[RoleColor] = "rectColor";
	roles[RoleAudioLevel] = "audioLevel";
	return roles;
}

void AudioInfoModel::start() {
	updateTimer.start();
}

void AudioInfoModel::stop() {
	updateTimer.stop();
}

void AudioInfoModel::onTimerUpdate() {
	QByteArray micData;
	QByteArray desktopData;
	audiowave->dataMutex.lock();
	micData = audiowave->audioData;
	desktopData = audiowave->audioDesktopData;
	audiowave->audioData.clear();
	audiowave->audioDesktopData.clear();
	audiowave->dataMutex.unlock();

	int f1 = micData.size() / sizeof(float);
	int f2 = desktopData.size() / sizeof(float);
	float *fd1 = (float *)micData.data();
	float *fd2 = (float *)desktopData.data();
	int finalLen = f1 >= f2 ? f1 : f2;

	if (finalAudioData.size() < finalLen * sizeof(float))
		finalAudioData.resize(finalLen * sizeof(float));

	float *finalData = (float *)finalAudioData.data();
	int k = 0;
	for (; k < f1 && k < f2; k++) {
		finalData[k] = fd1[k] + fd2[k];
	}

	for (int i = k; i < f1; i++)
		finalData[k++] = fd1[i];


	for (int i = k; i < f2; i++)
		finalData[k++] = fd2[i];


	float *fd = finalData;
	int frames = finalLen;

	int framesToUse = frames <= AudioRectCount ? frames : (frames / AudioRectCount)*AudioRectCount;


	if (framesToUse > 0) {
		int step = framesToUse / AudioRectCount;
		if (step == 0)
			step = 1;

		int index = 0;
		for (int i = 0; i < framesToUse; i += step) {
			float f = 0.f;
			for (int j = i; j < i + step; j++) {
				f += qAbs(fd[j]);
			}

			f = f / step;
			AudioInfo &info = audioInfos[index];
			info.audioLevel = f;
			index++;
		}

		for (int i = index; i < AudioRectCount; i++) {
			AudioInfo &info = audioInfos[i];
			info.audioLevel = 0.f;
		}
	} else {
		for (int i = 0; i < AudioRectCount; i++) {
			AudioInfo &info = audioInfos[i];
			info.audioLevel = 0.f;
		}
	}
	for (int i = 0; i < AudioRectCount; i++) {
		auto ii = createIndex(i, 0, nullptr);
		QVector<int> changedRoles;
		changedRoles.append(RoleAudioLevel);
		emit dataChanged(ii, ii, changedRoles);
	}
}

static void audio_raw_mic_data_received(void *param, uint8_t *data, uint32_t frames) { //obs use AUDIO_FORMAT_FLOAT_PLANAR as audio output 
	AudioWave *object = (AudioWave *)param;
	object->dataMutex.lock();
	object->audioData = QByteArray((const char *)data, sizeof(float)*frames);
	object->dataMutex.unlock();
}

static void audio_raw_desktop_data_received(void *param, uint8_t *data, uint32_t frames) { //obs use AUDIO_FORMAT_FLOAT_PLANAR as audio output 
	AudioWave *object = (AudioWave *)param;
	object->dataMutex.lock();
	object->audioDesktopData = QByteArray((const char *)data, sizeof(float)*frames);
	object->dataMutex.unlock();
}


AudioWave::AudioWave(QObject *parent /* = nullptr */)
	: QmlSourceBase(parent)
{
	audioModel = new AudioInfoModel(this);
	addProperties("audioModel", audioModel);
	audioMicSource = obs_get_output_source(3); //1 桌面音频 3 麦克风
	obs_source_release(audioMicSource);
	micVolmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_add_raw_data_callback(micVolmeter, audio_raw_mic_data_received, this);
	obs_volmeter_attach_source(micVolmeter, audioMicSource);

	audioDesktopSource = obs_get_output_source(1); //1 桌面音频 3 麦克风
	obs_source_release(audioDesktopSource);
	desktopVolmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_add_raw_data_callback(desktopVolmeter, audio_raw_desktop_data_received, this);
	obs_volmeter_attach_source(desktopVolmeter, audioDesktopSource);

	audioModel->start();
}

AudioWave::~AudioWave() {
	audioModel->stop();

	obs_volmeter_remove_raw_data_callback(micVolmeter, audio_raw_mic_data_received, this);
	obs_volmeter_destroy(micVolmeter);

	obs_volmeter_remove_raw_data_callback(desktopVolmeter, audio_raw_desktop_data_received, this);
	obs_volmeter_destroy(desktopVolmeter);
}

void AudioWave::default(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "file",
				    "qrc:/qmlfiles/AudioWave.qml");
	obs_data_set_default_int(settings, "fps", 0);
}

static const char *audiowave_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("quickaudiowaveshow");
}

static void audiowave_source_update(void *data, obs_data_t *settings)
{
	AudioWave *s = (AudioWave *)data;
	s->baseUpdate(settings);
}

static void *audiowave_source_create(obs_data_t *settings, obs_source_t *source)
{
	AudioWave *context = new AudioWave();
	context->setSource(source);

	audiowave_source_update(context, settings);
	return context;
}

static void audiowave_source_destroy(void *data)
{
	if (!data)
		return;
	AudioWave *s = (AudioWave *)data;
	s->baseDestroy();
	s->deleteLater();
}

static void audiowave_source_defaults(obs_data_t *settings)
{
	QmlSourceBase::baseDefault(settings);
	obs_data_set_default_int(settings, "width", 1920);
	obs_data_set_default_int(settings, "height", 1080);
	AudioWave::default(settings);
}

static obs_properties_t *audiowave_source_properties(void *data)
{
	if (!data)
		return nullptr;
	AudioWave *s = (AudioWave *)data;
	obs_properties_t *props = s->baseProperties();

	obs_properties_add_text(props, "backgroundImage", u8"背景图", OBS_TEXT_DEFAULT);
	obs_properties_add_text(props, "currentTime", u8"当前计时", OBS_TEXT_DEFAULT);
	return props;
}

struct obs_source_info quickaudiowave_source_info = {
	"quickaudiowave_source",
	OBS_SOURCE_TYPE_INPUT,
	OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION | OBS_SOURCE_DO_NOT_DUPLICATE,
	audiowave_source_get_name,
	audiowave_source_create,
	audiowave_source_destroy,
	base_source_getwidth,
	base_source_getheight,
	audiowave_source_defaults,
	audiowave_source_properties,
	audiowave_source_update,
	nullptr,
	nullptr,
	base_source_show,
	base_source_hide,
	nullptr,
	base_source_render,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	base_source_mouse_click,
	base_source_mouse_move,
	base_source_mouse_wheel,
	base_source_focus,
	base_source_key_click,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr
};
