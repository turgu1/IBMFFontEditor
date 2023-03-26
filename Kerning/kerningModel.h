#pragma once

#include <QAbstractTableModel>
#include <QVector>

#include "../IBMFDriver/ibmf_font_mod.hpp"
#include "kernEntry.h"

class KerningModel : public QAbstractTableModel {
  Q_OBJECT
public:
  explicit KerningModel(IBMFDefs::GlyphCode glyphCode, IBMFDefs::GlyphKernSteps *glyphKernSteps,
                        QObject *parent = nullptr);

  int           rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int           columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant      data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
  bool          setData(const QModelIndex &index, const QVariant &value, int role) override;
  QVariant      headerData(int section, Qt::Orientation orientation, int role) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;

  void addKernEntry(KernEntry entry);

signals:
  void editCompleted();

private:
  QVector<KernEntry>        _kernEntries;
  IBMFDefs::GlyphCode       _glyphCode;
  IBMFDefs::GlyphKernSteps *_glyphKernSteps;
};
