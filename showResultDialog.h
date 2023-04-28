#ifndef SHOWRESULTDIALOG_H
#define SHOWRESULTDIALOG_H

#include <QDialog>
#include <QTextStream>

namespace Ui {
class ShowResultDialog;
}

class ShowResultDialog : public QDialog {
  Q_OBJECT

public:
  explicit ShowResultDialog(const QString &title, const QString &baseName, const QString &content,
                            QWidget *parent = nullptr);
  ~ShowResultDialog();

private slots:
  void on_okButton_clicked();

  void on_saveAsButton_clicked();

private:
  Ui::ShowResultDialog *ui;

  const QString &baseName_;
};

#endif // SHOWRESULTDIALOG_H
