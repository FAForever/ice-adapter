#pragma once

#include <QtCore/QSet>
#include <QtWidgets/QWidget>
#include <QtGui/QResizeEvent>
#include <QtSvg/QSvgWidget>

namespace faf {

class ChristmasWidget : public QSvgWidget
{
  Q_OBJECT
public:
  explicit ChristmasWidget(QWidget *parent = 0);
  void update();
  void onPingStats(int peerId, float ping, int pendPings, int lostPings, int succPings);
  void switchOff(int peerId);
  void clear();
  QSize sizeHint() const override;
protected:
  void resizeEvent(QResizeEvent * event) override;
  void switchGreen(int peerId);
  void switchRed(int peerId);
  QHash<int, QString> mPeerBulb;
  QHash<int, int> mPeerSuccPings;
  QSet<QString> mAvailableRedBulbs;
  QSet<QString> mAvailableGreenBulbs;
  int mLastHeight;
};

} // namespace faf
