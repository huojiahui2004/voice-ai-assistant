#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    // Qt6 默认开启 High DPI,这里设置圆整策略,避免半像素模糊
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Voice AI Assistant"));
    app.setOrganizationName(QStringLiteral("LocalAI"));

    MainWindow w;
    w.show();

    return app.exec();
}
