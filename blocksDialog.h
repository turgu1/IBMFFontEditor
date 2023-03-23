#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QSet>

#include "Unicode/UBlocks.hpp"
#include "freeType.h"

namespace Ui {
class BlocksDialog;
}

class BlocksDialog : public QDialog {
  Q_OBJECT

public:
  explicit BlocksDialog(FreeType &ft, QString fontFile, QString fontName,
                        QWidget *parent = nullptr);
  SelectedBlockIndexesPtr getSelectedBlockIndexes() { return selectedBlockIndexes_; }
  CodePointBlocksPtr      getCodePointBlocks() { return codePointBlocks_; }

  ~BlocksDialog();

private slots:
  void tableSectionClicked(int idx);
  void cbClicked(bool checked);
  void on_createButton_clicked();
  void on_backButton_clicked();

private:
  Ui::BlocksDialog *ui;
  FreeType          ft_;

  FT_Face                 face_;
  bool                    allChecked_;
  int                     codePointQty_;
  SelectedBlockIndexesPtr selectedBlockIndexes_;
  CodePointBlocksPtr      codePointBlocks_;

  void checkCreateReady();
  void updateQtyLabel();
  void saveData();
};
