import QtQuick 2.14
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.1
import QtGraphicalEffects 1.0

Item {
    id: root
    width: audioLiveLinkProperties.isMuliti? 1440:720
    height: 1080
    x: 0
    y: 0
    Image {
		id: background
        width: audioLiveLinkProperties.isMuliti? 1440:720
		height: 1080
		source: audioLiveLinkProperties.path
		fillMode:Image.PreserveAspectCrop
		sourceSize: Qt.size(parent.width, parent.height)
		smooth: true
		visible: true
    }
	
    FastBlur {
		anchors.fill: background
		source: background
		radius: 64
    }
	
	
	  CommonAnimateImage{
      id: voicewave
	  anchors.left: parent.left
      anchors.leftMargin: audioLiveLinkProperties.isMuliti?461:213
      anchors.top: parent.top
      anchors.topMargin: audioLiveLinkProperties.isMuliti?215:350
      width: audioLiveLinkProperties.isMuliti?517:293
      height: audioLiveLinkProperties.isMuliti?517:293
      suffix: "apng"
      cacheSource: audioLiveLinkProperties.wave
	  cache: true
	  visible:true
      playing: false
      Rectangle {
		  id: avartar
		  x: audioLiveLinkProperties.isMuliti?97:52
		  y: audioLiveLinkProperties.isMuliti?97:52
		  width: audioLiveLinkProperties.isMuliti?324:190
		  height:audioLiveLinkProperties.isMuliti?324:190
		  radius: audioLiveLinkProperties.isMuliti?162:95
		  Rectangle {
			  id: imgeBg
			  anchors.left: parent.left
			  anchors.top: parent.top
			  width:parent.width
			  height:parent.height
			  radius: audioLiveLinkProperties.isMuliti?162:95
			  visible: false
			  Image {
				  id: squareavatarImage
				  smooth: true
				  visible: true
				  anchors.fill: parent
				  source:  audioLiveLinkProperties.path
				  sourceSize: audioLiveLinkProperties.isMuliti?Qt.size(324,324):Qt.size(190,190)
				  antialiasing: true
			  }

			  Rectangle {
			  id: border1
			  color: "#00000000"
			  anchors.fill: parent
			  radius: audioLiveLinkProperties.isMuliti?162:95
			  visible: true
			  antialiasing: true
			  smooth: true
			  border.width:2
			  border.color:"#FFFFFF"
		  }
		  }
		  Rectangle {
			  id: mask1
			  color: "black"
			  anchors.fill: parent
			  radius: audioLiveLinkProperties.isMuliti?162:95
			  visible: false
			  antialiasing: true
			  smooth: true
		  }
		  OpacityMask {
			  id: maskAvatar1
			  anchors.fill: imgeBg
			  anchors.centerIn: parent
			  source: imgeBg
			  maskSource: mask1
			  visible: true
			  antialiasing: true
		  }
	  }
    }
	
	Text {
	id: anchorname
		text: audioLiveLinkProperties.name
	    x: audioLiveLinkProperties.isMuliti?573:213
	    y: 643
	    width:293
	    height:58
		elide: Text.ElideRight
		font.family: "阿里巴巴普惠体 M"
		font.pixelSize: 42
		font.bold: true
		color:"#FFFFFF"
		visible: audioLiveLinkProperties.isMuliti?false:true
		horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
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

    Connections {
		target: audioLiveLinkProperties
        onPlay:
        {
            voicewave.play();
		}
        onStop:
        {
            voicewave.stop();
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
