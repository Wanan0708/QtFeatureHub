#ifndef OPENGLWINDOW_H
#define OPENGLWINDOW_H

#include <QMainWindow>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>

class QTimer;

class SimpleGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit SimpleGLWidget(QWidget *parent = nullptr);
    ~SimpleGLWidget() override;

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private slots:
    void onAnimate();

private:
    QTimer *m_timer;
    float m_phase;
};

class OpenGLTestWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit OpenGLTestWindow(QWidget *parent = nullptr);
    ~OpenGLTestWindow() override;
};

#endif // OPENGLWINDOW_H
