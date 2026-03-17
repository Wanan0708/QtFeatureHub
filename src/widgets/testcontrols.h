#ifndef TESTCONTROLS_H
#define TESTCONTROLS_H

#include <QMainWindow>

namespace Ui { class TestControlsWindow; }

class TestControlsWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit TestControlsWindow(QWidget *parent = nullptr);
    ~TestControlsWindow() override;

private:
    Ui::TestControlsWindow *ui;
};

#endif // TESTCONTROLS_H
