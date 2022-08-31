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
		height: 1080
		x: audioLiveLinkProperties.posX
		y: audioLiveLinkProperties.posY
		transformOrigin:Item.TopLeft
		scale: audioLiveLinkProperties.scale
		Image {
			id: background
			width: audioLiveLinkProperties.backWidth
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
		  Rectangle {
			  id: avartar
			  x: audioLiveLinkProperties.avatarPos
			  y: audioLiveLinkProperties.avatarPos
			  width: audioLiveLinkProperties.avatarSize
			  height: audioLiveLinkProperties.avatarSize
			  radius: audioLiveLinkProperties.avatarSize/2
			  Rectangle {
				  id: imgeBg
				  anchors.left: parent.left
				  anchors.top: parent.top
				  width:parent.width
				  height:parent.height
				  radius: audioLiveLinkProperties.avatarSize/2
				  visible: false
				  Image {
					  id: squareavatarImage
					  smooth: true
					  visible: true
					  anchors.fill: parent
					  source:  audioLiveLinkProperties.path
				      sourceSize:Qt.size(audioLiveLinkProperties.avatarSize,audioLiveLinkProperties.avatarSize)
					  antialiasing: true
				  }

				  Rectangle {
				  id: border1
				  color: "#00000000"
				  anchors.fill: parent
				  radius: audioLiveLinkProperties.avatarSize/2
				  visible: true
				  antialiasing: true
				  smooth: true
				  border.width:audioLiveLinkProperties.borderWidth
				  border.color:"#FFFFFF"
			  }
			  }
			  Rectangle {
				  id: mask1
				  color: "black"
				  anchors.fill: parent
				  radius: audioLiveLinkProperties.avatarSize/2
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
