#include "characterSelector.h"

#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

CharacterSelector::CharacterSelector(const IBMFDefs::CharCodes *chars, QString title, QString info,
                                     QWidget *parent)
    : QDialog(parent) {

  setModal(true);
  setWindowTitle(title == nullptr ? "Character Selector" : title);
  setMinimumHeight(800);
  setMinimumWidth(600);

  QFont fnt;
  fnt.setPointSize(14);
  fnt.setFamily("Arial");

  charsTable_ = new QTableWidget();
  okButton_   = new QPushButton("Ok");
  okButton_->setEnabled(false);

  QVBoxLayout *mainLayout    = new QVBoxLayout();
  QLabel      *subTitle      = new QLabel(info == nullptr ? "Please select a character" : info);
  QHBoxLayout *buttonsLayout = new QHBoxLayout();
  subTitle->setMinimumHeight(60);
  subTitle->setFont(fnt);
  subTitle->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);

  QFrame      *buttonsFrame = new QFrame();
  QPushButton *cancelButton = new QPushButton("Cancel", buttonsFrame);
  buttonsLayout->addStretch();
  buttonsLayout->addWidget(okButton_);
  buttonsLayout->addWidget(cancelButton);
  buttonsFrame->setLayout(buttonsLayout);

  mainLayout->addWidget(subTitle);
  mainLayout->addWidget(charsTable_);
  mainLayout->addWidget(buttonsFrame);

  this->setLayout(mainLayout);

  QFont charsTableFont("Tahoma");
  charsTableFont.setPointSize(16);
  charsTableFont.setBold(true);

  charsTable_->setFont(charsTableFont);
  charsTable_->verticalHeader()->setDefaultSectionSize(50);
  charsTable_->horizontalHeader()->setDefaultSectionSize(50);
  charsTable_->horizontalHeader()->hide();
  charsTable_->verticalHeader()->hide();

  columnCount_ = charsTable_->width() / 50;
  int rowCount = (chars->size() + columnCount_ - 1) / columnCount_;
  charsTable_->setColumnCount(columnCount_);
  charsTable_->setRowCount(rowCount);
  charsTable_->setSelectionBehavior(QAbstractItemView::SelectItems);
  charsTable_->setSelectionMode(QAbstractItemView::SingleSelection);

  fnt.setPointSize(18);

  int idx = 0;

  for (int row = 0; row < rowCount; row++) {
    for (int col = 0; col < columnCount_; col++, idx++) {
      auto item = new QTableWidgetItem;
      item->setFlags(item->flags() & ~Qt::ItemIsEditable);
      item->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
      item->setSizeHint(QSize(50, 50));
      item->setFont(fnt);
      if (idx < chars->size()) {
        item->setData(Qt::EditRole, QChar((*chars)[idx]));

        item->setToolTip(
            QString("Index: %1, Unicode: U+%2").arg(idx).arg((*chars)[idx], 4, 16, QChar('0')));
      } else {
        item->setFlags(Qt::NoItemFlags);
      }
      charsTable_->setItem(row, col, item);
    }
  }
  QHeaderView *header = charsTable_->horizontalHeader();
  header->setSectionResizeMode(QHeaderView::Stretch);

  QObject::connect(charsTable_, &QTableWidget::doubleClicked, this,
                   &CharacterSelector::onDoubleClick);
  QObject::connect(charsTable_, &QTableWidget::itemSelectionChanged, this,
                   &CharacterSelector::onSelected);
  QObject::connect(okButton_, &QPushButton::clicked, this, &CharacterSelector::onOk);
  QObject::connect(cancelButton, &QPushButton::clicked, this, &CharacterSelector::onCancel);
}

void CharacterSelector::onDoubleClick(const QModelIndex &index) {
  auto items = charsTable_->selectedItems();
  if (!items.isEmpty()) {
    selectedCharIndex_ = items.first()->row() * columnCount_ + items.first()->column();
    accept();
  }
}

void CharacterSelector::onOk(bool checked) {
  auto items = charsTable_->selectedItems();
  if (!items.isEmpty()) {
    selectedCharIndex_ = items.first()->row() * columnCount_ + items.first()->column();
    accept();
  }
}

void CharacterSelector::onCancel(bool checked) { reject(); }

void CharacterSelector::onSelected() {
  okButton_->setEnabled(!charsTable_->selectedItems().isEmpty());
}
