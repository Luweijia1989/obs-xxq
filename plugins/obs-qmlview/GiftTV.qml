import QtQuick 2.7
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.2
import QtGraphicalEffects 1.12

Item {
    id: container
    width: 800/*engine.width*/
    height: 600/*engine.height*/
    focus: true
    property var giftArray:[]

    function clear() {
        let i
        for(i = 0; i < giftArray.length; ++i)
        {
            var gift = giftArray[i]
            gift.destory()
        }
        giftArray.length = 0
    }

    Component {
        id: normalGift
        Rectangle {
            width:456
            height:100

            Rectangle {
                id: infoRectangle
                height:100
                width:308
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter

                CircularRectangle{
                    id: avatar
                    anchors.leftMargin: 8
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: 64
                    height: 64
                    radius: 32
                    imgSrc: "avatar.png"
                }

                ColumnLayout{
                    anchors.leftMargin: 10
                    anchors.left: avatar.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text {
                        id: name
                        text: qsTr("可乐鸡翅啊")
                        font.pixelSize: 24
                        //                        color: "#FFFFFF"
//                        font.family: "Alibaba PuHuiTi M"
                        font.bold: true
                    }

                    RowLayout{
                        Text {
                            text: qsTr("赠送")
                            font.pixelSize: 20
//                            color: "#FFFFFF"
//                            font.family: "Alibaba PuHuiTi R"
                        }

                        Text {
                            id: giftName
                            text: qsTr("棒棒糖")
                            font.pixelSize: 20
                            //                            color: "#FFFFFF"
//                            font.family: "Alibaba PuHuiTi M"
                            font.bold: true
                        }
                    }
                }

                Image {
                    id: giftIcon
                    anchors.leftMargin: 210
                    anchors.left: parent.left
                    source: "gift.png"
                    sourceSize: Qt.size(80, 80)
                    antialiasing: true
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
            }

            Text {
                id: groupNum
                anchors.topMargin: 6
                anchors.leftMargin: 20
                anchors.left: infoRectangle.right
                anchors.top: infoRectangle.top
                text: qsTr("9999")
                font.pixelSize: 24
                //                color: "#FFFFFF"
                //                            font.family: "Alibaba PuHuiTi M"
                font.bold: true
            }

            Rectangle {
                id: batterRecatangle
                property string batterStr: "1234"
                width: 140
                height: 40
                anchors.topMargin: 42
                anchors.leftMargin: 8
                anchors.top: infoRectangle.top
                anchors.left: infoRectangle.right

                RowLayout{
                    spacing:0
                    Repeater {
                        model: batterRecatangle.batterStr.length + 1
                        Image {
                            source: index === 0 ? "image/pic_ride.png" : "image/" + batterRecatangle.batterStr.charAt(index - 1) + ".png"
                        }
                    }
                }
            }
        }
    }

    Component {
        id: advancedGift

        BorderImage {
            width: 932
            height: 120
            border { left: 314; top: 0; right: 556; bottom: 120 }
            horizontalTileMode: BorderImage.Stretch
            verticalTileMode: BorderImage.Stretch
            source: "image/pic_red_batch.png"

            Text {
                id: name
                anchors.leftMargin: 34
                anchors.left: parent.left
                anchors.topMargin: 26
                anchors.top: parent.top
                text: qsTr("巴拉巴拉小魔女变身呀")
                font.pixelSize: 28
                //                        color: "#FFFFFF"
//                        font.family: "Alibaba PuHuiTi M"
                font.bold: true
            }

            Text {
                anchors.leftMargin: 34
                anchors.left: parent.left
                anchors.bottomMargin: 36
                anchors.bottom: parent.bottom
                text: qsTr("赠送")
                font.pixelSize: 20
                //                            color: "#FFF7F0"
//                            font.family: "Alibaba PuHuiTi R"
            }

            Text {
                anchors.leftMargin: 79
                anchors.left: parent.left
                anchors.bottomMargin: 36
                anchors.bottom: parent.bottom
                text: qsTr("黑暗小魔仙")
                font.pixelSize: 20
                //                            color: "#FFF7F0"
//                            font.family: "Alibaba PuHuiTi R"
            }

            Image {
                id: giftIcon
                anchors.leftMargin: 584
                anchors.left: parent.left
                source: "gift.png"
                sourceSize: Qt.size(120, 120)
                antialiasing: true
            }

            Text {
                anchors.leftMargin: 718
                anchors.left: parent.left
                anchors.topMargin: 22
                anchors.top: parent.top
                text: qsTr("1314")
                font.pixelSize: 24
                //                            color: "#FFF7F0"
//                            font.family: "Alibaba PuHuiTi R"
            }

            Text {
                anchors.leftMargin: 718
                anchors.left: parent.left
                anchors.bottomMargin: 32
                anchors.bottom: parent.bottom
                text: qsTr("x8888")
                font.pixelSize: 36
                //                            color: "#FFF7F0"
//                            font.family: "Alibaba PuHuiTi R"
            }
        }
    }

    Connections {
        target: gifttvProperties
        onPreferChanged: {
            console.log("AAAAAAAAAAAAAAAAAAAAAA")
        }

    }

    Timer {
        interval: 2000
        running: true
        repeat: false
        onTriggered: {
//            var component = normalGift.createObject(container);
            var component = advancedGift.createObject(container);
            giftArray.push(component);
        }
    }
}
