#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QSet>

#include "Unicode/UBlocks.hpp"

typedef std::function<bool(char32_t *, bool)> GetNextUTF32;
typedef std::function<bool(char32_t, bool)>   IsAvailable;
namespace Ui {
class BlocksDialog;
}

class BlocksDialog : public QDialog {
  Q_OBJECT

public:
  explicit BlocksDialog(GetNextUTF32 nextUTF32, IsAvailable isAvailable, QString fontName,
                        QWidget *parent = nullptr);
  SelectedBlockIndexesPtr getSelectedBlockIndexes() { return selectedBlockIndexes_; }
  CodePointBlocksPtr      getCodePointBlocks() { return codePointBlocks_; }

  ~BlocksDialog();

private slots:
  void tableSectionClicked(int idx);
  void cbClicked(bool checked);
  void on_createButton_clicked();
  void on_backButton_clicked();

  void on_blocksTable_cellDoubleClicked(int row, int column);

private:
  Ui::BlocksDialog *ui;

  QString                 fontName_;
  bool                    allChecked_;
  int                     codePointQty_;
  SelectedBlockIndexesPtr selectedBlockIndexes_;
  CodePointBlocksPtr      codePointBlocks_;
  IsAvailable             isAvailable_;

  void checkCreateReady();
  void updateQtyLabel();
  void saveData();
};
