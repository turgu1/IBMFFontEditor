#include "kerningEditor.h"

KerningEditor::KerningEditor(QWidget *parent) : QWidget(parent) {
  setMouseTracking(true);
  setAutoFillBackground(true);
}

void KerningEditor::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  _kerningItem->paint(&painter, rect(), palette(), KerningItem::EditMode::Editable);
}

// void KerningEditor::mouseReleaseEvent(QMouseEvent *event) {
//   emit editingFinished();
//   QWidget::mouseReleaseEvent(event);
// }

QSize KerningEditor::sizeHint() const {
  return _kerningItem->sizeHint();
}
