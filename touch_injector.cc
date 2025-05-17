#include "touch_injector.h"

#include <QApplication>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QWidget>
#include <QWindow>
#include <QTextStream>
#include <QFile>
#include <QDateTime>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <cstring>

// Debug file logging
static QFile debugFile("/data/touch_debug.log");
static QTextStream debugStream;

static void writeDebug(const QString& message) {
    if (!debugFile.isOpen()) {
        debugFile.open(QIODevice::WriteOnly | QIODevice::Append);
        debugStream.setDevice(&debugFile);
    }
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    debugStream << "[" << timestamp << "] " << message << endl;
    debugStream.flush();
    
    // Also write to qDebug for standard logging
    qDebug() << message;
}

TouchInjector::TouchInjector(QWidget* target) : m_target(target), m_listening(false), m_isDragging(false) {
  // Initialize debug file
  writeDebug("=== TouchInjector Starting with Scroll Support ===");
  writeDebug(QString("Target widget: %1").arg((qintptr)m_target));
  writeDebug(QString("Target widget class: %1").arg(m_target->metaObject()->className()));
  
  // Set up Unix domain socket server
  unlink("/tmp/ui_touch_socket");
  
  struct sockaddr_un addr;
  m_socket = socket(AF_UNIX, SOCK_STREAM, 0);
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, "/tmp/ui_touch_socket", sizeof(addr.sun_path) - 1);
  
  if (bind(m_socket, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
    listen(m_socket, 5);
    
    // Start listening thread
    m_listening = true;
    m_thread = std::thread(&TouchInjector::listenForInputs, this);
    writeDebug("Touch injector socket created successfully at /tmp/ui_touch_socket");
  } else {
    writeDebug(QString("Failed to create touch injector socket: %1").arg(strerror(errno)));
  }
}

TouchInjector::~TouchInjector() {
  m_listening = false;
  if (m_thread.joinable()) {
    m_thread.join();
  }
  close(m_socket);
  unlink("/tmp/ui_touch_socket");
  writeDebug("=== TouchInjector Destroyed ===");
  debugFile.close();
}

void TouchInjector::listenForInputs() {
  writeDebug("Input listener thread started");
  
  while (m_listening) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(m_socket, &readfds);
    
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    
    int activity = select(m_socket + 1, &readfds, NULL, NULL, &timeout);
    
    if (activity > 0 && FD_ISSET(m_socket, &readfds)) {
      int client_socket = accept(m_socket, NULL, NULL);
      if (client_socket >= 0) {
        char buffer[2048] = {0};
        int bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
        
        if (bytes_read > 0) {
          writeDebug(QString("Received raw data: %1").arg(QString::fromLatin1(buffer, bytes_read)));
          
          QJsonDocument doc = QJsonDocument::fromJson(QByteArray(buffer, bytes_read));
          QJsonObject obj = doc.object();
          
          // Handle different input types
          QString type = obj["type"].toString();
          writeDebug(QString("Parsed input type: %1").arg(type));
          
          // Emit signal to inject on main thread
          if (type == "touch" || type == "click" || type == "tap") {
            // Legacy touch/click handling
            int x = obj["x"].toInt();
            int y = obj["y"].toInt();
            QMetaObject::invokeMethod(this, "injectTouch", Qt::QueuedConnection,
                                    Q_ARG(int, x), Q_ARG(int, y), Q_ARG(QString, type));
          } else {
            // New input handling
            QMetaObject::invokeMethod(this, "injectInput", Qt::QueuedConnection,
                                    Q_ARG(QJsonObject, obj));
          }
        }
        close(client_socket);
      }
    }
  }
  writeDebug("Input listener thread ended");
}

void TouchInjector::injectInput(const QJsonObject& inputData) {
  QString type = inputData["type"].toString();
  writeDebug(QString("=== Processing %1 Input ===").arg(type));
  
  if (type == "scroll") {
    int x = inputData["x"].toInt();
    int y = inputData["y"].toInt();
    double deltaY = inputData["deltaY"].toDouble();
    injectScroll(x, y, deltaY);
  } else if (type == "drag") {
    int startX = inputData["startX"].toInt();
    int startY = inputData["startY"].toInt();
    int currentX = inputData["x"].toInt();
    int currentY = inputData["y"].toInt();
    injectDrag(startX, startY, currentX, currentY);
  } else if (type == "dragend") {
    m_isDragging = false;
    writeDebug("Drag operation ended");
  } else if (type == "click" || type == "tap") {
    int x = inputData["x"].toInt();
    int y = inputData["y"].toInt();
    injectClick(x, y);
  } else if (type == "mousedown" || type == "touchstart") {
    int x = inputData["x"].toInt();
    int y = inputData["y"].toInt();
    m_dragStart = QPoint(x, y);
    writeDebug(QString("Started potential drag at (%1, %2)").arg(x).arg(y));
  }
}

void TouchInjector::injectScroll(int x, int y, double deltaY) {
  writeDebug(QString("Injecting scroll at (%1, %2) deltaY=%3").arg(x).arg(y).arg(deltaY));
  
  // Find the widget at this position
  QPoint localPos(x, y);
  QPoint globalPos = m_target->mapToGlobal(localPos);
  QWidget* widgetUnderPoint = QApplication::widgetAt(globalPos);
  
  if (widgetUnderPoint) {
    writeDebug(QString("Scrolling widget: %1").arg(widgetUnderPoint->metaObject()->className()));
    
    // Convert to local coordinates for the widget
    QPoint widgetLocalPos = widgetUnderPoint->mapFromGlobal(globalPos);
    
    // Create wheel event - positive deltaY = scroll down, negative = scroll up
    QWheelEvent wheelEvent(widgetLocalPos, globalPos, QPoint(0, 0), QPoint(0, int(deltaY * 15)),
                          Qt::NoButton, Qt::NoModifier, Qt::ScrollPhase::NoScrollPhase, false);
    
    bool result = QApplication::sendEvent(widgetUnderPoint, &wheelEvent);
    writeDebug(QString("Wheel event sent, result: %1").arg(result));
    
    // Alternative: Also try sending to the main target
    QWheelEvent wheelEvent2(localPos, globalPos, QPoint(0, 0), QPoint(0, int(deltaY * 15)),
                           Qt::NoButton, Qt::NoModifier, Qt::ScrollPhase::NoScrollPhase, false);
    QApplication::sendEvent(m_target, &wheelEvent2);
  } else {
    writeDebug("No widget found under scroll position");
  }
}

void TouchInjector::injectDrag(int startX, int startY, int currentX, int currentY) {
  writeDebug(QString("Injecting drag from (%1, %2) to (%3, %4)")
             .arg(startX).arg(startY).arg(currentX).arg(currentY));
  
  if (!m_isDragging) {
    // Start drag with mouse press
    m_isDragging = true;
    QPoint startPos(startX, startY);
    QPoint globalStartPos = m_target->mapToGlobal(startPos);
    
    QMouseEvent pressEvent(QEvent::MouseButtonPress, startPos, globalStartPos,
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(m_target, &pressEvent);
    writeDebug("Started drag with mouse press");
  }
  
  // Send mouse move event
  QPoint currentPos(currentX, currentY);
  QPoint globalCurrentPos = m_target->mapToGlobal(currentPos);
  
  QMouseEvent moveEvent(QEvent::MouseMove, currentPos, globalCurrentPos,
                       Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(m_target, &moveEvent);
  writeDebug(QString("Sent mouse move to (%1, %2)").arg(currentX).arg(currentY));
  
  // Also try sending to widget under cursor
  QWidget* widgetUnderPoint = QApplication::widgetAt(globalCurrentPos);
  if (widgetUnderPoint && widgetUnderPoint != m_target) {
    QPoint widgetLocalPos = widgetUnderPoint->mapFromGlobal(globalCurrentPos);
    QMouseEvent moveEvent2(QEvent::MouseMove, widgetLocalPos, globalCurrentPos,
                          Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(widgetUnderPoint, &moveEvent2);
  }
}

void TouchInjector::injectClick(int x, int y) {
  writeDebug(QString("Injecting click at (%1, %2)").arg(x).arg(y));
  
  // Create local and global positions
  QPoint localPos(x, y);
  QPoint globalPos = m_target->mapToGlobal(localPos);
  
  // Find widget under point
  QWidget* widgetUnderPoint = QApplication::widgetAt(globalPos);
  QWidget* targetWidget = widgetUnderPoint ? widgetUnderPoint : m_target;
  
  if (widgetUnderPoint) {
    localPos = widgetUnderPoint->mapFromGlobal(globalPos);
    writeDebug(QString("Clicking on widget: %1").arg(widgetUnderPoint->metaObject()->className()));
  }
  
  // Create and send mouse events
  QMouseEvent* pressEvent = new QMouseEvent(QEvent::MouseButtonPress, localPos, globalPos,
                                           Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QMouseEvent* releaseEvent = new QMouseEvent(QEvent::MouseButtonRelease, localPos, globalPos,
                                             Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
  
  bool pressResult = QApplication::sendEvent(targetWidget, pressEvent);
  writeDebug(QString("Press event sent, result: %1").arg(pressResult));
  
  // Schedule release event
  QTimer::singleShot(50, [=]() {
    bool releaseResult = QApplication::sendEvent(targetWidget, releaseEvent);
    writeDebug(QString("Release event sent, result: %1").arg(releaseResult));
    delete pressEvent;
    delete releaseEvent;
  });
}

// Legacy method for backward compatibility
void TouchInjector::injectTouch(int x, int y, const QString& type) {
  writeDebug(QString("Legacy touch injection: %1 at (%2, %3)").arg(type).arg(x).arg(y));
  injectClick(x, y);
}