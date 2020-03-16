#pragma once

#include <QObject>
#include <QAbstractListModel>
#include <QMutex>
#include "qmlsourcebase.h"



#define RoleName            Qt::UserRole+1
#define RoleNameFontName    Qt::UserRole+2
#define RoleNameFontSize    Qt::UserRole+3
#define RoleNameFontColor   Qt::UserRole+4
#define RoleDiamondNum      Qt::UserRole+5
#define RoleDiamondFontName Qt::UserRole+6
#define RoleDiamondFontSize Qt::UserRole+7
#define RoleDiamondFontColor Qt::UserRole+8
#define RoleDiamondImage     Qt::UserRole+9
#define RoleRankNum         Qt::UserRole+10
#define RoleRankFontName    Qt::UserRole+11
#define RoleRankFontSize    Qt::UserRole+12
#define RoleRankFontColor   Qt::UserRole+13
#define RoleRankBackImage   Qt::UserRole+14
#define RoleNickNameBold    Qt::UserRole+15
#define RoleRankBold        Qt::UserRole+16
#define RoleDiamondBold     Qt::UserRole+17

struct RankInfo
{
	RankInfo() {}
	QString nickname;
	QString fontName;
	int fontSize;
	QString color;
	QString diamondNum;
	QString diamondFontName;
	int diamondFontSize;
	QString diamondColor;
	QString diamondImage;
	int rankNum;
	QString rankFontName;
	int rankFontSize;
	QString rankColor;
	QString rankNumBackgroundImage;
	bool nicknameBold;
	bool rankBold;
	bool diamondBold;
};

class RankModel : public QAbstractListModel
{
	Q_OBJECT
public:
	RankModel(QObject *parent = nullptr);

	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	QHash<int, QByteArray> roleNames() const override;

	Q_INVOKABLE void initModelData(const QString &data);

private:
	QVector<RankInfo> rankInfos;
};


class Rank : public QmlSourceBase {
	Q_OBJECT
public:
	DEFINE_PROPERTY(QString, rankName)
	DEFINE_PROPERTY(QColor, fillColor)
	DEFINE_PROPERTY(QColor, frontColor)
	DEFINE_PROPERTY(bool, bold)
	DEFINE_PROPERTY(bool, italic)
	DEFINE_PROPERTY(bool, strikeout)
	DEFINE_PROPERTY(bool, underline)
	DEFINE_PROPERTY(QString, font)
	DEFINE_PROPERTY(int, fontSize)
	DEFINE_PROPERTY(bool, hasRankType)
	DEFINE_PROPERTY(bool, hasRankNum)
	DEFINE_PROPERTY(bool, hasDiamond)
	DEFINE_PROPERTY(QString, rankList)
	DEFINE_PROPERTY(int, leftMargin)
	DEFINE_PROPERTY(int, topMargin)
	DEFINE_PROPERTY(int, rightMargin)
	DEFINE_PROPERTY(int, bottomMargin)
	DEFINE_PROPERTY(int, spacing)

public:
	Rank(QObject *parent = nullptr);
	static void default(obs_data_t *settings);

public:
	RankModel *rankModel;
};
