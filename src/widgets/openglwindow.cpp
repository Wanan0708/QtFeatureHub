#include "widgets/openglwindow.h"
#include "customize/framelesshelper.h"
#include "customize/customtitlebar.h"
#include <QApplication>
#include <QStyle>
#include <QDebug>
#include <QTimer>
#include <QtMath>
#include <QOpenGLFunctions>

// SimpleGLWidget
SimpleGLWidget::SimpleGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , m_timer(new QTimer(this))
    , m_phase(0.0f)
{
    connect(m_timer, &QTimer::timeout, this, &SimpleGLWidget::onAnimate);
    m_timer->start(16);
}

SimpleGLWidget::~SimpleGLWidget()
{
}

void SimpleGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void SimpleGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void SimpleGLWidget::paintGL()
{
    float r = 0.5f + 0.5f * qSin(m_phase);
    float g = 0.5f + 0.5f * qSin(m_phase + 2.0f);
    float b = 0.5f + 0.5f * qSin(m_phase + 4.0f);
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void SimpleGLWidget::onAnimate()
{
    m_phase += 0.04f;
    update();
}

// OpenGLTestWindow
OpenGLTestWindow::OpenGLTestWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // create GL widget first so installTitleBar can find central widget
    SimpleGLWidget *w = new SimpleGLWidget(this);
    setCentralWidget(w);

    // make window frameless and install custom title bar
    FramelessHelper::instance()->setFrameless(this);
    CustomTitleBar *titleBar = FramelessHelper::instance()->installTitleBar(
        this,
        "OpenGL Test Window",
        "#4c5df5"
    );
    QPixmap pixmap(":/images/open_globe_title.png");
    if (titleBar) {
        if (!pixmap.isNull()) {
            titleBar->setIcon(pixmap.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            qDebug() << "[OpenGLTestWindow] failed to load resource :/images/open_globe_title.png";
            QIcon stdIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
            titleBar->setIcon(stdIcon.pixmap(24, 24));
        }
    }
}

OpenGLTestWindow::~OpenGLTestWindow()
{
}
