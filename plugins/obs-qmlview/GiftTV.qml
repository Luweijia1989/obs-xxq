import QtQuick 2.7
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.2
import QtGraphicalEffects 1.12

Item {
    id: container
    width: engine.width
    height: engine.height
    focus: true

    property var tvGrid:gridUpdate(4, 2)
    property var giftArray:[]
    property var giftComponentArray:[]
    property int prefer:1
    property int rows:4
    property int cols:2
    property int gridWidth:456
    property int gridHeight:120

    property string giftInfoArray: gifttvProperties.giftArray

    onGiftInfoArrayChanged: {
        clearComponent()
        console.log("onGiftInfoArrayChanged")
        if (giftInfoArray === "")
            return

        console.log("onGiftInfoArrayChanged", giftInfoArray)

        let array = JSON.parse(giftInfoArray)

        console.log("onGiftInfoArrayChanged", array)

        let i = 0;
        for(i = 0; i < array.length; ++i)
        {
            var giftComponent = createComponent(array[i])
            giftComponent.x = array[i].xPosition
            giftComponent.y = array[i].yPosition
        }
    }

    function getComponentByGiftUid(giftUid)
    {
        let i
        for(i = 0; i < giftComponentArray.length; ++i)
        {
            if (giftComponentArray[i] !== undefined && giftComponentArray[i].giftInfo.giftUid === giftUid)
                return giftComponentArray[i]

        }
        return false
    }

    function gridUpdate(rows, cols) {
        let tempGrid = new Array(rows)
        for(let i = 0; i < rows; i++)
            tempGrid[i] = new Array(cols)
        console.log("gridUpdate", rows, " ", cols)
        return tempGrid
    }

    function clearComponent() {
        console.log("ClearGifts")
        let components = []
        let i = 0;
        for(i = 0; i < container.children.length; ++i)
        {
            components.push(container.children[i])
        }

        for(i = 0; i < container.children.length; ++i)
        {
            components[i].destroy();
        }
    }

    function removeGift(content) {
        let i
        for(i = 0; i < giftComponentArray.length; ++i)
        {
            if (giftComponentArray[i] === content)
            {
                var temp = giftComponentArray[i]
                giftComponentArray.splice(i, 1)
                console.log("delete content", content, " ", content.destroy)
                content.destroy()
            }
        }
    }

    function createComponent(obj) {
        console.log("giftType", obj.giftType)
        var giftComponent
        if (obj.giftType === 0)
            giftComponent = normalGift.createObject(container, {giftInfo : obj});
        else
            giftComponent = advancedGift.createObject(container, {giftInfo : obj});

//        var componentPoint = /*gifttvProperties.prefer === 0*/true ? updateGiftPositionForHorizontal(giftComponent) : updateGiftPositionForVertical(giftComponent);

//        giftComponent.x = componentPoint.x
//        giftComponent.y = componentPoint.y

        return giftComponent
    }

    function refurshGift() {
        let j
        let hasAdvanced = false
        for(j = 0; j < giftArray.length; ++j)
        {
            if(giftArray[j].giftInfo.giftType === 1)
            {
                hasAdvanced = true
                break
            }
        }

        if (cols == 1 && hasAdvanced){
            gifttvProperties.autoExtendTvCols(2)
            return
        }

        console.log("refurshGift", rows, "x", (cols))
        clearComponent()

        for(j = 0; j < giftArray.length; ++j)
        {
            let giftComponent = createComponent(giftArray[j])
            giftComponentArray.push(giftComponent)
        }

        // 更新礼物缓存
        giftArray.length = 0
        for(let k = 0; k < giftComponentArray.length; ++k)
        {
            giftArray.push(giftComponentArray[k].giftInfo)
        }
    }

    function getGiftPositionForHorizontal(content) {
        let row = 0
        let col = 0
        let xPosition = 0
        let yPosition = 0

        for (row = 0; row < rows; ++row)
        {
            for (col = 0; col < cols; ++col)
            {
                var giftContent = tvGrid[row][col];
                // 若为空，表示栅格位置空闲
                if (giftContent === undefined)
                {
                    // 若为小礼物，则只需要一个单元格，故直接计算坐标点
                    if (content.giftInfo.giftType === 0)
                    {
                        xPosition = col * (gridWidth + 20);
                        yPosition = row * gridHeight + 10;
                        tvGrid[row][col] = content;
                        console.log("updateGiftPositionForHorizontal222:", xPosition, " ", yPosition, " ", col, row)
                        return {x:xPosition, y:yPosition};
                    }

                    // 若为大礼物，则需要两个单元格，故继续判断下一个水平单元格是否为空，并计算坐标点
                    // 否则继续查找可以放下大礼物的位置
                    if (col + 1 < cols && tvGrid[row][col + 1] === undefined)
                    {
                        xPosition = col * (gridWidth + 20);
                        yPosition = row * gridHeight;
                        tvGrid[row][col] = content;
                        tvGrid[row][col + 1] = "content";
                        return {x:xPosition, y:yPosition};
                    }
                }
            }
        }
        //没有找到可摆放礼物的空闲位置则返回false
        return false
    }

    function removeGrid(type, length) {
        let min = -1
        let minTimeStamp = -1
        let tempRow = 0
        let tempCol = 0
        let tempGiftType = 0
        let needGrid = type === 0 ? 1 : 2
        let freeGift = []
        let row = 0
        let col = 0
        needGrid += length
        console.log("removeGrid needGrid", needGrid)
        do {
            min = -1
            for (row = 0; row < rows; ++row) {
                for (col = 0; col < cols; ++col) {
                    if (tvGrid[row][col] === undefined){
                        needGrid--;
                        freeGift.push({row:row,col:col})
                        if (needGrid === 0)
                            return freeGift
                    }
                    else if (tvGrid[row][col] === "content")
                    {
                        console.log("continue", tvGrid, " ", row, " ", col)
                        continue
                    }
                    else
                    {
                        if (min === -1)
                        {
                            min = tvGrid[row][col].giftInfo.diamond
                            minTimeStamp = tvGrid[row][col].giftInfo.timeStamp
                            tempRow = row
                            tempCol = col
                            tempGiftType = tvGrid[row][col].giftInfo.giftType
                        }
                        console.log("sssssssss", row, " ", col, " ", tvGrid[row][col].giftInfo.diamond, " ", min);
                        if (tvGrid[row][col].giftInfo.diamond <= min)
                        {
                            if (tvGrid[row][col].giftInfo.diamond < min)
                            {
                                min = tvGrid[row][col].giftInfo.diamond
                                minTimeStamp = tvGrid[row][col].giftInfo.timeStamp
                                tempRow = row
                                tempCol = col
                                tempGiftType = tvGrid[row][col].giftInfo.giftType
                                console.log("sssssssss", tvGrid[row][col].giftInfo.diamond, " ", row, " ", col);
                            }
                            else{
                                if (tvGrid[row][col].giftInfo.timeStamp < minTimeStamp)
                                {
                                    min = tvGrid[row][col].giftInfo.diamond
                                    minTimeStamp = tvGrid[row][col].giftInfo.timeStamp
                                    tempRow = row
                                    tempCol = col
                                    tempGiftType = tvGrid[row][col].giftInfo.giftType
                                    console.log("sssssssss", tvGrid[row][col].giftInfo.diamond, " ", row, " ", col);
                                }
                            }
                        }
                    }
                }
            }

            if (tvGrid[tempRow][tempCol] !== undefined)
                removeGift(tvGrid[tempRow][tempCol])

            tvGrid[tempRow][tempCol] = undefined
            if (tempGiftType === 1) {
                tvGrid[tempRow][tempCol + 1] = undefined
            }
        }while(needGrid > 0)

        return freeGift
    }

    function updateGiftPositionForHorizontal(content) {
        // 查找可摆放的位置并直接返回坐标点
        var res = getGiftPositionForHorizontal(content)
        if(res !== false)
            return res

        // 若没有找到，则先根据需插入的礼物需要占用的单元格数，按礼物价值从低到高开始删除，并返回删除后的空闲位置数组
        let freeGift = removeGrid(content.giftInfo.giftType, 0);

        // 根据空闲位置显示新增的礼物
        let row = 0
        let col = 0
        let xPosition = 0
        let yPosition = 0
        if (content.giftInfo.giftType === 0)
        {
            // 如果需要插入的礼物为普通礼物
            row = freeGift[0].row
            col = freeGift[0].col
            xPosition = freeGift[0].col * (gridWidth + 20);
            yPosition = freeGift[0].row * gridHeight + 10;
            tvGrid[row][col] = content;
        }
        else {
            // 如果需要插入的礼物为大礼物
            let find = false
            while (!find)
            {
                let i = 0
                for(i = 0; i < freeGift.length; ++i)
                {
                    row = freeGift[i].row
                    col = freeGift[i].col
                    console.log("updateGiftPositionForHorizontal:freeGift ", row, " ", col)
                    if (col < cols - 1 && tvGrid[row][col + 1] === undefined)
                    {
                        find = true
                        xPosition = col * (gridWidth + 20);
                        yPosition = row * gridHeight;
                        tvGrid[row][col] = content;
                        tvGrid[row][col + 1] = "content";
                        break;
                    }
                }
                console.log("while whilewhilewhilewhilewhile")
                if(!find)
                {
                    freeGift = removeGrid(0, freeGift.length);
                }
            }
        }

        return {x:xPosition, y:yPosition};
    }

    function updateGiftPositionForVertical(content) {
        let row = 0
        let col = 0
        let xPosition = 0
        let yPosition = 0
        let type = content.giftInfo.giftType

        for (col = 0; col < cols; ++col)
        {
            for (row = 0; row < rows; ++row)
            {
                var giftContent = tvGrid[row][col];
                // 若为空，表示栅格位置空闲
                if (giftContent === undefined)
                {
                    // 若为小礼物，则只需要一个单元格，故直接计算坐标点
                    if (type === 0)
                    {
                        xPosition = col * (gridWidth + 20);
                        yPosition = row * gridHeight + 10;
                        tvGrid[row][col] = content;
                        return {x:xPosition, y:yPosition};
                    }

                    // 若为大礼物，则需要两个单元格，故继续判断下一个水平单元格是否为空，并计算坐标点
                    // 否则继续查找可以放下大礼物的位置
                    if (col + 1 < cols && tvGrid[row][col + 1] === undefined)
                    {
                        xPosition = col * (gridWidth + 20);
                        yPosition = row * gridHeight;
                        tvGrid[row][col] = content;
                        tvGrid[row][col + 1] = content;
                        return {x:xPosition, y:yPosition};
                    }
                }
            }
        }

        // 若没有找到，则先根据需插入的礼物需要占用的单元格数，按礼物价值从低到高开始删除
        let min = -1
        let minTimeStamp = -1
        let tempRow = 0
        let tempCol = 0
        let tempGiftType = 0
        let needGrid = type === 0 ? 1 : 2
        var tempGiftContent = undefined
        row = 0
        col = 0

        while(needGrid > 0){
            for (col = 0; col < cols; ++col) {
                for (row = 0; row < rows; ++row) {
                    tempGiftContent = tvGrid[row][col];
                    if (tempGiftContent === undefined){
                        needGrid--;
                    }
                    else if (tempGiftContent === "content")
                    {
                        continue
                    }
                    else
                    {
                        if (min === -1)
                        {
                            min = tempGiftContent.giftInfo.diamond
                            minTimeStamp = tempGiftContent.giftInfo.timeStamp
                            tempRow = row
                            tempCol = col
                            tempGiftType = tempGiftContent.giftInfo.giftType
                        }

                        if (tempGiftContent.giftInfo.diamond <= min)
                        {
                            if (tempGiftContent.giftInfo.diamond < min)
                            {
                                min = tempGiftContent.giftInfo.diamond
                                minTimeStamp = tempGiftContent.giftInfo.timeStamp
                                tempRow = row
                                tempCol = col
                                tempGiftType = tempGiftContent.giftInfo.giftType
                            }
                            else{
                                if (tempGiftContent.giftInfo.timeStamp < minTimeStamp)
                                {
                                    min = tempGiftContent.giftInfo.diamond
                                    minTimeStamp = tempGiftContent.giftInfo.timeStamp
                                    tempRow = row
                                    tempCol = col
                                    tempGiftType = tempGiftContent.giftInfo.giftType
                                }
                            }
                        }
                    }
                }
            }

            tvGrid[tempRow][tempCol] = undefined
            if (tempGiftType === 1) {
                tvGrid[tempRow][tempCol + 1] = undefined
            }

            if (tempGiftContent !== undefined)
                removeGift(tempGiftContent)
        }

        // 显示新增的礼物
        if (type === 0)
        {
            // 如果需要插入的礼物为普通礼物
            xPosition = tempCol * (gridWidth + 20);
            yPosition = tempRow * gridHeight + 10;
            tvGrid[tempRow][tempCol] = content;
        }
        else {
            // 如果需要插入的礼物为大礼物
            xPosition = tempCol * (gridWidth + 20);
            yPosition = tempRow * gridHeight;
            tvGrid[tempRow][tempCol] = content;
            tvGrid[tempRow][tempCol + 1] = "content";
        }

        return {x:xPosition, y:yPosition};
    }

    Component {
        id: normalGift
        Item {
            property var giftInfo

            width:456
            height:100

            Image {
                id: infoRectangle
                height:100
                width:308
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                sourceSize:Qt.size(308, 100)
                source: "image/pic_gift_scroll_level_" + giftInfo.rangeLevel + ".png"

                CircularRectangle{
                    id: avatar
                    anchors.leftMargin: 8
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: 64
                    height: 64
                    radius: 32
                    imgSrc: giftInfo.avatarPath
                }

                ColumnLayout{
                    anchors.leftMargin: 10
                    anchors.left: avatar.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text {
                        id: name
                        Layout.preferredWidth: 120
                        text: qsTr(giftInfo.name)
                        elide: Text.ElideRight
                        font.pixelSize: 24
                        color: "#FFFFFF"
                        font.family: "Alibaba PuHuiTi M"
                        font.bold: true
                    }

                    RowLayout{
                        Text {
                            text: qsTr("赠送")
                            font.pixelSize: 20
                            color: "#FFFFFF"
//                            font.family: "Alibaba PuHuiTi R"
                        }

                        Text {
                            id: giftName
                            text: qsTr(giftInfo.gfitName)
                            font.pixelSize: 20
                            color: "#FFFFFF"
                            font.family: "Alibaba PuHuiTi M"
                            font.bold: true
                        }
                    }
                }

                Image {
                    id: giftIcon
                    anchors.leftMargin: 210
                    anchors.left: parent.left
                    source: giftInfo.giftPath
                    sourceSize: Qt.size(80, 80)
                    antialiasing: true
                }
            }

            Image {
                source: "image/pic_nospeed_" + giftInfo.rangeLevel.toString() + ".png"
                sourceSize: Qt.size(62, 100)
                anchors.right:infoRectangle.right
                anchors.bottom:infoRectangle.bottom
                visible: giftInfo.rangeLevel > 1 && giftInfo.rangeLevel < 7
            }

            Item {
                width: 62
                height: giftInfo.barPercent
                clip:true
                anchors.right:infoRectangle.right
                anchors.bottom:infoRectangle.bottom
                visible: giftInfo.rangeLevel > 1 && giftInfo.rangeLevel < 7
                Image {
                    source: "image/pic_speed_" + giftInfo.rangeLevel.toString() + ".png"
                    sourceSize: Qt.size(62, 100)
                    anchors.right:parent.right
                    anchors.bottom:parent.bottom
                }
            }

            BorderImage {
              width: groupNum.width + 20
              height: 12
              anchors.topMargin: 16
              anchors.leftMargin: 8
              anchors.left: infoRectangle.right
              anchors.top: infoRectangle.top
              border { left: 8; top: 0; right: 8; bottom: 12 }
              horizontalTileMode: BorderImage.Stretch
              verticalTileMode: BorderImage.Stretch
              source: "image/pic_red_batch.png"
              visible: parseInt(giftInfo.count) > 1
            }

            Text {
                id: groupNum
                anchors.topMargin: 6
                anchors.leftMargin: 20
                anchors.left: infoRectangle.right
                anchors.top: infoRectangle.top
                text: giftInfo.count
                visible: parseInt(giftInfo.count) > 1
                font.pixelSize: 24
                color: "#FFFFFF"
                font.family: "Alibaba PuHuiTi M"
                font.bold: true
            }

            Item {
                id: batterRecatangle
                width: 140
                height: 40
                anchors.topMargin: 42
                anchors.leftMargin: 8
                anchors.top: infoRectangle.top
                anchors.left: infoRectangle.right

                RowLayout{
                    spacing:0
                    Repeater {
                        model: giftInfo.hitCount.length + 1
                        Image {
                            source: index === 0 ? "image/pic_ride.png" : "image/" + giftInfo.hitCount.charAt(index - 1) + ".png"
                        }
                    }
                }
            }
        }
    }

    Component {
        id: advancedGift
        Item{
            property var giftInfo

            width: 932
            height: 120

            Image {
                width: 932
                height: 120
                sourceSize:Qt.size(932, 120)
                source: "image/pic_big_banner.png"
            }

            Item{
//                anchors.leftMargin: 34
                anchors.left: parent.left
                anchors.topMargin: 30
                anchors.top: parent.top
                anchors.bottomMargin: 30
                anchors.bottom: parent.bottom

                Text {
                    id: name
                    anchors.leftMargin: 34
                    anchors.left: parent.left
    //                anchors.topMargin: 34
                    anchors.top: parent.top
                    text: qsTr(giftInfo.name)
                    font.pixelSize: 28
                    color: "#FFFFFF"
                    font.family: "Alibaba PuHuiTi M"
                    font.bold: true
                }

                Text {
                    anchors.leftMargin: 34
                    anchors.left: parent.left
    //                anchors.bottomMargin: 36
                    anchors.bottom: parent.bottom
                    text: qsTr("赠送")
                    font.pixelSize: 20
                    color: "#FFF7F0"
    //                font.family: "Alibaba PuHuiTi R"
                }

                Text {
                    anchors.leftMargin: 79
                    anchors.left: parent.left
    //                anchors.bottomMargin: 36
                    anchors.bottom: parent.bottom
                    text: giftInfo.recName
                    font.pixelSize: 20
                    color: "#FFF7F0"
    //                font.family: "Alibaba PuHuiTi R"
                }
            }

            Image {
                id: giftIcon
                anchors.leftMargin: 584
                anchors.left: parent.left
                source: giftInfo.giftPath
                sourceSize: Qt.size(120, 120)
                antialiasing: true
            }

            Text {
                anchors.leftMargin: 718
                anchors.left: parent.left
                anchors.topMargin: 22
                anchors.top: parent.top
                text: qsTr(giftInfo.count)
                font.pixelSize: 24
                color: "#FFF7F0"
//                font.family: "Alibaba PuHuiTi R"
                visible: parseInt(giftInfo.count) > 1
            }

            Text {
                anchors.leftMargin: 718
                anchors.left: parent.left
                anchors.bottomMargin: 32
                anchors.bottom: parent.bottom
                text: qsTr("x"+giftInfo.hitCount)
                font.pixelSize: 36
                color: "#FFF7F0"
//                font.family: "Alibaba PuHuiTi R"
            }
        }
    }

    Connections {
        target: gifttvProperties
        onPreferChanged: {
            console.log("AAAAAAAAAAAAAAAAAAAAAA")

        }

        onUpdateGiftChanged: {
            console.log("onUpdateGiftChanged:", gifttvProperties.updateGift)
            if (gifttvProperties.updateGift === "")
                return

            let obj = JSON.parse(gifttvProperties.updateGift)
            let i = 0
            let find = false
            for(i = 0; i < container.children.length; ++i)
            {
                console.log("onUpdateGiftChanged", i, obj.hitBatchId === container.children[i].giftInfo.hitBatchId)
                if(obj.hitBatchId === container.children[i].giftInfo.hitBatchId)
                {
                    find = true
                    break
                }
            }

            if (find)
                container.children[i].giftInfo = obj;
        }

        onInvalidGiftChanged: {
            console.log("onInvalidGiftChanged:", gifttvProperties.invalidGift)
            if (gifttvProperties.invalidGift === "")
                return

            let obj = JSON.parse(gifttvProperties.invalidGift)
            let i = 0
            let find = false
            for(i = 0; i < container.children.length; ++i)
            {
                console.log("onInvalidGiftChanged", i, obj.hitBatchId === container.children[i].giftInfo.hitBatchId)
                if(obj.hitBatchId === container.children[i].giftInfo.hitBatchId)
                {
                    find = true
                    break
                }
            }

            if (find)
                container.children[i].destroy();
        }

        onGiftChanged: {
            console.log("onGiftChanged:", gifttvProperties.gift)
            if (gifttvProperties.gift === "")
                return

            let obj = JSON.parse(gifttvProperties.gift)
            var giftComponent = createComponent(obj)
            giftComponent.x = obj.xPosition
            giftComponent.y = obj.yPosition
        }

        onRowChanged: {
//            console.log("onRowChanged:")
//            refurshGift()
        }

        onColChanged: {
//            console.log("onColChanged:")
//            refurshGift()
        }
    }
}
