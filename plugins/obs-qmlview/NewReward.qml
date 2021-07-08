import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
Item {
    id: container
    width: backGroundWidth(newRewardProperties.themetype)
    height: backGroundHeight(newRewardProperties.themetype)
    focus: true
    function backGroundPath(themeType)
    {
        if(themeType === 1)
        {
            return "qrc:/qmlfiles/image/newreward/background_gift1.png";
        }
        else
        {
            return "qrc:/qmlfiles/image/newreward/background_gift2.png";
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

    function descText(firstName)
    {
        if(firstName === "")
        {
            return "打赏主播立刻上电视哦～";
        }
        else
        {
            return "刚刚打赏了主播";
        }
    }

    function avartarPath(namePath)
    {
        if(namePath === "")
        {
            return "qrc:/qmlfiles/image/empty.png";
        }
        else
        {
            return namePath;
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
	
    function caculateTextBottomMargin(firstName,themeType)
    {
        if(firstName === "")
        {
			return 52;
		}
		else
		{
			return themetype === 1 ? 22 : 25;
		}
	}

    Rectangle {
        id:rec1
        width: backGroundWidth(newRewardProperties.themetype)
        height: backGroundHeight(newRewardProperties.themetype)
        x:0
        y:0
        color:"#00000000";
        visible: true;
        Image {
            id: backGroundImage
            opacity: newRewardProperties.transparence
            anchors.centerIn: parent
            source: backGroundPath(newRewardProperties.themetype)
            fillMode: Image.Pad
        }
        Item{
            id:item1
            anchors.left: parent.left
            anchors.leftMargin: newRewardProperties.themetype ===1 ? 12 : 17
            anchors.top: parent.top
            anchors.topMargin:  newRewardProperties.themetype ===1 ? 12 : 17
            width:newRewardProperties.themetype ===1 ? parent.width - 24 : parent.width - 34
            height:newRewardProperties.themetype ===1 ? parent.height - 24 : parent.height - 34
            CircularRectangle{
                id: avartar
                anchors.leftMargin: newRewardProperties.themetype ===1 ? 24 : 12
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.topMargin: 11
                width: 118
                height: 118
                radius: 59
                imgSrc: avartarPath(newRewardProperties.avatarpath)
            }

            Text {
                id: text1
                anchors.left: avartar.right
                anchors.leftMargin: newRewardProperties.themetype ===1 ? 82 : 24
                anchors.top: avartar.top
                anchors.topMargin: 13
                text: newRewardProperties.firstname
                font.family: newRewardProperties.themefont
                font.pixelSize:32
                font.bold: newRewardProperties.themebold
                font.italic: newRewardProperties.themeitalic
                color: newRewardProperties.themefontcolor
                visible: newRewardProperties.firstname === ""? false: true;
            }
            Text {
                id: text2
                text: descText(newRewardProperties.firstname)
                anchors.left: avartar.right
                anchors.leftMargin: newRewardProperties.themetype ===1 ? 82 : 24
                anchors.bottom:parent.bottom
                anchors.bottomMargin:newRewardProperties.firstname === ""?48:24
                font.family: newRewardProperties.datafont
                font.pixelSize: 32
                font.bold: newRewardProperties.databold
                font.italic: newRewardProperties.dataitalic
                color: newRewardProperties.datafontcolor
                verticalAlignment: Text.AlignVBottom
            }
        }
    }
    property var index: 1
    Timer {
        id:previewtimer
        interval: 18000
        triggeredOnStart: true;
        repeat: false
        onTriggered: {
                    index = 1
       }
    }
    Connections {
            target: newRewardProperties
            onReplay:{
                if(index <4)
                {
                    avartar.imgSrc = sprintf("qrc:/qmlfiles/image/boke%d.png",index)
                    text1.text = sprintf("啵克%d号",index)
					text1.visible = true
                    text2.text = descText(text1.text)
					text2.anchors.bottomMargin = text1.text === ""?48:24
                    index = index+1;
               }
               else if(index === 4)
               {
                    avartar.imgSrc = avartarPath(newRewardProperties.avatarpath)
                    text1.text = newRewardProperties.firstname
					text1.visible = newRewardProperties.firstname === ""? false: true
                    text2.text = descText(newRewardProperties.firstname)
					text2.anchors.bottomMargin = newRewardProperties.firstname === ""?48:24
                    index = 1
               }
               previewtimer.start();
        }
        onUpdate:{
                    avartar.imgSrc = avartarPath(newRewardProperties.avatarpath)
                    text1.text = newRewardProperties.firstname
					text1.visible = newRewardProperties.firstname === ""? false: true
                    text2.text = descText(newRewardProperties.firstname)
					text2.anchors.bottomMargin = newRewardProperties.firstname === ""?48:24
                    index = 1
        }
    }
}

