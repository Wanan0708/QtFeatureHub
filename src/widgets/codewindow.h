// codewindow.h
#ifndef CODEWINDOW_H
#define CODEWINDOW_H

#include "customize/framelesswindow.h"
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTableWidget>

class CodeWindow : public FramelessWindow
{
    Q_OBJECT

public:
    explicit CodeWindow(QWidget *parent = nullptr);
    ~CodeWindow();

private slots:
    void onAddClicked();
    void onDeleteClicked();

private:
    QLabel *m_label = nullptr;
    QLineEdit *m_lineEdit = nullptr;
    QPushButton *m_btnAdd = nullptr;
    QPushButton *m_btnDelete = nullptr;
    QTableWidget *m_tableWidget = nullptr;
    
    void setupUI();
};

#endif // CODEWINDOW_H
