#include "widgets/testcontrols.h"
#include "ui_testcontrols.h"
#include <QMenu>
#include <QAction>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QListWidget>
#include <QFormLayout>
#ifdef QAXSERVER
#include <QAxWidget>
#endif

TestControlsWindow::TestControlsWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::TestControlsWindow)
{
    ui->setupUi(this);
    // 丰富 ToolButton：添加下拉菜单
    if (ui->toolButton_tool) {
        QMenu *tbMenu = new QMenu(this);
        tbMenu->addAction("New");
        tbMenu->addAction("Open");
        tbMenu->addAction("Save");
        ui->toolButton_tool->setMenu(tbMenu);
        ui->toolButton_tool->setPopupMode(QToolButton::MenuButtonPopup);
    }

    // 丰富 ToolBox：添加两个示例页面
    if (ui->toolBox_toolbox) {
        QWidget *pageA = new QWidget();
        QVBoxLayout *layA = new QVBoxLayout(pageA);
        layA->addWidget(new QLabel("ToolBox Page A - quick actions"));
        QPushButton *actA = new QPushButton("Action A");
        layA->addWidget(actA);
        ui->toolBox_toolbox->addItem(pageA, "Page A");

        QWidget *pageB = new QWidget();
        QVBoxLayout *layB = new QVBoxLayout(pageB);
        QListWidget *list = new QListWidget();
        list->addItems({"Item 1", "Item 2", "Item 3"});
        layB->addWidget(list);
        ui->toolBox_toolbox->addItem(pageB, "Page B");
    }

    // 丰富 TabWidget：添加两个标签页
    if (ui->tabWidget_tabs) {
        QWidget *tab1 = new QWidget();
        QFormLayout *form = new QFormLayout(tab1);
        form->addRow("Name:", new QLineEdit());
        form->addRow("Email:", new QLineEdit());
        ui->tabWidget_tabs->addTab(tab1, "Profile");

        QWidget *tab2 = new QWidget();
        QVBoxLayout *tab2lay = new QVBoxLayout(tab2);
        QTextEdit *te = new QTextEdit();
        te->setPlainText("Notes...\nYou can type here.");
        tab2lay->addWidget(te);
        ui->tabWidget_tabs->addTab(tab2, "Notes");
    }

    // 如果系统支持 ActiveX（QAxWidget），用真正的 QAxWidget 替换占位标签
    // 如果系统支持 ActiveX（QAxWidget），用真正的 QAxWidget 替换占位标签
#ifdef QAXSERVER
    if (ui->label_ax) {
        QAxWidget *ax = new QAxWidget(ui->centralwidget);
        ax->setObjectName("axWidget_real");
        ax->setToolTip("QAxWidget (ActiveX)");
        // 替换布局中的占位 label
        QLayout *lay = ui->label_ax->parentWidget()->layout();
        if (lay) {
            lay->replaceWidget(ui->label_ax, ax);
        }
        ui->label_ax->hide();
    }
#endif
}

TestControlsWindow::~TestControlsWindow()
{
    delete ui;
}
