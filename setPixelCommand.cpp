#include "setPixelCommand.h"

SetPixelCommand::SetPixelCommand(BitmapRenderer *renderer, BitmapRenderer::PixelType pixelType,
                                 QPoint atPos, QUndoCommand *parent)
    : QUndoCommand(parent), renderer(renderer), pixelType(pixelType), atPos(atPos) {
  renderer->paintPixel(pixelType, atPos);
  setText(QObject::tr("Set pixel at [%1, %2] to %3.")
              .arg(atPos.x())
              .arg(atPos.y())
              .arg(pixelType == BitmapRenderer::PixelType::WHITE ? "White" : "Black"));
}

void SetPixelCommand::undo() {
  renderer->paintPixel(pixelType == BitmapRenderer::PixelType::WHITE
                           ? BitmapRenderer::PixelType::BLACK
                           : BitmapRenderer::PixelType::WHITE,
                       atPos);
}

void SetPixelCommand::redo() { renderer->paintPixel(pixelType, atPos); }
