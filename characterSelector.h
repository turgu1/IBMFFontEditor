#pragma once

#include <QDialog>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>

#include "IBMFDriver/IBMFDefs.hpp"

class CharacterSelector : public QDialog {
  Q_OBJECT
public:
  CharacterSelector(const IBMFDefs::CharCodes *chars, QString title = nullptr,
                    QString info = nullptr, QWidget *parent = nullptr);
  int selectedCharIndex() { return selectedCharIndex_; }

private slots:
  void onDoubleClick(const QModelIndex &index);
  void onOk(bool checked = false);
  void onCancel(bool checked = false);
  void onSelected();

private:
  int           selectedCharIndex_{-1};
  int           columnCount_;
  QTableWidget *charsTable_;
  QPushButton  *okButton_;
};
