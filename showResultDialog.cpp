#include "showResultDialog.h"

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>

#include "ui_showResultDialog.h"
ShowResultDialog::ShowResultDialog(const QString &title, const QString &baseName,
                                   const QString &content, QWidget *parent)
    : QDialog(parent), ui(new Ui::ShowResultDialog), baseName_(baseName) {

  ui->setupUi(this);

  setWindowTitle(title);
  ui->contentPlainText->setPlainText(content);
}

ShowResultDialog::~ShowResultDialog() { delete ui; }

void ShowResultDialog::on_okButton_clicked() { accept(); }

void ShowResultDialog::on_saveAsButton_clicked() {

  QSettings settings("ibmf", "IBMFEditor");

  QString logPath = settings.value("logFolder", settings.value("ibmfFolder").toString()).toString();

  QString newFilePath = QFileDialog::getSaveFileName(this, "Save Result to File",
                                                     logPath + "/" + baseName_ + ".txt", "*.txt");

  if (!newFilePath.isEmpty()) {
    QFileInfo fi(newFilePath);
    settings.setValue("logFolder", fi.absoluteFilePath());

    QFile outFile;
    outFile.setFileName(newFilePath);
    if (outFile.open(QIODevice::WriteOnly)) {
      outFile.write(ui->contentPlainText->toPlainText().toStdString().c_str(),
                    ui->contentPlainText->toPlainText().length());
      outFile.close();
      QMessageBox::information(this, "Content Saved", "Content saved to file: " + newFilePath);
    } else {
      QMessageBox::warning(this, "Error", "Unable to create file " + newFilePath);
    }
  }
}
