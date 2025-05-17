#pragma once

#include <QObject>
#include <QWidget>
#include <QString>
#include <QTimer>
#include <QJsonObject>
#include <thread>

// Touch and scroll event injector class
class TouchInjector : public QObject {
  Q_OBJECT

public:
  TouchInjector(QWidget* target);
  ~TouchInjector();

public slots:
  void injectTouch(int x, int y, const QString& type);
  void injectInput(const QJsonObject& inputData);

private:
  void listenForInputs();
  void injectScroll(int x, int y, double deltaY);
  void injectDrag(int startX, int startY, int currentX, int currentY);
  void injectClick(int x, int y);

  QWidget* m_target;
  int m_socket;
  bool m_listening;
  std::thread m_thread;
  
  // Drag state
  bool m_isDragging;
  QPoint m_dragStart;
};