#pragma once

#include <QObject>
#include <QAbstractListModel>
#include <QMutex>
#include "qmlsourcebase.h"

#define NewRoleOptionContent Qt::UserRole + 1
#define NewRoleVoteCount Qt::UserRole + 2
#define NewRoleFontFamily Qt::UserRole + 3
#define NewRoleFontSize Qt::UserRole + 4
#define NewRoleOptionColor Qt::UserRole + 5
#define NewRoleBold Qt::UserRole + 6
#define NewRoleItalic2 Qt::UserRole + 7

struct NewVoteOption {
	NewVoteOption() {}
	QString optionContent;
	int voteCount;

	QColor color;
	QString fontFamily;
	bool bold;
	bool italic;
};

class NewVoteModel : public QAbstractListModel {
	Q_OBJECT
public:
	NewVoteModel(QObject *parent = nullptr);

	QVariant data(const QModelIndex &index,
		      int role = Qt::DisplayRole) const override;
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	QHash<int, QByteArray> roleNames() const override;

	Q_INVOKABLE void initModelData(const QJsonArray &array);
	Q_INVOKABLE void updateVoteCount(const QJsonObject &data);

private:
	QVector<NewVoteOption> voteOptionInfos;
};

class NewVote : public QmlSourceBase {
	Q_OBJECT
public:
	DEFINE_PROPERTY(QString, voteSubject)
	DEFINE_PROPERTY(QString, themeColor)
	DEFINE_PROPERTY(bool, themeBold)
	DEFINE_PROPERTY(bool, themeItalic)
	DEFINE_PROPERTY(QString, themeFontFamily)
	DEFINE_PROPERTY(int, themeType)
	DEFINE_PROPERTY(int, transparence)

	DEFINE_PROPERTY(int, duration)
	DEFINE_PROPERTY(bool, useDuration)

	DEFINE_PROPERTY(QString, downColor)
	DEFINE_PROPERTY(bool, downBold)
	DEFINE_PROPERTY(bool, downItalic)
	DEFINE_PROPERTY(QString, downFontFamily)

	DEFINE_PROPERTY(QString, dataColor)
	DEFINE_PROPERTY(bool, dataBold)
	DEFINE_PROPERTY(bool, dataItalic)
	DEFINE_PROPERTY(QString, dataFontFamily)

	DEFINE_PROPERTY(int, imageWidth)
	DEFINE_PROPERTY(int, imageHeight)
	DEFINE_PROPERTY(int, progress)

public:
	NewVote(obs_data_t *s, QObject *parent = nullptr);
	static void default(obs_data_t *settings);
	void resetModel(const QJsonArray &array);
	void updateVoteCount(const QJsonObject &data);

public:
	NewVoteModel *newVoteModel;
	obs_data_t *m_s;
};
