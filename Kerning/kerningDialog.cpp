#include "kerningDialog.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QListView>
#include <QPushButton>
#include <QVBoxLayout>

#include "../IBMFDriver/IBMFFontMod.hpp"
#include "../characterSelector.h"
#include "kerningDelegate.h"
#include "kerningModel.h"

KerningDialog::KerningDialog(IBMFFontModPtr font, int faceIdx, KerningModel *model, QWidget *parent)
    : QDialog(parent), font_(font), kerningModel_(model) {

  setWindowTitle(QString("Kerning Table for letter '%1'")
                     .arg(QChar(font_->getUTF32(kerningModel_->getGlyphCode()))));
  setMinimumSize(QSize(300, 600));

  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  listView_ = new QListView;

  listView_->setModel(kerningModel_);
  listView_->setItemDelegate(new KerningDelegate(font_, faceIdx));

  QFrame      *buttonsFace   = new QFrame();
  QHBoxLayout *buttonsLayout = new QHBoxLayout(buttonsFace);
  QPushButton *okButton      = new QPushButton("Ok");
  QPushButton *cancelButton  = new QPushButton("Cancel");
  QPushButton *addButton     = new QPushButton("+");
  QPushButton *removeButton  = new QPushButton("-");

  addButton->setMaximumWidth(30);
  removeButton->setMaximumWidth(30);

  buttonsLayout->setContentsMargins(0, 10, 0, 10);

  buttonsLayout->addWidget(addButton);
  buttonsLayout->addWidget(removeButton);
  buttonsLayout->addStretch();
  buttonsLayout->addWidget(okButton);
  buttonsLayout->addWidget(cancelButton);

  mainLayout->addWidget(listView_);
  mainLayout->addWidget(buttonsFace);

  this->setLayout(mainLayout);

  QObject::connect(okButton, &QPushButton::clicked, this, &KerningDialog::onOkButtonClicked);
  QObject::connect(cancelButton, &QPushButton::clicked, this,
                   &KerningDialog::onCancelButtonClicked);
  QObject::connect(addButton, &QPushButton::clicked, this, &KerningDialog::onAddButtonClicked);
  QObject::connect(removeButton, &QPushButton::clicked, this,
                   &KerningDialog::onRemoveButtonClicked);
}

void KerningDialog::onAddButtonClicked() {
  char32_t ch = font_->getUTF32(kerningModel_->getGlyphCode());

  CharacterSelector *charSelector = new CharacterSelector(
      font_->characterCodes(), QString("New Kerning Entry for '%1'").arg(QChar(ch)),
      QString("Select the character that follows '%1'").arg(QChar(ch)), this);

  if (charSelector->exec() == QDialog::Accepted) {
    int row = kerningModel_->rowCount();
    kerningModel_->addKernEntry(
        (KernEntry(kerningModel_->getGlyphCode(), charSelector->selectedCharIndex(), 0)));
    listView_->setCurrentIndex(kerningModel_->index(row, 0));
  }
}

void KerningDialog::onRemoveButtonClicked() {}

void KerningDialog::onOkButtonClicked() {
  accept();
}

void KerningDialog::onCancelButtonClicked() {
  reject();
}
