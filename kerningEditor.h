#pragma once

#include <QWidget>

#include "kerningItem.h"

class KerningEditor : public QWidget {
  Q_OBJECT
public:
  KerningEditor(QWidget *parent = nullptr);

  QSize       sizeHint() const override;
  void        setKerningItem(const KerningItem &KerningItem) { myKerningItem = KerningItem; }
  KerningItem KerningItem() { return myKerningItem; }

signals:
  void editingFinished();

protected:
  void paintEvent(QPaintEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  int starAtPosition(int x) const;

  KerningItem myKerningItem;
};
