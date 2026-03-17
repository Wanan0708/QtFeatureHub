#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QByteArray>
#include <string>

#include <poppler/cpp/poppler-global.h>

#include "widgets/mainwindow.h"

namespace {

void ignorePopplerDebugError(const std::string &, void *)
{
}

}

int main(int argc, char *argv[])
{
    const QStringList candidatePopplerDataDirs = {
        QStringLiteral("C:/msys64/mingw64/share/poppler"),
        QStringLiteral("C:/msys64/ucrt64/share/poppler"),
        QStringLiteral("C:/msys64/clang64/share/poppler")
    };
    for (const QString &dirPath : candidatePopplerDataDirs) {
        if (QFileInfo::exists(dirPath) && QDir(dirPath).exists()) {
            qputenv("POPPLER_DATADIR", QDir::toNativeSeparators(dirPath).toLocal8Bit());
            poppler::set_data_dir(QDir::toNativeSeparators(dirPath).toStdString());
            break;
        }
    }

    poppler::set_debug_error_function(ignorePopplerDebugError, nullptr);

    QApplication app(argc, argv);
    
    MainWindow window;
    window.show();
    
    return app.exec();
}