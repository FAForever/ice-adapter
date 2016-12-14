#pragma once
#include <QtWidgets/QGroupBox>
#include "ui_PeerWidget.h"

namespace faf {

class PeerWidget : public QGroupBox
{
  Q_OBJECT

public:
  explicit PeerWidget(QWidget *parent = 0);
  ~PeerWidget();

  Ui::PeerWidget *ui;
protected:
  void changeEvent(QEvent *e);
};


} // namespace faf
