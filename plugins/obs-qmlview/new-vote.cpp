#include "new-vote.h"
#include "renderer.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

NewVoteModel::NewVoteModel(QObject *parent) : QAbstractListModel(parent) {}

QVariant NewVoteModel::data(const QModelIndex &index, int role) const
{
	if (index.row() < 0 || index.row() >= voteOptionInfos.count())
		return QVariant();

	const NewVoteOption &d = voteOptionInfos[index.row()];
	switch (role) {
	case NewRoleOptionContent:
		return d.optionContent;
	case NewRoleVoteCount:
		return d.voteCount;
	case NewRoleOptionColor:
		return d.color;
	case NewRoleFontFamily:
		return d.fontFamily;
	case NewRoleFontSize:
		return 24;
	case NewRoleItalic:
		return d.italic;
	case NewRoleBold:
		return d.bold;
	default:
		return QVariant();
	}
}

int NewVoteModel::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent)
	return voteOptionInfos.count();
}

QHash<int, QByteArray> NewVoteModel::roleNames() const
{
	QHash<int, QByteArray> roles;
	roles[NewRoleOptionContent] = "optionContent";
	roles[NewRoleVoteCount] = "voteCount";
	roles[NewRoleFontFamily] = "fontFamily";
	roles[NewRoleFontSize] = "fontSize";
	roles[NewRoleOptionColor] = "color";
	roles[NewRoleItalic] = "italic";
	roles[NewRoleBold] = "bold";
	return roles;
}

void NewVoteModel::initModelData(const QJsonArray &array)
{
	beginResetModel();
	voteOptionInfos.clear();
	for (int i = 0; i < array.size(); i++) {
		QJsonObject obj = array.at(i).toObject();
		NewVoteOption vo;
		vo.optionContent = obj["optionContent"].toString();
		vo.voteCount = 0;

		voteOptionInfos.append(vo);
	}
	endResetModel();
}

void NewVoteModel::updateVoteCount(const QJsonObject &data)
{
	if (data.isEmpty()) {
		for (int i = 0; i < voteOptionInfos.count(); i++) {
			NewVoteOption &vo = voteOptionInfos[i];
			vo.voteCount = 0;
		}
		auto begin = createIndex(0, 0, nullptr);
		auto end = createIndex(voteOptionInfos.count() - 1, 0, nullptr);
		QVector<int> changedRoles;
		changedRoles.append(NewRoleVoteCount);
		emit dataChanged(begin, end, changedRoles);
	} else {
		int row = data["index"].toInt();
		if (row >= voteOptionInfos.count())
			return;

		NewVoteOption &vo = voteOptionInfos[row];
		vo.voteCount = data["count"].toInt();

		auto ii = createIndex(row, 0, nullptr);
		QVector<int> changedRoles;
		changedRoles.append(NewRoleVoteCount);
		emit dataChanged(ii, ii, changedRoles);
	}
}

NewVote::NewVote(obs_data_t *s, QObject *parent /* = nullptr */)
	: QmlSourceBase(parent), m_s(s)
{
	newVoteModel = new NewVoteModel(this);
	addProperties("newVoteModel", newVoteModel);
	addProperties("newVoteProperties", this);
}

void NewVote::default(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "file", "qrc:/qmlfiles/NewVote.qml");

	obs_data_set_default_string(settings, "subject", "");

	obs_data_set_default_int(settings, "themetype", 1);
	obs_data_set_default_int(settings, "transparence", 100);

	obs_data_set_default_string(settings, "themefontcolor", "#FFFFFF");
	obs_data_set_default_bool(settings, "themebold", false);
	obs_data_set_default_bool(settings, "themeitalic", false);
	obs_data_set_default_string(settings, "themefont", u8"阿里巴巴普惠体 M");

	obs_data_set_default_string(settings, "downfontcolor", "#FFFFFF");
	obs_data_set_default_bool(settings, "downbold", false);
	obs_data_set_default_bool(settings, "downitalic", false);
	obs_data_set_default_string(settings, "downfont", u8"阿里巴巴普惠体 R");

	obs_data_set_default_string(settings, "datafontcolor", "#FFFFFF");
	obs_data_set_default_bool(settings, "databold", false);
	obs_data_set_default_bool(settings, "dataitalic", false);
	obs_data_set_default_string(settings, "datafont", u8"阿里巴巴普惠体 R");

	obs_data_set_default_string(settings, "option",
		u8R"([{"optionContent":"","voteCount":0}])");
	obs_data_set_default_int(settings, "duration", 120);
	obs_data_set_default_bool(settings, "useDuration", false);
	obs_data_set_default_int(settings, "progress", 0);

	obs_data_set_default_int(settings, "imgWidth", 576);
	obs_data_set_default_int(settings, "imgHeight", 437);
	obs_data_set_default_string(settings, "voteCount", "");
}

void NewVote::resetModel(const QJsonArray &array)
{
	QMetaObject::invokeMethod(newVoteModel, "initModelData",
				  Q_ARG(QJsonArray, array));
}

void NewVote::updateVoteCount(const QJsonObject &data)
{
	QMetaObject::invokeMethod(newVoteModel, "updateVoteCount",
				  Q_ARG(QJsonObject, data));
}

static const char *new_vote_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("quickvote");
}

static void new_vote_source_update(void *data, obs_data_t *settings)
{
	NewVote *s = (NewVote *)data;

	// 主题
	QString subject = obs_data_get_string(settings, "subject");
	s->setvoteSubject(subject);
	s->setthemeColor(obs_data_get_string(settings, "themefontcolor"));
	s->setthemeFontFamily(obs_data_get_string(settings, "themefont"));
	s->setthemeBold(obs_data_get_bool(settings, "themebold"));
	s->setthemeItalic(obs_data_get_bool(settings, "themeitalic"));
	s->setthemeType(obs_data_get_int(settings, "themetype"));
	s->settransparence(obs_data_get_int(settings, "transparence"));

	s->setdownColor(obs_data_get_string(settings, "downfontcolor"));
	s->setdownFontFamily(obs_data_get_string(settings, "downfont"));
	s->setdownBold(obs_data_get_bool(settings, "downbold"));
	s->setdownItalic(obs_data_get_bool(settings, "downitalic"));

	s->setdataColor(obs_data_get_string(settings, "datafontcolor"));
	s->setdataFontFamily(obs_data_get_string(settings, "datafont"));
	s->setdataBold(obs_data_get_bool(settings, "databold"));
	s->setdataItalic(obs_data_get_bool(settings, "dataitalic"));

	s->setimageWidth(576);
	s->setimageHeight(437);

	{
		QString option = obs_data_get_string(settings, "option");
		if (s->m_lastOption != option)
		{
			s->m_lastOption = option;
			QJsonDocument jd =
				QJsonDocument::fromJson(option.toUtf8());
			auto array = jd.array();
			int itemCount = array.size();
			s->resetModel(array);

			int optionHeight =
				itemCount = 0 ? 0 : itemCount * 49 + (itemCount -1) * 8;
			int h = 209 + optionHeight - 49;
			obs_data_set_int(settings, "height", h);
		}
	}

	int duration = obs_data_get_int(settings, "duration");
	s->setduration(duration);
	s->setuseDuration(obs_data_get_bool(settings, "useDuration"));

	s->setprogress(obs_data_get_int(settings, "progress"));

	{
		QString voteCount = obs_data_get_string(settings, "voteCount");
		QJsonDocument jd = QJsonDocument::fromJson(voteCount.toUtf8());
		s->updateVoteCount(jd.object());
	}

	s->baseUpdate(settings);
}

static void *new_vote_source_create(obs_data_t *settings, obs_source_t *source)
{
	NewVote *context = new NewVote(settings);
	context->setSource(source);

	new_vote_source_update(context, settings);
	return context;
}

static void new_vote_source_destroy(void *data)
{
	if (!data)
		return;
	NewVote *s = (NewVote *)data;
	s->baseDestroy();
	s->deleteLater();
}

static void new_vote_source_defaults(obs_data_t *settings)
{
	QmlSourceBase::baseDefault(settings);
	obs_data_set_default_int(settings, "width", 576);
	obs_data_set_default_int(settings, "height", 209);
	NewVote::default(settings);
}

static obs_properties_t *new_vote_source_properties(void *data)
{
	if (!data)
		return nullptr;
	NewVote *s = (NewVote *)data;
	obs_properties_t *props = s->baseProperties();
	obs_properties_add_text(props, "subject", u8"投票描述",
				OBS_TEXT_DEFAULT);

	obs_properties_add_int(props, "themetype", u8"主题背景", 1, 4, 1);
	obs_properties_add_int(props, "transparence", u8"主题背景透明度", 0, 100, 100);
	obs_properties_add_text(props, "themefontcolor", u8"主题颜色", OBS_TEXT_DEFAULT);
	obs_properties_add_bool(props, "themebold", u8"主题粗体");
	obs_properties_add_bool(props, "themeitalic", u8"主题斜体");
	obs_properties_add_text(props, "themefont", u8"主题字体", OBS_TEXT_DEFAULT);

	obs_properties_add_text(props, "downfontcolor", u8"倒计时文字颜色", OBS_TEXT_DEFAULT);
	obs_properties_add_bool(props, "downbold", u8"倒计时粗体");
	obs_properties_add_bool(props, "downitalic", u8"倒计时斜体");
	obs_properties_add_text(props, "downfont", u8"倒计时字体", OBS_TEXT_DEFAULT);

	obs_properties_add_text(props, "datafontcolor", u8"选项文字颜色", OBS_TEXT_DEFAULT);
	obs_properties_add_bool(props, "databold", u8"选项粗体");
	obs_properties_add_bool(props, "dataitalic", u8"选项斜体");
	obs_properties_add_text(props, "datafont", u8"选项字体", OBS_TEXT_DEFAULT);

	obs_properties_add_int(props, "imgWidth", u8"背景图宽", 1, 9999, 576);
	obs_properties_add_int(props, "imgHeight", u8"背景图高", 1, 9999, 437);

	obs_properties_add_int(props, "duration", u8"投票时长", -1, 99999, 1);
	obs_properties_add_bool(props, "useDuration", u8"使用投票时长");

	obs_properties_add_text(props, "option", u8"投票选项",
		OBS_TEXT_DEFAULT);
	obs_properties_add_text(props, "voteCount", u8"投票数更新",
		OBS_TEXT_DEFAULT);

	obs_properties_add_int(props, "progress", u8"当前状态", 0, 2, 1);
	return props;
}

struct obs_source_info quickvote_source_info = {
	"quickvote_source",
	OBS_SOURCE_TYPE_INPUT,
	OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION | OBS_SOURCE_DO_NOT_DUPLICATE,
	new_vote_source_get_name,
	new_vote_source_create,
	new_vote_source_destroy,
	base_source_getwidth,
	base_source_getheight,
	new_vote_source_defaults,
	new_vote_source_properties,
	new_vote_source_update,
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
	nullptr};
