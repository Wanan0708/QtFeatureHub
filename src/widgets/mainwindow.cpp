#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QDebug>
#include <QIntValidator>
#include <QAction>
#include <QApplication>
#include <QStyle>
#include "customize/framelesshelper.h"
#include "customize/customtitlebar.h"
#include "widgets/codewindow.h"
#include "widgets/openglwindow.h"
#include "widgets/testcontrols.h"
#include "dialogs/mydialog.h"
#include "database/databasemanager.h"
#include "widgets/pdfviewer.h"
#include <QCoreApplication>
#include <QDir>

namespace {

QTableWidgetItem *createClassmateItem(bool isClassmate)
{
    QTableWidgetItem *item = new QTableWidgetItem(isClassmate ? "是" : "否");
    item->setCheckState(isClassmate ? Qt::Checked : Qt::Unchecked);
    return item;
}

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_titleBar(nullptr)
{
    ui->setupUi(this);

    // 尝试连接本地 MySQL（默认配置，可在代码中修改）
    // 使用 SQLite 文件数据库，存放在可执行程序目录下
    QString dbPath = QCoreApplication::applicationDirPath() + QDir::separator() + "QtFeatureHub.db";
    bool dbOk = DatabaseManager::instance()->connectToFile(dbPath);
    if (!dbOk)
    {
        QMessageBox::information(this, "数据库", "无法打开本地 SQLite 数据库，应用将以本地内存表格运行。");
    }

    initTitleBar();
    initTableWidget();
    initInputWidgets();
    initConnections();

    // 从数据库加载已有记录
    if (DatabaseManager::instance()->isOpen()) {
        auto rows = DatabaseManager::instance()->loadAll();
        for (const QVariantMap &r : rows) {
            insertTableRow(r.value("isClassmate").toBool(), r.value("name").toString(), QString::number(r.value("age").toInt()));
            int row = ui->tableWidget->rowCount() - 1;
            if (ui->tableWidget->item(row, 0)) {
                ui->tableWidget->item(row, 0)->setData(Qt::UserRole, r.value("id").toLongLong());
            }
        }
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initTableWidget()
{
    ui->tableWidget->setColumnCount(3);
    ui->tableWidget->setHorizontalHeaderLabels(QStringList() << "isClassmate" << "Name" << "Age");

    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    ui->tableWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    ui->tableWidget->setColumnWidth(0, 80);
    ui->tableWidget->setColumnWidth(1, 100);
    ui->tableWidget->setColumnWidth(2, 100);
}

void MainWindow::initInputWidgets()
{
    QAction *clearNameAction = ui->lineEdit_name->addAction(QApplication::style()->standardIcon(QStyle::SP_LineEditClearButton), QLineEdit::TrailingPosition);
    clearNameAction->setToolTip("清除姓名输入");
    connect(clearNameAction, &QAction::triggered, ui->lineEdit_name, &QLineEdit::clear);

    QAction *clearAgeAction = ui->lineEdit_age->addAction(QApplication::style()->standardIcon(QStyle::SP_DialogResetButton), QLineEdit::TrailingPosition);
    clearAgeAction->setToolTip("清除年龄输入");
    connect(clearAgeAction, &QAction::triggered, ui->lineEdit_age, &QLineEdit::clear);

    ui->lineEdit_age->setValidator(new QIntValidator(0, 150, this));
}

void MainWindow::initConnections()
{
    connect(ui->tableWidget, &QTableWidget::itemChanged, this, &MainWindow::onTableWidgetItemChanged);

    connect(ui->pushButton_openCodeWindow, &QPushButton::clicked, this, [this]() {
        CodeWindow *codeWindow = new CodeWindow();
        codeWindow->setAttribute(Qt::WA_DeleteOnClose);
        codeWindow->move(this->geometry().center() - codeWindow->rect().center());
        codeWindow->show();
    });
    connect(ui->pushButton_openDialog, &QPushButton::clicked, this, [this]() {
        MyDialog dialog(this);
        dialog.setModal(true);
        if (dialog.exec() == QDialog::Accepted) {
            QString inputText = dialog.findChild<QLineEdit*>("lineEdit")->text();
            QMessageBox::information(this, "对话框输入", "你输入了: " + inputText);
        }
    });
    // UI contains pushButton_showTestUI added in designer; connect it to show TestControlsWindow
    connect(ui->pushButton_showTestUI, &QPushButton::clicked, this, [this]() {
        TestControlsWindow *w = new TestControlsWindow();
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->show();
    });
    connect(ui->pushButton_openPdf, &QPushButton::clicked, this, [this]() {
        PdfViewer *pv = new PdfViewer();
        pv->setAttribute(Qt::WA_DeleteOnClose);
        pv->show();
    });
}

void MainWindow::initTitleBar()
{
    FramelessHelper::instance()->setFrameless(this);
    m_titleBar = FramelessHelper::instance()->installTitleBar(
        this,
        "主窗口 - UI方式",
        "#4c5df5"
    );

    QIcon stdIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    m_titleBar->setIcon(stdIcon.pixmap(24, 24));
}

void MainWindow::insertTableRow(bool isClassmate, const QString &name, const QString &age)
{
    const int row = ui->tableWidget->rowCount();
    ui->tableWidget->insertRow(row);
    ui->tableWidget->setItem(row, 0, createClassmateItem(isClassmate));
    ui->tableWidget->setItem(row, 1, new QTableWidgetItem(name));
    ui->tableWidget->setItem(row, 2, new QTableWidgetItem(age));
}

void MainWindow::resetInputFields()
{
    ui->lineEdit_name->clear();
    ui->lineEdit_age->clear();
    ui->checkBox_isClassmate->setChecked(false);
}

void MainWindow::swapRows(int sourceRow, int targetRow)
{
    if (sourceRow == targetRow) {
        return;
    }

    for (int column = 0; column < ui->tableWidget->columnCount(); ++column) {
        QTableWidgetItem *sourceItem = ui->tableWidget->takeItem(sourceRow, column);
        QTableWidgetItem *targetItem = ui->tableWidget->takeItem(targetRow, column);

        if (sourceItem) {
            ui->tableWidget->setItem(targetRow, column, sourceItem);
        }
        if (targetItem) {
            ui->tableWidget->setItem(sourceRow, column, targetItem);
        }
    }
}

void MainWindow::on_pushButton_add_clicked()
{
    const bool isClassmate = ui->checkBox_isClassmate->isChecked();
    const QString name = ui->lineEdit_name->text().trimmed();
    const QString age = ui->lineEdit_age->text().trimmed();
    if (name.isEmpty() || age.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请输入姓名和年龄！");
        return;
    }

    qint64 id = -1;
    if (DatabaseManager::instance()->isOpen()) {
        id = DatabaseManager::instance()->addPerson(isClassmate, name, age.toInt());
        if (id < 0) {
            QMessageBox::warning(this, "数据库错误", "向数据库插入记录失败，已取消写入。");
            return;
        }
    }

    insertTableRow(isClassmate, name, age);
    int row = ui->tableWidget->rowCount() - 1;
    if (id > 0 && ui->tableWidget->item(row, 0)) {
        ui->tableWidget->item(row, 0)->setData(Qt::UserRole, id);
    }
    resetInputFields();
}

void MainWindow::on_pushButton_delete_clicked()
{
    const int currentRow = ui->tableWidget->currentRow();
    if (currentRow >= 0) {
        QTableWidgetItem *idItem = ui->tableWidget->item(currentRow, 0);
        qint64 id = -1;
        if (idItem && idItem->data(Qt::UserRole).isValid()) {
            id = idItem->data(Qt::UserRole).toLongLong();
        }

        if (id > 0 && DatabaseManager::instance()->isOpen()) {
            if (!DatabaseManager::instance()->removePersonById(id)) {
                QMessageBox::warning(this, "数据库错误", "从数据库删除记录失败。");
                return;
            }
        }

        ui->tableWidget->removeRow(currentRow);
    } else {
        QMessageBox::warning(this, "删除错误", "请先选择要删除的行！");
    }
}

void MainWindow::on_pushButton_up_clicked()
{
    const int currentRow = ui->tableWidget->currentRow();
    if (currentRow <= 0) return;

    const int targetRow = currentRow - 1;
    swapRows(currentRow, targetRow);
    ui->tableWidget->setCurrentCell(targetRow, 0);
}

void MainWindow::on_pushButton_down_clicked()
{
    const int currentRow = ui->tableWidget->currentRow();
    if (currentRow < 0 || currentRow >= ui->tableWidget->rowCount() - 1) return;

    const int targetRow = currentRow + 1;
    swapRows(currentRow, targetRow);
    ui->tableWidget->setCurrentCell(targetRow, 0);
}

void MainWindow::on_pushButton_init_clicked()
{
    OpenGLTestWindow *glWin = new OpenGLTestWindow();
    glWin->setAttribute(Qt::WA_DeleteOnClose);
    glWin->resize(800, 600);
    glWin->show();
}

void MainWindow::onTableWidgetItemChanged(QTableWidgetItem *item)
{
    if (!item || item->column() != 0) {
        return;
    }

    const bool isClassmate = item->checkState() == Qt::Checked;
    const QString targetText = isClassmate ? "是" : "否";

    if (item->text() != targetText) {
        ui->tableWidget->blockSignals(true);
        item->setText(targetText);
        ui->tableWidget->blockSignals(false);
    }
}
