// mydialog.cpp
#include "mydialog.h"
#include "ui_mydialog.h"
#include "customize/framelesshelper.h"
#include "customize/customtitlebar.h"

#include <QMessageBox>

MyDialog::MyDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MyDialog)
    , m_titleBar(nullptr)
{
    ui->setupUi(this);
    
    // 设置无边框并安装标题栏
    FramelessHelper::instance()->setFrameless(this);
    m_titleBar = FramelessHelper::instance()->installTitleBar(
        this, 
        "对话框 - UI方式",
        "#4a9eff"  // 蓝色标题栏
    );
    
    // 隐藏最大化按钮（对话框通常不需要）
    m_titleBar->setButtonVisible(true, false, true);
    
    // 设置固定大小
    setFixedSize(400, 300);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, [=]{
        // 这里可以添加一些验证逻辑
        if (ui->lineEdit->text().isEmpty()) {
            QMessageBox::warning(this, "输入错误", "请输入一些内容！");
            return;
        }
        accept();
    });
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &MyDialog::reject);
}

MyDialog::~MyDialog()
{
    delete ui;
}
