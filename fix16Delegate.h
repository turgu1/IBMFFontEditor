#pragma once

#include <QDoubleSpinBox>
#include <QSharedPointer>
#include <QStyledItemDelegate>

// The following is to support Fix16 fields editing in QTableWidgets
class Fix16Delegate : public QStyledItemDelegate {
  Q_OBJECT
public:
  Fix16Delegate(QWidget *parent = nullptr) : QStyledItemDelegate(parent) {}
  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &style,
                        const QModelIndex &index) const {
    QDoubleSpinBox *box = new QDoubleSpinBox(parent);

    box->setDecimals(4);

    // you can also set these
    box->setSingleStep(0.01);
    box->setMinimum(0);
    box->setMaximum(1000);

    return box;
  }
};
