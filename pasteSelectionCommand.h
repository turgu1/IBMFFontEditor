#pragma once

#include <IBMFDriver/IBMFDefs.hpp>
#include <bitmapRenderer.h>

#include <QUndoCommand>

class PasteSelectionCommand : public QUndoCommand {
private:
  const int ID = 1;

public:
  PasteSelectionCommand(BitmapRenderer *renderer, IBMFDefs::BitmapPtr doSelection,
                        IBMFDefs::BitmapPtr undoSelection, QPoint atPos, QUndoCommand *parent = 0);
  ~PasteSelectionCommand(){};

  void undo() Q_DECL_OVERRIDE;
  void redo() Q_DECL_OVERRIDE;
  int  id() const Q_DECL_OVERRIDE { return ID; }

private:
  BitmapRenderer     *renderer_;
  IBMFDefs::BitmapPtr doSelection_;
  IBMFDefs::BitmapPtr undoSelection_;
  QPoint              atPos_;
};
