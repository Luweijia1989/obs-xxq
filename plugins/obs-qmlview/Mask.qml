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
            x: 107 * sharkScale
            y: 248 * sharkScale
            active: maskProperties.status === 0

            sourceComponent: Component {
                Item {
                    width: 120
                    height: 167
                    scale: sharkScale
                    transformOrigin: Item.TopLeft
                    Image {
                        x: 16
                        y: 0
                        source: "qrc:/qmlfiles/image/img_tips_selecting.png"

                        Text {
                            x: 8
                            y: 3
                            text: "正在选择中…"
                            font.pixelSize: 12
                            font.family: "Alibaba PuHuiTi M"
                            color: "#FFFFFF"
                        }
                    }

                    AnimatedImage {
                        x: 0
                        y: 27
                        width: 120
                        height: 120
                        source: "qrc:/qmlfiles/image/choosing.apng"
                    }
                }
            }
        }

        Loader {
            x: 73 * sharkScale
            y: 160 * sharkScale
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
            x: 107 * sharkScale
            y: 201 * sharkScale
            active: maskProperties.status === 1

            sourceComponent: Component {
                Image {
                    transformOrigin: Item.TopLeft
                    scale: sharkScale
                    width: 120
                    height: 100
                    source: "qrc:/qmlfiles/image/popup_5.png"

                    Text {
                        x: 23
                        y: 35
                        text: "选择" + maskProperties.toothIndex + "号牙齿\n挑战成功"
                        font.pixelSize: 14
                        color: "#FFFFFF"
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }
}
