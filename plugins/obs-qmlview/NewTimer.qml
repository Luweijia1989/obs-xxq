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
            return "qrc:/qmlfiles/image/timer/theme4.png";
        }
        else if(themeType === 3)
        {
            return "qrc:/qmlfiles/image/timer/theme3.png";
        }
        else if(themeType === 4)
        {
            return "qrc:/qmlfiles/image/timer/theme2.png";
        }
        else if(themeType === 5)
        {
            return "qrc:/qmlfiles/image/timer/switch.png";
        }
    }

    function backGroundWidth(themeType)
    {
        if(themeType === 1)
        {
            return 512;
        }
        else if(themeType === 2)
        {
            return 521;
        }
        else if(themeType === 3)
        {
            return 494;
        }
        else if(themeType === 4)
        {
            return 488;
        }
        else if(themeType === 5)
        {
            return 518;
        }
    }

    function backGroundHeight(themeType)
    {
        if(themeType === 1)
        {
            return 158;
        }
        else if(themeType === 2)
        {
            return 157;
        }
        else if(themeType === 3)
        {
            return 183;
        }
        else if(themeType === 4)
        {
            return 303;
        }
        else if(themeType === 5)
        {
            return 144;
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
	
	function itemPosX(themeType)
    {
        if(themeType === 1)
        {
            return 93;
        }
        else if(themeType === 2)
        {
            return 75;
        }
        else if(themeType === 3)
        {
            return 85;
        }
        else if(themeType === 4)
        {
            return 77;
        }
        else if(themeType === 5)
        {
            return 95;
        }
    }
	
	function itemPosY(themeType)
    {
        if(themeType === 1)
        {
            return 38;
        }
        else if(themeType === 2)
        {
            return 37;
        }
        else if(themeType === 3)
        {
            return 60;
        }
        else if(themeType === 4)
        {
            return 159;
        }
        else if(themeType === 5)
        {
            return 31;
        }
    }
	
	function itemPosWidth(themeType)
    {
        if(themeType === 1)
        {
            return 355;
        }
        else if(themeType === 2)
        {
            return 340;
        }
        else if(themeType === 3)
        {
            return 342;
        }
        else if(themeType === 4)
        {
            return 343;
        }
        else if(themeType === 5)
        {
            return 327;
        }
    }
	
	function itemPosHeight(themeType)
    {
        if(themeType === 1)
        {
            return 80;
        }
        else if(themeType === 2)
        {
            return 80;
        }
        else if(themeType === 3)
        {
            return 80;
        }
        else if(themeType === 4)
        {
            return 80;
        }
        else if(themeType === 5)
        {
            return 80;
        }
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
            width:itemPosWidth(newTimerProperties.themetype)
            height:itemPosHeight(newTimerProperties.themetype)
            Text {
                id: text1
				anchors.centerIn: parent
                text: newTimerProperties.text
                font.family: newTimerProperties.themefont
                font.pixelSize:58
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

