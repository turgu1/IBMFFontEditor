#include "blocksDialog.h"

#include <iostream>
#include <set>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QTableWidget>

#include "IBMFDriver/IBMFDefs.hpp"
#include "Unicode/UBlocks.hpp"
#include "characterViewer.h"
#include "ui_blocksDialog.h"

BlocksDialog::BlocksDialog(FreeType &ft, QString fontFile, QString fontName, QWidget *parent)
    : QDialog(parent), ft_(ft), fontName_(fontName), ui(new Ui::BlocksDialog) {
  ui->setupUi(this);

  ui->createButton->setEnabled(false);

  codePointBlocks_      = new CodePointBlocks;
  selectedBlockIndexes_ = new SelectedBlockIndexes;

  this->setWindowTitle("Font Content");
  ui->titleLabel->setText("CodePoint Selection from " + fontName);

  if ((face_ = ft.openFace(fontFile)) == nullptr) {
    reject();
    return;
  }

  if (face_->charmap == nullptr) {
    QMessageBox::warning(this, "Not Unicode", "There is no Unicode charmap in this font!");
    reject();
    return;
  }

  for (int n = 0; n < face_->num_charmaps; n++) {
    std::cout << "PlatformId: " << face_->charmap[n].platform_id
              << " EncodingId: " << face_->charmap[n].encoding_id << std::endl;
  }

  struct CustomCmp {
    bool operator()(const CodePointBlock *a, const CodePointBlock *b) const {
      return a->blockIdx_ < b->blockIdx_;
    }
  };

  std::set<CodePointBlock *, CustomCmp> blocksSet;

  FT_UInt  index;
  char32_t ch = FT_Get_First_Char(face_, &index);

  while (index != 0) {
    int idx;
    //    std::cout << "CodePoint: " << +ch << "(" << ch << ")" << std::endl;
    if ((ch >= 0x00021) && (ch != 0x000A0) && ((ch < 0x02000) || (ch >= 0x2010))) {
      if ((idx = UnicodeBlocs::findUBloc(ch)) != -1) {
        // std::cout << "block: " << uBlocks[idx].caption_ << std::endl;
        CodePointBlock blk(idx, 1);
        auto           it = blocksSet.find(&blk);
        if (it != blocksSet.end()) {
          (*it)->codePointCount_++;
        } else {
          blocksSet.insert(new CodePointBlock(idx, 1));
        }
      }
    }
    ch = FT_Get_Next_Char(face_, ch, &index);
  }

  codePointBlocks_->clear();
  codePointBlocks_->reserve(blocksSet.size());

  std::copy(blocksSet.begin(), blocksSet.end(), std::back_inserter(*codePointBlocks_));

  // Set header to allow the select column to be clickable
  // to set/unset all selections at once
  QHeaderView *headerView = ui->blocksTable->horizontalHeader();
  headerView->setSectionResizeMode(0, QHeaderView::Stretch);
  headerView->setSectionsClickable(true);

  QObject::connect(headerView, &QHeaderView::sectionClicked, this,
                   &BlocksDialog::tableSectionClicked);

  for (auto block : *codePointBlocks_) {
    int row = ui->blocksTable->rowCount();
    ui->blocksTable->insertRow(row);

    QTableWidgetItem *item = new QTableWidgetItem;
    item->setData(Qt::DisplayRole, uBlocks[block->blockIdx_].caption_);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    ui->blocksTable->setItem(row, 0, item);

    item = new QTableWidgetItem;
    item->setData(Qt::DisplayRole,
                  QString("U+%1").arg(uBlocks[block->blockIdx_].first_, 5, 16, QChar('0')));
    item->setTextAlignment(Qt::AlignHCenter);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    ui->blocksTable->setItem(row, 1, item);

    item = new QTableWidgetItem;
    item->setTextAlignment(Qt::AlignHCenter);
    item->setData(Qt::DisplayRole,
                  QString("U+%1").arg(int(uBlocks[block->blockIdx_].last_), 5, 16, QChar('0')));
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    ui->blocksTable->setItem(row, 2, item);

    item = new QTableWidgetItem;
    item->setData(Qt::DisplayRole, block->codePointCount_);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setTextAlignment(Qt::AlignHCenter);
    ui->blocksTable->setItem(row, 3, item);

    QWidget     *frame          = new QFrame();
    QCheckBox   *checkBox       = new QCheckBox();
    QHBoxLayout *layoutCheckBox = new QHBoxLayout(frame);
    layoutCheckBox->addWidget(checkBox);
    layoutCheckBox->setAlignment(Qt::AlignCenter);
    layoutCheckBox->setContentsMargins(0, 0, 0, 0);
    checkBox->setObjectName(QString("%1").arg(row));
    ui->blocksTable->setCellWidget(row, 4, frame);

    QObject::connect(checkBox, &QCheckBox::clicked, this, &BlocksDialog::cbClicked);
  }

  allChecked_   = false;
  codePointQty_ = 0;
  updateQtyLabel();
}

BlocksDialog::~BlocksDialog() { delete ui; }

void BlocksDialog::checkCreateReady() {
  ui->createButton->setEnabled(codePointQty_ > 0);
  if (ui->createButton->isEnabled()) ui->createButton->setFocus(Qt::OtherFocusReason);
}

void BlocksDialog::tableSectionClicked(int idx) {
  if (idx == 4) {
    allChecked_ = !allChecked_;
    for (int i = 0; i < ui->blocksTable->rowCount(); i++) {
      QFrame    *frame = (QFrame *)(ui->blocksTable->cellWidget(i, 4));
      QCheckBox *cb    = (QCheckBox *)(frame->layout()->itemAt(0)->widget());
      cb->setChecked(allChecked_);
    }
    codePointQty_ = allChecked_ ? face_->num_glyphs : 0;
    updateQtyLabel();
    checkCreateReady();
  }
}

void BlocksDialog::cbClicked(bool checked) {
  QCheckBox *sender = (QCheckBox *)QObject::sender();
  int        row    = sender->objectName().toInt();
  int        qty    = ui->blocksTable->item(row, 3)->data(Qt::DisplayRole).toInt();
  codePointQty_ += checked ? qty : -qty;
  updateQtyLabel();
  checkCreateReady();
}

void BlocksDialog::updateQtyLabel() {
  ui->qtyLabel->setText(QString("Number of CodePoints selected: %1").arg(codePointQty_));
}

void BlocksDialog::saveData() {
  selectedBlockIndexes_->clear();
  for (int row = 0; row < ui->blocksTable->rowCount(); row++) {
    QFrame    *frame = (QFrame *)(ui->blocksTable->cellWidget(row, 4));
    QCheckBox *cb    = (QCheckBox *)(frame->layout()->itemAt(0)->widget());
    if (cb->isChecked()) {
      selectedBlockIndexes_->insert((*codePointBlocks_)[row]->blockIdx_);
    }
  }
}

void BlocksDialog::on_createButton_clicked() {
  saveData();
  FT_Done_Face(face_);
  accept();
}

void BlocksDialog::on_backButton_clicked() {
  saveData();
  FT_Done_Face(face_);
  reject();
}

void BlocksDialog::on_blocksTable_cellDoubleClicked(int row, int column) {
  if (column == 0) {
    auto                 block     = (*codePointBlocks_)[row];
    char32_t             first     = uBlocks[block->blockIdx_].first_;
    char32_t             last      = uBlocks[block->blockIdx_].last_;
    IBMFDefs::CharCodes *charCodes = new IBMFDefs::CharCodes;

    for (char32_t ch = first; ch <= last; ch++) {
      if ((ch >= 0x0021) && (ch != 0x00A0) && ((ch < 0x02000) || (ch >= 0x2010))) {
        FT_UInt index = FT_Get_Char_Index(face_, ch);
        if (index != 0) {
          charCodes->push_back(ch);
        }
      }
    }
    CharacterViewer *viewer = new CharacterViewer(
        charCodes, fontName_,
        QString("Available characters in the block %1").arg(uBlocks[block->blockIdx_].caption_),
        this);
    viewer->exec();
  }
}
