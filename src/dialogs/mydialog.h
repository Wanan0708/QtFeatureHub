// mydialog.h
#ifndef MYDIALOG_H
#define MYDIALOG_H

#include <QDialog>

class CustomTitleBar;

QT_BEGIN_NAMESPACE
namespace Ui { class MyDialog; }
QT_END_NAMESPACE

class MyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MyDialog(QWidget *parent = nullptr);
    ~MyDialog();

private:
    Ui::MyDialog *ui;
    CustomTitleBar *m_titleBar;
};

#endif // MYDIALOG_H