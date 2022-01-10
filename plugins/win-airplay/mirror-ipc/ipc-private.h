#pragma once
#include "rpc/RPCService.h"

class MirrorRPCPrivate : public RPCService {
	Q_OBJECT
public:
	MirrorRPCPrivate() : RPCService("MirrorIPCChannel", 1024) {}
	DEF_PROCESS_INVOKE(MirrorRPCPrivate)

	void sendCMD(const QString &cmd);

protected:
	void paramParse(int actionType, const QString &strParam)
	{
		Q_UNUSED(actionType)
		emit commandReceived(strParam);
	}

signals:
	void commandReceived(const QString &cmd);
};
