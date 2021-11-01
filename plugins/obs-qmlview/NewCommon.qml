import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
import QtGraphicalEffects 1.12
Item {
    function backGroundPath(themeType,uiType)
    {
        if(themeType === 1)
        {
			return "qrc:/qmlfiles/image/newcom/orangeblack.png";
        }
        else if(themeType === 2)
        {
			return "qrc:/qmlfiles/image/newcom/waterblue.png";
        }
        else if(themeType === 3)
        {
			return "qrc:/qmlfiles/image/newcom/tip.png";
		}
        else if(themeType === 4)
        {
			return "qrc:/qmlfiles/image/newcom/switch.png";
		}
        else if(themeType === 5)
        {
			return "qrc:/qmlfiles/image/newcom/grayblue.png";
		}
        else if(themeType === 6)
        {
			return "qrc:/qmlfiles/image/newcom/bluewhale.png";
		}
        else
        {
			return "qrc:/qmlfiles/image/newcom/pinkHeart.png";
		}
    }

    function backGroundWidth(themeType)
    {
        if(themeType === 1)
        {
            return 600;
        }
        else if(themeType === 2)
        {
            return 707;
        }
        else if(themeType === 3)
        {
            return 560;
        }
        else if(themeType === 4)
        {
            return 700;
        }
		 else if(themeType === 5)
        {
            return 600;
		}
		 else if(themeType === 6)
        {
            return 700;
		}
        else
        {
            return 707;
        }
    }

    function backGroundHeight(themeType)
    {
         return 176;
    }
	
	function itemPosX(themeType)
    {
        if(themeType === 1)
        {
            return 12;
        }
        else if(themeType === 2)
        {
            return 17;
        }
        else if(themeType === 3)
        {
            return 20;
        }
        else if(themeType === 4)
        {
            return 16
        }
        else
        {
            return 16;
        }
    }
	
	function itemPosY(themeType)
    {
        if(themeType === 1)
        {
            return 12;
        }
        else if(themeType === 2)
        {
            return 17
        }
        else if(themeType === 3)
        {
            return 20;
        }
        else if(themeType === 4)
        {
            return 16;
        }
        else
        {
            return 16;
        }
    }
	

    function theNameText(listType)
    {
        if(listType === 1)
        {
            return "日榜冠名";
        }
        else
        {
            return "周榜冠名";
        }
    }

    function descText(firstName,uiType)
    {
		if(uiType === 5)
		{
			return "加入直播间";
		}
		else
		{
			if(firstName === "")
			{
				if(uiType === 1)
					return "打赏主播立刻上电视～";
				else if(uiType === 2)
					return "关注主播立刻上电视～";
				else
					return "还没有人上榜";
			}
			else
			{
				if(uiType === 1)
					return "刚刚打赏了主播";
				else if(uiType === 2)
					return "刚刚关注了主播";
				else 
					return firstName;
			}
		}
    }

    function avartarPath(namePath)
    {
        if(namePath === "")
        {
            return "qrc:/qmlfiles/image/empty.png";
        }
        else
        {
            return namePath;
        }
    }

    function sprintf() {
        let args = arguments, string = args[0];
        for (let i = 1; i < args.length; i++) {
            let item = arguments[i];
            string   = string.replace('%d', item);
        }
        return string;
    }
	
    function caculateTextBottomMargin(firstName,themeType)
    {
        if(firstName === "")
        {
			return 52;
		}
		else
		{
			return themetype === 1 ? 22 : 25;
		}
	}
    function avatarRadius(themeType)
    {
        if(themeType === 3 || themeType === 4)
        {
            return 56;
        }
        else
        {
            return 62;
        }
	}
	
    function avatarPosX(themeType)
    {
		if(themeType === 3)
        {
            return 32;
        }
        else if(themeType === 4)
        {
            return 130;
        }
        else
        {
            return 40;
        }
	}
	
    function avatarPosY(themeType)
    {
		if(themeType === 3)
        {
            return 32;
        }
        else if(themeType === 4)
        {
            return 32;
        }
        else
        {
            return 26;
        }
	}
	
    function textPosX(themeType)
    {
        if(themeType === 1)
        {
            return 88;
        }
        else
        {
            return 24;
        }
	}

    function topTextPosY(themeType)
    {
        if(themeType === 3 || themeType === 4)
        {
            return 0;
        }
        else
        {
            return 6;
        }
	}
	
	function bottomTextPosY(empty,uiType)
    {
        if(newProperties.themetype === 1)
        {
			if(uiType!== 3&&uiType!== 5)
			{
				if(empty)
					return 66;
				else
					return 32;
			}
			else
				return 32;
        }
        else if(newProperties.themetype === 2)
        {
			if(uiType!== 3&&uiType!== 5)
			{
				if(empty)
					return 66;
				else
					return 32;
			}
			else
				return 32;
        }
        else if(newProperties.themetype === 3)
        {
			if(uiType!== 3&&uiType!== 5)
			{
				if(empty)
					return 66;
				else
					return 32;
			}
			else
				return 32;
        }
        else if(newProperties.themetype === 4)
        {
			if(uiType!== 3&&uiType!== 5)
			{
				if(empty)
					return 66;
				else
					return 32;
			}
			else
				return 32;
        }
        else
        {
			if(uiType!== 3&&uiType!== 5)
			{
				if(empty)
					return 66;
				else
					return 32;
			}
			else
				return 32;
        }
	}
	function getTopFontSize()
    {
       return 44;
	}
	function getBottomFontSize()
    {
       return 32;
	}
    id: container
	visible:newProperties.uitype===5?false:true
    width: backGroundWidth(newProperties.themetype)
    height: backGroundHeight(newProperties.themetype)
    focus: true
	
    Image {
            id: backGroundImage
            opacity: newProperties.transparence
            anchors.centerIn: parent
            source: backGroundPath(newProperties.themetype,newProperties.uitype)
            fillMode: Image.Pad
    }
	
    Rectangle {
        id:rec1
        width: backGroundWidth(newProperties.themetype)
        height: backGroundHeight(newProperties.themetype)
        x:0
        y:0
        color:"#00000000";
        visible: true;
        Item{
            id:item1
            anchors.left: parent.left
            anchors.top: parent.top
            width:parent.width
            height:parent.height
			
			Rectangle {
                id: avartar
                anchors.left: parent.left
                anchors.leftMargin: avatarPosX(newProperties.themetype)
                anchors.top: parent.top
                anchors.topMargin: avatarPosY(newProperties.themetype)
                width: 2*avatarRadius(newProperties.themetype)
                height:2*avatarRadius(newProperties.themetype)
                radius: avatarRadius(newProperties.themetype)
				Rectangle {
					id: imgeBg
					anchors.left: parent.left
					anchors.top: parent.top
					width:parent.width
					height:parent.height
					radius: avartar.radius
					visible: false
					Image {
						id: squareavatarImage
						smooth: true
						visible: true
						anchors.fill: parent
						source: avartarPath(newProperties.avatarpath)
						sourceSize: Qt.size(avatarRadius(newProperties.themetype)*2 - (newProperties.themetype === 5 ? 5:0), avatarRadius(newProperties.themetype)*2 - (newProperties.themetype === 5 ? 5:0))
						antialiasing: true
					}
					
					Rectangle {
					id: border1
					color: "#00000000"
					anchors.fill: parent
					radius: avartar.radius
					visible: true
					antialiasing: true
					smooth: true
					border.width:0
					border.color:"#00000000"
				}
				}
				Rectangle {
					id: mask1
					color: "black"
					anchors.fill: parent
					radius: avartar.radius
					visible: false
					antialiasing: true
					smooth: true
				}
				OpacityMask {
					id: maskAvatar1
					anchors.fill: imgeBg
					anchors.centerIn: parent
					source: imgeBg
					maskSource: mask1
					visible: true
					antialiasing: true
				}
			}


            Text {
                id: text1
                anchors.left: avartar.right
                anchors.leftMargin: textPosX(newProperties.themetype)
                anchors.top: avartar.top
                anchors.topMargin: topTextPosY(newProperties.themetype)
                text: newProperties.uitype === 3? theNameText(newProperties.listtype):newProperties.firstname
                font.family: newProperties.themefont
                font.pixelSize: getTopFontSize()
                font.bold: newProperties.themebold
                font.italic: newProperties.themeitalic
                color: newProperties.themefontcolor
                visible: newProperties.uitype === 5? true:(newProperties.uitype === 3? true :(newProperties.firstname === ""? false: true));
            }
            Text {
                id: text2
                text: descText(newProperties.firstname,newProperties.uitype)
                anchors.left: avartar.right
                anchors.leftMargin: textPosX(newProperties.themetype)
                anchors.bottom:parent.bottom
                anchors.bottomMargin: bottomTextPosY(newProperties.firstname === "",newProperties.uitype)
                font.family: newProperties.datafont
                font.pixelSize: getBottomFontSize()
                font.bold: newProperties.databold
                font.italic: newProperties.dataitalic
                color: newProperties.datafontcolor
                verticalAlignment: Text.AlignVBottom
            }
        }
    }
	
    NumberAnimation {
        id:enterIn
        target: rec1
		from:-1 * backGroundHeight(newProperties.themetype)
        to:0
        property: "y"
        duration: 300
        easing.type: Easing.InOutQuad
    }
	
    NumberAnimation {
        id:getOut
        target: rec1
		from:0
        to:backGroundHeight(newProperties.themetype)
        property: "y"
        duration: 300
        easing.type: Easing.InOutQuad
        onStopped:{
			if(nameList.length === 0)
				return;
			if(newProperties.uitype !== 3)
			{
				text1.text = nameList.shift()
				squareavatarImage.source = avartarPath(pathList.shift())
				if(newProperties.uitype !== 5)
				{
					text1.visible = text1.text === ""? false: true
				}
				else
				{
					text1.visible = true
				}
				text2.text = descText(text1.text,newProperties.uitype)
				text2.anchors.bottomMargin = bottomTextPosY(text1.text === "",newProperties.uitype)
				index = 1
			}
			else
			{
				squareavatarImage.source = avartarPath(pathList.shift())
				text2.text = descText(nameList.shift(),newProperties.uitype)
				index = 1
			}
			enterIn.start()
			if(nameList.length > 0)
				enterOutTimer.start()
        }
    }
    property var pathList: []
	property var nameList: []
    property var index: 1
    Timer {
        id:enterOutTimer
        interval: 1000;
        triggeredOnStart: true;
        repeat: false
        onTriggered: {
		if(nameList.length > 0)
			getOut.start();
       }
    }
    Connections {
            target: newProperties
            onReplay:{
                if(index <4)
                {
                    squareavatarImage.source = sprintf("qrc:/qmlfiles/image/boke%d.png",index)
					if(newProperties.uitype !== 3)
					{
	                    text1.text = sprintf("啵克%d号",index)
						text1.visible = true
						text2.text = descText(text1.text,newProperties.uitype)
						text2.anchors.bottomMargin = bottomTextPosY(text1.text === "",newProperties.uitype)
					}
					else{
						text2.text = sprintf("啵克%d号",index)
					}
					index = index+1;
				}
               else if(index === 4)
               {
				   if(newProperties.uitype !== 5)
				   {
						squareavatarImage.source = avartarPath(newProperties.avatarpath)
						if(newProperties.uitype !== 3)
						{
							text1.visible = newProperties.firstname === ""? false: true
							text1.text = newProperties.firstname
							text2.text = descText(newProperties.firstname,newProperties.uitype)
							text2.anchors.bottomMargin = bottomTextPosY(newProperties.firstname === "",newProperties.uitype)
						}
						else
						{
							text2.text = descText(newProperties.firstname,newProperties.uitype)
						}
					}
					else
					{
						if(newProperties.firstname!=="")
						{
							squareavatarImage.source = avartarPath(newProperties.avatarpath)
							text1.text = newProperties.firstname
						}
						else
						{
							container.visible = false
						}
						text1.visible = true
					}
					 index = 1
               }
        }
        onUpdate:{
		    pathList.push(newProperties.avatarpath);
			nameList.push(newProperties.firstname);
			console.log(sprintf("-------------%d",nameList.length))
			if(nameList.length === 1)
				getOut.start();
        }
		
        onShow:{
					if(newProperties.uitype === 5)
					{
						container.visible = true
					}
        }
        onHide:{
					if(newProperties.uitype === 5)
					{
						container.visible = false
					}
        }
		onEnter:{
					if(newProperties.uitype === 5)
					{
						squareavatarImage.source = avartarPath(newProperties.avatarpath)
						text1.text = newProperties.firstname
						container.visible = true
					}
		}
    }
}

