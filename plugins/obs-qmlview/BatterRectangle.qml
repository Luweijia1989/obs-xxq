import QtQuick 2.12
import QtQuick.Layouts 1.2

Rectangle {
    property string batterStr
    RowLayout{
        spacing:0
        Repeater {
            model: batterStr.length + 1
            Image {
                source: index === 0 ? "image/pic_ride.png" : "image/" + batterStr.charAt(index - 1) + ".png"
            }
        }
    }
}
