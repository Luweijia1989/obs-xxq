#include "ipc-private.h"

BEGIN_INVOKE(MirrorRPCPrivate)
ON_INVOKE_2(paramParse, int, QString)
END_INVOKE

void MirrorRPCPrivate::sendCMD(const QString &cmd)
{
	RPCService::Invoke< int, QString >(u8"paramParse", 0, cmd);
}
