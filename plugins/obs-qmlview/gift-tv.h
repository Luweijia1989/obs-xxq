#pragma once

#include <QObject>
#include <QVector>
#include "qmlsourcebase.h"

struct GiftInfo {
	int giftType;        //0:普通 1:大礼物
	int count;           //礼物个数
	int hitCount;        //连击次数
	int giftPrice;       //礼物单价
	int diamond;         //礼物总价值
	int giftId;          //礼物id
	long long timeStamp; //送礼时间
	QString hitBatchId; //客户端自己维护的同一个人送同种礼物的唯一标识
	QString giftPath;            //礼物icon
	QString gfitName;            //礼物名称
	QString name;                //送礼人
	QString recName;             //收礼人
	QString avatarPath;          //送礼人头像
	QString backgroundImagePath; //背景图片
	int x;
	int y;
	int xPosition;
	int yPosition;
	bool isNew = true;
	bool isPlaceholder = false;
};

class GiftTV : public QmlSourceBase {
	Q_OBJECT
public:
	enum PreferLayout { Horizontal = 0, Vertical };
	Q_ENUM(PreferLayout)

	DEFINE_PROPERTY(QString, giftArray);
	DEFINE_PROPERTY(QString, gift);
	DEFINE_PROPERTY(QString, invalidGift);
	DEFINE_PROPERTY(QString, updateGift);
	DEFINE_PROPERTY(int, clearGift);

public:
	Q_INVOKABLE void autoExtendTvCols(int cols);

public:
	GiftTV(QObject *parent = nullptr);
	virtual ~GiftTV();
	static void default(obs_data_t *settings);

	void setTriggerCondition(int triggerCondition);
	void setTriggerConditionValue(int triggerConditionValue);
	void refurshGrid(int rows, int cols);
	void loadGiftArray(QStringList array);
	bool findAdvancedGrid(int rows, int cols, QJsonObject &giftInfo);
	bool findNormalGrid(int rows, int cols, QJsonObject &giftInfo);
	void removeGrid();
	void getPositonForNewGiftToGrid(QJsonObject &giftInfo);
	void updateMap();
	QJsonArray getGiftListByMap();
	void updatePrefer(int prefer);
	void updateMode(int mode);
	void updateDisappear(int disappear);
	void add(QString data);
	void stopTimer();
	void gridPrintf();
	int mode();
	int disappear();
	bool needExtendCols(int cols);
	int currentRows();
	int currentCols();
	void startPreview(QString isPreview);
	void clear(QString key);
	// ===========================
	void notifyQMLLoadArray();
	void notifyQMLInsertNewGift(QJsonObject &giftInfo);
	void notifyQMLUpdateGift(QJsonObject &giftInfo);
	void notifyQMLDeleteGift(QJsonObject &giftInfo);
	void notifyQMLClearArray();

private:
	PreferLayout m_preferLayout = Horizontal;
	int m_mode = 0;
	bool m_load = false;
	int m_triggerCondition = 0;
	int m_triggerConditionValue = 1000;
	QVector<QVector<QJsonObject>> m_grid;
	QMap<QString, QJsonObject> m_giftInfoMap;

	QTimer *m_timer;
	int modeList[2] = {5000, 2000};
	int m_disappear = 0;
	QString m_isPreview;
};
