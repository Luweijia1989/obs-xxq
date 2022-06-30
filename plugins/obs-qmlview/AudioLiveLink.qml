import QtQuick 2.14
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.1
import QtGraphicalEffects 1.0
Item {
	function getWidth(count)
	{
		if(count === 4)
		{
			return 1440;
		}
		else
		{
			return 1200;
		}
	}

	function anchorFaceSize(count)
	{
		if(count === 4)
		{
			return 517;
		}
		else
		{
			return 458;
		}
	}

	function anchorFaceLeftMargin(count)
	{
		if(count === 4)
		{
			return 461;
		}
		else
		{
			return 369;
		}
	}

	function anchorFaceTopMargin(count)
	{
		if(count === 4)
		{
			return 215;
		}
		else
		{
			return 310;
		}
	}

	function anchorFaceImageMargin(count)
	{
		if(count === 4)
		{
			return 97;
		}
		else
		{
			return 86;
		}
	}

	function anchorFaceImageSize(count)
	{
		if(count === 4)
		{
			return 324;
		}
		else
		{
			return 286;
		}
	}
    id: root
    width: audioLiveLinkProperties.isMuliti? getWidth(audioLiveLinkProperties.multiCount):720
    height: 1080
    x: 0
    y: 0
    Image {
		id: background
        width: audioLiveLinkProperties.isMuliti? getWidth(audioLiveLinkProperties.multiCount):720
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
      anchors.leftMargin: audioLiveLinkProperties.isMuliti?anchorFaceLeftMargin(audioLiveLinkProperties.multiCount):213
      anchors.top: parent.top
      anchors.topMargin: audioLiveLinkProperties.isMuliti?anchorFaceTopMargin(audioLiveLinkProperties.multiCount):350
      width: audioLiveLinkProperties.isMuliti?anchorFaceSize(audioLiveLinkProperties.multiCount):293
      height: audioLiveLinkProperties.isMuliti?anchorFaceSize(audioLiveLinkProperties.multiCount):293
      suffix: "apng"
      cacheSource: audioLiveLinkProperties.wave
	  cache: true
	  visible:true
      playing: false
      Rectangle {
		  id: avartar
		  x: audioLiveLinkProperties.isMuliti?anchorFaceImageMargin(audioLiveLinkProperties.multiCount):52
		  y: audioLiveLinkProperties.isMuliti?anchorFaceImageMargin(audioLiveLinkProperties.multiCount):52
		  width: audioLiveLinkProperties.isMuliti?anchorFaceImageSize(audioLiveLinkProperties.multiCount):190
		  height:audioLiveLinkProperties.isMuliti?anchorFaceImageSize(audioLiveLinkProperties.multiCount):190
		  radius: audioLiveLinkProperties.isMuliti?anchorFaceImageSize(audioLiveLinkProperties.multiCount)/2:95
		  Rectangle {
			  id: imgeBg
			  anchors.left: parent.left
			  anchors.top: parent.top
			  width:parent.width
			  height:parent.height
			  radius: audioLiveLinkProperties.isMuliti?anchorFaceImageSize(audioLiveLinkProperties.multiCount)/2:95
			  visible: false
			  Image {
				  id: squareavatarImage
				  smooth: true
				  visible: true
				  anchors.fill: parent
				  source:  audioLiveLinkProperties.path
				  sourceSize: audioLiveLinkProperties.isMuliti?Qt.size(anchorFaceImageSize(audioLiveLinkProperties.multiCount),anchorFaceImageSize(audioLiveLinkProperties.multiCount)):Qt.size(190,190)
				  antialiasing: true
			  }

			  Rectangle {
			  id: border1
			  color: "#00000000"
			  anchors.fill: parent
			  radius: audioLiveLinkProperties.isMuliti?anchorFaceImageSize(audioLiveLinkProperties.multiCount)/2:95
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
			  radius: audioLiveLinkProperties.isMuliti?anchorFaceImageSize(audioLiveLinkProperties.multiCount)/2:95
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
		onLink:
		{
			if(audioLiveLinkProperties.isMuliti)
			{
				if(audioLiveLinkProperties.multiCount === 4)
				{
					voicewave.anchors.leftMargin = 331;
					voicewave.anchors.topMargin = 97;
					voicewave.width = 778;
					voicewave.height = 778;
					avartar.x = 187;
					avartar.y = 187;
					avartar.width = 404;
					avartar.height = 404;
					avartar.radius = 202;
					imgeBg.radius = 202;
					squareavatarImage.sourceSize.width = 404;
					squareavatarImage.sourceSize.width = 404;
					border1.radius = 202;
					border1.border.width = 9;
					mask1.radius = 202;
				}
				else
				{
					voicewave.anchors.leftMargin = 210;
					voicewave.anchors.topMargin = 97;
					voicewave.width = 780;
					voicewave.height = 780;
					avartar.x = 192;
					avartar.y = 192;
					avartar.width = 396;
					avartar.height = 396;
					avartar.radius = 198;
					imgeBg.radius = 198;
					squareavatarImage.sourceSize.width = 396;
					squareavatarImage.sourceSize.width = 396;
					border1.radius = 198;
					border1.border.width = 11;
					mask1.radius = 198;
				}
			}
		}
	}
}
