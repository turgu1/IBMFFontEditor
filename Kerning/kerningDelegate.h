#pragma once

#include <QModelIndex>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QWidget>

#include "../IBMFDriver/ibmf_font_mod.hpp"

class KerningDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  KerningDelegate(IBMFFontMod *font, int faceIdx)
      : QStyledItemDelegate(), font_(font), faceIdx_(faceIdx) {}

  void     paint(QPainter *painter, const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override;
  QSize    sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                        const QModelIndex &index) const override;
  void     setEditorData(QWidget *editor, const QModelIndex &index) const override;
  void     setModelData(QWidget *editor, QAbstractItemModel *model,
                        const QModelIndex &index) const override;

private slots:
  void commitAndCloseEditor();

private:
  IBMFFontMod *font_;
  int          faceIdx_;
};