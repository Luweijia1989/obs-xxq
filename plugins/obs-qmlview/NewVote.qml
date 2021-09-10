import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3
import QtGraphicalEffects 1.12

//progress 0 准备中 1投票中 2已结束

Item {
	id: root
    visible: true
    width: engine.width
    height: engine.height
	property int durationCache

    function backGroundPath(themeType) {
        if(themeType === 1)
        {
            return "qrc:/qmlfiles/image/newvote/black.png";
        }
        else if(themeType === 2)
        {
            return "qrc:/qmlfiles/image/newvote/blue.png";
        }
        else if(themeType === 3)
        {
            return "qrc:/qmlfiles/image/newvote/grey.png";
        }
        else if(themeType === 4)
        {
            return "qrc:/qmlfiles/image/newvote/tips.png";
        }
    }
	
	function getOptionText(index, optionContent) {
		if (index < 0)
			return "";
			
        return (newVoteProperties.progress !== 0 && optionContent === "") ? "投给选项"+(index+1) : optionContent
	}
	
	function getCountintText() {
        if (newVoteProperties.progress === 2)
			return "已结束"
        else if (newVoteProperties.progress === 0)
			return "倒计时:--"
		else 
            return !newVoteProperties.useDuration ? "倒计时:"+durationCache+"S" : "倒计时:--"
	}
	
	Timer {
		interval: 1000
        running: newVoteProperties.progress === 1 && !newVoteProperties.useDuration
		repeat: true
        onTriggered: {
			durationCache--
			
			if (durationCache < 0)
				durationCache = 0
		}
		
		onRunningChanged: {
			if (running)
                durationCache = newVoteProperties.duration
		}
	}

    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        Image {
            id: backgroundImage
            source: backGroundPath(newVoteProperties.themeType)
            fillMode: Image.PreserveAspectFit
            opacity: newVoteProperties.transparence
        }

        Item {
            id: statusTitle
            anchors.top: parent.top
            anchors.topMargin: 32
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.right: parent.right
            anchors.rightMargin: 24
            height: 38

            Text {
                anchors.left: parent.left
                text: qsTr("弹幕投票")
                color: newVoteProperties.downColor
                font.pixelSize: 28
                font.family: newVoteProperties.downFontFamily
                font.bold: newVoteProperties.downBold
                font.italic: newVoteProperties.downItalic
            }

            Text {
                anchors.right: parent.right
                verticalAlignment: Text.verticalCenter
                text: getCountintText()
                color: newVoteProperties.downColor
                font.pixelSize: 24
                font.family: newVoteProperties.downFontFamily
                font.bold: newVoteProperties.downBold
                font.italic: newVoteProperties.downItalic
            }
        }

        ColumnLayout {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.top: parent.top
            anchors.topMargin: 86
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
            spacing: 12

            Item {
                Layout.preferredHeight: 38
                Layout.fillWidth: true

                Text {
                    id: voteTitle
                    anchors.left: parent.left
                    verticalAlignment: Text.verticalCenter
                    text: (newVoteProperties.progress !== 0 && newVoteProperties.voteSubject === "") ? "正在进行一场投票" : newVoteProperties.voteSubject
                    color: newVoteProperties.themeColor
                    font.pixelSize: 28
                    font.family: newVoteProperties.themeFontFamily
                    font.bold: newVoteProperties.themeBold
                    font.italic: newVoteProperties.themeItalic
                }
            }

            Column {
                Layout.fillHeight: true
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    id: optionRepeater
                    model: newVoteModel
                    delegate: Item {
                        height: 49
                        width: parent.width

                        LinearGradient {
                            anchors.fill: parent
                            start: Qt.point(0, 0);
                            end: Qt.point(parent.width, 0);
                            gradient: Gradient {
                                GradientStop{ position: 0.0; color: newVoteProperties.themeType !== 4 ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(0.98, 0.83, 0.77, 0.2)}
                                GradientStop{ position: 1.0; color: newVoteProperties.themeType !== 4 ? Qt.rgba(1, 1, 1, 0.02) : Qt.rgba(0.84, 0.95, 0.92, 0.6)}
                            }
                        }

                        RowLayout {
                                anchors.fill: parent
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.topMargin: 5
                                spacing: 0

                                Text {
                                    id: itemTitle
                                    anchors.left: parent.left
                                    anchors.leftMargin: 16
                                    text: "弹幕发 \"" +(index+1)+ "\" : "
                                    font.pixelSize: 24
                                    color: newVoteProperties.dataColor
                                    font.family: newVoteProperties.dataFontFamily
                                    font.bold: newVoteProperties.dataBold
                                    font.italic: newVoteProperties.dataItalic
                                }

                                Text {
                                    anchors.left: itemTitle.right
                                    text: getOptionText(index, optionContent)
                                    font.pixelSize: 24
                                    color: newVoteProperties.dataColor
                                    font.family: newVoteProperties.dataFontFamily
                                    font.bold: newVoteProperties.dataBold
                                    font.italic: newVoteProperties.dataItalic
                                }

                                Text {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 16
                                    visible: newVoteProperties.progress !== 0
                                    text: voteCount+"票"
                                    font.pixelSize: 24
                                    color: newVoteProperties.dataColor
                                    font.family: newVoteProperties.dataFontFamily
                                    font.bold: newVoteProperties.dataBold
                                    font.italic: newVoteProperties.dataItalic
                                }
                        }
                    }
                }
            }
        }
    }
}
