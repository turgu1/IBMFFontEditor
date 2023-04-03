#include "kerningItem.h"

#include <iostream>

#include <QApplication>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

KerningItem::KerningItem(KernEntry kernEntry, IBMFFontModPtr font, int faceIdx, QWidget *parent)
    : QWidget(parent), kernEntry_(kernEntry), font_(font) {

  frame_            = new QFrame(this);
  QFrame *editFrame = new QFrame(frame_);
  editFrame->setAccessibleName("editFrame");

  editFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  editFrame->setFrameShape(QFrame::Box);

  //  _editFrame->setAutoFillBackground(true);

  kerningRenderer_ = new KerningRenderer(editFrame, font_, faceIdx, &kernEntry_);

  // editFrame->setAutoFillBackground(true);

  QHBoxLayout *editFrameLayout = new QHBoxLayout();
  editFrameLayout->setContentsMargins(0, 0, 0, 0);
  editFrameLayout->addWidget(kerningRenderer_);
  editFrame->setLayout(editFrameLayout);

  QFrame *buttonsFrame = new QFrame(frame_);
  leftButton_          = new QPushButton("<", buttonsFrame);
  rightButton_         = new QPushButton(">", buttonsFrame);
  doneButton_          = new QPushButton("Done", buttonsFrame);
  kernLabel_           = new QLabel("%1", buttonsFrame);

  leftButton_->setMaximumWidth(40);
  rightButton_->setMaximumWidth(40);

  kernLabel_->setMinimumWidth(50);
  kernLabel_->setAlignment(Qt::AlignCenter);
  leftButton_->setEnabled(false);
  rightButton_->setEnabled(false);
  doneButton_->setEnabled(false);

  QObject::connect(leftButton_, &QPushButton::clicked, this, &KerningItem::onLeftButtonClicked);
  QObject::connect(rightButton_, &QPushButton::clicked, this, &KerningItem::onRightButtonClicked);
  QObject::connect(doneButton_, &QPushButton::clicked, this, &KerningItem::onDoneButtonClicked);

  QHBoxLayout *buttonsLayout = new QHBoxLayout();
  buttonsLayout->setContentsMargins(0, 0, 0, 0);
  buttonsLayout->addWidget(leftButton_);
  buttonsLayout->addStretch();
  buttonsLayout->addWidget(kernLabel_);
  buttonsLayout->addStretch();
  buttonsLayout->addWidget(rightButton_);
  buttonsLayout->addWidget(doneButton_);

  buttonsFrame->setLayout(buttonsLayout);

  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setContentsMargins(10, 10, 10, 10);
  mainLayout->addWidget(editFrame);
  mainLayout->addWidget(buttonsFrame);

  frame_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  frame_->setLayout(mainLayout);
  frame_->setAttribute(Qt::WA_DontShowOnScreen, true);

  std::cout << "Size: [" << width() << ", " << height() << "], Maximum Size: [" << maximumWidth()
            << ", " << maximumHeight() << "]" << std::endl;

  frame_->show();
}

void KerningItem::paint(QPainter *painter, const QRect &rect, const QPalette &palette,
                        EditMode mode) const {
  kernLabel_->setText(QString::number(kernEntry_.kern));

  painter->save();
  painter->translate(rect.topLeft());
  frame_->render(painter, QPoint(), QRegion(), QWidget::DrawChildren);
  painter->restore();
}

QSize KerningItem::sizeHint() const { return frame_->minimumSizeHint(); }

void KerningItem::editing() {
  leftButton_->setEnabled(true);
  rightButton_->setEnabled(true);
  doneButton_->setEnabled(true);
}

void KerningItem::onLeftButtonClicked() {
  kernEntry_.kern--;
  this->update();
}

void KerningItem::onRightButtonClicked() {
  kernEntry_.kern++;
  this->update();
}

void KerningItem::onDoneButtonClicked() {
  leftButton_->setEnabled(false);
  rightButton_->setEnabled(false);
  doneButton_->setEnabled(false);
  emit editingCompleted();
}
