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

  KerningItem(KernEntry kernEntry, IBMFFontMod *font, int faceIdx, QWidget *parent = nullptr);
  KerningItem() {}

  void  paint(QPainter *painter, const QRect &rect, const QPalette &palette, EditMode mode) const;
  QSize sizeHint() const;

  KernEntry kernEntry() const { return kernEntry_; }

  void setKernEntry(KernEntry kernEntry) { kernEntry_ = kernEntry; }

private slots:
  void onLeftButtonClicked();
  void onRightButtonClicked();

private:
  KernEntry    kernEntry_;
  IBMFFontMod *font_;

  QFrame          *frame_;
  QLabel          *kernLabel_;
  KerningRenderer *kerningRenderer_;
};

Q_DECLARE_METATYPE(KerningItem)
