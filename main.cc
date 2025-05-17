#include <sys/resource.h>

#include <QApplication>
#include <QTranslator>
#include <QTimer>
#include <QPixmap>
#include <QDateTime>

#include "system/hardware/hw.h"
#include "selfdrive/ui/qt/qt_window.h"
#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/qt/window.h"
#include "selfdrive/ui/touch_injector.h"

int main(int argc, char *argv[]) {
  setpriority(PRIO_PROCESS, 0, -20);

  qInstallMessageHandler(swagLogMessageHandler);
  initApp(argc, argv);

  QTranslator translator;
  QString translation_file = QString::fromStdString(Params().get("LanguageSetting"));
  if (!translator.load(QString(":/%1").arg(translation_file)) && translation_file.length()) {
    qCritical() << "Failed to load translation file:" << translation_file;
  }

  QApplication a(argc, argv);
  a.installTranslator(&translator);

  MainWindow w;
  setMainWindow(&w);
  a.installEventFilter(&w);

  // Set up screenshot timer (your existing code)
  QTimer screenshot_timer;
  QObject::connect(&screenshot_timer, &QTimer::timeout, [&w]() {
    QPixmap pixmap = w.grab();  // Capture the current window
    QString filename = QString("/tmp/ui_frame_%1.png").arg(QDateTime::currentSecsSinceEpoch());
    if (!pixmap.save(filename)) {
      qWarning() << "Failed to save screenshot to" << filename;
    } else {
      qDebug() << "Screenshot saved to" << filename;
    }
  });
  screenshot_timer.start(2000);  // 2 seconds for better responsiveness
  
  // Set up touch injector
  TouchInjector touchInjector(&w);
  
  qDebug() << "UI started with touch injection support";
  
  return a.exec();
}