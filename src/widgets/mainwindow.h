// mainwindow.h (moved to widgets)
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidgetItem>

class CustomTitleBar;
class QFileSystemModel;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pushButton_init_clicked();
    void on_pushButton_add_clicked();
    void on_pushButton_delete_clicked();
    void on_pushButton_up_clicked();
    void on_pushButton_down_clicked();
    void onTableWidgetItemChanged(QTableWidgetItem *item);

private:
    void initTableWidget();
    void initInputWidgets();
    void initConnections();
    void initTitleBar();
    void insertTableRow(bool isClassmate, const QString &name, const QString &age);
    void resetInputFields();
    void swapRows(int sourceRow, int targetRow);

    Ui::MainWindow *ui;
    CustomTitleBar *m_titleBar;
};

#endif // MAINWINDOW_H
