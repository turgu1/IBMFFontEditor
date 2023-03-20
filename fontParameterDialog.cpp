#include "fontParameterDialog.h"
#include "ui_fontParameterDialog.h"

#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>

FontParameterDialog::FontParameterDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::FontParameterDialog) {
  ui->setupUi(this);

  QHBoxLayout *fontFilename = new QHBoxLayout();
  ibmfFontFilename_         = new QLineEdit();
  QPushButton *browse       = new QPushButton("...");
  browse->setMaximumWidth(50);
  fontFilename->addWidget(ibmfFontFilename_);
  fontFilename->addWidget(browse);

  QHBoxLayout *dpi = new QHBoxLayout();
  dpi75_           = new QRadioButton("75 dpi");
  dpi100_          = new QRadioButton("100 dpi");
  dpi120_          = new QRadioButton("120 dpi");
  dpi75_->setChecked(true);
  dpi->addWidget(dpi75_);
  dpi->addWidget(dpi100_);
  dpi->addWidget(dpi120_);

  QHBoxLayout *pointSizes = new QHBoxLayout();
  pt12_                   = new QCheckBox("12pt");
  pt14_                   = new QCheckBox("14pt");
  pt17_                   = new QCheckBox("17pt");
  pt14_->setChecked(true);
  pointSizes->addWidget(pt12_);
  pointSizes->addWidget(pt14_);
  pointSizes->addWidget(pt17_);

  QFormLayout *formLayout = new QFormLayout;
  formLayout->addRow(tr("&IBMF Font File Name:"), fontFilename);
  formLayout->addRow(tr("&Resolution:"), dpi);
  formLayout->addRow(tr("&Face Sizes:"), pointSizes);

  ui->mainFrame->setLayout(formLayout);

  QObject::connect(browse, &QPushButton::clicked, this, &FontParameterDialog::browseFontFilename);
}

FontParameterDialog::~FontParameterDialog() {
  delete ui;
}

void FontParameterDialog::on_buttonBox_accepted() {
  fontParameters_ = {.dpi      = dpi75_->isChecked()    ? 75
                                 : dpi100_->isChecked() ? 100
                                                        : 120,
                     .pt12     = pt12_->isChecked(),
                     .pt14     = pt14_->isChecked(),
                     .pt17     = pt17_->isChecked(),
                     .filename = ibmfFontFilename_->text()};
}

void FontParameterDialog::browseFontFilename() {
  QString newFilePath =
      QFileDialog::getSaveFileName(this, "New IBMF Font File", ibmfFontFilename_->text(), "*.ibmf");
  if (!newFilePath.isEmpty()) { ibmfFontFilename_->setText(newFilePath); }
}
