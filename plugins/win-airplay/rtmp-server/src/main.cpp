#include <map>
#include <signal.h>
#include <iostream>

#include "Util/MD5.h"
#include "Util/File.h"
#include "Util/logger.h"
#include "Util/SSLBox.h"
#include "Util/onceToken.h"
#include "Network/TcpServer.h"
#include "Poller/EventPoller.h"

#include "Common/config.h"
#include "Rtsp/UDPServer.h"
#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
#include "Shell/ShellSession.h"
#include "Rtmp/FlvMuxer.h"
#include "Player/PlayerProxy.h"
#include "Http/WebSocketSession.h"
#include "common-define.h"

#include <QCoreApplication>

using namespace std;
using namespace toolkit;
using namespace mediakit;

IPCClient *ipc_client = NULL;

void initEventListener() {
    static onceToken s_token([]() {
        //监听rtsp/rtmp推流事件，返回结果告知是否有推流权限
        NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastMediaPublish, [](BroadcastMediaPublishArgs) {
            DebugL << "推流鉴权：" << args._schema << " " << args._vhost << " " << args._app << " " << args._streamid << " "
                   << args._param_strs;
            invoker("", false, false);//鉴权成功

	    send_status(ipc_client, MIRROR_START);
        });

        //监听播放或推流结束时消耗流量事件
        NoticeCenter::Instance().addListener(nullptr, Broadcast::kBroadcastFlowReport, [](BroadcastFlowReportArgs) {
            DebugL << "播放器(推流器)断开连接事件:" << args._schema << " " << args._vhost << " " << args._app << " "
                   << args._streamid << " " << args._param_strs
                   << "\r\n使用流量:" << totalBytes << " bytes,连接时长:" << totalDuration << "秒";

	    send_status(ipc_client, MIRROR_STOP);
        });
    }, nullptr);
}


int main(int argc,char *argv[]) {
    QCoreApplication app(argc, argv);

    ipc_client_create(&ipc_client);

    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().add(std::make_shared<FileChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    initEventListener();

    TcpServer::Ptr rtmpSrv(new TcpServer());

    rtmpSrv->start<RtmpSession>(1935);//默认1935

    MirrorRPC rpc;
    QObject::connect(&rpc, &MirrorRPC::quit, &app, &QCoreApplication::quit);

    app.exec();

    rtmpSrv.reset();

    ipc_client_destroy(&ipc_client);

    return 0;
}

