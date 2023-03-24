#pragma once

#include <QDialog>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>

#include "IBMFDriver/ibmf_defs.hpp"

class CharacterViewer : public QDialog {
  Q_OBJECT
public:
  CharacterViewer(const IBMFDefs::CharCodes *chars, QString title = nullptr, QString info = nullptr,
                  QWidget *parent = nullptr);
private slots:
  void onOk(bool checked = false);

private:
  int           _columnCount;
  QTableWidget *_charsTable;
  QPushButton  *_okButton;
};
