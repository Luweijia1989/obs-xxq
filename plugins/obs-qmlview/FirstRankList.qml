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
            return "qrc:/qmlfiles/image/listfirst/background_zhoubang1.png";
        }
        else
        {
            return "qrc:/qmlfiles/image/listfirst/background_zhoubang2.png";
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
            anchors.leftMargin: firstRankListProperties.themetype ===1 ? 12 : 17
            anchors.top: parent.top
            anchors.topMargin:  firstRankListProperties.themetype ===1 ? 12 : 17
            width:firstRankListProperties.themetype ===1 ? parent.width - 24 : parent.width - 34
            height:firstRankListProperties.themetype ===1 ? parent.height - 24 : parent.height - 34
            CircularRectangle{
                id: avartar
                anchors.leftMargin: firstRankListProperties.themetype ===1 ? 24 : 12
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.topMargin: 11
                width: 118
                height: 118
                radius: 59
                imgSrc: avartarPath(firstRankListProperties.avatarpath)
            }

            Text {
                id: text1
                anchors.left: avartar.right
                anchors.leftMargin: firstRankListProperties.themetype ===1 ? 82 : 24
                anchors.top: avartar.top
                anchors.topMargin: firstRankListProperties.themetype ===1 ?17:15
                text: theNameText(firstRankListProperties.listtype)
                font.family: firstRankListProperties.themefont
                font.pixelSize:32
                font.bold: firstRankListProperties.themebold
                font.italic: firstRankListProperties.themeitalic
                color: firstRankListProperties.themefontcolor
            }
            Text {
                id: text2
                anchors.left: avartar.right
                anchors.leftMargin: firstRankListProperties.themetype ===1 ? 82 : 24
                anchors.bottom:parent.bottom
                anchors.bottomMargin: firstRankListProperties.themetype ===1 ? 22 : 25
                text: firNameText(firstRankListProperties.firstname)
                font.family: firstRankListProperties.datafont
                font.pixelSize: 26
                font.bold: firstRankListProperties.databold
                font.italic: firstRankListProperties.dataitalic
                color: firstRankListProperties.datafontcolor
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

