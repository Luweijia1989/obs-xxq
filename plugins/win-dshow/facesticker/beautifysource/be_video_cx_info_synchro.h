#ifndef BE_VIDEO_CX_INFO_SYNCHRO_H
#define BE_VIDEO_CX_INFO_SYNCHRO_H

#include <QObject>
#include "bef_effect_ai_c1.h"
#include "bef_effect_ai_c2.h"
#include "bef_effect_ai_video_cls.h"

class BEVideoCXInfoSynchro : public QObject
{
	Q_OBJECT
public:
	BEVideoCXInfoSynchro();
	~BEVideoCXInfoSynchro();

	void syncVideoClsInfo(bef_ai_video_cls_ret* videoClsInfo);
	void syncC1Info(bef_ai_c1_output* c1Info);
	void syncC2Info(bef_ai_c2_ret* c2Info);

signals:
	void videoClsInfoChanged(QString title, QString value);
	void c1Changed(QString title, QString value);
	void c2Changed(QString title, QString value);

private:
	void registerC1Type();
private:
	QString m_c1Info;
	QString m_c2Info;
	QString m_VideoClsInfo;
};


#endif
