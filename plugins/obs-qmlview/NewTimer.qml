import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
Item {
    id: container
    width: backGroundWidth(newTimerProperties.themetype)
    height: backGroundHeight(newTimerProperties.themetype)
    focus: true
    function backGroundPath(themeType)
    {
        if(themeType === 1)
        {
            return "qrc:/qmlfiles/image/timer/theme1.png";
        }
        else if(themeType === 2)
        {
            return "qrc:/qmlfiles/image/timer/theme2.png";
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
            return "qrc:/qmlfiles/image/newcom/grayblue.png";
        }
		else if(themeType === 6)
        {
            return "qrc:/qmlfiles/image/timer/theme6.png";
        }
        else if(themeType === 7)
        {
            return "qrc:/qmlfiles/image/timer/theme7.png";
        }
    }

    function backGroundWidth(themeType)
    {
        if(themeType === 1)
        {
            return 600;
        }
        else if(themeType === 2)
        {
            return 600;
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
            return 600;
        }
        else if(themeType === 6)
        {
            return 600;
        }
        else if(themeType === 7)
        {
            return 600;
        }
    }

    function backGroundHeight(themeType)
    {
		return 176;
    }

    function sprintf() {
        let args = arguments, string = args[0];
        for (let i = 1; i < args.length; i++) {
            let item = arguments[i];
            string   = string.replace('%d', item);
        }
        return string;
    }
	
	function itemPosX(themeType)
    {
        if(themeType === 3)
        {
            return 30;
        }
        else if(themeType === 4)
        {
            return 100;
        }
        else
        {
            return 50;
        }
    }
	
	function itemPosY(themeType)
    {
        return 43;
    }

    Rectangle {
        id:rec1
        width: backGroundWidth(newTimerProperties.themetype)
        height: backGroundHeight(newTimerProperties.themetype)
        x:0
        y:0
        color:"#00000000";
        visible: true;
        Image {
            id: backGroundImage
            opacity: newTimerProperties.transparence
            anchors.centerIn: parent
            source: backGroundPath(newTimerProperties.themetype)
            fillMode: Image.Pad
        }
        Item{
            id:item1
            anchors.left: parent.left
            anchors.leftMargin: itemPosX(newTimerProperties.themetype)
            anchors.top: parent.top
            anchors.topMargin:  itemPosY(newTimerProperties.themetype)
            width:500
            height:90
            Text {
                id: text1
				anchors.centerIn: parent
                text: newTimerProperties.text
                font.family: newTimerProperties.themefont
                font.pixelSize:64
                font.bold: newTimerProperties.themebold
                font.italic: newTimerProperties.themeitalic
                color: newTimerProperties.themefontcolor
                visible: true
				horizontalAlignment: Text.AlignHCenter
				verticalAlignment: Text.AlignVCenter
            }
        }
    }
	
    Timer {
        id:timer1
        interval: 1000
        triggeredOnStart: true;
        repeat: true
        onTriggered: {
       }
    }
    Connections {
        target: newTimerProperties
        onUpdate:{
        }
    }
}

