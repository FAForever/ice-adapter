#include "PeerWidget.h"

namespace faf {

PeerWidget::PeerWidget(QWidget *parent) :
  QGroupBox(parent),
  ui(new Ui::PeerWidget)
{
  ui->setupUi(this);
}

PeerWidget::~PeerWidget()
{
  delete ui;
}

void PeerWidget::changeEvent(QEvent *e)
{
  QGroupBox::changeEvent(e);
  switch (e->type()) {
    case QEvent::LanguageChange:
      ui->retranslateUi(this);
      break;
    default:
      break;
  }
}

} // namespace faf
