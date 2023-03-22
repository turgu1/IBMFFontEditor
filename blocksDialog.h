#pragma once

#include <freetype/freetype.h>

#include <QCheckBox>
#include <QDialog>
#include <QSet>

#include "Unicode/UBlocks.hpp"
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_TYPES_H
#include FT_OUTLINE_H
#include FT_RENDER_H

namespace Ui {
class BlocksDialog;
}

class BlocksDialog : public QDialog {
  Q_OBJECT

public:
  typedef QSet<int>             SelectedBlockIndexes;
  typedef SelectedBlockIndexes *SelectedBlockIndexesPtr;

  explicit BlocksDialog(QString fontFile, QString fontName,
                        QWidget *parent = nullptr);
  SelectedBlockIndexesPtr getSelectedBlockIndexes() {
    return selectedBlockIndexes_;
  }
  CodePointBlocksPtr getCodePointBlocks() { return codePointBlocks_; }

  ~BlocksDialog();

private slots:
  void tableSectionClicked(int idx);
  void cbClicked(bool checked);
  void on_createButton_clicked();
  void on_backButton_clicked();

private:
  Ui::BlocksDialog *ui;

  FT_Library              ftLib_;
  FT_Face                 ftFace_;
  bool                    allChecked_;
  int                     codePointQty_;
  SelectedBlockIndexesPtr selectedBlockIndexes_;
  CodePointBlocksPtr      codePointBlocks_;

  void checkCreateReady();
  void updateQtyLabel();
  void saveData();
};
