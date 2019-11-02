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

    function imageSize(baseWidth, baseHeight, sourceWidth, sourceHeight)
    {
        if (baseWidth === 0 || baseHeight === 0 || sourceWidth === 0 || sourceHeight === 0)
            return {
                    retw: 0,
                    reth: 0,
                };

        var w, h
        var b_ratio = baseWidth/baseHeight
        var s_ratio = sourceWidth/sourceHeight

        if (b_ratio > s_ratio) {
            h = baseHeight
            w = h * s_ratio
        } else {
            w = baseWidth
            h = w / s_ratio
        }

        return {
                retw: w,
                reth: h,
            };
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

    Component {
        id: delegate
        Item {
            width: container.width; height: container.height
            Rectangle {
                anchors.fill: parent
                color: imageSlideProperties.fillColor
                AnimatedImage {
                    x: hPos(imageSlideProperties.horizontalAlignment, parent.width, width)
                    y: vPos(imageSlideProperties.verticalAlignment, parent.height, height)
                    width: imageSize(parent.width, parent.height, sourceSize.width, sourceSize.height).retw
                    height: imageSize(parent.width, parent.height, sourceSize.width, sourceSize.height).reth
                    source: modelData
                    fillMode: Image.PreserveAspectFit
                }
            }
        }
    }

    PathView {
        id: view

        anchors.fill: parent
        model: imageSlideProperties.imageFiles

        delegate: delegate
        cacheItemCount: 2
        pathItemCount: 2
        path: direction2path(imageSlideProperties.direction)
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
        interval: speed2time(imageSlideProperties.speed)
        running: true
        repeat: true
        onTriggered: {
            if (imageSlideProperties.direction ===  3 || imageSlideProperties.direction ===  1)
                view.incrementCurrentIndex()
            else
                view.decrementCurrentIndex()
        }
    }
}
