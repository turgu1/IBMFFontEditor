#pragma once

#include <freetype/freetype.h>

#include <QCheckBox>
#include <QDialog>
#include <QSet>
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
  explicit BlocksDialog(QString fontFile, QString fontName, QWidget *parent = nullptr);
  const QSet<int> &getSelectedBlockIndexes() { return selectedBlockIndexes_; }
  ~BlocksDialog();

private slots:
  void tableSectionClicked(int idx);
  void cbClicked(bool checked);
  void on_buttonBox_accepted();

  void on_buttonBox_rejected();

private:
  struct Block {
    int blockIdx_;
    int codePointCount_;
    Block(int idx, int count) : blockIdx_(idx), codePointCount_(count) {}
  };

  Ui::BlocksDialog *ui;

  FT_Library           ftLib_;
  FT_Face              ftFace_;
  bool                 allChecked_;
  int                  codePointQty_;
  QSet<int>            selectedBlockIndexes_;
  std::vector<Block *> blocks_;

  void updateQtyLabel();
};
