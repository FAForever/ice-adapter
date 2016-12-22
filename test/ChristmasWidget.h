#pragma once

#include <QtCore/QSet>
#include <QtWidgets/QWidget>
#include <QtSvg/QSvgWidget>

namespace faf {

class ChristmasWidget : public QWidget
{
  Q_OBJECT
public:
  explicit ChristmasWidget(QWidget *parent = 0);
  void update();
  void onPingStats(int peerId, float ping, int pendPings, int lostPings, int succPings);
  void switchOff(int peerId);
  void clear();
protected:
  void switchGreen(int peerId);
  void switchRed(int peerId);
  QHash<int, QString> mPeerBulb;
  QHash<int, int> mPeerSuccPings;
  QSvgWidget mSvgWidget;
  QSet<QString> mAvailableRedBulbs;
  QSet<QString> mAvailableGreenBulbs;
};

} // namespace faf
