#pragma once

#include <QDialog>
#include <QListView>

#include "../IBMFDriver/ibmf_font_mod.hpp"
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
  IBMFFontModPtr font_;
  KerningModel  *_kerningModel;
  QListView     *_listView;
};
