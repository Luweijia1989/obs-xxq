import QtQuick 2.14
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.1
import QtGraphicalEffects 1.0
Item{
	id: root
	width: audioLiveLinkProperties.basicWidth
	height: audioLiveLinkProperties.basicHeight
	x: 0
	y: 0
	visible: true
	Rectangle {
		id: audiolink
        width: audioLiveLinkProperties.backWidth
        height: parent.height
        anchors.fill: parent
		x: audioLiveLinkProperties.posX
		y: audioLiveLinkProperties.posY
		transformOrigin:Item.TopLeft
		scale: audioLiveLinkProperties.scale
		Image {
			id: background
            anchors.fill: parent
			source: audioLiveLinkProperties.path
			fillMode:Image.PreserveAspectCrop
			sourceSize: Qt.size(parent.width, parent.height)
			smooth: true
			visible: true
		}
		
		GaussianBlur {
			anchors.fill: background
			source: background
			radius: 400
			samples:400
		}
		
		Rectangle 
		{
			id:blurmask
			x: 0
			y: 0; 
			width: audioLiveLinkProperties.basicWidth
			height: audioLiveLinkProperties.basicHeight
			color: "#66000000"
		}
		
		  CommonAnimateImage{
		  id: voicewave
		  anchors.left: parent.left
		  anchors.leftMargin: audioLiveLinkProperties.voiceWaveLeftMargin
		  anchors.top: parent.top
		  anchors.topMargin: audioLiveLinkProperties.voiceWaveTopMargin
		  width: audioLiveLinkProperties.voiceWaveSize
		  height: audioLiveLinkProperties.voiceWaveSize
		  suffix: "apng"
		  cacheSource: audioLiveLinkProperties.wave
		  cache: true
		  visible:true
		  playing: false
          playCount: 1
          Rectangle {
              id: avartar
              anchors.centerIn: parent
              width: audioLiveLinkProperties.avatarSize + audioLiveLinkProperties.borderWidth * 2
              height: width
              radius: width/2
              border.width:audioLiveLinkProperties.borderWidth
              border.color:"#FFFFFF"

              Item {
                  anchors.centerIn: parent
                  width: audioLiveLinkProperties.avatarSize
                  height: width

                  CommonAnimateImage {
                      id: img
                      smooth: true
                      visible: false
                      anchors.fill: parent
                      cacheSource: audioLiveLinkProperties.path
                      antialiasing: true
                  }

                  Rectangle {
                      id: mask
                      visible: false
                      antialiasing: true
                      anchors.fill: parent
                      radius: parent.width/2
                  }

                  OpacityMask {
                      source: img
                      maskSource: mask
                      anchors.fill: img
                      antialiasing: true
                      visible: img.status === Image.Ready
                  }
              }
          }
		}

		  CommonAnimateImage{
		  id: faceeffect
		  anchors.left: parent.left
		  anchors.leftMargin: 114
		  anchors.top: parent.top
		  anchors.topMargin: 233
		  width: 513
		  height: 513
		  suffix: "apng"
		  cacheSource: audioLiveLinkProperties.effect
		  cache: true
		  visible:false
		  playing: false
		  }
	}
	
	
		Connections {
		target: audioLiveLinkProperties
		onPlay:
		{
			voicewave.play();
		}

		onShowPkEffect:
		{
			faceeffect.visible = true;
			faceeffect.cacheSource = audioLiveLinkProperties.effect
			faceeffect.play();
		}
		onStopPkEffect:
		{
			faceeffect.visible = false;
			faceeffect.cacheSource = "";
			faceeffect.stop();
		}
	}
}
