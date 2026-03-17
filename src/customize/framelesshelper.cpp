// framelesshelper.cpp
#include "framelesshelper.h"
#include "customtitlebar.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

#include <QMainWindow>
#include <QLayout>
#include <QBoxLayout>
#include <QVBoxLayout>
#include <QSizePolicy>

FramelessHelper::FramelessHelper(QObject *parent)
    : QObject(parent)
{
}

FramelessHelper* FramelessHelper::instance()
{
    static FramelessHelper instance;
    return &instance;
}

void FramelessHelper::toggleMaximizeState(CustomTitleBar *titleBar, QWidget *window)
{
    if (!titleBar || !window) {
        return;
    }

    const bool shouldMaximize = !window->isMaximized();
    if (shouldMaximize) {
        window->showMaximized();
    } else {
        window->showNormal();
    }

    titleBar->setMaximizedState(shouldMaximize);
}

void FramelessHelper::setFrameless(QWidget *window)
{
    if (!window) return;
    
    window->setWindowFlags(window->windowFlags() | 
                           Qt::FramelessWindowHint | 
                           Qt::WindowMinMaxButtonsHint);
    window->setAttribute(Qt::WA_Hover, true);
    window->setMouseTracking(true);
}

CustomTitleBar* FramelessHelper::installTitleBar(QWidget *window,
                                                  const QString &title,
                                                  const QString &bgColor)
{
    if (!window) return nullptr;

    if (CustomTitleBar *existingTitleBar = m_titleBars.value(window, nullptr)) {
        existingTitleBar->setTitle(title.isEmpty() ? window->windowTitle() : title);
        existingTitleBar->setBackgroundColor(bgColor);
        return existingTitleBar;
    }
    
    QMainWindow *mainWindow = qobject_cast<QMainWindow*>(window);
    if (mainWindow) {
        // QMainWindow 使用 menuWidget 作为顶栏，避免破坏 centralWidget（尤其是 QOpenGLWidget）
        CustomTitleBar *titleBar = new CustomTitleBar(mainWindow);
        titleBar->setTitle(title.isEmpty() ? window->windowTitle() : title);
        titleBar->setBackgroundColor(bgColor);
        titleBar->setContentsMargins(0, 0, 0, 0);
        titleBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        mainWindow->setMenuWidget(titleBar);

        bindTitleBarSignals(titleBar, window);

        connect(window, &QObject::destroyed, this, [this, window]() {
            m_titleBars.remove(window);
        });
        connect(titleBar, &QObject::destroyed, this, [this, window]() {
            m_titleBars.remove(window);
        });

        m_titleBars[window] = titleBar;
        return titleBar;
    }

    QWidget *central = window;
    QLayout *oldLayout = central->layout();
    QWidget *container = central;

    // 创建并配置标题栏，使其成为 container 的子控件（确保宽度填充）
    CustomTitleBar *titleBar = new CustomTitleBar(container);
    titleBar->setTitle(title.isEmpty() ? window->windowTitle() : title);
    titleBar->setBackgroundColor(bgColor);
    titleBar->setContentsMargins(0, 0, 0, 0);
    titleBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    if (oldLayout) {
        // 非 QMainWindow：优先直接插入到现有顶层 box layout，避免重新包裹破坏布局。
        if (QBoxLayout *boxLayout = qobject_cast<QBoxLayout*>(oldLayout)) {
            boxLayout->setContentsMargins(0, 0, 0, 0);
            boxLayout->setSpacing(0);
            boxLayout->insertWidget(0, titleBar);
        } else {
            // 回退方案：创建外层布局容器。
            QWidget *wrapper = new QWidget(central);
            wrapper->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            QVBoxLayout *wrapperLayout = new QVBoxLayout(wrapper);
            wrapperLayout->setContentsMargins(0, 0, 0, 0);
            wrapperLayout->setSpacing(0);
            wrapperLayout->addWidget(titleBar);
            // 确保标题栏固定高度且内容在下面占据剩余空间
            wrapperLayout->setStretch(0, 0);

            while (QLayoutItem *item = oldLayout->takeAt(0)) {
                if (QWidget *w = item->widget()) {
                    w->setParent(wrapper);
                    wrapperLayout->addWidget(w);
                } else if (QLayout *sub = item->layout()) {
                    wrapperLayout->addLayout(sub);
                }
                delete item;
            }
            delete oldLayout;

            QVBoxLayout *outer = new QVBoxLayout(central);
            outer->setContentsMargins(0, 0, 0, 0);
            outer->setSpacing(0);
            outer->addWidget(wrapper, 1);
        }
    } else {
        QVBoxLayout *outer = new QVBoxLayout(central);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);
        outer->addWidget(titleBar);
    }
    
    // 绑定信号
    bindTitleBarSignals(titleBar, window);

    connect(window, &QObject::destroyed, this, [this, window]() {
        m_titleBars.remove(window);
    });
    connect(titleBar, &QObject::destroyed, this, [this, window]() {
        m_titleBars.remove(window);
    });
    
    // 保存映射
    m_titleBars[window] = titleBar;
    
    return titleBar;
}

void FramelessHelper::bindTitleBarSignals(CustomTitleBar *titleBar, QWidget *window)
{
    if (!titleBar || !window) return;
    
    connect(titleBar, &CustomTitleBar::minimizeClicked, window, &QWidget::showMinimized);
    
    connect(titleBar, &CustomTitleBar::maximizeClicked, [=]() {
        toggleMaximizeState(titleBar, window);
    });
    
    connect(titleBar, &CustomTitleBar::doubleClicked, [=]() {
        toggleMaximizeState(titleBar, window);
    });
    
    connect(titleBar, &CustomTitleBar::closeClicked, window, &QWidget::close);
}

bool FramelessHelper::handleNativeEvent(QWidget *window, 
                                         const QByteArray &eventType, 
                                         void *message, 
                                         long *result)
{
#ifdef Q_OS_WIN
    Q_UNUSED(eventType);
    MSG *msg = static_cast<MSG*>(message);
    if (msg->message == WM_NCHITTEST) {
        CustomTitleBar *titleBar = m_titleBars.value(window);
        if (titleBar) {
            int xPos = GET_X_LPARAM(msg->lParam) - window->frameGeometry().x();
            int yPos = GET_Y_LPARAM(msg->lParam) - window->frameGeometry().y();
            
            if (xPos >= 0 && xPos <= window->width() && 
                yPos >= 0 && yPos <= titleBar->height()) {
                *result = HTCAPTION;
                return true;
            }
        }
    }
#else
    Q_UNUSED(window);
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return false;
}