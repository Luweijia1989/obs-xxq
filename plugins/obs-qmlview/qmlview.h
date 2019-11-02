#pragma once

#include <QtQuick>
#include <QtWidgets/QtWidgets>
#include <QtQuickWidgets/QQuickWidget>

#include <QMutex>
#include <QPointer>

class WindowSingleThreaded;
class FrameCounter;

#include <obs-module.h>
#include <obs-source.h>
#include <util/dstr.h>
#include <graphics/graphics.h>

class OBSQuickview : public QObject {
	Q_OBJECT
public:
	QUrl m_source;

	QSize m_size;
	QImage m_canvas;
	bool m_enabled;
	bool m_ready;

	// Or... just... shared context stuff:
	GLuint m_texid;
	quint8 *m_bits;
	bool m_delete;

public:
	OBSQuickview(QObject *parent = NULL);
	~OBSQuickview();

	WindowSingleThreaded *m_quickView;

	obs_source_t *source;
	gs_texture *texture;
	bool m_persistent;

	FrameCounter *m_renderCounter, *m_qmlFrameCounter;

	// Limit FPS to a specific rate:
	bool m_frameLimited;
	double m_nextFrame;
	quint32 m_fps;
	QString m_last_url;

	void obsshow();
	void obshide();
	bool obsdraw();
	void renderFrame(gs_effect_t *effect);

	void makeWidget();
	void makeTexture();
	void loadUrl(QUrl url);
	void resize(quint32 w, quint32 h);
	void tick(quint64 seconds);

	bool frameDue();    // is it time for a new frame?
	void frameSynced(); // mark last frame time as now

public slots:
	quint32 width();
	quint32 height();

signals:
	void wantLoad();
	void wantUnload();
	void wantResize(quint32 w, quint32 h);
	void frameRendered();
	void qmlWarnings(QStringList warnings);

private slots:
	void doLoad();
	void doUnload();
	void doResize(quint32 w, quint32 h);
	/*
    void qmlStatus(QQuickWidget::Status status);
    void qmlWarning(const QList<QQmlError> &warnings);
*/
	void qmlFrame();
	void qmlCheckFrame();
	void qmlCopy(GLuint textid);
};
