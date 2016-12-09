#pragma once

#include <memory>

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QListWidgetItem>
#include <QtWidgets/QTableWidget>
#include <QtCore/QProcess>

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
    ColumnRemoteCand
  };

public:
  explicit Testclient(QWidget *parent = 0);
  ~Testclient();

protected Q_SLOTS:
  void on_pushButton_hostGame_clicked();
  void on_pushButton_leave_clicked();
  void on_listWidget_games_itemClicked(QListWidgetItem *item);

protected:
  void connectRpcMethods();
  void updateStatus();
  void startIceAdapter();
  void startGpgnetClient();
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
  int mGameId;
  QProcess mIceAdapterProcess;
  GPGNetClient mGpgClient;
  Json::Value mStatus;
  int mGpgnetPort;
  int mIcePort;
};


} // namespace faf
