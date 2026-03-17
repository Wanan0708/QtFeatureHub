// customtitlebar.cpp
#include "customtitlebar.h"
#include <QApplication>
#include <QStyle>
#include <QSpacerItem>

CustomTitleBar::CustomTitleBar(QWidget *parent)
    : QWidget(parent)
    , m_labelIcon(nullptr)
    , m_labelTitle(nullptr)
    , m_btnMinimize(nullptr)
    , m_btnMaximize(nullptr)
    , m_btnClose(nullptr)
    , m_isDragging(false)
    , m_titleBarHeight(40)
    , m_isMaximized(false)
    , m_iconSpacer(nullptr)
{
    setObjectName("customTitleBar");
    setAttribute(Qt::WA_StyledBackground, true);
    initUI();
    connectSignals();
}

CustomTitleBar::~CustomTitleBar()
{
}

void CustomTitleBar::initUI()
{
    setFixedHeight(m_titleBarHeight);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 0, 0);
    layout->setSpacing(0);
    
    // 图标
    m_labelIcon = new QLabel();
    m_labelIcon->setFixedSize(24, 24);
    m_labelIcon->setScaledContents(true);
    m_labelIcon->hide();
    layout->addWidget(m_labelIcon);
    // 可控的图标间隔，默认隐藏（宽度为0）
    m_iconSpacer = new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
    layout->addItem(m_iconSpacer);
    
    // 标题
    m_labelTitle = new QLabel();
    m_labelTitle->setStyleSheet("color: #ffffff; font-size: 13px;");
    m_labelTitle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_labelTitle->setMinimumWidth(0);
    layout->addWidget(m_labelTitle);
    
    // 按钮样式
    QString btnStyle = R"(
        QPushButton {
            background: transparent;
            border: none;
            min-width: 46px;
            min-height: 40px;
            padding: 0;
            color: #ffffff;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover {
            background: rgba(255, 255, 255, 0.1);
        }
        QPushButton:pressed {
            background: rgba(255, 255, 255, 0.2);
        }
    )";
    
    // 最小化按钮
    m_btnMinimize = new QPushButton("—");
    m_btnMinimize->setStyleSheet(btnStyle);
    m_btnMinimize->setCursor(Qt::PointingHandCursor);
    m_btnMinimize->setFixedHeight(m_titleBarHeight);
    m_btnMinimize->setFixedWidth(40);
    m_btnMinimize->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout->addWidget(m_btnMinimize);
    
    // 最大化按钮
    m_btnMaximize = new QPushButton("□");
    m_btnMaximize->setStyleSheet(btnStyle);
    m_btnMaximize->setCursor(Qt::PointingHandCursor);
    m_btnMaximize->setFixedHeight(m_titleBarHeight);
    m_btnMaximize->setFixedWidth(40);
    m_btnMaximize->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout->addWidget(m_btnMaximize);
    
    // 关闭按钮
    m_btnClose = new QPushButton("✕");
    m_btnClose->setStyleSheet(btnStyle + R"(
        QPushButton:hover {
            background: #e81123;
        }
    )");
    m_btnClose->setCursor(Qt::PointingHandCursor);
    m_btnClose->setFixedHeight(m_titleBarHeight);
    m_btnClose->setFixedWidth(40);
    m_btnClose->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout->addWidget(m_btnClose);
    
    // 默认样式
    // setBackgroundColor("#ffffff");
}

void CustomTitleBar::connectSignals()
{
    connect(m_btnMinimize, &QPushButton::clicked, this, &CustomTitleBar::minimizeClicked);
    connect(m_btnMaximize, &QPushButton::clicked, this, &CustomTitleBar::maximizeClicked);
    connect(m_btnClose, &QPushButton::clicked, this, &CustomTitleBar::closeClicked);
}

void CustomTitleBar::setTitle(const QString &title)
{
    m_labelTitle->setText(title);
}

void CustomTitleBar::setIcon(const QPixmap &icon)
{
    if (icon.isNull()) {
        m_labelIcon->clear();
        m_labelIcon->hide();
        if (m_iconSpacer) m_iconSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
    } else {
        m_labelIcon->setPixmap(icon);
        m_labelIcon->show();
        if (m_iconSpacer) m_iconSpacer->changeSize(8, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
    }
    // 使布局重新计算间隔
    if (layout()) layout()->invalidate();
}

void CustomTitleBar::setHeight(int height)
{
    m_titleBarHeight = height;
    setFixedHeight(height);
    if (m_btnMinimize) {
        m_btnMinimize->setFixedHeight(height);
    }
    if (m_btnMaximize) {
        m_btnMaximize->setFixedHeight(height);
    }
    if (m_btnClose) {
        m_btnClose->setFixedHeight(height);
    }
}

void CustomTitleBar::setBackgroundColor(const QString &color)
{
    setStyleSheet(QString("QWidget#customTitleBar { background-color: %1; }").arg(color));
}

void CustomTitleBar::setTitleColor(const QString &color)
{
    m_labelTitle->setStyleSheet(QString("color: %1; font-size: 13px;").arg(color));
}

void CustomTitleBar::setButtonVisible(bool minimize, bool maximize, bool close)
{
    m_btnMinimize->setVisible(minimize);
    m_btnMaximize->setVisible(maximize);
    m_btnClose->setVisible(close);
}

void CustomTitleBar::setMaximizedState(bool maximized)
{
    m_isMaximized = maximized;
    m_btnMaximize->setText(maximized ? "❐" : "□");
}

QWidget* CustomTitleBar::parentWindow() const
{
    QWidget *p = parentWidget();
    while (p && !p->isWindow()) {
        p = p->parentWidget();
    }
    return p;
}

void CustomTitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        QWidget *w = parentWindow();
        if (w) {
            m_dragPosition = event->globalPos() - w->frameGeometry().topLeft();
        }
    }
    QWidget::mousePressEvent(event);
}

void CustomTitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        QWidget *w = parentWindow();
        if (w) {
            // 如果窗口最大化，先恢复正常
            if (w->isMaximized()) {
                w->showNormal();
                setMaximizedState(false);
            }
            w->move(event->globalPos() - m_dragPosition);
        }
    }
    QWidget::mouseMoveEvent(event);
}

void CustomTitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_isDragging = false;
    QWidget::mouseReleaseEvent(event);
}

void CustomTitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit doubleClicked();
    }
    QWidget::mouseDoubleClickEvent(event);
}
