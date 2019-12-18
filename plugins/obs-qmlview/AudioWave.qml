import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

Rectangle {
    id: root
    visible: true
    color: "#000000"

    property int baseLevelHeight: height
    property int singleWidth: width/(74+73*2)
    property int lefSpace: (width-singleWidth*(74+73*2))/2

    Row {
        anchors.fill: parent
        anchors.leftMargin: lefSpace
        spacing: 2*singleWidth
        Repeater {
            anchors.fill: parent
            model: audioModel
            delegate: Item {
                width: singleWidth
                height: parent.height
                Rectangle {
                    id: colorRect
					property int ch: baseLevelHeight*audioLevel
                    anchors.centerIn: parent
                    height: ch < 10 ? 10 : ch
                    width: parent.width
                    color: rectColor
                    radius: width/2

                    Behavior on height {
                        NumberAnimation {
                            duration: 300
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }
        }
    }

}
