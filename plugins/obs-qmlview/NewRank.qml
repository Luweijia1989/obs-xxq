import QtQuick 2.7
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3

Item {
	function backGroundPath(themeType,hasRankType)
    {
        if(themeType === 1)
        {
			if(hasRankType === true)
				return "qrc:/qmlfiles/image/newrank/showBackGround/orangeblack.png";
			else
				return "qrc:/qmlfiles/image/newrank/hideBackGround/orangeblack.png";
        }
        else if(themeType === 2)
        {
			if(hasRankType === true)
				return "qrc:/qmlfiles/image/newrank/showBackGround/waterblue.png";
			else
				return "qrc:/qmlfiles/image/newrank/hideBackGround/waterblue.png";
        }
        else if(themeType === 3)
        {
			if(hasRankType === true)
				return "qrc:/qmlfiles/image/newrank/showBackGround/deepblue.png";
			else
				return "qrc:/qmlfiles/image/newrank/hideBackGround/deepblue.png";
		}
        else if(themeType === 4)
        {
			if(hasRankType === true)
				return "qrc:/qmlfiles/image/newrank/showBackGround/tip.png";
			else
				return "qrc:/qmlfiles/image/newrank/hideBackGround/tip.png";
		}
    }
	


    function backGroundHeight(hasRankType)
    {
        if(hasRankType === true)
        {
            return 416;
        }
        else
        {
            return 388;
        }
    }
    id: container
    width:  388
    height: backGroundHeight(rankProperties.hasRankType)
    focus: true

    Rectangle{
        anchors.fill: parent
        color:"#00000000";
        Image {
			id: backGroundImage
            opacity: rankProperties.transparence
            anchors.centerIn: parent
            source:backGroundPath(rankProperties.themetype,rankProperties.hasRankType)
            fillMode: Image.Pad
        }
        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: rankProperties.leftMargin
            anchors.rightMargin: rankProperties.rightMargin
            anchors.topMargin: rankProperties.topMargin
            anchors.bottomMargin: rankProperties.bottomMargin
            spacing: rankProperties.spacing

            Text {
                text: rankProperties.rankName
	            anchors.left: parent.left
				anchors.leftMargin: 24
				anchors.top: parent.top
				anchors.topMargin:32
                font.family: rankProperties.themefont
                font.pixelSize: 28
                color: rankProperties.themefontcolor
                visible: rankProperties.hasRankType
                font.bold: rankProperties.themebold
                font.italic: rankProperties.themeitalic
            }

            ListView {
				anchors.left: parent.left
				anchors.leftMargin: 24
				anchors.right: parent.right
				anchors.rightMargin: 24
				anchors.top: parent.top
				anchors.topMargin:rankProperties.hasRankType?96:48
                Layout.fillHeight: true
                Layout.preferredWidth: parent.width
                model: newRankModel
                spacing: 24

                delegate: Item {
                    height: 40
                    width: parent.width
                    RowLayout {
                        anchors.fill: parent
                        Item {
                            Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
                            Layout.preferredWidth: 26
                            Layout.preferredHeight: 36
							Layout.leftMargin : 0
                            Text {
                                anchors.centerIn: parent
                                text: rankNum
                                font.family: rankFontName
                                font.pixelSize: 24
                                color: rankColor
                                font.bold: rankBold
                                font.italic: italic
                            }
                            visible: rankProperties.hasRankNum
                        }
						
						CircularRectangle{
                            Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
							Layout.leftMargin : 0
							id: avatar
							width: 40
							height: 40
							radius: 20
							imgSrc: avarstarPath
						}

						
                        Text {
                            Layout.alignment: Qt.AlignVCenter|Qt.AlignLeft
                            Layout.fillWidth: true
							Layout.leftMargin : 8
							Layout.preferredWidth:120
                            text: nickname
                            elide: Text.ElideRight
                            font.family: fontName
                            font.pixelSize: 24
                            color: fontColor
                            font.bold: nicknameBold
                            font.italic: italic
                        }

                        Text {
                            Layout.alignment: Qt.AlignRight
							Layout.rightMargin : 0
                            Layout.maximumWidth: 141
                            rightPadding: 18
                            text: diamondNum
                            horizontalAlignment: Text.AlignRight
                            font.family: diamondFontName
                            font.pixelSize: 24
                            color: diamondColor
                            visible: rankProperties.hasDiamond
                            font.bold: diamondBold
                            font.italic: italicc

                            Image {
                                source: diamondImage
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
								width:18
								height:26
                            }
                        }
                    }
                }
            }
        }
    }
}
