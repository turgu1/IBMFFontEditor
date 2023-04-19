#include "pasteSelectionCommand.h"

PasteSelectionCommand::PasteSelectionCommand(BitmapRenderer     *renderer,
                                             IBMFDefs::BitmapPtr doSelection,
                                             IBMFDefs::BitmapPtr undoSelection, QPoint atPos,
                                             QUndoCommand *parent)
    : QUndoCommand(parent), renderer_(renderer), doSelection_(doSelection),
      undoSelection_(undoSelection), atPos_(atPos) {
  // renderer->paintPixel(pixelType, atPos);
  setText(QObject::tr("Paste selection at [%1, %2].").arg(atPos.x()).arg(atPos.y()));
}

void PasteSelectionCommand::undo() { renderer_->pasteSelection(undoSelection_, &atPos_); }

void PasteSelectionCommand::redo() { renderer_->pasteSelection(doSelection_, &atPos_); }
