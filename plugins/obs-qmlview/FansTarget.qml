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
        else
        {
            return "qrc:/qmlfiles/image/fanstarget/background_fans2.png";
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
        else
        {
            return 593;
        }
    }

    function backGroundHeight(themeType)
    {
        if(themeType === 1)
        {
            return 164;
        }
        else
        {
            return 174;
        }
    }

    function titleTextColor(themeType)
    {
        if(themeType === 1)
        {
            return "#333333";
        }
        else
        {
            return "#FFFFFF";
        }
    }

    function progressTextColor(themeType)
    {
        if(themeType === 1)
        {
            return "#FFC70B";
        }
        else
        {
            return "#FFFFFF";
        }
    }

    function progressColor(themeType)
    {
        if(themeType === 1)
        {
            return "#FFFFFF";
        }
        else
        {
            return "#C3E3EE";
        }
    }

    function processWidth(themeType)
    {
        if(themeType === 1)
        {
            return 578
        }
        else
        {
             return 448
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
            anchors.leftMargin: fansTargetProperties.themetype === 1 ? 32:45
            anchors.top: parent.top
            anchors.topMargin: fansTargetProperties.themetype === 1 ? 30:40
            anchors.bottom: parent.bottom
            anchors.bottomMargin:73
            height: 84
			width:fansTargetProperties.themetype === 1 ? 546:448
            Text {
                id: text1
                text: "目标粉丝"
                font.family: fansTargetProperties.themefont
                font.pixelSize:32
                font.bold: fansTargetProperties.themebold
                font.italic: fansTargetProperties.themeitalic
                color: fansTargetProperties.themefontcolor//titleTextColor(fansTargetProperties.themetype)
            }

            Text {
                id: text2
                anchors.right:parent.right
                anchors.bottom:text1.bottom
                text: progressText(fansTargetProperties.realfans,fansTargetProperties.totalfans)
                font.family: fansTargetProperties.datafont
                font.pixelSize: 32
                font.bold: fansTargetProperties.databold
                font.italic: fansTargetProperties.dataitalic
                color: fansTargetProperties.datafontcolor
            }
        }
        ProgressBar {
            id: progressBar1
            width: processWidth(fansTargetProperties.themetype)
            height: fansTargetProperties.themetype === 1 ? 22:24
            x:fansTargetProperties.themetype === 1 ? 12 : 44
            y:fansTargetProperties.themetype === 1 ? 128 :117
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
            radius:fansTargetProperties.themetype === 1 ? 0:12
            }
          }
         }
    }
}

