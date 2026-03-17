// framelesswindow.h
#ifndef FRAMELESSWINDOW_H
#define FRAMELESSWINDOW_H

#include <QWidget>
#include <QVBoxLayout>
#include "customtitlebar.h"

class FramelessWindow : public QWidget
{
    Q_OBJECT

public:
    explicit FramelessWindow(QWidget *parent = nullptr);
    virtual ~FramelessWindow();
    
    // 设置方法
    void setTitle(const QString &title);
    void setTitleBarColor(const QString &color);
    void setContentWidget(QWidget *widget);
    CustomTitleBar* titleBar() const;

protected:
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;

private:
    CustomTitleBar *m_titleBar;
    QWidget *m_contentArea;
    QVBoxLayout *m_mainLayout;
};

#endif // FRAMELESSWINDOW_H