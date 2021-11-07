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
            return "qrc:/qmlfiles/image/newcom/tip.png";
        }
        else if(themeType === 4)
        {
            return "qrc:/qmlfiles/image/newcom/switch.png";
        }
        else if(themeType === 5)
        {
            return "qrc:/qmlfiles/image/fanstarget/grayBlue.png";
        }
        else if(themeType === 6)
        {
            return "qrc:/qmlfiles/image/fanstarget/background_fans6.png";
        }
        else if(themeType === 7)
        {
            return "qrc:/qmlfiles/image/fanstarget/background_fans7.png";
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
            return 400;
        }
        else if(themeType === 2)
        {
            return 400;
        }
        else if(themeType === 3)
        {
            return 560;
        }
        else if(themeType === 4)
        {
            return 700;
        }
        else if(themeType === 5)
        {
            return 400;
        }
        else if(themeType === 6)
        {
            return 400;
        }
        else if(themeType === 7)
        {
            return 400;
        }
    }

    function backGroundHeight(themeType)
    {
        if(themeType === 3 || themeType === 4)
        {
            return 176;
        }
        else
        {
            return 240;
        }
    }

    function progressColor(themeType,step)
    {
        if(themeType === 1)
        {
            return "#FFC80B";
        }
        else if(themeType === 2)
        {
            return "#32A6FF";
        }
        else if(themeType === 3)
        {
            return "#31A76F";
        }
        else if(themeType === 4)
        {
			if(step === 1)
				return "#0692EF";
			else
				return "#7654FF";
        }
        else if(themeType === 5)
        {
            return "#184C89";
        }
        else if(themeType === 6)
        {
            return "#32A6FF";
        }
        else
        {
            return "#FF32DB";
        }
    }

    function processWidth(themeType)
    {
		if(themeType === 3)
        {
             return 342
        }
        else if(themeType === 4)
        {
             return 306
        }
        else
        {
             return 352
        }
    }
	
    function processHeight(themeType)
    {
        return 24

    }
	
	function processPosX()
	{
		if(fansTargetProperties.themetype === 3)
        {
             return 35
        }
        else if(fansTargetProperties.themetype === 4)
        {
             return 123
        }
        else
        {
             return 24
        }
	}

	function processPosY()
	{
		if(fansTargetProperties.themetype === 3)
        {
             return 117
        }
        else if(fansTargetProperties.themetype === 4)
        {
             return 117
        }
        else
        {
             return 181
        }
	}
	
	function progressBkColor(themeType)
	{
        if(themeType === 1)
        {
            return "#FFFFFF";
        }
        else if(themeType === 2)
        {
            return "#FFFFFF";
        }
        else if(themeType === 3)
        {
            return "#BAD3CC";
        }
        else if(themeType === 4)
        {
            return "#BABED3";
        }
        else if(themeType === 5)
        {
            return "#FFFFFF";
        }
        else if(themeType === 6)
        {
            return "#FFFFFF";
        }
        else
        {
            return "#FFFFFF";
        }
	}
	
	function processRadius()
	{
		return 12;
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
	
	function textFirPosX()
	{
        if(fansTargetProperties.themetype === 3)
        {
             return 32
        }
        else if(fansTargetProperties.themetype === 4)
        {
             return 120
        }
        else
        {
             return 24
        }
	}
	
	function textFirPosY()
	{
		if(fansTargetProperties.themetype === 3)
        {
             return 31
        }
        else if(fansTargetProperties.themetype === 4)
        {
             return 31
        }
        else
        {
             return 44
        }
	}
	
	function textSecPosX()
	{
        if(fansTargetProperties.themetype === 3)
        {
             return 394
        }
        else if(fansTargetProperties.themetype === 4)
        {
             return 446
        }
        else
        {
             return 24
        }
	}
	
	function textSecPosY()
	{
		if(fansTargetProperties.themetype === 3)
        {
             return -7
        }
        else if(fansTargetProperties.themetype === 4)
        {
             return -7
        }
        else
        {
             return 131
        }
	}

	function topTextSize()
	{
		if(fansTargetProperties.themetype === 3)
        {
             return 44
        }
        else if(fansTargetProperties.themetype === 4)
        {
             return 44
        }
        else
        {
             return 40
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
		
		Rectangle{
		    id: progressBarbackground
			anchors.left: parent.left
			anchors.leftMargin: processPosX()
			anchors.top: parent.top
			anchors.topMargin: processPosY()
            width: processWidth(fansTargetProperties.themetype)
            height: processHeight(fansTargetProperties.themetype)
            color:progressBkColor(fansTargetProperties.themetype);
			radius:processRadius()
        }
        ProgressBar {
            id: progressBar1
            width: processWidth(fansTargetProperties.themetype)-6
            height: processHeight(fansTargetProperties.themetype)-6
            x:processPosX()+3
            y:processPosY()+3
            value:caculateProcessValue(fansTargetProperties.realfans,fansTargetProperties.totalfans)
            maximumValue: 100
            minimumValue: 0
            opacity: fansTargetProperties.transparence
            style: ProgressBarStyle{
            background: Rectangle{
            color:"#00000000";
            }
            progress: Rectangle{
			radius:processRadius()-3
			gradient:Gradient {
            GradientStop {position: 0.0; color: progressColor(fansTargetProperties.themetype,1)}
            GradientStop {position: 1.0; color: progressColor(fansTargetProperties.themetype,2)}
			}
            }
          }
         }
		Text {
			id: text1
			anchors.left: parent.left
			anchors.leftMargin: textFirPosX()
			anchors.top: parent.top
			anchors.topMargin: textFirPosY()
			text: "目标粉丝"
			font.family: fansTargetProperties.themefont
			font.pixelSize:topTextSize()
			font.bold: fansTargetProperties.themebold
			font.italic: fansTargetProperties.themeitalic
			color: fansTargetProperties.themefontcolor
		}

		Text {
			id: text2
			anchors.left: parent.left
			anchors.leftMargin: textSecPosX()
			anchors.top: (fansTargetProperties.themetype === 3 || fansTargetProperties.themetype === 4)? progressBar1.top:parent.top
			anchors.topMargin: textSecPosY()
			text: progressText(fansTargetProperties.realfans,fansTargetProperties.totalfans)
			font.family: fansTargetProperties.datafont
			font.pixelSize:28
			font.bold: fansTargetProperties.databold
			font.italic: fansTargetProperties.dataitalic
			color: fansTargetProperties.datafontcolor
			horizontalAlignment: Text.AlignLeft
			verticalAlignment: Text.AlignTop
		}

		Image {
            id: processIcon
            opacity: fansTargetProperties.transparence
            x:processPosX()+caculateProcessValue(fansTargetProperties.realfans,fansTargetProperties.totalfans)*processWidth(fansTargetProperties.themetype)/100 -30
            y:processPosY()-11
            source: "qrc:/qmlfiles/image/fanstarget/processIcon.png"
            fillMode: Image.Pad
			visible:fansTargetProperties.themetype=== 5&&caculateProcessValue(fansTargetProperties.realfans,fansTargetProperties.totalfans)!==0
        }
    }
}

