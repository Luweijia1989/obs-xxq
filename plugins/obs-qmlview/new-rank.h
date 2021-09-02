#pragma once

#include <QObject>
#include <QAbstractListModel>
#include <QMutex>
#include "qmlsourcebase.h"



#define NewRoleName             Qt::UserRole+1
#define NewRoleNameFontName     Qt::UserRole+2
#define NewRoleNameFontSize     Qt::UserRole+3
#define NewRoleNameFontColor    Qt::UserRole+4
#define NewRoleDiamondNum       Qt::UserRole+5
#define NewRoleDiamondFontName  Qt::UserRole+6
#define NewRoleDiamondFontSize  Qt::UserRole+7
#define NewRoleDiamondFontColor Qt::UserRole+8
#define NewRoleDiamondImage     Qt::UserRole+9
#define NewRoleRankNum         Qt::UserRole+10
#define NewRoleRankFontName    Qt::UserRole+11
#define NewRoleRankFontSize    Qt::UserRole+12
#define NewRoleRankFontColor   Qt::UserRole+13
#define NewRoleRankBackImage   Qt::UserRole+14
#define NewRoleNickNameBold    Qt::UserRole+15
#define NewRoleRankBold        Qt::UserRole+16
#define NewRoleDiamondBold     Qt::UserRole+17
#define NewRoleAvarstarPath    Qt::UserRole+18
#define NewRoleItalic          Qt::UserRole+19

struct NewRankInfo
{
	NewRankInfo() {}
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
	QString avarstarPath;
	bool italic;
};

class NewRankModel : public QAbstractListModel
{
	Q_OBJECT
public:
	NewRankModel(QObject *parent = nullptr);

	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	QHash<int, QByteArray> roleNames() const override;

	Q_INVOKABLE void initModelData(const QString &data);

private:
	QVector<NewRankInfo> rankInfos;
};


class NewRank : public QmlSourceBase {
	Q_OBJECT
public:
	DEFINE_PROPERTY(QString, rankName)
		DEFINE_PROPERTY(QColor, fillColor)

		DEFINE_PROPERTY(int, themetype)
		DEFINE_PROPERTY(QString, themefont)
		DEFINE_PROPERTY(bool, themebold)
		DEFINE_PROPERTY(bool, themeitalic)
		DEFINE_PROPERTY(QString, themefontcolor)
		DEFINE_PROPERTY(QColor, datafontcolor)
		DEFINE_PROPERTY(bool, databold)
		DEFINE_PROPERTY(bool, dataitalic)
		DEFINE_PROPERTY(QString, datafont)
		DEFINE_PROPERTY(bool, hasRankType)
		DEFINE_PROPERTY(bool, hasRankNum)
		DEFINE_PROPERTY(bool, hasDiamond)
		DEFINE_PROPERTY(QString, rankList)
		DEFINE_PROPERTY(int, leftMargin)
		DEFINE_PROPERTY(int, topMargin)
		DEFINE_PROPERTY(int, rightMargin)
		DEFINE_PROPERTY(int, bottomMargin)
		DEFINE_PROPERTY(int, spacing)
		DEFINE_PROPERTY(float, transparence)
public:
	NewRank(QObject *parent = nullptr);
	static void default(obs_data_t *settings);

public:
	NewRankModel *rankModel;
};
