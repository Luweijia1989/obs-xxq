import QtQuick 2.7
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

Item {
    id: container
    width: engine.width
    height: engine.height
    focus: true

    Rectangle{
        anchors.fill: parent
        color: rankProperties.fillColor
        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: rankProperties.leftMargin
            anchors.rightMargin: rankProperties.rightMargin
            anchors.topMargin: rankProperties.topMargin
            anchors.bottomMargin: rankProperties.bottomMargin
            spacing: rankProperties.spacing

            Text {
                text: rankProperties.rankName
                font.family: rankProperties.font
                font.pixelSize: rankProperties.fontSize
                color: rankProperties.frontColor
                visible: rankProperties.hasRankType
                font.bold: rankProperties.bold
                font.underline: rankProperties.underline
                font.italic: rankProperties.italic
                font.strikeout: rankProperties.strikeout
            }

            ListView {
                Layout.fillHeight: true
                Layout.preferredWidth: parent.width
                model: rankModel

                delegate: Item {
                    height: 40
                    width: parent.width
                    RowLayout {
                        anchors.fill: parent
                        spacing: 12

                        Item {
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: 26
                            Layout.preferredHeight: 26
                            Image {
                                anchors.fill: parent
                                source: rankNumBackgroundImage.length === 0 ? "" : rankNumBackgroundImage
                            }
                            Text {
                                anchors.centerIn: parent
                                text: rankNum
                                font.family: rankFontName
                                font.pixelSize: rankFontSize
                                color: rankColor
                                font.bold: rankBold
                                font.underline: rankProperties.underline
                                font.italic: rankProperties.italic
                                font.strikeout: rankProperties.strikeout
                            }
                            visible: rankProperties.hasRankNum
                        }

                        Text {
                            Layout.alignment: Qt.AlignVCenter
                            Layout.fillWidth: true
                            text: nickname
                            elide: Text.ElideRight
                            font.family: fontName
                            font.pixelSize: fontSize
                            color: fontColor
                            font.bold: nicknameBold
                            font.underline: rankProperties.underline
                            font.italic: rankProperties.italic
                            font.strikeout: rankProperties.strikeout
                        }

                        Text {
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: 100
                            leftPadding: 24
                            text: diamondNum
                            horizontalAlignment: Text.AlignLeft
                            font.family: diamondFontName
                            font.pixelSize: diamondFontSize
                            color: diamondColor
                            visible: rankProperties.hasDiamond
                            font.bold: diamondBold
                            font.underline: rankProperties.underline
                            font.italic: rankProperties.italic
                            font.strikeout: rankProperties.strikeout

                            Image {
                                source: diamondImage
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                            }
                        }
                    }
                }
            }
        }
    }
}
