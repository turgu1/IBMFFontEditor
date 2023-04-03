#include "kerningEditor.h"

KerningEditor::KerningEditor(QWidget *parent) : QWidget(parent) {
  setMouseTracking(true);
  setAutoFillBackground(true);
}

void KerningEditor::setKerningItem(KerningItem *kerningItem) {
  kerningItem_ = kerningItem;
  QObject::connect(kerningItem_, &KerningItem::editingCompleted, this,
                   &KerningEditor::editingCompleted);
  kerningItem_->editing();
}

void KerningEditor::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  kerningItem_->paint(&painter, rect(), palette(), KerningItem::EditMode::Editable);
}

// void KerningEditor::mouseReleaseEvent(QMouseEvent *event) {
//   emit editingFinished();
//   QWidget::mouseReleaseEvent(event);
// }

QSize KerningEditor::sizeHint() const { return kerningItem_->sizeHint(); }

void KerningEditor::editingCompleted() { emit editingFinished(); }

// void KerningEditor::focusOutEvent(QFocusEvent *event) { emit editingFinished(); }
