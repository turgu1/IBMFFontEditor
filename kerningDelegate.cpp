#include "kerningDelegate.h"

void KerningDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const {
  if (index.data().canConvert<KerningItem>()) {
    KerningItem kerningItem = qvariant_cast<KerningItem>(index.data());

    if (option.state & QStyle::State_Selected)
      painter->fillRect(option.rect, option.palette.highlight());

    kerningItem.paint(painter, option.rect, option.palette, KerningItem::EditMode::ReadOnly);
  } else {
    QStyledItemDelegate::paint(painter, option, index);
  }

  QWidget *KerningDelegate::createEditor(QWidget * parent, const QStyleOptionViewItem &option,
                                         const QModelIndex &index) const {
    if (index.data().canConvert<KerningItem>()) {
      KerningEditor *editor = new KerningEditor(parent);
      connect(editor, &KerningEditor::editingFinished, this,
              &KerningDelegate::commitAndCloseEditor);
      return editor;
    }
    return QStyledItemDelegate::createEditor(parent, option, index);
  }

  void KerningDelegate::commitAndCloseEditor() {
    KerningEditor *editor = qobject_cast<KerningEditor *>(sender());
    emit           commitData(editor);
    emit           closeEditor(editor);
  }

  void KerningDelegate::setEditorData(QWidget * editor, const QModelIndex &index) const {
    if (index.data().canConvert<KerningItem>()) {
      KerningItem    kerningItem   = qvariant_cast<KerningItem>(index.data());
      KerningEditor *kerningEditor = qobject_cast<KerningEditor *>(editor);
      kerningEditor->setKerningItem(kerningItem);
    } else {
      QStyledItemDelegate::setEditorData(editor, index);
    }
  }

  void KerningDelegate::setModelData(QWidget * editor, QAbstractItemModel * model,
                                     const QModelIndex &index) const {
    if (index.data().canConvert<KerningItem>()) {
      KerningEditor *kerningEditor = qobject_cast<KerningEditor *>(editor);
      model->setData(index, QVariant::fromValue(kerningEditor->kerningItem()));
    } else {
      QStyledItemDelegate::setModelData(editor, model, index);
    }
  }

  QSize KerningDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index)
      const {
    if (index.data().canConvert<KerningItem>()) {
      KerningItem kerningItem = qvariant_cast<KerningItem>(index.data());
      return kerningItem.sizeHint();
    }
    return QStyledItemDelegate::sizeHint(option, index);
  }
