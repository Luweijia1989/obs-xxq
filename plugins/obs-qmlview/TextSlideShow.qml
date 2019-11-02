import QtQuick 2.7
import QtQuick.Controls 2.5

Item {
    id: container
    width: engine.width
    height: engine.height
    focus: true

    function speed2time(speed)
    {
        if (speed === 1)
            return 3000;
        else if (speed === 2)
            return 2000;
        else
            return 1000;
    }

    function direction2path(direction)
    {
        if (view.count === 1)
            return default_path;

        if (direction === 3 || direction === 4)
            return left_right_path;
        else
            return top_bottom_path;
    }

    function hPos(align, baseWidth, selfWidth)
    {
        switch (align) {
        case Qt.AlignLeft:
            return 0;
        case Qt.AlignHCenter:
            return (baseWidth-selfWidth)/2;
        case Qt.AlignRight:
            return baseWidth-selfWidth;
        }
    }

    function vPos(align, baseHeight, selfHeight)
    {
        switch (align) {
        case Qt.AlignTop:
            return 0;
        case Qt.AlignVCenter:
            return (baseHeight-selfHeight)/2;
        case Qt.AlignBottom:
            return baseHeight-selfHeight;
        }
    }

    function needAppendSpace(text, row, col, fm)
    {
        var maxCharLen = fm.maximumCharacterWidth
        var ret = new Array(col).fill(false)

        for (var i=0; i<col; i++) {
            var lastCharWidth = 0
            for (var j=0; j<row; j++) {
                var c = text.charAt(i*row+j)
                if (c === '')
                    break

                var charLen = fm.advanceWidth(c)
                if (lastCharWidth === 0) {
                    lastCharWidth = charLen
                    continue
                } else {
                    if (lastCharWidth !== charLen) {
                        ret[i] = true
                        break
                    } else
                        lastCharWidth = charLen
                }
            }
        }

        return ret
    }

    function convertString(text, fm, totalHeight)
    {
        var result = ""
        var maxCharLen = fm.maximumCharacterWidth
        var row = Math.floor(totalHeight/fm.height)
        var col = Math.ceil(text.length/row)
        var paddingInfo = needAppendSpace(text, row, col, fm)

        var lines = new Array(row).fill("")
        for (var i=0; i<col; i++) {
            for (var j=0; j<row; j++) {
                var needPadding = paddingInfo[i]
                var c = text.charAt(i*row+j)
                var charLen = fm.advanceWidth(c)
                if (c === '') {
                    if (needPadding) {
                            lines[j] += "\u00a0"
                    } else {
                        if (fm.advanceWidth(text.charAt(i*row)) !== maxCharLen)
                            lines[j] += " "
                        else
                            lines[j] += "\u00a0"
                    }
                } else {
                    if (needPadding && charLen !== maxCharLen)
                        lines[j] += " " + c
                    else
                        lines[j] += c
                }
            }
        }

        for(var m=0; m<row; m++) {
            if (lines[m].length !== 0) {
                if (m !== 0)
                    result += "\n"
                result += lines[m]
            } else
                break
        }
        console.log(result)
        return result
    }

    function makeModel(text, row, col, hAlign, vAlign) {
        var newModel = modelComponent.createObject(null)
        if (row === 0)
            return newModel

        var paddingCount = 0
        if (hAlign === Text.AlignHCenter) {
            var cc = Math.ceil(text.length / row)
            var padding = Math.floor((col-cc)/2)
            for (var j=0; j<padding*row; j++) {
                text = "\u00A0" + text
            }
            paddingCount = padding
        }

        for (var i = 0, l = text.length; i < l/row; i++) {
            var a = text.slice(row*i, row*(i+1));

            var ss = ""
            for (var w=0; w<a.length; w++) {
                if (w !== 0)
                    ss += "\n" + a.charAt(w)
                else
                    ss += a.charAt(w)
            }

            if (hAlign === Text.AlignLeft || hAlign === Text.AlignHCenter) {
                newModel.append({"textData": ss, "shouldVisible": i>=paddingCount})
            } else if (hAlign === Text.AlignRight)
                newModel.insert(0, {"textData": ss, "shouldVisible": i>=paddingCount})
        }

        return newModel;
    }

    function vMarin(align, parentHeight, contentHeight)
    {
        switch (align)
        {
        case Text.AlignTop:
            return 0;
        case Text.AlignVCenter:
            return (parentHeight-contentHeight)/2;
        case Text.AlignBottom:
            return parentHeight-contentHeight;
        }
    }


    Component {
        id: modelComponent
        ListModel {
        }
    }




    Component {
        id: vdelegate

        Item {
            width: container.width; height: container.height

            FontMetrics {
                id: fm
                font.pixelSize: textSlideProperties.fontSize
                font.family: textSlideProperties.font
            }

            Row {
                id: rrr
                anchors.fill: parent
                layoutDirection: textSlideProperties.horizontalAlignment === Text.AlignRight ? Qt.RightToLeft : Qt.LeftToRight
                Repeater {
                    id: repeater
                    model: makeModel(modelData, Math.floor(rrr.height/fm.height), Math.ceil(rrr.width/fm.maximumCharacterWidth), textSlideProperties.horizontalAlignment, textSlideProperties.verticalAlignment)

                    Item {
                        width: fm.maximumCharacterWidth
                        height: parent.height

                        Rectangle {
                            x: 0
                            y: 0
                            width: parent.width
                            height: parent.height
                            color: textSlideProperties.fillColor
                            visible: shouldVisible
                        }

                        Text {
                            id: tt
                            anchors.fill: parent
                            clip: true
                            visible: shouldVisible
                            text: textData
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: textSlideProperties.verticalAlignment
                            color: textSlideProperties.frontColor
                            font.bold: textSlideProperties.bold
                            font.italic: textSlideProperties.italic
                            font.pixelSize: fm.font.pixelSize
                            font.family: fm.font.family

                            Rectangle {
                                width: 2
                                height: tt.contentHeight
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.top: parent.top
                                anchors.topMargin: vMarin(textSlideProperties.verticalAlignment, parent.height, tt.contentHeight)
                                color: textSlideProperties.frontColor
                                visible: textSlideProperties.strikeout
                            }

                            Rectangle {
                                width: 2
                                height: tt.contentHeight
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.topMargin: vMarin(textSlideProperties.verticalAlignment, parent.height, tt.contentHeight)
                                color: textSlideProperties.frontColor
                                visible: textSlideProperties.underline
                            }
                        }
                    }
                }
            }
        }
    }





    Component {
        id: hdelegate
        Item {
            width: container.width; height: container.height
            Rectangle {
                x: hPos(textSlideProperties.horizontalAlignment, parent.width, tt.contentWidth)
                y: vPos(textSlideProperties.verticalAlignment, parent.height, tt.contentHeight)
                width: tt.contentWidth > tt.width ? tt.width : tt.contentWidth
                height: tt.contentHeight > tt.height ? tt.height : tt.contentHeight
                color: textSlideProperties.fillColor
            }

            FontMetrics {
                id: fm
                font: tt.font
            }

            Text {
                id: tt
                anchors.fill: parent
                clip: true
                horizontalAlignment: textSlideProperties.horizontalAlignment
                verticalAlignment: textSlideProperties.verticalAlignment
                text: modelData
                color: textSlideProperties.frontColor
                font.bold: textSlideProperties.bold
                font.italic: textSlideProperties.italic
                font.strikeout: textSlideProperties.strikeout
                font.underline: textSlideProperties.underline
                rightPadding: 4
                font.pixelSize: textSlideProperties.fontSize
                font.family: textSlideProperties.font
                wrapMode: textSlideProperties.hDisplay ? Text.WrapAnywhere : Text.NoWrap
            }
        }
    }

    PathView {
        id: view

        anchors.fill: parent
        model: textSlideProperties.textStrings

        delegate: textSlideProperties.hDisplay ? hdelegate : vdelegate
        cacheItemCount: 2
        pathItemCount: 2
        path: direction2path(textSlideProperties.direction)
    }

    Path {
        id: default_path
        startX: container.width/2; startY: container.height/2
        PathLine { relativeX: container.width; y: container.height/2;  }
        PathLine { relativeX: container.width; y: container.height/2;  }
    }

    Path {
        id: left_right_path
        startX: -container.width/2; startY: container.height/2
        PathLine { relativeX: container.width; y: container.height/2;  }
        PathLine { relativeX: container.width; y: container.height/2;  }
    }

    Path {
        id: top_bottom_path
        startX: container.width/2; startY: -container.height/2
        PathLine { x: container.width/2; relativeY: container.height;  }
        PathLine { x: container.width/2; relativeY: container.height;  }
    }

    Timer {
        interval: speed2time(textSlideProperties.speed)
        running: true
        repeat: true
        onTriggered: {
            if (textSlideProperties.direction ===  3 || textSlideProperties.direction ===  1)
                view.incrementCurrentIndex()
            else
                view.decrementCurrentIndex()
        }
    }
}
