#include "new-rank.h"
#include "renderer.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

NewRankModel::NewRankModel(QObject *parent)
	: QAbstractListModel(parent)
{
}

QVariant NewRankModel::data(const QModelIndex &index, int role) const
{
	if (index.row() < 0 || index.row() >= rankInfos.count())
		return QVariant();

	const NewRankInfo &d = rankInfos[index.row()];
	switch (role) {
	case NewRoleName:
		return d.nickname;
	case NewRoleNameFontName:
		return d.fontName;
	case NewRoleNameFontSize:
		return d.fontSize;
	case NewRoleNameFontColor:
		return d.color;
	case NewRoleDiamondNum:
		return d.diamondNum;
	case NewRoleDiamondFontName:
		return d.diamondFontName;
	case NewRoleDiamondFontSize:
		return d.diamondFontSize;
	case NewRoleDiamondFontColor:
		return d.diamondColor;
	case NewRoleDiamondImage:
		return d.diamondImage;
	case NewRoleRankNum:
		return d.rankNum;
	case NewRoleRankFontName:
		return d.rankFontName;
	case NewRoleRankFontSize:
		return d.rankFontSize;
	case NewRoleRankFontColor:
		return d.rankColor;
	case NewRoleRankBackImage:
		return d.rankNumBackgroundImage;
	case NewRoleNickNameBold:
		return d.nicknameBold;
	case NewRoleRankBold:
		return d.rankBold;
	case NewRoleDiamondBold:
		return d.diamondBold;
	case NewRoleAvarstarPath:
		return d.avarstarPath;
	case NewRoleItalic:
		return d.italic;
	default:
		return QVariant();
	}
}

int NewRankModel::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent)
		return rankInfos.count();
}

QHash<int, QByteArray> NewRankModel::roleNames() const
{
	QHash<int, QByteArray> roles;
	roles[NewRoleName] = "nickname";
	roles[NewRoleNameFontName] = "fontName";
	roles[NewRoleNameFontSize] = "fontSize";
	roles[NewRoleNameFontColor] = "fontColor";
	roles[NewRoleDiamondNum] = "diamondNum";
	roles[NewRoleDiamondFontName] = "diamondFontName";
	roles[NewRoleDiamondFontSize] = "diamondFontSize";
	roles[NewRoleDiamondFontColor] = "diamondColor";
	roles[NewRoleDiamondImage] = "diamondImage";
	roles[NewRoleRankNum] = "rankNum";
	roles[NewRoleRankFontName] = "rankFontName";
	roles[NewRoleRankFontSize] = "rankFontSize";
	roles[NewRoleRankFontColor] = "rankColor";
	roles[NewRoleRankBackImage] = "rankNumBackgroundImage";
	roles[NewRoleNickNameBold] = "nicknameBold";
	roles[NewRoleRankBold] = "rankBold";
	roles[NewRoleDiamondBold] = "diamondBold";
	roles[NewRoleAvarstarPath] = "avarstarPath";
	roles[NewRoleItalic] = "italic";
	return roles;
}

void NewRankModel::initModelData(const QString &data)
{
	QJsonDocument jd = QJsonDocument::fromJson(data.toUtf8());
	auto array = jd.array();
	beginResetModel();
	rankInfos.clear();
	for (int i = 0; i < array.count(); i++) {
		QJsonObject obj = array.at(i).toObject();
		NewRankInfo info;
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
		info.avarstarPath = obj["avarstarPath"].toString();
		info.italic = obj["italic"].toBool();
		rankInfos.append(info);
	}
	endResetModel();
}



NewRank::NewRank(QObject *parent /* = nullptr */)
	: QmlSourceBase(parent)
{
	rankModel = new NewRankModel(this);
	addProperties("newRankModel", rankModel);
	addProperties("rankProperties", this);
}

void NewRank::default(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "file",
		"qrc:/qmlfiles/NewRank.qml");
	obs_data_set_default_string(settings, "themefontcolor", "#FFFFFF");
	obs_data_set_default_string(settings, "themefont", u8"阿里巴巴普惠体 M");
	obs_data_set_default_bool(settings, "themebold", false);
	obs_data_set_default_bool(settings, "themeitalic", false);
	obs_data_set_default_int(settings, "themetype", 1);
	obs_data_set_default_string(settings, "datafontcolor", "#FFFFFF");
	obs_data_set_default_string(settings, "datafont", u8"阿里巴巴普惠体 R");
	obs_data_set_default_bool(settings, "dataitalic", false);
}

static const char *newrank_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("quicknewrankshow");
}

static void newrank_source_update(void *data, obs_data_t *settings)
{
	NewRank *s = (NewRank *)data;
	s->baseUpdate(settings);

	const char *arrayList = obs_data_get_string(settings, "arrayList");
	QMetaObject::invokeMethod(s->rankModel, "initModelData", Q_ARG(QString, QString(arrayList)));

	bool bold1 = obs_data_get_bool(settings, "databold");
	s->setdatabold(bold1);

	bool italic1 = obs_data_get_bool(settings, "dataitalic");
	s->setdataitalic(italic1);

	const char *font1 = obs_data_get_string(settings, "datafont");
	s->setdatafont(font1);

	const char *datafontcolor =
		obs_data_get_string(settings, "datafontcolor");
	s->setdatafontcolor(datafontcolor);

	bool bold2 = obs_data_get_bool(settings, "themebold");
	s->setthemebold(bold2);

	bool italic2 = obs_data_get_bool(settings, "themeitalic");
	s->setthemeitalic(italic2);

	const char *font2 = obs_data_get_string(settings, "themefont");
	s->setthemefont(font2);

	const char *themefontcolor = obs_data_get_string(settings, "themefontcolor");
	s->setthemefontcolor(themefontcolor);

	int themeType = obs_data_get_int(settings, "themetype");
	s->setthemetype(themeType);

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
	s->setrightMargin(rightMargin);

	int bottomMargin = obs_data_get_int(settings, "bottomMargin");
	s->setbottomMargin(bottomMargin);

	int spacing = obs_data_get_int(settings, "spacing");
	s->setspacing(spacing);

	float transparence = (float)(qMax((int)obs_data_get_int(settings, "transparence"), 1));
	transparence = transparence / 100.0f;
	s->settransparence(transparence);
}

static void *newrank_source_create(obs_data_t *settings, obs_source_t *source)
{
	NewRank *context = new NewRank();
	context->setSource(source);

	newrank_source_update(context, settings);
	return context;
}

static void newrank_source_destroy(void *data)
{
	if (!data)
		return;
	NewRank *s = (NewRank *)data;
	s->baseDestroy();
	s->deleteLater();
}

static void newrank_source_defaults(obs_data_t *settings)
{
	QmlSourceBase::baseDefault(settings);
	obs_data_set_default_int(settings, "width", 388);
	obs_data_set_default_int(settings, "height", 416);
	NewRank::default(settings);
}

static obs_properties_t *newrank_source_properties(void *data)
{
	if (!data)
		return nullptr;
	NewRank *s = (NewRank *)data;
	obs_properties_t *props = s->baseProperties();

	obs_properties_add_text(props, "themefontcolor", u8"文字颜色",
		OBS_TEXT_DEFAULT);
	obs_properties_add_bool(props, "themebold", u8"粗体");
	obs_properties_add_bool(props, "themeitalic", u8"斜体");
	obs_properties_add_text(props, "themefont", u8"字体", OBS_TEXT_DEFAULT);
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

struct obs_source_info quicknewrank_source_info = {
	"quicknewrank_source",
	OBS_SOURCE_TYPE_INPUT,
	OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION | OBS_SOURCE_DO_NOT_DUPLICATE,
	newrank_source_get_name,
	newrank_source_create,
	newrank_source_destroy,
	base_source_getwidth,
	base_source_getheight,
	newrank_source_defaults,
	newrank_source_properties,
	newrank_source_update,
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
