// customtitlebar.h
#ifndef CUSTOMTITLEBAR_H
#define CUSTOMTITLEBAR_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QSpacerItem>

class CustomTitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit CustomTitleBar(QWidget *parent = nullptr);
    ~CustomTitleBar();
    
    // 设置方法
    void setTitle(const QString &title);
    void setIcon(const QPixmap &icon);
    void setHeight(int height);
    void setBackgroundColor(const QString &color);
    void setTitleColor(const QString &color);
    void setButtonVisible(bool minimize, bool maximize, bool close);
    void setMaximizedState(bool maximized);
    
    // 获取所属窗口
    QWidget *parentWindow() const;

signals:
    void minimizeClicked();
    void maximizeClicked();
    void closeClicked();
    void doubleClicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void initUI();
    void connectSignals();

    QLabel *m_labelIcon;
    QLabel *m_labelTitle;
    QPushButton *m_btnMinimize;
    QPushButton *m_btnMaximize;
    QPushButton *m_btnClose;
    
    QPoint m_dragPosition;
    bool m_isDragging;
    int m_titleBarHeight;
    bool m_isMaximized;
    QSpacerItem *m_iconSpacer;
};

#endif // CUSTOMTITLEBAR_H
