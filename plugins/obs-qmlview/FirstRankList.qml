import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
Item {
    id: container
    width: backGroundWidth(firstRankListProperties.themetype)
    height: backGroundHeight(firstRankListProperties.themetype)
    focus: true
    function backGroundPath(themeType)
    {
        if(themeType === 1)
        {
            return "qrc:/qmlfiles/image/listfirst/zhuti_bangyi.png";
        }
        else
        {
            return "qrc:/qmlfiles/image/background.png";
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
            return 245;
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

    function theNameText(listType)
    {
        if(listType === 1)
        {
            return "日榜冠名";
        }
        else
        {
            return "周榜冠名";
        }
    }

    function firNameText(firstName)
    {
        if(firstName === "")
        {
            return "还没有人上榜";
        }
        else
        {
            return firstName;
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

    Rectangle {
        id:rec1
        width: backGroundWidth(firstRankListProperties.themetype)
        height: backGroundHeight(firstRankListProperties.themetype)
        x:0
        y:0
        color:"#00000000";
        visible: true;
        Image {
            id: backGroundImage
            opacity: firstRankListProperties.transparence
            anchors.centerIn: parent
            source: backGroundPath(firstRankListProperties.themetype)
            fillMode: Image.Pad
        }
        Item{
            id:item1
            anchors.left: parent.left
            anchors.leftMargin: 15
            anchors.top: parent.top
            anchors.topMargin: 15
            width: parent.width - 15
            height:parent.height - 15

            CircularRectangle{
                id: avartar
                anchors.leftMargin: firstRankListProperties.themetype ===1 ? 9 : 4
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.topMargin: 4
                width: 44
                height: 44
                radius: 22
                imgSrc: avartarPath(firstRankListProperties.avatarpath)
            }

            Text {
                id: text1
                anchors.left: avartar.right
                anchors.leftMargin: firstRankListProperties.themetype ===1 ? 30 : 6
                anchors.top: avartar.top
                anchors.topMargin: firstRankListProperties.themetype ===1 ?3:1
                text: theNameText(firstRankListProperties.listtype)
                font.family: firstRankListProperties.themefont
                font.pixelSize:firstRankListProperties.themetype ===1 ? 14 : 16
                font.bold: firstRankListProperties.themebold
                font.italic: firstRankListProperties.themeitalic
                color: firstRankListProperties.themefontcolor
            }
            Text {
                id: text2
                anchors.left: avartar.right
                anchors.leftMargin: firstRankListProperties.themetype ===1 ? 30 : 6
                anchors.bottom:parent.bottom
                anchors.bottomMargin: 21
                text: firNameText(firstRankListProperties.firstname)
                font.family: firstRankListProperties.datafont
                font.pixelSize: 12
                font.bold: firstRankListProperties.databold
                font.italic: firstRankListProperties.dataitalic
                color: firstRankListProperties.datafontcolor
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
                index = 1;
       }
    }
    Connections {
            target: firstRankListProperties
            onReplay:{
                if(index <4)
                {
                    avartar.imgSrc = sprintf("qrc:/qmlfiles/image/boke%d.png",index)
                    text2.text = sprintf("啵克%d号",index)
                    index = index+1;
               }
               else if(index === 4)
               {
                    avartar.imgSrc = avartarPath(firstRankListProperties.avatarpath)
                    text2.text = firNameText(firstRankListProperties.firstname)
                    index = 1
               }
               previewtimer.start();
        }
            onUpdate:{
                avartar.imgSrc = avartarPath(firstRankListProperties.avatarpath)
                text2.text = firNameText(firstRankListProperties.firstname)
                index = 1
            }
    }
}

