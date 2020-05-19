import QtQuick 2.7
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

Item {
    id: container
    width: engine.width
    height: engine.height
    focus: true

    Image {
        anchors.fill: parent
        source: "qrc:/qmlfiles/pic_background_yr.png"
        Rectangle {
            width: 600
            height: 150
            color: "#292A2F"
            opacity: 1.0
            radius: 75
            anchors.centerIn: parent

            Text {
                anchors.centerIn: parent
                text: leaveProperties.currentTime
                font.pixelSize: 45
                color: "#D4D4D5"
                font.family: "Alibaba PuHuiTi M"
                font.bold: true
            }
        }
    }
}
