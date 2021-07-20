import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
Item {
    id: container
    width: backGroundWidth(fansTargetProperties.themetype)
    height: backGroundHeight(fansTargetProperties.themetype)
    focus: true
    function backGroundPath(themeType)
    {
        if(themeType === 1)
        {
            return "qrc:/qmlfiles/image/fanstarget/background_fans1.png";
        }
        else if(themeType === 2)
        {
            return "qrc:/qmlfiles/image/fanstarget/background_fans2.png";
        }
        else if(themeType === 3)
        {
            return "qrc:/qmlfiles/image/fanstarget/tip.png";
        }
        else if(themeType === 4)
        {
            return "qrc:/qmlfiles/image/fanstarget/switch.png";
        }
        else
        {
            return "qrc:/qmlfiles/image/fanstarget/deepblue.png";
        }
    }


    function sprintf() {
        let args = arguments, string = args[0];
        for (let i = 1; i < args.length; i++) {
            let item = arguments[i];
            string   = string.replace('%d', item);
        }
        return string;
    }

    function progressText(realfans,totalfans)
    {
        if(totalfans === 0)
        {
            return "未设置";
        }
        else if(realfans>= totalfans)
        {
            return "已完成";
        }
        else
        {
            let template = '%d/%d';
            return sprintf(template , realfans, totalfans);
        }

    }

    function backGroundWidth(themeType)
    {
        if(themeType === 1)
        {
            return 602;
        }
        else if(themeType === 2)
        {
            return 593;
        }
        else if(themeType === 3)
        {
            return 482;
        }
        else if(themeType === 4)
        {
            return 643;
        }
        else
        {
            return 568;
        }
    }

    function backGroundHeight(themeType)
    {
        if(themeType === 1)
        {
            return 164;
        }
        else if(themeType === 2)
        {
            return 174;
        }
        else if(themeType === 3)
        {
            return 247;
        }
        else if(themeType === 4)
        {
            return 171;
        }
        else
        {
            return 166;
        }
    }

    function progressColor(themeType)
    {
        if(themeType === 1)
        {
            return "#FFFFFF";
        }
        else if(themeType === 2)
        {
            return "#C3E3EE";
        }
        else if(themeType === 3)
        {
            return "#8692C7";
        }
        else if(themeType === 4)
        {
            return "#71E776";
        }
        else
        {
            return "#7E94C8";
        }
    }

    function processWidth(themeType)
    {
        if(themeType === 1)
        {
            return 578
        }
        else if(themeType === 2)
        {
             return 448
        }
        else if(themeType === 3)
        {
             return 310
        }
        else if(themeType === 4)
        {
             return 403
        }
        else
        {
             return 478
        }
    }
	
    function processHeight(themeType)
    {
        if(themeType === 1)
        {
            return 22
        }
        else if(themeType === 2)
        {
             return 24
        }
        else if(themeType === 3)
        {
             return 18
        }
        else if(themeType === 4)
        {
             return 16
        }
        else
        {
             return 20
        }
    }
	
	function processPosX()
	{
        if(fansTargetProperties.themetype === 1)
        {
            return 12
        }
        else if(fansTargetProperties.themetype === 2)
        {
             return 44
        }
        else if(fansTargetProperties.themetype === 3)
        {
             return 81
        }
        else if(fansTargetProperties.themetype === 4)
        {
             return 136
        }
        else
        {
             return 46
        }
	}

	function processPosY()
	{
	    if(fansTargetProperties.themetype === 1)
        {
            return 128
        }
        else if(fansTargetProperties.themetype === 2)
        {
             return 117
        }
        else if(fansTargetProperties.themetype === 3)
        {
             return 162
        }
        else if(fansTargetProperties.themetype === 4)
        {
             return 108
        }
        else
        {
             return 107
        }
	}
	
	function processRadius()
	{
	    if(fansTargetProperties.themetype === 1)
        {
            return 0
        }
        else if(fansTargetProperties.themetype === 2)
        {
             return 12
        }
        else if(fansTargetProperties.themetype === 3)
        {
             return 18
        }
        else if(fansTargetProperties.themetype === 4)
        {
             return 16
        }
        else
        {
             return 20
        }
	}
	
    function caculateProcessValue(realfans,totalfans)
    {
        if(totalfans === 0)
        {
            return 0
        }
        else if(totalfans <= realfans)
        {
            return 100
        }
        else
        {
            var a = realfans.toFixed(2)
            var b = totalfans.toFixed(2)
            var c = parseInt((a/b)*100.00)
                if(c>=100)
                    c = 100
                return c
        }
    }
	
	function itemPosX()
	{
	    if(fansTargetProperties.themetype === 1)
        {
            return 32
        }
        else if(fansTargetProperties.themetype === 2)
        {
             return 45
        }
        else if(fansTargetProperties.themetype === 3)
        {
             return 73
        }
        else if(fansTargetProperties.themetype === 4)
        {
             return 102
        }
        else
        {
             return 34
        }
	}
	
	function itemPosY()
	{
	    if(fansTargetProperties.themetype === 1)
        {
            return 30
        }
        else if(fansTargetProperties.themetype === 2)
        {
             return 40
        }
        else if(fansTargetProperties.themetype === 3)
        {
             return 48
        }
        else if(fansTargetProperties.themetype === 4)
        {
             return 36
        }
        else
        {
             return 27
        }
	}
	
	function itemWidth()
	{
	    if(fansTargetProperties.themetype === 1)
        {
            return 546
        }
        else if(fansTargetProperties.themetype === 2)
        {
             return 448
        }
        else if(fansTargetProperties.themetype === 3)
        {
             return 142
        }
        else if(fansTargetProperties.themetype === 4)
        {
             return 435
        }
        else
        {
             return 478
        }
	}
	
	function itemHeight()
	{
	    if(fansTargetProperties.themetype === 1)
        {
            return 84
        }
        else if(fansTargetProperties.themetype === 2)
        {
             return 84
        }
        else if(fansTargetProperties.themetype === 3)
        {
             return 117
        }
        else if(fansTargetProperties.themetype === 4)
        {
             return 69
        }
        else
        {
             return 83
        }
	}
	
	function topTextSize()
	{
	    if(fansTargetProperties.themetype === 1)
        {
            return 32
        }
        else if(fansTargetProperties.themetype === 2)
        {
             return 32
        }
        else if(fansTargetProperties.themetype === 3)
        {
             return 36
        }
        else if(fansTargetProperties.themetype === 4)
        {
             return 36
        }
        else
        {
             return 36
        }
	}
	
	function bottomTextSize()
	{
	    if(fansTargetProperties.themetype === 1)
        {
            return 32
        }
        else if(fansTargetProperties.themetype === 2)
        {
             return 32
        }
        else if(fansTargetProperties.themetype === 3)
        {
             return 32
        }
        else if(fansTargetProperties.themetype === 4)
        {
             return 32
        }
        else
        {
             return 36
        }
	}

    Rectangle {
        id:rec1
        width: backGroundWidth(fansTargetProperties.themetype)
        height: backGroundHeight(fansTargetProperties.themetype)
        x:0
        y:0
        color:"#00000000";
        visible: true;
        Image {
            id: backGroundImage
            opacity: fansTargetProperties.transparence
            anchors.centerIn: parent
            source: backGroundPath(fansTargetProperties.themetype)
            fillMode: Image.Pad
        }
        Item{
            id:item1
            anchors.left: parent.left
            anchors.leftMargin: itemPosX()
            anchors.top: parent.top
            anchors.topMargin: itemPosY()
            height: itemHeight()
			width:itemWidth()
            Text {
                id: text1
                text: "目标粉丝"
                font.family: fansTargetProperties.themefont
                font.pixelSize:topTextSize()
                font.bold: fansTargetProperties.themebold
                font.italic: fansTargetProperties.themeitalic
                color: fansTargetProperties.themefontcolor
            }

            Text {
                id: text2
				anchors.right:parent.right
                anchors.bottom:text1.bottom
                text: progressText(fansTargetProperties.realfans,fansTargetProperties.totalfans)
                font.family: fansTargetProperties.datafont
                font.pixelSize:bottomTextSize()
                font.bold: fansTargetProperties.databold
                font.italic: fansTargetProperties.dataitalic
                color: fansTargetProperties.datafontcolor
				visible:fansTargetProperties.themetype!== 3
            }
			
			Text {
                id: text3
				anchors.left:parent.left
                anchors.bottom:parent.bottom
                anchors.bottomMargin:9
                text: progressText(fansTargetProperties.realfans,fansTargetProperties.totalfans)
                font.family: fansTargetProperties.datafont
                font.pixelSize:bottomTextSize()
                font.bold: fansTargetProperties.databold
                font.italic: fansTargetProperties.dataitalic
                color: fansTargetProperties.datafontcolor
				visible:fansTargetProperties.themetype=== 3
            }
        }
		
        ProgressBar {
            id: progressBar1
            width: processWidth(fansTargetProperties.themetype)
            height: processHeight(fansTargetProperties.themetype)
            x:processPosX()
            y:processPosY()
            value:caculateProcessValue(fansTargetProperties.realfans,fansTargetProperties.totalfans)
            maximumValue: 100
            minimumValue: 0
            opacity: fansTargetProperties.transparence
            style: ProgressBarStyle{
            background: Rectangle{
            color:"#00000000";
            }
            progress: Rectangle{
            color: progressColor(fansTargetProperties.themetype)
            radius:processRadius()
			border.width: fansTargetProperties.themetype === 5 ? 1:0
			border.color: newProperties.themetype === 5 ?"#7E94C8":"#00000000"
            }
          }
         }
		Image {
            id: processIcon
            opacity: fansTargetProperties.transparence
            x:processPosX()+caculateProcessValue(fansTargetProperties.realfans,fansTargetProperties.totalfans)*processWidth(fansTargetProperties.themetype)/100 -30
            y:processPosY()-11
            source: "qrc:/qmlfiles/image/fanstarget/processIcon.png"
            fillMode: Image.Pad
			visible:fansTargetProperties.themetype=== 5&&caculateProcessValue(fansTargetProperties.realfans,fansTargetProperties.totalfans)!=0
        }
    }
}

