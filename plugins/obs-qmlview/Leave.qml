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
        source: "file:///" + leaveProperties.backgroundImage

        Rectangle {
            width: 340
            height: 85
            color: "#222222"
            opacity: 0.8
            radius: 46
            anchors.centerIn: parent

            Text {
                anchors.centerIn: parent
                text: leaveProperties.currentTime
                font.pixelSize: 18
                color: "white"
                font.family: "Micrsoft YaHei"
                font.bold: true
            }
        }
    }
}
