#include "audio-live-link.h"
#include "renderer.h"
#include <QDebug>

AudioLiveLink::AudioLiveLink(QObject *parent /* = nullptr */)
	: QmlSourceBase(parent)
{
	addProperties("audioLiveLinkProperties", this);
}

void AudioLiveLink::default(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "file",
				    "qrc:/qmlfiles/AudioLiveLink.qml");
}

static const char *audio_livelink_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("audioLiveLinkShow");
}

static void audio_livelink_source_update(void *data, obs_data_t *settings)
{
	AudioLiveLink *s = (AudioLiveLink *)data;
	const char *path = obs_data_get_string(settings, "path");
	s->setpath(path);

	const char *name = obs_data_get_string(settings, "name");
	s->setname(name);

	const char *wavePath = obs_data_get_string(settings, "wavepath");
	s->setwave(wavePath);

	bool isMulitiLink = obs_data_get_bool(settings, "ismuliti");
	s->setisMuliti(isMulitiLink);

	const char *effectPath = obs_data_get_string(settings, "effectpath");
	s->seteffect(effectPath);

	int posX = obs_data_get_int(settings, "posX");
	s->setposX(posX);

	int posY = obs_data_get_int(settings, "posY");
	s->setposY(posY);

	float scale = obs_data_get_double(settings, "scale");
	s->setscale(scale);

	int basicWidth = obs_data_get_int(settings, "width");
	s->setbasicWidth(basicWidth);

	int basicHeight = obs_data_get_int(settings, "height");
	s->setbasicHeight(basicHeight);

	int voiceWaveLeftMargin =
		obs_data_get_int(settings, "voiceWaveLeftMargin");
	s->setvoiceWaveLeftMargin(voiceWaveLeftMargin);

	int voiceWaveTopMargin =
		obs_data_get_int(settings, "voiceWaveTopMargin");
	s->setvoiceWaveTopMargin(voiceWaveTopMargin);

	int voiceWaveSize = obs_data_get_int(settings, "voiceWaveSize");
	s->setvoiceWaveSize(voiceWaveSize);

	int avatarPos = obs_data_get_int(settings, "avatarPos");
	s->setavatarPos(avatarPos);

	int avatarSize = obs_data_get_int(settings, "avatarSize");
	s->setavatarSize(avatarSize);

	int borderWidth = obs_data_get_int(settings, "borderWidth");
	s->setborderWidth(borderWidth);

	int backWidth = obs_data_get_int(settings, "backWidth");
	s->setbackWidth(backWidth);
	s->baseUpdate(settings);
}

static void *audio_livelink_source_create(obs_data_t *settings,
					  obs_source_t *source)
{
	AudioLiveLink *context = new AudioLiveLink();
	context->setSource(source);

	audio_livelink_source_update(context, settings);
	return context;
}

static void audio_livelink_source_destroy(void *data)
{
	if (!data)
		return;
	AudioLiveLink *s = (AudioLiveLink *)data;
	s->baseDestroy();
	s->deleteLater();
}

static void audio_livelink_source_defaults(obs_data_t *settings)
{
	QmlSourceBase::baseDefault(settings);
	obs_data_set_default_bool(settings, "ismuliti", false);
	obs_data_set_default_int(settings, "width", 720);
	obs_data_set_default_int(settings, "height", 1080);
	obs_data_set_default_int(settings, "posX", 0);
	AudioLiveLink::default(settings);
}

static obs_properties_t *audio_livelink_source_properties(void *data)
{
	if (!data)
		return nullptr;
	AudioLiveLink *s = (AudioLiveLink *)data;
	obs_properties_t *props = s->baseProperties();
	return props;
}

static void audioLiveLinkCommand(void *data, obs_data_t *cmd)
{
	AudioLiveLink *s = (AudioLiveLink *)data;
	const char *cmdType = obs_data_get_string(cmd, "type");
	if (strcmp("play", cmdType) == 0 && s) {
		emit s->play();
	} else if (strcmp("stop", cmdType) == 0 && s) {
		emit s->stop();
	} else if (strcmp("showpkface", cmdType) == 0 && s) {
		const char *effectPath = obs_data_get_string(cmd, "effectpath");
		s->seteffect(effectPath);
		emit s->showPkEffect();
	} else if (strcmp("stoppkface", cmdType) == 0 && s) {
		emit s->stopPkEffect();
	}
}

struct obs_source_info audio_livelink_source_info = {
	"audio_livelink_source",
	OBS_SOURCE_TYPE_INPUT,
	OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION | OBS_SOURCE_DO_NOT_DUPLICATE,
	audio_livelink_source_get_name,
	audio_livelink_source_create,
	audio_livelink_source_destroy,
	base_source_getwidth,
	base_source_getheight,
	audio_livelink_source_defaults,
	audio_livelink_source_properties,
	audio_livelink_source_update,
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
	nullptr,
	audioLiveLinkCommand};
