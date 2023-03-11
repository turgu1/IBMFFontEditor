#pragma once

#include <bitmapRenderer.h>

#include <QUndoCommand>

class SetPixelCommand : public QUndoCommand {
private:
  const int ID = 1;

public:
  SetPixelCommand(BitmapRenderer *renderer, BitmapRenderer::PixelType pixelType, QPoint atPos,
                  QUndoCommand *parent = 0);
  ~SetPixelCommand(){};

  void undo() Q_DECL_OVERRIDE;
  void redo() Q_DECL_OVERRIDE;
  int  id() const Q_DECL_OVERRIDE { return ID; }

private:
  BitmapRenderer           *renderer;
  BitmapRenderer::PixelType pixelType;
  QPoint                    atPos;
};
