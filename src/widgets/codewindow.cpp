// codewindow.cpp
#include "codewindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDateTime>
#include <QTableWidgetItem>

CodeWindow::CodeWindow(QWidget *parent)
    : FramelessWindow(parent)
{
    // 设置窗口属性
    setTitle("纯代码窗口 - 继承方式");
    setTitleBarColor("#28a745");  // 绿色标题栏
    
    // 设置内容
    setupUI();

    setMinimumSize(640, 480);
    resize(680, 520);
}

CodeWindow::~CodeWindow()
{
}

void CodeWindow::setupUI()
{
    // 创建内容 Widget
    QWidget *content = new QWidget();
    content->setStyleSheet("background: #ffffff;");
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(15);
    
    // 标签
    m_label = new QLabel("这是纯代码创建的窗口（继承 FramelessWindow）");
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setStyleSheet("font-size: 16px; color: #333;");
    layout->addWidget(m_label);
    
    // 输入框
    m_lineEdit = new QLineEdit();
    m_lineEdit->setPlaceholderText("请输入姓名...");
    m_lineEdit->setStyleSheet(R"(
        QLineEdit {
            padding: 8px;
            border: 1px solid #ccc;
            border-radius: 4px;
        }
        QLineEdit:focus {
            border: 1px solid #4a9eff;
        }
    )");
    layout->addWidget(m_lineEdit);
    
    // 按钮布局
    QHBoxLayout *btnLayout = new QHBoxLayout();
    
    m_btnAdd = new QPushButton("添加");
    m_btnAdd->setStyleSheet(R"(
        QPushButton {
            padding: 8px 20px;
            background: #4a9eff;
            color: white;
            border: none;
            border-radius: 4px;
        }
        QPushButton:hover {
            background: #3a8eef;
        }
    )");
    btnLayout->addWidget(m_btnAdd);
    
    m_btnDelete = new QPushButton("删除选中");
    m_btnDelete->setStyleSheet(R"(
        QPushButton {
            padding: 8px 20px;
            background: #dc3545;
            color: white;
            border: none;
            border-radius: 4px;
        }
        QPushButton:hover {
            background: #c82333;
        }
    )");
    btnLayout->addWidget(m_btnDelete);
    btnLayout->addStretch();
    
    layout->addLayout(btnLayout);
    
    // 表格
    m_tableWidget = new QTableWidget();
    m_tableWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_tableWidget->setColumnCount(3);
    m_tableWidget->setHorizontalHeaderLabels({"选择", "姓名", "时间"});
    m_tableWidget->setColumnWidth(0, 60);
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    m_tableWidget->setAlternatingRowColors(true);
    layout->addWidget(m_tableWidget);
    layout->setStretch(3, 1);
    
    // 设置内容到窗口
    setContentWidget(content);
    
    // 连接信号
    connect(m_btnAdd, &QPushButton::clicked, this, &CodeWindow::onAddClicked);
    connect(m_btnDelete, &QPushButton::clicked, this, &CodeWindow::onDeleteClicked);
}

void CodeWindow::onAddClicked()
{
    QString name = m_lineEdit->text().trimmed();
    if (name.isEmpty()) return;
    
    int row = m_tableWidget->rowCount();
    m_tableWidget->insertRow(row);
    
    // 复选框
    QTableWidgetItem *checkItem = new QTableWidgetItem();
    checkItem->setCheckState(Qt::Unchecked);
    m_tableWidget->setItem(row, 0, checkItem);
    
    // 姓名
    m_tableWidget->setItem(row, 1, new QTableWidgetItem(name));
    
    // 时间
    m_tableWidget->setItem(row, 2, new QTableWidgetItem(
        QDateTime::currentDateTime().toString("hh:mm:ss")
    ));
    
    m_lineEdit->clear();
}

void CodeWindow::onDeleteClicked()
{
    for (int row = m_tableWidget->rowCount() - 1; row >= 0; --row) {
        QTableWidgetItem *item = m_tableWidget->item(row, 0);
        if (item && item->checkState() == Qt::Checked) {
            m_tableWidget->removeRow(row);
        }
    }
}
