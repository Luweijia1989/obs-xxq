#pragma once

#include <QObject>
#include "qmlsourcebase.h"
#include <obs.hpp>

#define RoleColor		Qt::UserRole+1
#define RoleAudioLevel		Qt::UserRole+2

class AudioWave;

struct AudioInfo {
	QString color;
	qreal audioLevel;
};

class AudioInfoModel : public QAbstractListModel {
	Q_OBJECT
public:
	AudioInfoModel(AudioWave *w, QObject *parent = nullptr);

	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	QHash<int, QByteArray> roleNames() const override;
	void start();
	void stop();

public slots:
	void onTimerUpdate();

private:
	QVector<AudioInfo> audioInfos;
	QTimer updateTimer;
	AudioWave *audiowave;
	QByteArray finalAudioData;
};

class AudioWave : public QmlSourceBase {
	Q_OBJECT
public:
	AudioWave(QObject *parent = nullptr);
	~AudioWave();
	static void default(obs_data_t *settings);

	AudioInfoModel *audioModel;
	OBSSource audioMicSource;
	OBSSource audioDesktopSource;
	obs_volmeter_t *micVolmeter;
	obs_volmeter_t *desktopVolmeter;
	QMutex dataMutex;
	QByteArray audioData;
	QByteArray audioDesktopData;
};
