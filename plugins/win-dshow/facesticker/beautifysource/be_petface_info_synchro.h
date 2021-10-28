#ifndef BE_PETFACE_INFO_SYNCHRO
#define BE_PETFACE_INFO_SYNCHRO


/*
* 与qml进行交互，发送宠物脸的数据
*/

#include <QObject>
#include <QMetaType>
#include "bef_effect_ai_pet_face.h"

class PetFaceInfo {
	Q_GADGET
	Q_PROPERTY(qint32 petType READ petType WRITE setPetType)
	Q_PROPERTY(qint32 action READ action WRITE setAction)
public:
	PetFaceInfo();
	~PetFaceInfo();

	void init(qint32 petType, qint32 action);

	qint32 petType() const { return m_petType; }
	void setPetType(qint32 type) { m_petType = type; }
	qint32 action() const { return m_action; }
	void setAction(qint32 action) { m_action = action; }

private:
	qint32 m_action;
	qint32 m_petType;
};

Q_DECLARE_METATYPE(PetFaceInfo);

class BEPetFaceInfoSynchro : public QObject {
	Q_OBJECT

public:
	BEPetFaceInfoSynchro();
	~BEPetFaceInfoSynchro();

public:
	void syncPetFaceDetectInfo(bef_ai_pet_face_result* petFaceInfo);
signals:
	void petFaceInfoChanged(QVariantList petList);
};


#endif