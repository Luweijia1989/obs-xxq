#include "rank.h"
#include "renderer.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

RankModel::RankModel(QObject *parent)
	: QAbstractListModel(parent)
{
}

QVariant RankModel::data(const QModelIndex &index, int role) const
{
	if (index.row() < 0 || index.row() >= rankInfos.count())
		return QVariant();

	const RankInfo &d = rankInfos[index.row()];
	switch (role) {
	case RoleName:
		return d.nickname;
	case RoleNameFontName:
		return d.fontName;
	case RoleNameFontSize:
		return d.fontSize;
	case RoleNameFontColor:
		return d.color;
	case RoleDiamondNum:
		return d.diamondNum;
	case RoleDiamondFontName:
		return d.diamondFontName;
	case RoleDiamondFontSize:
		return d.diamondFontSize;
	case RoleDiamondFontColor:
		return d.diamondColor;
	case RoleDiamondImage:
		return d.diamondImage;
	case RoleRankNum:
		return d.rankNum;
	case RoleRankFontName:
		return d.rankFontName;
	case RoleRankFontSize:
		return d.rankFontSize;
	case RoleRankFontColor:
		return d.rankColor;
	case RoleRankBackImage:
		return d.rankNumBackgroundImage;
	case RoleNickNameBold:
		return d.nicknameBold;
	case RoleRankBold:
		return d.rankBold;
	case RoleDiamondBold:
		return d.diamondBold;
	default:
		return QVariant();
	}
}

int RankModel::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent)
	return rankInfos.count();
}

QHash<int, QByteArray> RankModel::roleNames() const
{
	QHash<int, QByteArray> roles;
	roles[RoleName] = "nickname";
	roles[RoleNameFontName] = "fontName";
	roles[RoleNameFontSize] = "fontSize";
	roles[RoleNameFontColor] = "fontColor";
	roles[RoleDiamondNum] = "diamondNum";
	roles[RoleDiamondFontName] = "diamondFontName";
	roles[RoleDiamondFontSize] = "diamondFontSize";
	roles[RoleDiamondFontColor] = "diamondColor";
	roles[RoleDiamondImage] = "diamondImage";
	roles[RoleRankNum] = "rankNum";
	roles[RoleRankFontName] = "rankFontName";
	roles[RoleRankFontSize] = "rankFontSize";
	roles[RoleRankFontColor] = "rankColor";
	roles[RoleRankBackImage] = "rankNumBackgroundImage";
	roles[RoleNickNameBold] = "nicknameBold";
	roles[RoleRankBold] = "rankBold";
	roles[RoleDiamondBold] = "diamondBold";
	return roles;
}

void RankModel::initModelData(const QString &data)
{
	QJsonDocument jd = QJsonDocument::fromJson(data.toUtf8());
	auto array = jd.array();
	beginResetModel();
	rankInfos.clear();
	for (int i = 0; i < array.count(); i++) {
		QJsonObject obj = array.at(i).toObject();
		RankInfo info;
		info.nickname = obj["nickname"].toString();
		info.fontName = obj["fontName"].toString();
		info.fontSize = obj["fontSize"].toInt();
		info.color = obj["color"].toString();
		info.diamondNum = obj["diamondNum"].toString();
		info.diamondFontName = obj["diamondFontName"].toString();
		info.diamondFontSize = obj["diamondFontSize"].toInt();
		info.diamondColor = obj["diamondColor"].toString();
		info.diamondImage = obj["diamondImage"].toString();
		info.rankNum = obj["rankNum"].toInt();
		info.rankFontName = obj["rankFontName"].toString();
		info.rankFontSize = obj["rankFontSize"].toInt();
		info.rankColor = obj["rankColor"].toString();
		info.rankNumBackgroundImage = obj["rankNumBackgroundImage"].toString();
		info.nicknameBold = obj["nickNameBold"].toBool();
		info.rankBold = obj["rankBold"].toBool();
		info.diamondBold = obj["diamondBold"].toBool();
		rankInfos.append(info);
	}
	endResetModel();
}



Rank::Rank(QObject *parent /* = nullptr */)
	: QmlSourceBase(parent)
{
	rankModel = new RankModel(this);
	addProperties("rankModel", rankModel);
	addProperties("rankProperties", this);
}

void Rank::default(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "file",
				    "qrc:/qmlfiles/Rank.qml");
	obs_data_set_default_string(settings, "fillColor", "#00000000");
	obs_data_set_default_string(settings, "frontColor", "#FFFFFF");
	obs_data_set_default_int(settings, "fontSize", 36);
	obs_data_set_default_string(settings, "font", u8"黑体");
}

static const char *rank_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("quickrankshow");
}

static void rank_source_update(void *data, obs_data_t *settings)
{
	Rank *s = (Rank *)data;
	s->baseUpdate(settings);

	const char *arrayList = obs_data_get_string(settings, "arrayList");
	QMetaObject::invokeMethod(s->rankModel, "initModelData", Q_ARG(QString, QString(arrayList)));

	const char *color = obs_data_get_string(settings, "fillColor");
	s->setfillColor(QColor(color));

	const char *front_color = obs_data_get_string(settings, "frontColor");
	s->setfrontColor(QColor(front_color));

	bool bold = obs_data_get_bool(settings, "bold");
	s->setbold(bold);

	bool italic = obs_data_get_bool(settings, "italic");
	s->setitalic(italic);

	bool strikeout = obs_data_get_bool(settings, "strikeout");
	s->setstrikeout(strikeout);

	bool underline = obs_data_get_bool(settings, "underline");
	s->setunderline(underline);

	const char *font = obs_data_get_string(settings, "font");
	s->setfont(font);

	int fontSize = obs_data_get_int(settings, "fontSize");
	s->setfontSize(fontSize);

	bool hasRankType = obs_data_get_bool(settings, "hasRankType");
	s->sethasRankType(hasRankType);

	bool hasRankNum = obs_data_get_bool(settings, "hasRankNum");
	s->sethasRankNum(hasRankNum);

	bool hasDiamond = obs_data_get_bool(settings, "hasDiamond");
	s->sethasDiamond(hasDiamond);

	const char *rankName = obs_data_get_string(settings, "rankName");
	s->setrankName(rankName);

	int leftMargin = obs_data_get_int(settings, "leftMargin");
	s->setleftMargin(leftMargin);

	int topMargin = obs_data_get_int(settings, "topMargin");
	s->settopMargin(topMargin);

	int rightMargin = obs_data_get_int(settings, "rightMargin");
	s->setrightMarin(rightMargin);

	int bottomMargin = obs_data_get_int(settings, "bottomMargin");
	s->setbottomMargin(bottomMargin);

	int spacing = obs_data_get_int(settings, "spacing");
	s->setspacing(spacing);
}

static void *rank_source_create(obs_data_t *settings, obs_source_t *source)
{
	Rank *context = new Rank();
	context->setSource(source);

	rank_source_update(context, settings);
	return context;
}

static void rank_source_destroy(void *data)
{
	if (!data)
		return;
	Rank *s = (Rank *)data;
	s->baseDestroy();
	s->deleteLater();
}

static void rank_source_defaults(obs_data_t *settings)
{
	QmlSourceBase::baseDefault(settings);
	obs_data_set_default_int(settings, "width", 320);
	obs_data_set_default_int(settings, "height", 320);
	Rank::default(settings);
}

static void rank_source_show(void *data)
{
	if (!data)
		return;
	Rank *s = (Rank *)data;
	s->baseShow();
}

static void rank_source_hide(void *data)
{
	if (!data)
		return;
	Rank *s = (Rank *)data;
	s->baseHide();
}

static uint32_t rank_source_getwidth(void *data)
{
	if (!data)
		return 5;
	Rank *s = (Rank *)data;
	return s->baseGetWidth();
}

static uint32_t rank_source_getheight(void *data)
{
	if (!data)
		return 5;
	Rank *s = (Rank *)data;
	return s->baseGetHeight();
}

static void rank_source_render(void *data, gs_effect_t *effect)
{
	if (!data)
		return;
	Rank *s = (Rank *)data;
	s->baseRender(effect);
}

static obs_properties_t *rank_source_properties(void *data)
{
	if (!data)
		return nullptr;
	Rank *s = (Rank *)data;
	obs_properties_t *props = s->baseProperties();

	obs_properties_add_text(props, "fillColor", u8"填充颜色",
				OBS_TEXT_DEFAULT);
	obs_properties_add_text(props, "frontColor", u8"文字颜色",
				OBS_TEXT_DEFAULT);
	obs_properties_add_bool(props, "bold", u8"粗体");
	obs_properties_add_bool(props, "italic", u8"斜体");
	obs_properties_add_bool(props, "strikeout", u8"删除线");
	obs_properties_add_bool(props, "underline", u8"下划线");
	obs_properties_add_text(props, "font", u8"字体", OBS_TEXT_DEFAULT);
	obs_properties_add_int(props, "fontSize", u8"字体大小", 12, 72, 1);
	obs_properties_add_bool(props, "hasRankType", u8"显示榜单类型");
	obs_properties_add_bool(props, "hasRankNum", u8"显示榜单排名");
	obs_properties_add_bool(props, "hasDiamond", u8"显示打赏星钻");
	obs_properties_add_bool(props, "arrayList", u8"榜单内容");
	obs_properties_add_text(props, "rankName", u8"榜单名称", OBS_TEXT_DEFAULT);
	obs_properties_add_int(props, "leftMargin", u8"左边距", 0, 1920, 1);
	obs_properties_add_int(props, "topMargin", u8"上边距", 0, 1920, 1);
	obs_properties_add_int(props, "rightMargin", u8"右边距", 0, 1920, 1);
	obs_properties_add_int(props, "bottomMargin", u8"下边距", 0, 1920, 1);
	obs_properties_add_int(props, "spacing", u8"间隔", 0, 1920, 1);

	return props;
}

static void rank_source_mouse_click(void *data,
					 const struct obs_mouse_event *event,
					 int32_t type, bool mouse_up,
					 uint32_t click_count)
{
	if (!data)
		return;
	Rank *s = (Rank *)data;
	s->baseMouseClick(event->x, event->y, type, mouse_up, click_count);
}

static void rank_source_mouse_move(void *data,
					const struct obs_mouse_event *event,
					bool mouse_leave)
{
	if (!data)
		return;
	Rank *s = (Rank *)data;
	s->baseMouseMove(event->x, event->y, mouse_leave);
}

static void rank_source_mouse_wheel(void *data,
					 const struct obs_mouse_event *event,
					 int x_delta, int y_delta)
{
	if (!data)
		return;
	Rank *s = (Rank *)data;
	s->baseMouseWheel(x_delta, y_delta);
}

static void rank_source_focus(void *data, bool focus)
{
	if (!data)
		return;
	Rank *s = (Rank *)data;
	s->baseFocus(focus);
}

static void rank_source_key_click(void *data,
				       const struct obs_key_event *event,
				       bool key_up)
{
	if (!data)
		return;
	Rank *s = (Rank *)data;
	s->baseKey(event->native_scancode, event->native_vkey,
		   event->native_modifiers, event->text, key_up);
}

struct obs_source_info quickrank_source_info = {
	"quickrank_source",
	OBS_SOURCE_TYPE_INPUT,
	OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION | OBS_SOURCE_DO_NOT_DUPLICATE,
	rank_source_get_name,
	rank_source_create,
	rank_source_destroy,
	rank_source_getwidth,
	rank_source_getheight,
	rank_source_defaults,
	rank_source_properties,
	rank_source_update,
	nullptr,
	nullptr,
	rank_source_show,
	rank_source_hide,
	nullptr,
	rank_source_render,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	rank_source_mouse_click,
	rank_source_mouse_move,
	rank_source_mouse_wheel,
	rank_source_focus,
	rank_source_key_click,
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
