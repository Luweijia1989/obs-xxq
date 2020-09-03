import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

//progress 0 准备中 1投票中 2已结束

Item {
	id: root
    visible: true
    width: engine.width
    height: engine.height
	property int durationCache
	
	function getOptionText(index, optionContent) {
		if (index < 0)
			return "";
			
		return (voteProperties.progress !== 0 && optionContent === "") ? "投给选项"+(index+1) : optionContent
	}
	
	Timer {
		interval: 1000
		running: voteProperties.progress === 1
		repeat: true
        onTriggered: {
			durationCache--
			
			if (durationCache === 0)
				voteProperties.stop()
		}
		
		onRunningChanged: {
			if (running)
				durationCache = voteProperties.duration
		}
	}

    Rectangle {
        id: titleRec
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        width: 150
        height: 44
        color: "#46474B"
        Text {
            anchors.centerIn: parent
            text: qsTr("弹幕投票")
            font.pixelSize: 18
            color: "#D4D4D5"
        }
    }

    Rectangle {
        z: -1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: titleRec.verticalCenter
        anchors.topMargin: -2
        color: "#292A2F"
        Image {
            id: backgroundImage
            anchors.fill: parent
            source: voteProperties.backgroundImage
        }

        Text {
            id: statusTitle
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 32
            text: voteProperties.progress === 0 ?  "倒计时:--" : (voteProperties.progress === 1 ? "倒计时:"+durationCache+"S" : "已结束")
            font.pixelSize: 14
            color: voteProperties.progress !== 2 ? "#A2A3A5" : "#F35542"
        }

        ColumnLayout {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.top: statusTitle.bottom
            anchors.topMargin: 8
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
            spacing: 20

            Item {
                Layout.preferredHeight: 38
                Layout.fillWidth: true
                RowLayout {
                    anchors.fill: parent
                    spacing: 0
                    Rectangle {
                        Layout.preferredWidth: 56
                        Layout.fillHeight: true
                        color: "#46474B"
                        Text {
                            anchors.centerIn: parent
                            text: "主题"
                            color: "#D4D4D5"
                            font.pixelSize: 16
                        }
                    }

                    Rectangle {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        color: "#0AFFFFFF"

                        Rectangle
                        {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            width: childrenRect.width
                            height: childrenRect.height
                            color: voteProperties.backgroundColor
                            Text {
                                id: voteTitle
                                text: (voteProperties.progress !== 0 && voteProperties.voteSubject === "") ? "正在进行一场投票" : voteProperties.voteSubject
                                font.pixelSize: voteProperties.fontSize
								font.family: voteProperties.fontFamily
								color: voteProperties.color
								font.bold: voteProperties.bold
								font.italic: voteProperties.italic
								font.underline: voteProperties.underline
								font.strikeout: voteProperties.strikeout
                            }
                        }
                    }
                }

            }

            Column {
                Layout.fillHeight: true
                Layout.fillWidth: true
                spacing: 20
                Repeater {
                    id: optionRepeater
                    model: voteModel
                    delegate: Item {
                        height: 20
                        width: parent.width
                        RowLayout {
                            anchors.fill: parent
                            spacing: 4

                            Text {
                                text: "弹幕发\"" +(index+1)+ "\"："
                                font.pixelSize: 14
                                color: "#A2A3A5"
                            }

                            Item
                            {
                                Layout.fillWidth: true
                                Layout.preferredHeight: parent.height
                                Rectangle
                                {
									anchors.verticalCenter: parent.verticalCenter
                                    height: childrenRect.height
                                    width: childrenRect.width
                                    color: backgroundColor
                                    Text {
                                        text: getOptionText(index, optionContent)
										font.family: fontFamily
                                        font.pixelSize: fontSize
                                        font.bold: bold
                                        font.italic: italic
                                        font.underline: underline
                                        font.strikeout: strikeout
                                        color: model.color
                                    }
                                }

                            }


                            Image {
								visible: voteProperties.progress !== 0
                                source: "qrc:/qmlfiles/icon_ticket_display.png"
                            }

                            Text {
								visible: voteProperties.progress !== 0
                                text: voteCount+"票"
                                font.pixelSize: 14
                                color: "#D4D4D5"
                            }
                        }
                    }
                }
            }

			Rectangle 
			{
				Layout.preferredWidth: childrenRect.width
                Layout.preferredHeight: childrenRect.height
				color: voteProperties.ruleBackgroundColor
				Text {
					id: ruleText
					width: root.width-48
					visible: voteProperties.ruleVisible
					text: "弹幕发送选项对应的数字，例如给第1个选项投票，则发送弹幕\"1\"每人限投1票"
					font.pixelSize: voteProperties.ruleFontSize
					font.family: voteProperties.ruleFontFamily
					color: voteProperties.ruleColor
					font.bold: voteProperties.ruleBold
					font.italic: voteProperties.ruleItalic
					font.underline: voteProperties.ruleUnderline
					font.strikeout: voteProperties.ruleStrikeout
					wrapMode: Text.WrapAnywhere
					Layout.fillWidth: true
					horizontalAlignment: Qt.AlignHCenter
				}		
			}
        }
    }
}
