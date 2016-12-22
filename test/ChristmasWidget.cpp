#include "ChristmasWidget.h"

#include <QtCore/QXmlStreamWriter>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QBuffer>
#include <QtCore/QFile>

#include <QtWidgets/QHBoxLayout>

#include "logging.h"

namespace faf {

ChristmasWidget::ChristmasWidget(QWidget *parent) : QWidget(parent)
{
  this->setLayout(new QHBoxLayout(this));

  for(int i = 0; i < 11; ++i)
  {
    mAvailableRedBulbs.insert(QString("red%1").arg(i));
    mAvailableGreenBulbs.insert(QString("green%1").arg(i));
  }

  this->update();
  mSvgWidget.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  this->layout()->addWidget(&mSvgWidget);
}

void ChristmasWidget::update()
{
  QByteArray filteredSvgData;
  {
    QSet<QString> invisibleBulbs(mAvailableRedBulbs);
    invisibleBulbs.unite(mAvailableGreenBulbs);
    QBuffer buffer(&filteredSvgData);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);

    QFile xmlFile(":ChristmasWidget.svg");
    xmlFile.open(QIODevice::ReadOnly);
    QXmlStreamReader reader(&xmlFile);

     while (!reader.atEnd())
     {
       reader.readNext();
       if (!(reader.isStartElement() &&
            (invisibleBulbs.contains(reader.attributes().value("id").toString()))))
       {
         writer.writeCurrentToken(reader);
       }
       else
       {
         while(!reader.isEndElement())
         {
          reader.readNext();
         }
       }
     }
  }

  mSvgWidget.load(filteredSvgData);
}

void ChristmasWidget::onPingStats(int peerId, float ping, int pendPings, int lostPings, int succPings)
{
  if (!mPeerBulb.contains(peerId))
  {
    switchRed(peerId);
  }
  if (!mPeerSuccPings.contains(peerId))
  {
    mPeerSuccPings[peerId] = succPings;
  }

  if (succPings > mPeerSuccPings[peerId])
  {
    switchGreen(peerId);
  }
  else
  {
    switchRed(peerId);
  }
  mPeerSuccPings[peerId] = succPings;
  update();
}

void ChristmasWidget::switchOff(int peerId)
{
  if (mPeerBulb.contains(peerId))
  {
    if (mPeerBulb.value(peerId).startsWith("green"))
    {
      mAvailableGreenBulbs.insert(mPeerBulb.value(peerId));
    }
    else
    {
      mAvailableRedBulbs.insert(mPeerBulb.value(peerId));
    }
    mPeerBulb.remove(peerId);
  }
  update();
}

void ChristmasWidget::clear()
{
  for (int id: mPeerBulb.keys())
  {
    switchOff(id);
  }
  mPeerBulb.clear();
  mPeerSuccPings.clear();
  update();
}

void ChristmasWidget::switchGreen(int peerId)
{
  if (mPeerBulb.contains(peerId))
  {
    if (mPeerBulb.value(peerId).startsWith("red"))
    {
      mAvailableRedBulbs.insert(mPeerBulb.value(peerId));
    }
    else
    {
      return;
    }
  }
  if (mAvailableGreenBulbs.isEmpty())
  {
    FAF_LOG_ERROR << "not enough bulbs";
    return;
  }
  mPeerBulb[peerId] = *mAvailableGreenBulbs.begin();
  mAvailableGreenBulbs.erase(mAvailableGreenBulbs.begin());
}

void ChristmasWidget::switchRed(int peerId)
{
  if (mPeerBulb.contains(peerId))
  {
    if (mPeerBulb.value(peerId).startsWith("green"))
    {
      mAvailableGreenBulbs.insert(mPeerBulb.value(peerId));
    }
    else
    {
      return;
    }
  }
  if (mAvailableRedBulbs.isEmpty())
  {
    FAF_LOG_ERROR << "not enough bulbs";
    return;
  }
  mPeerBulb[peerId] = *mAvailableRedBulbs.begin();
  mAvailableRedBulbs.erase(mAvailableRedBulbs.begin());
}


} // namespace faf
