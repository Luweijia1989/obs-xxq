/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#pragma once

#include <QWindow>
#include <QMatrix4x4>
#include <QTimer>
#include <QMutex>
#include <QThread>
#include <QImage>
#include <QOpenGLTexture>
#include <QUrl>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>

QT_FORWARD_DECLARE_CLASS(QOpenGLContext)
QT_FORWARD_DECLARE_CLASS(QOpenGLFramebufferObject)
QT_FORWARD_DECLARE_CLASS(QOffscreenSurface)
QT_FORWARD_DECLARE_CLASS(QQuickRenderControl)
QT_FORWARD_DECLARE_CLASS(QQuickWindow)
QT_FORWARD_DECLARE_CLASS(QQmlEngine)
QT_FORWARD_DECLARE_CLASS(QQmlError)
QT_FORWARD_DECLARE_CLASS(QQmlComponent)
QT_FORWARD_DECLARE_CLASS(QQuickItem)

class RenderControl;

#include <QTimer>
#include <QElapsedTimer>

class FrameCounter : public QObject
{
    Q_OBJECT

    QString         m_myname;
    quint32         m_count;
    QElapsedTimer   m_timer;
    QTimer          m_printer;

public:
    FrameCounter(const QString &name) : QObject(), m_myname(name), m_count(0)
    {
        m_printer.setInterval(30000);
        m_printer.setSingleShot(false);
        connect( &m_printer, SIGNAL(timeout()), this, SLOT(printFPS()) );
        m_printer.start();
        m_timer.start();
    }

public slots:
    void inc() { m_count++; }
    void printFPS() {
        double fps = (double) m_count / ( (double)m_timer.elapsed() / 1000.0 );
        //qDebug() << m_myname << ": FPS = " << fps;
        m_count = 0;
        m_timer.restart();
    }
};

class WindowSingleThreaded : public QObject, protected QOpenGLFunctions
{
    Q_OBJECT

    QUrl        m_url;
    QStringList m_loadMessages;

public:
    WindowSingleThreaded(QObject *parent = nullptr);
    ~WindowSingleThreaded();

    QQmlEngine *engine() { return m_qmlEngine; }
    QSize quickWindowSize();

    Q_PROPERTY(QVariant query READ getQuery)

public slots:
    void startQuick(const QUrl &url);
    void resize(QSize newSize);
    void manualUpdated(bool onoff);
    void render();
    void triggerSnap();
    void unload();

    QImage *getImage();
    QSize fboSize();

    bool initialised() { return m_quickInitialized; }
    QVariant getQuery();

    //qreal calculateDelta(quint64 duration, qreal min, qreal max);

    void sendMouseClick(quint32 x, quint32 y, qint32 type, bool onoff, quint8 click_count);
    void sendMouseMove(quint32 x, quint32 y, bool leaving);
    void sendMouseWheel(qint32 xdelta, qint32 ydelta);
    void sendKey(quint32 keycode, quint32 vkey, quint32 modifiers, const char *text, bool updown);
    void sendFocus(bool onoff);

    void wantFrame();

    void addContextProperties(QString name, QObject *value);

signals:
    void capped(quint8* imageData);
    void resized();
    void messages(const QStringList &messages);
    void snapWanted();

private slots:
    void run();

    void createFbo();
    void destroyFbo();
    void handleScreenChange();
    void handleWarnings(const QList<QQmlError> &warnings);
    void handleFocusChanged(QObject *obj);
    void createPBO();
    void deletePBO();

private:
    void updateSizes();
    void resizeFbo();

    QImage m_image;
    bool m_frameChanged;
    bool m_forceRender;
    QOpenGLContext *m_context;
    QOffscreenSurface *m_offscreenSurface;
    RenderControl *m_renderControl;
    QQuickWindow *m_quickWindow;
    QQmlEngine *m_qmlEngine;
    QQmlComponent *m_qmlComponent;
    QQuickItem *m_rootItem;
    QOpenGLFramebufferObject *m_fbo;
    QObject *m_currentFocus;
    bool m_quickInitialized;
    bool m_quickReady;
    QTimer m_updateTimer;
    QMap<QString, QObject*> m_context_properties;
    QVector<QOpenGLBuffer*> m_pbos;
    QTimer m_pboTimer;
    int m_pboIndex = 0;
};
