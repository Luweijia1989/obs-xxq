#pragma once
#include <qobject.h>
#include <QThread>
#include <QSharedMemory>
#include <QSystemSemaphore>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMutex>
#include "invoke_def.h"
#include <cassert>

class RPCService : public QThread
{
	Q_OBJECT;
	enum class RpcServiveType
	{
		NO_SERVICE = 0,
		SERVER     = 1,
		CLIENT     = 2,
	} rpcServiceType_ = RpcServiveType::NO_SERVICE;
	static const int DEFAULT_SHARED_MEMORY_SIZE = 1024 * 1024;
public:
	RPCService(QString key, uint32_t sharedMemorySize = DEFAULT_SHARED_MEMORY_SIZE);
	~RPCService();
	void StopRpc();
protected:
	//调用远程方法
	inline void Invoke(QString funcName);
	template<typename T>
	void Invoke(QString funcName, T);
	template<typename T1, typename T2>
	void Invoke(QString funcName, T1, T2);
	template<typename T1, typename T2, typename T3>
	void Invoke(QString funcName, T1, T2, T3);
	virtual void ParseInvoke() = 0;
	QJsonDocument   m_funcPackObj;
private:
	virtual void run();
	void InternalInvoke(const char* data, int size);
	void InternalRecv();
private:
	//内部区分server/client,首先初始化的对象为server,后初始化的为client
	bool                          m_isServer = false;
	std::atomic<bool>             m_threadRun = false;

	int maxSharedMemorySize_ = DEFAULT_SHARED_MEMORY_SIZE;

	QMutex stopMutex_;
	//同步初始化
	QSystemSemaphore serverAquire_;
	//当前进程写锁,保证本进程同时只有一个线程写
	QMutex    m_WriteMutex;
	//等待client处理完数据
	QSystemSemaphore m_waitClientReadSemaphore;
	//通知server有数据到达
	QSystemSemaphore m_waitClientWriteSemaphore;
	//等待server处理完数据
	QSystemSemaphore m_waitServerReadSemaphore;
	//等待client有数据到达
	QSystemSemaphore m_waitServerWriteSemaphore;
	//server读client写通道
	QSharedMemory    sharedMemory_;

	QSharedMemory    ServerExistsFlag_;
	QSharedMemory    ClientExistsFlag_;
};

void RPCService::Invoke(QString funcName)
{
	QJsonObject packData;
	packData["function"] = funcName;
	QByteArray sendData = QJsonDocument(packData).toJson();
	if (maxSharedMemorySize_ < sendData.size())
	{
		assert(("invoke data size too big", false));
		return;
	}
	InternalInvoke(sendData.constData(), sendData.size());
}
template<typename T>
void RPCService::Invoke(QString funcName, T param)
{
	QJsonObject packData;
	packData["function"] = funcName;
	packData["0"] = param;
	QByteArray sendData = QJsonDocument(packData).toJson();
	if (maxSharedMemorySize_ < sendData.size())
	{
		assert(("invoke data size too big", false));
		return;
	}
	InternalInvoke(sendData.constData(), sendData.size());
}

template<typename T1, typename T2>
void RPCService::Invoke(QString funcName, T1 param1, T2 param2)
{
	QJsonObject packData;
	packData["function"] = funcName;
	packData["paramSize"] = 2;
	packData["0"] = param1;
	packData["1"] = param2;
	auto sendData = QJsonDocument(packData).toJson();
	if (maxSharedMemorySize_ < sendData.size())
	{
		assert(("invoke data size too big",false));
		return;
	}
	InternalInvoke(sendData.toStdString().c_str(), sendData.size());
}

template<typename T1, typename T2, typename T3>
void RPCService::Invoke(QString funcName, T1 param1, T2 param2, T3 param3)
{
	QJsonObject packData;
	packData["function"] = funcName;
	packData["paramSize"] = 3;
	packData["0"] = param1;
	packData["1"] = param2;
	packData["2"] = param3;
	auto sendData = QJsonDocument(packData).toJson();
	if (maxSharedMemorySize_ < sendData.size())
	{
		assert(("invoke data size too big", false));
		return;
	}
	InternalInvoke(sendData.toStdString().c_str(), sendData.size());
}

