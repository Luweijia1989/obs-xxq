import QtQuick 2.14
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.14

Item {
    id: root
    width: engine.width
    height: engine.height
    x: 0
    y: 0
    focus: true

    property real sharkScale: 2.1557

    Item {
        id: sharkMaskItem
        x: maskProperties.sharkMaskXPos
        width: 720
        height: 1080

        Loader {
            x: parent.width * 0.23653
            y: parent.height * 0.31337
            active: maskProperties.status === 0

            sourceComponent: Component {
                Item {
                    width: 180
                    height: 234
                    scale: sharkScale
                    transformOrigin: Item.TopLeft
                    Image {
                        x: 2
                        y: 0
                        source: "qrc:/qmlfiles/image/img_tips_selecting.png"

                        Text {
                            x: 16
                            y: 7
                            text: "正在选择中…"
                            font.pixelSize: 24
                            font.family: "Alibaba PuHuiTi M"
                            color: "#FFFFFF"
                        }
                    }

                    AnimatedImage {
                        x: 0
                        y: 54
                        width: 180
                        height: 180
                        source: "qrc:/qmlfiles/image/choosing.apng"
                    }
                }
            }
        }

        Loader {
            x: parent.width * 0.21856
            y: parent.height * 0.31936
            active: maskProperties.status === 2

            sourceComponent: Component {
                AnimatedImage {
                    transformOrigin: Item.TopLeft
                    scale: sharkScale
                    width: 188
                    height: 182
                    source: "qrc:/qmlfiles/image/fail.apng"
                }
            }
        }

        Loader {
            x: parent.width * 0.20958
            y: parent.height * 0.33732
            active: maskProperties.status === 1

            sourceComponent: Component {
                Image {
                    transformOrigin: Item.TopLeft
                    scale: sharkScale
                    width: 200
                    height: 168
                    source: "qrc:/qmlfiles/image/popup_5.png"

                    Text {
                        x: 34
                        y: 51
                        text: "选择" + maskProperties.toothIndex + "号牙齿\n挑战成功"
                        font.pixelSize: 24
                        color: "#FFFFFF"
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }
}
