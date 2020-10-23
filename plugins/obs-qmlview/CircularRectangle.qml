import QtQuick 2.12
import QtGraphicalEffects 1.12

Rectangle {
    property string imgSrc
    Image {
        id: avatar
        smooth: true
        visible: false
        anchors.fill: parent
        source: imgSrc
        sourceSize: Qt.size(parent.size, parent.size)
        antialiasing: true
    }
    Rectangle {
        id: mask
        color: "black"
        anchors.fill: parent
        radius: width / 2
        visible: false
        antialiasing: true
        smooth: true
    }
    OpacityMask {
        id: maskAvatar
        anchors.fill: avatar
        source: avatar
        maskSource: mask
        visible: true
        antialiasing: true
    }
}
