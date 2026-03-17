// framelesshelper.h
#ifndef FRAMELESSHELPER_H
#define FRAMELESSHELPER_H

#include <QObject>
#include <QWidget>
#include <QMap>

class CustomTitleBar;

class FramelessHelper : public QObject
{
    Q_OBJECT

public:
    static FramelessHelper* instance();
    
    // 为窗口添加自定义标题栏
    CustomTitleBar* installTitleBar(QWidget *window,
                                     const QString &title = "",
                                     const QString &bgColor = "#4c5df5");
    
    // 设置窗口为无边框
    void setFrameless(QWidget *window);
    
    // 绑定标题栏信号到窗口
    void bindTitleBarSignals(CustomTitleBar *titleBar, QWidget *window);
    
    // 处理 Windows Aero Snap
    bool handleNativeEvent(QWidget *window, const QByteArray &eventType, 
                          void *message, long *result);

private:
    explicit FramelessHelper(QObject *parent = nullptr);
    void toggleMaximizeState(CustomTitleBar *titleBar, QWidget *window);
    
    // 存储窗口和标题栏的对应关系
    QMap<QWidget*, CustomTitleBar*> m_titleBars;
};

#endif // FRAMELESSHELPER_H