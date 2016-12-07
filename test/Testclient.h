#pragma once

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QListWidgetItem>
#include <QtCore/QProcess>

#include <memory>

#include "test/JsonRpcClient.h"
#include "test/GPGNetClient.h"
#include "IceAdapter.h"


namespace faf {

namespace Ui {
class Testclient;
}

class Testclient : public QMainWindow
{
  Q_OBJECT
  enum Column_t
  {
    ColumnPlayer,
    ColumnState,
    ColumnPing,
    ColumnConntime,
    ColumnLocalCand,
    ColumnRemoteCand,
    ColumnLocalSdp,
    ColumnRemoteSdp
  };

public:
  explicit Testclient(QWidget *parent = 0);
  ~Testclient();

protected Q_SLOTS:
  void on_pushButton_hostGame_clicked(bool startHosting);
  void on_listWidget_games_itemClicked(QListWidgetItem *item);

protected:
  void connectRpcMethods();
  void updateStatus();
  void startIceAdapter();
  void onIceOutput();
  void onIceError();
  void changeEvent(QEvent *e);
  void onGPGNetMessageFromIceAdapter(GPGNetMessage const& msg);

private:
  Ui::Testclient *mUi;
  JsonRpcClient mServerClient;
  JsonRpcClient mIceClient;
  int mPlayerId;
  QString mPlayerLogin;
  QString mHostedGameName;
  QProcess mIceAdapterProcess;
  GPGNetClient mGpgClient;
  Json::Value mStatus;
};


} // namespace faf
