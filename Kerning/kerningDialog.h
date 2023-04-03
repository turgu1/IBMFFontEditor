#pragma once

#include <QDialog>
#include <QListView>

#include "../IBMFDriver/IBMFFontMod.hpp"
#include "kerningDelegate.h"
#include "kerningModel.h"

class KerningDialog : public QDialog {
public:
  KerningDialog(IBMFFontModPtr font, int faceIdx, KerningModel *model, QWidget *parent = nullptr);

private slots:
  void onAddButtonClicked();
  void onRemoveButtonClicked();
  void onOkButtonClicked();
  void onCancelButtonClicked();

private:
  IBMFFontModPtr   font_;
  KerningModel    *kerningModel_;
  QListView       *listView_;
  KerningDelegate *kerningDelegate_;
};
