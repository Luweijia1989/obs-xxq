import QtQuick 2.12
import QtQuick.Controls 2.12

AnimatedImage {
    id: main
    property int playCount: -1
    property string cacheSource: ""
    property bool imageCrop: false
    property string suffix: ""

    // 外部请勿修改
    property int tempPlayCount: 0

    signal playEnd()
    asynchronous: true
    cache: true
    smooth: true
    mipmap: true

    function replay() {
        currentFrame = 0
        tempPlayCount = playCount
        playing = true
    }

    onPlayCountChanged: tempPlayCount = playCount

    onCacheSourceChanged: {
        if(cacheSource === ""){
            main.source = ""
            return
        }

        if(main.cacheSource.indexOf("qrc:") !== -1 ||
          (main.cacheSource.indexOf("file:///") !== -1)){
            main.source = cacheSource
            return
        }

        var str = cacheSource
        if(imageCrop){
            var index = main.cacheSource.indexOf("imageView2")
            var urlSuffix = "?imageView2/0/w/" + Math.round(main.width * 2) + "/h/" + Math.round(main.height * 2)
            if(index === -1){
                str += urlSuffix
            }
            else{
                var pureUrl = main.cacheSource.substring(0, index - 1)
                pureUrl += urlSuffix
                str = pureUrl
            }
        }

        g_bridge.downLoad(str, downloadCallBack, suffix)
    }

    function downloadCallBack(res){
        main.source = res.file
    }

    onCurrentFrameChanged: {
        if(playCount == -1){
            return
        }

        if((frameCount != 0) && (currentFrame == frameCount - 1)){
            tempPlayCount--
            if(tempPlayCount == 0)
            {
                playing = false
                playEnd()
            }
        }
    }
}
