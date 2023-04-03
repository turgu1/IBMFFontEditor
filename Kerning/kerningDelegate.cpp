#include "kerningDelegate.h"

#include "kerningEditor.h"
#include "kerningItem.h"
#include "kerningModel.h"

void KerningDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const {
  if (index.data().canConvert<KernEntry>()) {
    KerningItem *kerningItem =
        new KerningItem(qvariant_cast<KernEntry>(index.data()), font_, faceIdx_);

    if (option.state & QStyle::State_Selected) {
      painter->fillRect(option.rect, option.palette.highlight());
    }

    kerningItem->paint(painter, option.rect, option.palette, KerningItem::EditMode::ReadOnly);
    delete kerningItem;
  } else {
    QStyledItemDelegate::paint(painter, option, index);
  }
}

QWidget *KerningDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                       const QModelIndex &index) const {
  if (index.data().canConvert<KernEntry>()) {
    KerningEditor *editor = new KerningEditor(parent);
    connect(editor, &KerningEditor::editingFinished, this, &KerningDelegate::commitAndCloseEditor);
    return editor;
  }
  return QStyledItemDelegate::createEditor(parent, option, index);
}

void KerningDelegate::commitAndCloseEditor() {
  KerningEditor *editor = qobject_cast<KerningEditor *>(sender());
  emit           commitData(editor);
  emit           closeEditor(editor);
}

void KerningDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const {
  if (index.data().canConvert<KernEntry>()) {
    KerningItem *kerningItem =
        new KerningItem(qvariant_cast<KernEntry>(index.data()), font_, faceIdx_, editor);
    KerningEditor *kerningEditor = qobject_cast<KerningEditor *>(editor);
    kerningEditor->setKerningItem(kerningItem);
  } else {
    QStyledItemDelegate::setEditorData(editor, index);
  }
}

void KerningDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                   const QModelIndex &index) const {
  if (index.data().canConvert<KernEntry>()) {
    KerningEditor *kerningEditor = qobject_cast<KerningEditor *>(editor);
    model->setData(index, QVariant::fromValue(kerningEditor->kerningItem()->kernEntry()));
  } else {
    QStyledItemDelegate::setModelData(editor, model, index);
  }
}

QSize KerningDelegate::sizeHint(const QStyleOptionViewItem &option,
                                const QModelIndex          &index) const {
  if (index.data().canConvert<KernEntry>()) {
    KerningItem kerningItem = KerningItem(qvariant_cast<KernEntry>(index.data()), font_, faceIdx_);
    QSize       size        = kerningItem.sizeHint();
    return size;
  }
  return QStyledItemDelegate::sizeHint(option, index);
}

void KerningDelegate::completed() {
  // if (editor_ != nullptr) closeEditor(editor_, QAbstractItemDelegate::SubmitModelCache);
}
