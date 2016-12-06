#pragma once

#include "GPGNetMessage.h"

#include <QtNetwork/QTcpSocket>

namespace faf
{

class GPGNetClient : public QTcpSocket
{
  Q_OBJECT
public:
  GPGNetClient();

Q_SIGNALS:
  void onGPGNetMessage(GPGNetMessage const& msg);

protected:
  void onReadyRead();
  void onConnected();
  std::vector<char> mMessage;
};

}
