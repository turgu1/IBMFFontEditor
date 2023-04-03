#pragma once

#include <QFrame>
#include <QLabel>
#include <QPainter>
#include <QSize>

#include "../IBMFDriver/IBMFFontMod.hpp"
#include "kerningModel.h"
#include "kerningRenderer.h"

class KerningItem : public QWidget {
  Q_OBJECT
public:
  enum class EditMode { Editable, ReadOnly };

  KerningItem(KernEntry kernEntry, IBMFFontModPtr font, int faceIdx, QWidget *parent = nullptr);
  KerningItem() {}

  void  paint(QPainter *painter, const QRect &rect, const QPalette &palette, EditMode mode) const;
  QSize sizeHint() const;

  KernEntry kernEntry() const { return kernEntry_; }

  void setKernEntry(KernEntry kernEntry) { kernEntry_ = kernEntry; }
  void editing();

private slots:
  void onLeftButtonClicked();
  void onRightButtonClicked();
  void onDoneButtonClicked();

signals:
  void editingCompleted();

private:
  KernEntry      kernEntry_;
  IBMFFontModPtr font_;

  QFrame          *frame_;
  QLabel          *kernLabel_;
  KerningRenderer *kerningRenderer_;

  QPushButton *leftButton_;
  QPushButton *rightButton_;
  QPushButton *doneButton_;
};

Q_DECLARE_METATYPE(KerningItem)
