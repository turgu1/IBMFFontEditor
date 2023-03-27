#include "characterViewer.h"

#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

CharacterViewer::CharacterViewer(const IBMFDefs::CharCodes *chars, QString title, QString info,
                                 QWidget *parent)
    : QDialog(parent) {

  setModal(true);
  setWindowTitle(title == nullptr ? "Character Selector" : title);
  setMinimumHeight(600);
  setMinimumWidth(500);

  QFont charsTableFont("Tahoma");
  charsTableFont.setPointSize(16);
  charsTableFont.setBold(true);

  QFont fnt;
  fnt.setPointSize(14);
  fnt.setFamily("Arial");

  _charsTable                = new QTableWidget();
  _okButton                  = new QPushButton("Ok");

  QVBoxLayout *mainLayout    = new QVBoxLayout();
  QLabel      *subTitle      = new QLabel(info == nullptr ? "Please select a character" : info);
  QHBoxLayout *buttonsLayout = new QHBoxLayout();
  subTitle->setMinimumHeight(60);
  subTitle->setFont(fnt);
  subTitle->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);

  QFrame *buttonsFrame = new QFrame();
  buttonsLayout->addStretch();
  buttonsLayout->addWidget(_okButton);
  buttonsFrame->setLayout(buttonsLayout);

  mainLayout->addWidget(subTitle);
  mainLayout->addWidget(_charsTable);
  mainLayout->addWidget(buttonsFrame);

  this->setLayout(mainLayout);

  _charsTable->setFont(charsTableFont);
  _charsTable->verticalHeader()->setDefaultSectionSize(50);
  _charsTable->horizontalHeader()->setDefaultSectionSize(50);
  _charsTable->horizontalHeader()->hide();
  _charsTable->verticalHeader()->hide();

  _columnCount = _charsTable->width() / 50;
  int rowCount = (chars->size() + _columnCount - 1) / _columnCount;
  _charsTable->setColumnCount(_columnCount);
  _charsTable->setRowCount(rowCount);
  _charsTable->setSelectionBehavior(QAbstractItemView::SelectItems);
  _charsTable->setSelectionMode(QAbstractItemView::SingleSelection);

  fnt.setPointSize(18);

  int idx = 0;

  for (int row = 0; row < rowCount; row++) {
    for (int col = 0; col < _columnCount; col++, idx++) {
      auto item = new QTableWidgetItem;
      item->setFlags(item->flags() & ~Qt::ItemIsEditable);
      item->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
      item->setSizeHint(QSize(50, 50));
      item->setFont(fnt);
      if (idx < chars->size()) {
        item->setData(Qt::EditRole, QChar((*chars)[idx]));

        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        item->setToolTip(
            QString("Index: %1, Unicode: U+%2").arg(idx).arg((*chars)[idx], 4, 16, QChar('0')));
      } else {
        item->setFlags(Qt::NoItemFlags);
      }
      _charsTable->setItem(row, col, item);
    }
  }
  QHeaderView *header = _charsTable->horizontalHeader();
  header->setSectionResizeMode(QHeaderView::Stretch);

  QObject::connect(_okButton, &QPushButton::clicked, this, &CharacterViewer::onOk);
}

void CharacterViewer::onOk(bool checked) { accept(); }
