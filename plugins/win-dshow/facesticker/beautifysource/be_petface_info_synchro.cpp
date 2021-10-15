#include "be_petface_info_synchro.h"
#include <QVariant>


PetFaceInfo::PetFaceInfo()
:m_petType(0),
m_action(0) {
	
}

PetFaceInfo::~PetFaceInfo() {
	
}

void PetFaceInfo::init(qint32 petType, qint32 action) {
	m_petType = petType;
	m_action = action;
}


/// /////////////////////////////////////////////////////////
BEPetFaceInfoSynchro::BEPetFaceInfoSynchro() {

}

BEPetFaceInfoSynchro::~BEPetFaceInfoSynchro() {

}

void BEPetFaceInfoSynchro::syncPetFaceDetectInfo(bef_ai_pet_face_result* petFaceInfo) {
	if(petFaceInfo == nullptr) {
		QVariantList list;
		emit petFaceInfoChanged(list);
		return;
	}

	QVariantList list;
	for (int i = 0; i < petFaceInfo->face_count; i++) {
		bef_ai_pet_face_info info = petFaceInfo->p_faces[i];

		PetFaceInfo petinfo;
		petinfo.init(info.type, info.action);
		list.append(QVariant::fromValue(petinfo));
	}

	emit petFaceInfoChanged(list);
}

