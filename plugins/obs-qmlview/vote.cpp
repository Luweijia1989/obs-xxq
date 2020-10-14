#include "vote.h"
#include "renderer.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

VoteModel::VoteModel(QObject *parent) : QAbstractListModel(parent) {}

QVariant VoteModel::data(const QModelIndex &index, int role) const
{
	if (index.row() < 0 || index.row() >= voteOptionInfos.count())
		return QVariant();

	const VoteOption &d = voteOptionInfos[index.row()];
	switch (role) {
	case RoleOptionContent:
		return d.optionContent;
	case RoleVoteCount:
		return d.voteCount;
	case RoleBackgroundColor:
		return d.backgroundColor;
	case RoleOptionColor:
		return d.color;
	case RoleFontFamily:
		return d.fontFamily;
	case RoleFontSize:
		return d.fontSize;
	case RoleItalic:
		return d.italic;
	case RoleBold:
		return d.bold;
	case RoleUnderLine:
		return d.underline;
	case RoleStrikeout:
		return d.strikeout;
	default:
		return QVariant();
	}
}

int VoteModel::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent)
	return voteOptionInfos.count();
}

QHash<int, QByteArray> VoteModel::roleNames() const
{
	QHash<int, QByteArray> roles;
	roles[RoleOptionContent] = "optionContent";
	roles[RoleVoteCount] = "voteCount";
	roles[RoleFontFamily] = "fontFamily";
	roles[RoleFontSize] = "fontSize";
	roles[RoleBackgroundColor] = "backgroundColor";
	roles[RoleOptionColor] = "color";
	roles[RoleItalic] = "italic";
	roles[RoleUnderLine] = "underline";
	roles[RoleStrikeout] = "strikeout";
	roles[RoleBold] = "bold";
	return roles;
}

void VoteModel::initModelData(const QJsonArray &array)
{

	beginResetModel();
	voteOptionInfos.clear();
	for (int i = 0; i < array.size(); i++) {
		QJsonObject obj = array.at(i).toObject();
		VoteOption vo;
		vo.optionContent = obj["optionContent"].toString();
		vo.voteCount = 0;

		QJsonObject fontInfo = obj["fontInfo"].toObject();
		vo.backgroundColor =
			QColor(fontInfo["backgroundColor"].toString());
		vo.color = QColor(fontInfo["color"].toString());
		vo.fontFamily = fontInfo["fontFamily"].toString();
		vo.fontSize = fontInfo["fontSize"].toInt();
		vo.bold = fontInfo["bold"].toBool();
		vo.italic = fontInfo["italic"].toBool();
		vo.underline = fontInfo["underline"].toBool();
		vo.strikeout = fontInfo["strikeout"].toBool();
		voteOptionInfos.append(vo);
	}
	endResetModel();
}

void VoteModel::updateVoteCount(const QJsonObject &data)
{
	if (data.isEmpty()) {
		for (int i = 0; i < voteOptionInfos.count(); i++) {
			VoteOption &vo = voteOptionInfos[i];
			vo.voteCount = 0;
		}
		auto begin = createIndex(0, 0, nullptr);
		auto end = createIndex(voteOptionInfos.count() - 1, 0, nullptr);
		QVector<int> changedRoles;
		changedRoles.append(RoleVoteCount);
		emit dataChanged(begin, end, changedRoles);
	} else {
		int row = data["index"].toInt();
		if (row >= voteOptionInfos.count())
			return;

		VoteOption &vo = voteOptionInfos[row];
		vo.voteCount = data["count"].toInt();

		auto ii = createIndex(row, 0, nullptr);
		QVector<int> changedRoles;
		changedRoles.append(RoleVoteCount);
		emit dataChanged(ii, ii, changedRoles);
	}
}

Vote::Vote(obs_data_t *s, QObject *parent /* = nullptr */)
	: QmlSourceBase(parent), m_s(s)
{
	voteModel = new VoteModel(this);
	addProperties("voteModel", voteModel);
	addProperties("voteProperties", this);
}

void Vote::default(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "file", "qrc:/qmlfiles/vote.qml");
}

void Vote::resetModel(const QJsonArray &array)
{
	QMetaObject::invokeMethod(voteModel, "initModelData",
				  Q_ARG(QJsonArray, array));
}

void Vote::updateVoteCount(const QJsonObject &data)
{
	QMetaObject::invokeMethod(voteModel, "updateVoteCount",
				  Q_ARG(QJsonObject, data));
}

static const char *vote_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("vote");
}

static void vote_source_update(void *data, obs_data_t *settings)
{
	Vote *s = (Vote *)data;

	{
		QString rule = obs_data_get_string(settings, "rule");
		if (rule != s->m_lastRuleString) {
			s->m_lastRuleString = rule;
			QJsonDocument jd =
				QJsonDocument::fromJson(rule.toUtf8());
			if (!jd.isNull()) {
				QJsonObject ruleObj = jd.object();
				s->setruleVisible(ruleObj["visible"].toBool());
				QJsonObject fontInfo =
					ruleObj["fontInfo"].toObject();
				s->setruleBackgroundColor(QColor(
					fontInfo["backgroundColor"].toString()));
				s->setruleColor(
					QColor(fontInfo["color"].toString()));
				s->setruleFontFamily(
					fontInfo["fontFamily"].toString());
				s->setruleFontSize(
					fontInfo["fontSize"].toInt());
				s->setruleBold(fontInfo["bold"].toBool());
				s->setruleItalic(fontInfo["italic"].toBool());
				s->setruleUnderline(
					fontInfo["underline"].toBool());
				s->setruleStrikeout(
					fontInfo["strikeout"].toBool());
			}
		}
	}

	{
		QString subject = obs_data_get_string(settings, "subject");
		if (s->m_lastSubjectString != subject) {
			s->m_lastSubjectString = subject;
			QJsonDocument jd =
				QJsonDocument::fromJson(subject.toUtf8());
			if (!jd.isNull()) {
				QJsonObject subjectObj = jd.object();
				s->setvoteSubject(
					subjectObj["subject"].toString());
				QJsonObject fontInfo =
					subjectObj["fontInfo"].toObject();
				s->setbackgroundColor(QColor(
					fontInfo["backgroundColor"].toString()));
				s->setcolor(
					QColor(fontInfo["color"].toString()));
				s->setfontFamily(
					fontInfo["fontFamily"].toString());
				s->setfontSize(fontInfo["fontSize"].toInt());
				s->setbold(fontInfo["bold"].toBool());
				s->setitalic(fontInfo["italic"].toBool());
				s->setunderline(fontInfo["underline"].toBool());
				s->setstrikeout(fontInfo["strikeout"].toBool());
			}
		}
	}

	{
		QString option = obs_data_get_string(settings, "option");
		if (s->m_lastOptionString != option) {
			s->m_lastOptionString = option;
			QJsonDocument jd =
				QJsonDocument::fromJson(option.toUtf8());
			auto array = jd.array();
			int itemCount = array.size();
			s->resetModel(array);

			int optionHeight =
				itemCount == 0 ? 0 : itemCount * 20 * 2 - 20;
			int h = s->ruleVisible() ? 236 + optionHeight
						 : 236 + optionHeight - 68;
			obs_data_set_int(settings, "height", h);
		}
	}

	int duration = obs_data_get_int(settings, "duration");
	s->setduration(duration);

	s->setuseDuration(obs_data_get_bool(settings, "useDuration"));

	s->setbackgroundImage(obs_data_get_string(settings, "backgroundImage"));
	s->setimageWidth(obs_data_get_int(settings, "imgWidth"));
	s->setimageHeight(obs_data_get_int(settings, "imgHeight"));
	s->setprogress(obs_data_get_int(settings, "progress"));

	{
		QString voteCount = obs_data_get_string(settings, "voteCount");
		QJsonDocument jd = QJsonDocument::fromJson(voteCount.toUtf8());
		s->updateVoteCount(jd.object());
	}

	s->baseUpdate(settings);
}

static void *vote_source_create(obs_data_t *settings, obs_source_t *source)
{
	Vote *context = new Vote(settings);
	context->setSource(source);

	vote_source_update(context, settings);
	return context;
}

static void vote_source_destroy(void *data)
{
	if (!data)
		return;
	Vote *s = (Vote *)data;
	s->baseDestroy();
	s->deleteLater();
}

static void vote_source_defaults(obs_data_t *settings)
{
	QmlSourceBase::baseDefault(settings);
	obs_data_set_default_int(settings, "width", 400);
	obs_data_set_default_int(settings, "height", 246);
	obs_data_set_default_string(
		settings, "rule",
		u8R"({"visible":true,"fontInfo":{"fontFamily":"阿里巴巴普惠体 R","fontSize":12,"color":"#747578","backgroundColor":"transparent","bold":false,"italic":false,"underline":false,"strikeout":false}})");
	obs_data_set_default_string(
		settings, "subject",
		u8R"({"subject":"","fontInfo":{"fontFamily":"阿里巴巴普惠体 M","fontSize":16,"color":"#D4D4D5","backgroundColor":"transparent","bold":false,"italic":false,"underline":false,"strikeout":false}})");
	obs_data_set_default_string(
		settings, "option",
		u8R"([{"optionContent":"","voteCount":0,"fontInfo":{"fontFamily":"阿里巴巴普惠体 R","fontSize":14,"color":"#D4D4D5","backgroundColor":"transparent","bold":false,"italic":false,"underline":false,"strikeout":false}},{"optionContent":"","voteCount":0,"fontInfo":{"fontFamily":"阿里巴巴普惠体 R","fontSize":14,"color":"#D4D4D5","backgroundColor":"transparent","bold":false,"italic":false,"underline":false,"strikeout":false}}])");
	obs_data_set_default_int(settings, "duration", 120);
	obs_data_set_default_int(settings, "cusDuration", 30);
	obs_data_set_default_bool(settings, "useDuration", true);
	obs_data_set_default_int(settings, "progress", 0);
	obs_data_set_default_int(settings, "startShortCut", -1);
	obs_data_set_default_int(settings, "stopShortCut", -1);
	obs_data_set_default_int(settings, "imgWidth", 400);
	obs_data_set_default_int(settings, "imgHeight", 396);
	obs_data_set_default_string(settings, "backgroundImage",
				    "qrc:/qmlfiles/pic_bullet_chat.png");
	obs_data_set_default_string(settings, "voteCount", "");
	Vote::default(settings);
}

static obs_properties_t *vote_source_properties(void *data)
{
	if (!data)
		return nullptr;
	Vote *s = (Vote *)data;
	obs_properties_t *props = s->baseProperties();
	obs_properties_add_text(props, "subject", u8"投票主题",
				OBS_TEXT_DEFAULT);
	obs_properties_add_text(props, "option", u8"投票选项",
				OBS_TEXT_DEFAULT);
	obs_properties_add_text(props, "voteCount", u8"投票数更新",
				OBS_TEXT_DEFAULT);
	obs_properties_add_int(props, "duration", u8"投票时长", -1, 99999, 1);
	obs_properties_add_int(props, "cusDuration", u8"投票时长", -1, 99999,
			       1);
	obs_properties_add_bool(props, "useDuration", u8"使用投票时长");
	obs_properties_add_text(props, "rule", u8"投票规则", OBS_TEXT_DEFAULT);
	obs_properties_add_text(props, "backgroundImage", u8"投票背景",
				OBS_TEXT_DEFAULT);
	obs_properties_add_int(props, "imgWidth", u8"背景图宽", 1, 9999, 1);
	obs_properties_add_int(props, "imgHeight", u8"背景图高", 1, 9999, 1);
	obs_properties_add_int(props, "startShortCut", u8"启动快捷键", INT_MIN,
			       INT_MAX, 1);
	obs_properties_add_int(props, "stopShortCut", u8"结束快捷键", INT_MIN,
			       INT_MAX, 1);
	obs_properties_add_int(props, "progress", u8"当前状态", 0, 2, 1);
	return props;
}

struct obs_source_info vote_source_info = {
	"vote_source",
	OBS_SOURCE_TYPE_INPUT,
	OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION | OBS_SOURCE_DO_NOT_DUPLICATE,
	vote_source_get_name,
	vote_source_create,
	vote_source_destroy,
	base_source_getwidth,
	base_source_getheight,
	vote_source_defaults,
	vote_source_properties,
	vote_source_update,
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
