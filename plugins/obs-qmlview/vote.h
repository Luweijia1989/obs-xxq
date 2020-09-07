#pragma once

#include <QObject>
#include <QAbstractListModel>
#include <QMutex>
#include "qmlsourcebase.h"

#define RoleOptionContent Qt::UserRole + 1
#define RoleVoteCount Qt::UserRole + 2
#define RoleFontFamily Qt::UserRole + 3
#define RoleFontSize Qt::UserRole + 4
#define RoleBackgroundColor Qt::UserRole + 5
#define RoleOptionColor Qt::UserRole + 6
#define RoleBold Qt::UserRole + 7
#define RoleItalic Qt::UserRole + 8
#define RoleStrikeout Qt::UserRole + 9
#define RoleUnderLine Qt::UserRole + 10

struct VoteOption {
	VoteOption() {}
	QString optionContent;
	int voteCount;
	QString fontFamily;
	int fontSize;
	QColor backgroundColor;
	QColor color;
	bool bold;
	bool italic;
	bool strikeout;
	bool underline;
};

class VoteModel : public QAbstractListModel {
	Q_OBJECT
public:
	VoteModel(QObject *parent = nullptr);

	QVariant data(const QModelIndex &index,
		      int role = Qt::DisplayRole) const override;
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	QHash<int, QByteArray> roleNames() const override;

	Q_INVOKABLE void initModelData(const QJsonArray &array);
	Q_INVOKABLE void updateVoteCount(const QJsonObject &data);

private:
	QVector<VoteOption> voteOptionInfos;
};

class Vote : public QmlSourceBase {
	Q_OBJECT
public:
	DEFINE_PROPERTY(QString, voteSubject)
	DEFINE_PROPERTY(QColor, backgroundColor)
	DEFINE_PROPERTY(QColor, color)
	DEFINE_PROPERTY(bool, bold)
	DEFINE_PROPERTY(bool, italic)
	DEFINE_PROPERTY(bool, strikeout)
	DEFINE_PROPERTY(bool, underline)
	DEFINE_PROPERTY(QString, fontFamily)
	DEFINE_PROPERTY(int, fontSize)
	DEFINE_PROPERTY(int, duration)
	DEFINE_PROPERTY(bool, useDuration)
	DEFINE_PROPERTY(bool, ruleVisible)
	DEFINE_PROPERTY(QColor, ruleBackgroundColor)
	DEFINE_PROPERTY(QColor, ruleColor)
	DEFINE_PROPERTY(bool, ruleBold)
	DEFINE_PROPERTY(bool, ruleItalic)
	DEFINE_PROPERTY(bool, ruleStrikeout)
	DEFINE_PROPERTY(bool, ruleUnderline)
	DEFINE_PROPERTY(QString, ruleFontFamily)
	DEFINE_PROPERTY(int, ruleFontSize)
	DEFINE_PROPERTY(QString, backgroundImage)
	DEFINE_PROPERTY(int, imageWidth)
	DEFINE_PROPERTY(int, imageHeight)
	DEFINE_PROPERTY(int, progress)

public:
	Vote(obs_data_t *s, QObject *parent = nullptr);
	static void default(obs_data_t *settings);
	void resetModel(const QJsonArray &array);
	void updateVoteCount(const QJsonObject &data);

public:
	VoteModel *voteModel;
	obs_data_t *m_s;
	QString m_lastRuleString;
	QString m_lastOptionString;
	QString m_lastSubjectString;
};
