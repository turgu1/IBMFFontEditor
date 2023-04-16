#include "kerningModel.h"

#include <QMessageBox>

KerningModel::KerningModel(GlyphCode glyphCode, const IBMFDefs::GlyphKernSteps &glyphKernSteps,
                           QObject *parent)
    : QAbstractTableModel(parent), glyphCode_(glyphCode) {

  for (const auto &entry : glyphKernSteps) {
    addKernEntry(KernEntry(glyphCode_, entry.nextGlyphCode, (float)(entry.kern / 64.0)));
  }
}

void KerningModel::save(GlyphKernSteps &glyphKernSteps) {
  glyphKernSteps.clear();
  for (auto entry : kernEntries_) {
    glyphKernSteps.push_back(GlyphKernStep{.nextGlyphCode = entry.nextGlyphCode,
                                           .kern          = static_cast<FIX16>(entry.kern * 64.0)});
  }
}

int KerningModel::rowCount(const QModelIndex & /*parent*/) const { return kernEntries_.length(); }

int KerningModel::columnCount(const QModelIndex & /*parent*/) const { return 1; }

void KerningModel::addKernEntry(KernEntry entry) {
  beginInsertRows(QModelIndex(), kernEntries_.length(), kernEntries_.length());
  kernEntries_.push_back(entry);
  endInsertRows();
}

void KerningModel::removeKernEntry(QModelIndex index) {
  if (kernEntries_.length() > index.row()) {
    beginRemoveRows(index, index.row(), index.row());
    kernEntries_.removeAt(index.row());
    endRemoveRows();
  }
}

QVariant KerningModel::data(const QModelIndex &index, int role) const {
  QVariant val;
  if (role == Qt::DisplayRole) {
    switch (index.column()) {
      case 0:
        val.setValue(kernEntries_[index.row()]);
        return val;
    }
  }
  return QVariant();
}

QVariant KerningModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
    switch (section) {
      case 0:
        return QString("Kerning");
    }
  }
  return QVariant();
}

bool KerningModel::setData(const QModelIndex &index, const QVariant &value, int role) {
  if (role == Qt::EditRole) {
    if (!checkIndex(index)) return false;
    switch (index.column()) {
      case 0:
        kernEntries_[index.row()] = value.value<KernEntry>();
        break;
    }
    emit editCompleted();

    //    QMessageBox::information(nullptr, "Model Update",
    //                             QString("Idx: %1, code: %2, next: %3, kern: %4")
    //                                 .arg(index.row())
    //                                 .arg(kernEntries_[index.row()].glyphCode)
    //                                 .arg(kernEntries_[index.row()].nextGlyphCode)
    //                                 .arg(kernEntries_[index.row()]._kern));
    return true;
  }
  return false;
}

Qt::ItemFlags KerningModel::flags(const QModelIndex &index) const {
  return Qt::ItemIsEditable | QAbstractTableModel::flags(index);
}
