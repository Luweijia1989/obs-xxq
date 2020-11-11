import QtQuick 2.7
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.2
import QtGraphicalEffects 1.12

Item {
    id: container
    width: engine.width
    height: engine.height
    focus: true

    property string giftInfoArray: gifttvProperties.giftArray

    onGiftInfoArrayChanged: {
        clearComponent()
        if (giftInfoArray === "")
            return

        let array = JSON.parse(giftInfoArray)

        let i = 0;
        for(i = 0; i < array.length; ++i)
        {
            var giftComponent = createComponent(array[i])
            giftComponent.x = array[i].xPosition
            giftComponent.y = array[i].yPosition
        }
    }

    function clearComponent() {
        let components = []
        let i = 0;
        for(i = 0; i < container.children.length; ++i)
        {
            components.push(container.children[i])
        }

        for(i = 0; i < container.children.length; ++i)
        {
            components[i].destroy();
        }
    }

    function createComponent(obj) {
        var giftComponent
        if (obj.giftType === 0)
            giftComponent = normalGift.createObject(container, {giftInfo : obj});
        else
            giftComponent = advancedGift.createObject(container, {giftInfo : obj});
        return giftComponent
    }

    Component {
        id: normalGift
        Item {
            property var giftInfo

            width:456
            height:100

            Image {
                id: infoRectangle
                height:100
                width:308
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                sourceSize:Qt.size(308, 100)
                source: "image/pic_gift_scroll_level_" + giftInfo.rangeLevel + ".png"

                CircularRectangle{
                    id: avatar
                    anchors.leftMargin: 8
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: 64
                    height: 64
                    radius: 32
                    imgSrc: giftInfo.avatarPath
                }

                ColumnLayout{
                    anchors.leftMargin: 10
                    anchors.left: avatar.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text {
                        id: name
                        Layout.preferredWidth: 120
                        text: qsTr(giftInfo.name)
                        elide: Text.ElideRight
                        font.pixelSize: 24
                        color: "#FFFFFF"
                        font.family: "Alibaba PuHuiTi M"
                        font.bold: true
                    }

                    RowLayout{
                        Text {
                            text: qsTr("赠送")
                            font.pixelSize: 20
                            color: "#FFFFFF"
//                            font.family: "Alibaba PuHuiTi R"
                        }

                        Text {
                            id: giftName
                            text: qsTr(giftInfo.gfitName)
                            font.pixelSize: 20
                            color: "#FFFFFF"
                            font.family: "Alibaba PuHuiTi M"
                            font.bold: true
                        }
                    }
                }

                Image {
                    id: giftIcon
                    anchors.leftMargin: 210
                    anchors.left: parent.left
                    source: giftInfo.giftPath
                    sourceSize: Qt.size(80, 80)
                    antialiasing: true
                }
            }

            Image {
                source: "image/pic_nospeed_" + giftInfo.rangeLevel.toString() + ".png"
                sourceSize: Qt.size(62, 100)
                anchors.right:infoRectangle.right
                anchors.bottom:infoRectangle.bottom
                visible: giftInfo.rangeLevel > 1 && giftInfo.rangeLevel < 7
            }

            Item {
                width: 62
                height: giftInfo.barPercent
                clip:true
                anchors.right:infoRectangle.right
                anchors.bottom:infoRectangle.bottom
                visible: giftInfo.rangeLevel > 1 && giftInfo.rangeLevel < 7
                Image {
                    source: "image/pic_speed_" + giftInfo.rangeLevel.toString() + ".png"
                    sourceSize: Qt.size(62, 100)
                    anchors.right:parent.right
                    anchors.bottom:parent.bottom
                }
            }

            BorderImage {
              width: groupNum.width + 20
              height: 12
              anchors.topMargin: 16
              anchors.leftMargin: 8
              anchors.left: infoRectangle.right
              anchors.top: infoRectangle.top
              border { left: 8; top: 0; right: 8; bottom: 12 }
              horizontalTileMode: BorderImage.Stretch
              verticalTileMode: BorderImage.Stretch
              source: "image/pic_red_batch.png"
              visible: parseInt(giftInfo.count) > 1
            }

            Text {
                id: groupNum
                anchors.topMargin: 6
                anchors.leftMargin: 20
                anchors.left: infoRectangle.right
                anchors.top: infoRectangle.top
                text: giftInfo.count
                visible: parseInt(giftInfo.count) > 1
                font.pixelSize: 24
                color: "#FFFFFF"
                font.family: "Alibaba PuHuiTi M"
                font.bold: true
            }

            Item {
                id: batterRecatangle
                width: 140
                height: 40
                anchors.topMargin: 42
                anchors.leftMargin: 8
                anchors.top: infoRectangle.top
                anchors.left: infoRectangle.right

                RowLayout{
                    spacing:0
                    Repeater {
                        model: giftInfo.hitCount.length + 1
                        Image {
                            source: index === 0 ? "image/pic_ride.png" : "image/" + giftInfo.hitCount.charAt(index - 1) + ".png"
                        }
                    }
                }
            }
        }
    }

    Component {
        id: advancedGift
        Item{
            property var giftInfo

            width: 932
            height: 120

            Image {
                width: 932
                height: 120
                sourceSize:Qt.size(932, 120)
                source: "image/pic_big_banner.png"
            }

            Item{
//                anchors.leftMargin: 34
                anchors.left: parent.left
                anchors.topMargin: 30
                anchors.top: parent.top
                anchors.bottomMargin: 30
                anchors.bottom: parent.bottom

                Text {
                    id: name
                    anchors.leftMargin: 34
                    anchors.left: parent.left
    //                anchors.topMargin: 34
                    anchors.top: parent.top
                    text: qsTr(giftInfo.name)
                    font.pixelSize: 28
                    color: "#FFFFFF"
                    font.family: "Alibaba PuHuiTi M"
                    font.bold: true
                }

                Text {
                    anchors.leftMargin: 34
                    anchors.left: parent.left
    //                anchors.bottomMargin: 36
                    anchors.bottom: parent.bottom
                    text: qsTr("赠送")
                    font.pixelSize: 20
                    color: "#FFF7F0"
    //                font.family: "Alibaba PuHuiTi R"
                }

                Text {
                    anchors.leftMargin: 79
                    anchors.left: parent.left
    //                anchors.bottomMargin: 36
                    anchors.bottom: parent.bottom
                    text: giftInfo.recName
                    font.pixelSize: 20
                    color: "#FFF7F0"
    //                font.family: "Alibaba PuHuiTi R"
                }
            }

            Image {
                id: giftIcon
                anchors.leftMargin: 584
                anchors.left: parent.left
                source: giftInfo.giftPath
                sourceSize: Qt.size(120, 120)
                antialiasing: true
            }

            Text {
                anchors.leftMargin: 718
                anchors.left: parent.left
                anchors.topMargin: 22
                anchors.top: parent.top
                text: qsTr(giftInfo.count)
                font.pixelSize: 24
                color: "#FFF7F0"
//                font.family: "Alibaba PuHuiTi R"
                visible: parseInt(giftInfo.count) > 1
            }

            Text {
                anchors.leftMargin: 718
                anchors.left: parent.left
                anchors.bottomMargin: 32
                anchors.bottom: parent.bottom
                text: qsTr("x"+giftInfo.hitCount)
                font.pixelSize: 36
                color: "#FFF7F0"
//                font.family: "Alibaba PuHuiTi R"
            }
        }
    }

    Connections {
        target: gifttvProperties

        onClearGiftChanged:{
            clearComponent()
        }

        onUpdateGiftChanged: {
            if (gifttvProperties.updateGift === "")
                return

            let obj = JSON.parse(gifttvProperties.updateGift)
            let i = 0
            let find = false
            for(i = 0; i < container.children.length; ++i)
            {
                if(obj.hitBatchId === container.children[i].giftInfo.hitBatchId)
                {
                    find = true
                    break
                }
            }

            if (find)
                container.children[i].giftInfo = obj;
        }

        onInvalidGiftChanged: {
            if (gifttvProperties.invalidGift === "")
                return

            let obj = JSON.parse(gifttvProperties.invalidGift)
            let i = 0
            let find = false
            for(i = 0; i < container.children.length; ++i)
            {
                if(obj.hitBatchId === container.children[i].giftInfo.hitBatchId)
                {
                    find = true
                    break
                }
            }

            if (find)
                container.children[i].destroy();
        }

        onGiftChanged: {
            if (gifttvProperties.gift === "")
                return

            let obj = JSON.parse(gifttvProperties.gift)
            var giftComponent = createComponent(obj)
            giftComponent.x = obj.xPosition
            giftComponent.y = obj.yPosition
        }
    }
}
