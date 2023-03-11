#include "kerningEditor.h"

KerningEditor::KerningEditor(QWidget *parent) : QWidget(parent) {
  setMouseTracking(true);
  setAutoFillBackground(true);
}

void KerningEditor::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  myKerningItem.paint(&painter, rect(), palette(), KerningItem::EditMode::Editable);
}

void KerningEditor::mouseMoveEvent(QMouseEvent *event) {
  const int star = starAtPosition(event->position().toPoint().x());

  if (star != myKerningItem.starCount() && star != -1) {
    myKerningItem.setStarCount(star);
    update();
  }
  QWidget::mouseMoveEvent(event);
}

void KerningEditor::mouseReleaseEvent(QMouseEvent *event) {
  emit editingFinished();
  QWidget::mouseReleaseEvent(event);
}

int KerningEditor::starAtPosition(int x) const {
  const int star = (x / (myKerningItem.sizeHint().width() / myKerningItem.maxStarCount())) + 1;
  if (star <= 0 || star > myKerningItem.maxStarCount()) return -1;

  return star;
}
