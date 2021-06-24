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
            return "qrc:/qmlfiles/image/fanstarget/background_fans.png";
        }
        else
        {
            return "qrc:/qmlfiles/image/fans_background.png";
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
            return 250;
        }
        else
        {
            return 225;
        }
    }

    function backGroundHeight(themeType)
    {
        if(themeType === 1)
        {
            return 80;
        }
        else
        {
            return 82;
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
            return 220
        }
        else
        {
             return 158
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
    //Component {
        //id: itemCompont
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
            anchors.leftMargin: fansTargetProperties.themetype === 1 ? 19:21
            anchors.top: parent.top
            anchors.topMargin: fansTargetProperties.themetype === 1 ? 25:27
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 27
            width: fansTargetProperties.themetype === 1 ? 212 :158
            height: 34
            Text {
                id: text1
                text: "目标粉丝"
                font.family: fansTargetProperties.themefont
                font.pixelSize:fansTargetProperties.themetype ===1 ? 16 : 14
                font.bold: fansTargetProperties.themebold
                font.italic: fansTargetProperties.themeitalic
                color: fansTargetProperties.themefontcolor//titleTextColor(fansTargetProperties.themetype)
            }

            Text {
                id: text2
                anchors.right:parent.right
                anchors.bottom:fansTargetProperties.themetype === 1 ? parent.bottom : text1.bottom
                text: progressText(fansTargetProperties.realfans,fansTargetProperties.totalfans)
                font.family: fansTargetProperties.datafont
                font.pixelSize: 12
                font.bold: fansTargetProperties.databold
                font.italic: fansTargetProperties.dataitalic
                color: fansTargetProperties.datafontcolor//progressTextColor(fansTargetProperties.themetype)
            }
        }
        ProgressBar {
            id: progressBar1
            width: processWidth(fansTargetProperties.themetype)
            height: fansTargetProperties.themetype === 1 ? 8:10
            x:fansTargetProperties.themetype === 1 ? 15 : 21
            y:fansTargetProperties.themetype === 1 ? 57 :50
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
            radius:fansTargetProperties.themetype === 1 ? 0:5
            }
          }
         }
    }
    //}
}

