// framelesswindow.cpp
#include "framelesswindow.h"
#include "framelesshelper.h"

#include <QLayout>
#include <QLayoutItem>
#include <QSizePolicy>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

void clearLayout(QLayout *layout)
{
    if (!layout) {
        return;
    }

    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }

        if (QLayout *childLayout = item->layout()) {
            clearLayout(childLayout);
            delete childLayout;
        }

        delete item;
    }
}

}

FramelessWindow::FramelessWindow(QWidget *parent)
    : QWidget(parent)
    , m_titleBar(nullptr)
    , m_contentArea(nullptr)
{
    // 设置无边框
    FramelessHelper::instance()->setFrameless(this);
    
    // 主布局
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    
    // 创建标题栏
    m_titleBar = new CustomTitleBar(this);
    m_mainLayout->addWidget(m_titleBar);
    
    // 内容区域
    m_contentArea = new QWidget();
    m_contentArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_mainLayout->addWidget(m_contentArea);
    m_mainLayout->setStretch(1, 1);
    
    // 绑定信号
    FramelessHelper::instance()->bindTitleBarSignals(m_titleBar, this);
}

FramelessWindow::~FramelessWindow()
{
}

void FramelessWindow::setTitle(const QString &title)
{
    m_titleBar->setTitle(title);
}

void FramelessWindow::setTitleBarColor(const QString &color)
{
    m_titleBar->setBackgroundColor(color);
}

void FramelessWindow::setContentWidget(QWidget *widget)
{
    // 清除旧内容和可能存在的嵌套布局/子控件
    QLayout *oldLayout = m_contentArea->layout();
    if (oldLayout) {
        clearLayout(oldLayout);
        delete oldLayout;
    }
    
    // 设置新内容
    QVBoxLayout *layout = new QVBoxLayout(m_contentArea);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    if (widget) {
        layout->addWidget(widget);
    }
}

CustomTitleBar* FramelessWindow::titleBar() const
{
    return m_titleBar;
}

bool FramelessWindow::nativeEvent(const QByteArray &eventType, 
                                   void *message, 
                                   long *result)
{
    if (FramelessHelper::instance()->handleNativeEvent(this, eventType, message, result)) {
        return true;
    }
    return QWidget::nativeEvent(eventType, message, result);
}