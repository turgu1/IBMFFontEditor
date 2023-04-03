#pragma once

#include <QAbstractTableModel>
#include <QVector>

#include "../IBMFDriver/IBMFFontMod.hpp"
#include "kernEntry.h"

class KerningModel : public QAbstractTableModel {
  Q_OBJECT
public:
  explicit KerningModel(IBMFDefs::GlyphCode            glyphCode,
                        IBMFDefs::GlyphKernStepsVecPtr glyphKernSteps, QObject *parent = nullptr);

  int           rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int           columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant      data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
  bool          setData(const QModelIndex &index, const QVariant &value, int role) override;
  QVariant      headerData(int section, Qt::Orientation orientation, int role) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  void          addKernEntry(KernEntry entry);
  void          removeKernEntry(QModelIndex entry);
  void          save();

  inline IBMFDefs::GlyphCode getGlyphCode() const { return glyphCode_; }

signals:
  void editCompleted();

private:
  QVector<KernEntry>             kernEntries_;
  IBMFDefs::GlyphCode            glyphCode_;
  IBMFDefs::GlyphKernStepsVecPtr glyphKernSteps_;
};
