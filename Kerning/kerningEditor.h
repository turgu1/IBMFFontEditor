#pragma once

#include <QWidget>

#include "kerningItem.h"

class KerningEditor : public QWidget {
  Q_OBJECT
public:
  KerningEditor(QWidget *parent = nullptr);
  ~KerningEditor() {}

  QSize        sizeHint() const override;
  void         setKerningItem(KerningItem *kerningItem) { _kerningItem = kerningItem; }
  KerningItem *kerningItem() { return _kerningItem; }

signals:
  void editingFinished();

protected:
  void paintEvent(QPaintEvent *event) override;
  //  void mouseMoveEvent(QMouseEvent *event) override;
  //  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  //  int starAtPosition(int x) const;

  KerningItem *_kerningItem;
};
