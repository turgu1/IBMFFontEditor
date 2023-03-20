#include "mainwindow.h"

#include <iostream>

#include <QDateTime>
#include <QRegularExpression>
#include <QSettings>

#include "./ui_mainwindow.h"
#include "blocksDialog.h"
#include "fontParameterDialog.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
  _initialized = false;
  ui->setupUi(this);

  setWindowTitle("IBMF Font Editor");

  // --> Undo / Redo Stack <--

  _undoStack = new QUndoStack(this);

  _undoAction = _undoStack->createUndoAction(this, tr("&Undo"));
  _undoAction->setShortcuts(QKeySequence::Undo);

  _redoAction = _undoStack->createRedoAction(this, tr("&Redo"));
  _redoAction->setShortcuts(QKeySequence::Redo);

  ui->editMenu->addAction(_undoAction);
  ui->editMenu->addAction(_redoAction);

  ui->undoButton->setAction(_undoAction);
  ui->redoButton->setAction(_redoAction);

  // --> Bitmap Renderers <--

  _bitmapRenderer = new BitmapRenderer(ui->bitmapFrame, 20, false, _undoStack);
  ui->bitmapFrame->layout()->addWidget(_bitmapRenderer);

  QObject::connect(_bitmapRenderer, &BitmapRenderer::bitmapHasChanged, this,
                   &MainWindow::bitmapChanged);

  _smallGlyph = new BitmapRenderer(ui->smallGlyphPreview, 2, true, _undoStack);
  ui->smallGlyphPreview->layout()->addWidget(_smallGlyph);
  _smallGlyph->connectTo(_bitmapRenderer);

  _largeGlyph = new BitmapRenderer(ui->largeGlyphPreview, 4, true, _undoStack);
  ui->largeGlyphPreview->layout()->addWidget(_largeGlyph);
  _largeGlyph->connectTo(_bitmapRenderer);

  // ---

  centerScrollBarPos();

  // --> Captions in the tables are not editable <--

  for (int row = 0; row < ui->fontHeader->rowCount(); row++) {
    this->clearEditable(ui->fontHeader, row, 0);
  }

  for (int row = 0; row < ui->faceHeader->rowCount(); row++) {
    this->clearEditable(ui->faceHeader, row, 0);
  }

  for (int row = 0; row < ui->characterMetrics->rowCount(); row++) {
    this->clearEditable(ui->characterMetrics, row, 0);
  }

  // --> Tables' Last Column Stretch <--

  QHeaderView *header = ui->fontHeader->horizontalHeader();
  header->setStretchLastSection(true);

  header = ui->faceHeader->horizontalHeader();
  header->setStretchLastSection(true);

  header = ui->characterMetrics->horizontalHeader();
  header->setStretchLastSection(true);

  header = ui->ligTable->horizontalHeader();
  header->setStretchLastSection(true);

  header = ui->kernTable->horizontalHeader();
  header->setStretchLastSection(true);

  // --> Characters Table <-- TODO: adapt against the upcoming Unicode table

  ui->charactersList->setColumnCount(5);
  ui->charactersList->setRowCount((174 + 4) / 5);

  int idx = 0;
  for (int row = 0; row < ui->charactersList->rowCount(); row++) {
    ui->charactersList->setRowHeight(row, 50);
    for (int col = 0; col < 5; col++) {
      auto item = new QTableWidgetItem;
      item->setFlags(item->flags() & ~Qt::ItemIsEditable);
      item->setTextAlignment(Qt::AlignCenter);
      if (idx < 174) {
        item->setData(Qt::EditRole, QChar(fontFormat0CharacterCodes[idx]));

        item->setToolTip(QString("%1  0o%2  0x%3")
                             .arg(idx)
                             .arg(idx, 3, 8, QChar('0'))
                             .arg(idx, 2, 16, QChar('0')));
      } else {
        item->setFlags(Qt::NoItemFlags);
      }
      ui->charactersList->setItem(row, col, item);
      idx += 1;
    }
  }

  header = ui->charactersList->horizontalHeader();
  header->setSectionResizeMode(QHeaderView::Stretch);

  // --> Tables columns width <--

  ui->fontHeader->setColumnWidth(0, 120);
  ui->faceHeader->setColumnWidth(0, 120);
  ui->characterMetrics->setColumnWidth(0, 120);
  ui->fontHeader->setColumnWidth(1, 100);
  ui->faceHeader->setColumnWidth(1, 100);
  ui->characterMetrics->setColumnWidth(1, 100);

  // --> Splitters <--

  auto setHPolicy = [](auto widget1, auto widget2, int value1, int value2) {
    QSizePolicy sp;
    sp = widget1->sizePolicy();
    sp.setHorizontalStretch(value1);
    widget1->setSizePolicy(sp);
    sp = widget2->sizePolicy();
    sp.setHorizontalStretch(value2);
    widget2->setSizePolicy(sp);
  };

  auto setVPolicy = [](auto widget1, auto widget2, int value1, int value2) {
    QSizePolicy sp;
    sp = widget1->sizePolicy();
    sp.setVerticalStretch(value1);
    widget1->setSizePolicy(sp);
    sp = widget2->sizePolicy();
    sp.setVerticalStretch(value2);
    widget2->setSizePolicy(sp);
  };

  ui->leftFrame->setSizes(QList<int>() << 50 << 200);
  ui->FaceCharsSplitter->setSizes(QList<int>() << 100 << 200);

  setHPolicy(ui->leftFrame, ui->rightSplitter, 12, 20);
  setHPolicy(ui->centralFrame, ui->rightFrame, 20, 8);
  setVPolicy(ui->charsMetrics, ui->tabs, 20, 20);
  setVPolicy(ui->fontHeaderFrame, ui->FaceCharsSplitter, 3, 4);
  // setVPolicy(ui->faceFrame, ui->charsFrame, 1,  4);

  // --> Config settings <--

  createRecentFileActionsAndConnections();
  readSettings();

  // --> Overall variables initialization <--

  _ibmfFont        = nullptr;
  _ibmfGlyphCode   = 0;
  _currentFilePath = "";
  _fontChanged     = false;
  _glyphChanged    = false;
  _faceChanged     = false;
  _glyphReloading  = false;
  _faceReloading   = false;

  this->clearAll();

  _initialized = true;

  show();
}

MainWindow::~MainWindow() {
  writeSettings();
  delete ui;
}

void MainWindow::createUndoView() {
  _undoView = new QUndoView(_undoStack);
  _undoView->setWindowTitle(tr("Command List"));
  _undoView->show();
  _undoView->setAttribute(Qt::WA_QuitOnClose, false);
}

void MainWindow::createRecentFileActionsAndConnections() {
  QAction *recentFileAction;
  QMenu   *recentFilesMenu = ui->menuOpenRecent;
  for (auto i = 0; i < MAX_RECENT_FILES; ++i) {
    recentFileAction = new QAction(this);
    recentFileAction->setVisible(false);
    QObject::connect(recentFileAction, &QAction::triggered, this, &MainWindow::openRecent);
    _recentFileActionList.append(recentFileAction);
    recentFilesMenu->addAction(_recentFileActionList.at(i));
  }

  updateRecentActionList();
}

void MainWindow::updateRecentActionList() {
  QSettings   settings("ibmf", "IBMFEditor");
  QStringList recentFilePaths = settings.value("recentFiles").toStringList();

  auto itEnd = 0u;

  if (recentFilePaths.size() <= MAX_RECENT_FILES) {
    itEnd = recentFilePaths.size();
  } else {
    itEnd = MAX_RECENT_FILES;
  }
  for (auto i = 0u; i < itEnd; ++i) {
    QString strippedName = QFileInfo(recentFilePaths.at(i)).fileName();
    _recentFileActionList.at(i)->setText(strippedName);
    _recentFileActionList.at(i)->setData(recentFilePaths.at(i));
    _recentFileActionList.at(i)->setVisible(true);
  }

  for (auto i = itEnd; i < MAX_RECENT_FILES; ++i) {
    _recentFileActionList.at(i)->setVisible(false);
  }
}

void MainWindow::openRecent() {
  QAction *action = qobject_cast<QAction *>(sender());
  if (action && checkFontChanged()) {
    QFile file;
    file.setFileName(action->data().toString());

    if (!file.open(QIODevice::ReadOnly)) {
      QMessageBox::warning(this, "Warning", "Unable to open file " + file.errorString());
    } else {
      if (loadFont(file)) {
        _currentFilePath = action->data().toString();
        adjustRecentsForCurrentFile();
        _fontChanged = false;
        setWindowTitle("IBMF Font Editor - " + _currentFilePath);
      } else {
        QMessageBox::warning(this, "Warning", "Unable to load IBMF file " + _currentFilePath);
      }
      file.close();
    }
  }
}

void MainWindow::writeSettings() {
  QSettings settings("ibmf", "IBMFEditor");

  settings.beginGroup("MainWindow");
  settings.setValue("geometry", saveGeometry());
  settings.setValue("RightSplitter", ui->rightSplitter->saveState());
  settings.setValue("LeftSplitter", ui->leftSplitter->saveState());
  settings.setValue("LeftFrame", ui->leftFrame->saveState());
  settings.setValue("FaceCharsSplitter", ui->FaceCharsSplitter->saveState());
  settings.setValue("RightFrame", ui->rightFrame->saveState());
  settings.endGroup();
}

void MainWindow::readSettings() {
  QSettings settings("ibmf", "IBMFEditor");

  settings.beginGroup("MainWindow");
  const auto geometry = settings.value("geometry", QByteArray()).toByteArray();
  if (geometry.isEmpty()) setGeometry(200, 200, 800, 800);
  else restoreGeometry(geometry);

  const auto rightSplitterState = settings.value("RightSplitter", QByteArray()).toByteArray();
  if (!rightSplitterState.isEmpty()) { ui->rightSplitter->restoreState(rightSplitterState); }

  const auto leftSplitterState = settings.value("LeftSplitter", QByteArray()).toByteArray();
  if (!leftSplitterState.isEmpty()) { ui->leftSplitter->restoreState(leftSplitterState); }

  const auto leftFrameState = settings.value("LeftFrame", QByteArray()).toByteArray();
  if (!leftFrameState.isEmpty()) { ui->leftFrame->restoreState(leftFrameState); }

  const auto faceCharsSplitterState =
      settings.value("FaceCharsSplitter", QByteArray()).toByteArray();
  if (!faceCharsSplitterState.isEmpty()) {
    ui->FaceCharsSplitter->restoreState(faceCharsSplitterState);
  }

  const auto rightFrameState = settings.value("RightFrame", QByteArray()).toByteArray();
  if (!rightFrameState.isEmpty()) { ui->rightFrame->restoreState(rightFrameState); }

  settings.endGroup();
}

void MainWindow::adjustRecentsForCurrentFile() {
  QSettings   settings("ibmf", "IBMFEditor");
  QStringList recentFilePaths = settings.value("recentFiles").toStringList();
  recentFilePaths.removeAll(_currentFilePath);
  recentFilePaths.prepend(_currentFilePath);
  while (recentFilePaths.size() > MAX_RECENT_FILES) recentFilePaths.removeLast();
  settings.setValue("recentFiles", recentFilePaths);
  updateRecentActionList();
}

void MainWindow::closeEvent(QCloseEvent *event) {
  if (!checkFontChanged()) event->ignore();
}

void MainWindow::resizeEvent(QResizeEvent *event) {}

void MainWindow::paintEvent(QPaintEvent *event) {
  setScrollBarSizes(_bitmapRenderer->getPixelSize());
  updateBitmapOffsetPos();
}

void MainWindow::bitmapChanged(Bitmap &bitmap) {
  if (_ibmfFont != nullptr) {
    if (!_fontChanged) {
      _fontChanged = true;
      this->setWindowTitle(this->windowTitle() + '*');
    }
    _glyphChanged = true;
  }
}

bool MainWindow::saveFont(bool askToConfirmName) {
  QString            newFilePath;
  QRegularExpression theDateTimeWithExt("_\\d\\d\\d\\d\\d\\d\\d\\d_\\d\\d\\d\\d\\d\\d\\.ibmf$");
  QRegularExpressionMatch match = theDateTimeWithExt.match(_currentFilePath);
  if (!match.hasMatch()) {
    QRegularExpression extension("\\.ibmf$");
    match = extension.match(_currentFilePath);
  }
  if (match.hasMatch()) {
    QMessageBox::information(this, "match:", match.captured());
    QString newExt   = "_" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd_hhmmss") + ".ibmf";
    QString filePath = _currentFilePath;
    filePath.replace(match.captured(), newExt);
    if (askToConfirmName) {
      newFilePath = QFileDialog::getSaveFileName(this, "Save IBMF Font File", filePath, "*.ibmf");
    } else {
      newFilePath = filePath;
    }
  } else {
    newFilePath = QFileDialog::getSaveFileName(this, "Save NEW IBMF Font File", "*.ibmf");
  }
  if (newFilePath.isEmpty()) {
    return false;
  } else {
    QFile outFile;
    outFile.setFileName(newFilePath);
    if (outFile.open(QIODevice::WriteOnly)) {
      QDataStream out(&outFile);
      if (_ibmfFont->save(out)) {
        _currentFilePath = newFilePath;
        adjustRecentsForCurrentFile();
        _fontChanged = false;
        setWindowTitle("IBMF Font Editor - " + _currentFilePath);
        outFile.close();
      } else {
        outFile.close();
        QMessageBox::critical(this, "Unable to save font!!", "Not able to save font!");
        return false;
      }
    } else {
      QMessageBox::critical(this, "Unable to save font!!", "Not able to save font!");
      return false;
    }
  }
  return true;
}

bool MainWindow::loadFont(QFile &file) {
  QByteArray content = file.readAll();
  file.close();
  clearAll();
  _ibmfFont = IBMFFontModPtr(new IBMFFontMod((uint8_t *) content.data(), content.size()));
  if (_ibmfFont->isInitialized()) {
    _ibmfPreamble = _ibmfFont->getPreample();

    char marker[5];
    memcpy(marker, _ibmfPreamble.marker, 4);
    marker[4] = 0;

    putValue(ui->fontHeader, 0, 1, QByteArray(marker), false);
    putValue(ui->fontHeader, 1, 1, _ibmfPreamble.faceCount, false);
    putValue(ui->fontHeader, 2, 1, _ibmfPreamble.bits.version, false);
    putValue(ui->fontHeader, 3, 1, _ibmfPreamble.bits.fontFormat, false);

    for (int i = 0; i < _ibmfPreamble.faceCount; i++) {
      IBMFDefs::FaceHeaderPtr face_header = _ibmfFont->getFaceHeader(i);
      ui->faceIndex->addItem(QString::number(face_header->pointSize).append(" pts"));
    }

    loadFace(0);

    _fontChanged  = false;
    _faceChanged  = false;
    _glyphChanged = false;
    _undoStack->clear();
  }
  return _ibmfFont->isInitialized();
}

bool MainWindow::checkFontChanged() {
  if (_fontChanged) {
    saveGlyph();
    QMessageBox::StandardButton button =
        QMessageBox::question(this, "File Changed", "File was changed. Do you want to save it ?",
                              QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (button == QMessageBox::Yes) {
      return saveFont(true);
    } else {
      return button != QMessageBox::Cancel;
    }
  }
  return true;
}

void MainWindow::on_actionOpen_triggered() {
  if (!checkFontChanged()) return;

  QFile file;

  QString filePath = QFileDialog::getOpenFileName(this, "Open IBMF Font File", ".", "*.ibmf");
  if (!filePath.isEmpty()) {
    file.setFileName(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
      QMessageBox::warning(this, "Warning", "Unable to open file " + file.errorString());
    } else {
      if (loadFont(file)) {
        _currentFilePath = filePath;
        adjustRecentsForCurrentFile();
        _fontChanged = false;
        setWindowTitle("IBMF Font Editor - " + filePath);
      } else {
        QMessageBox::warning(this, "Warning", "Unable to load IBMF file " + _currentFilePath);
      }
      file.close();
    }
  }
}

void MainWindow::clearAll() {
  ui->faceIndex->clear();
  _bitmapRenderer->clearAndEmit(); // Will clear other renderers through signaling
}

void MainWindow::on_actionExit_triggered() {
  if (checkFontChanged()) QApplication::exit();
}

void MainWindow::clearEditable(QTableWidget *w, int row, int col) {
  auto item = w->item(row, col);
  item->setFlags(item->flags() & ~Qt::ItemIsEditable);
}

void MainWindow::putValue(QTableWidget *w, int row, int col, QVariant value, bool editable) {
  auto item = new QTableWidgetItem();
  item->setData(Qt::EditRole, value);
  if (!editable) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
  w->setItem(row, col, item);
}

void MainWindow::putFix16Value(QTableWidget *w, int row, int col, QVariant value, bool editable) {
  auto item = new QTableWidgetItem();
  item->setData(Qt::EditRole, value);
  if (!editable) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
  w->setItem(row, col, item);
  QAbstractItemDelegate *delegate;
  if ((delegate = w->itemDelegateForRow(row)) != nullptr) delete delegate;
  w->setItemDelegateForRow(row, new Fix16Delegate);
}

QVariant MainWindow::getValue(QTableWidget *w, int row, int col) {
  return w->item(row, col)->data(Qt::DisplayRole);
}

void MainWindow::saveFace() {
  if ((_ibmfFont != nullptr) && _ibmfFont->isInitialized() && _faceChanged) {
    IBMFDefs::FaceHeader face_header;

    face_header.pointSize        = getValue(ui->faceHeader, 0, 1).toUInt();
    face_header.lineHeight       = getValue(ui->faceHeader, 1, 1).toUInt();
    face_header.dpi              = getValue(ui->faceHeader, 2, 1).toUInt();
    face_header.xHeight          = getValue(ui->faceHeader, 3, 1).toFloat() * 64.0;
    face_header.emSize           = getValue(ui->faceHeader, 4, 1).toFloat() * 64.0;
    face_header.slantCorrection  = getValue(ui->faceHeader, 5, 1).toFloat() * 64.0;
    face_header.maxHeight        = getValue(ui->faceHeader, 6, 1).toUInt();
    face_header.descenderHeight  = getValue(ui->faceHeader, 7, 1).toUInt();
    face_header.spaceSize        = getValue(ui->faceHeader, 8, 1).toUInt();
    face_header.glyphCount       = getValue(ui->faceHeader, 9, 1).toUInt();
    face_header.ligKernStepCount = getValue(ui->faceHeader, 10, 1).toUInt();

    _ibmfFont->saveFaceHeader(_ibmfFaceIdx, face_header);
    _faceChanged = false;
  }
}

bool MainWindow::loadFace(uint8_t faceIdx) {
  saveFace();
  if ((_ibmfFont != nullptr) && (_ibmfFont->isInitialized()) &&
      (faceIdx < _ibmfPreamble.faceCount)) {
    _ibmfFaceHeader = _ibmfFont->getFaceHeader(faceIdx);

    _faceReloading = true;

    putValue(ui->faceHeader, 0, 1, _ibmfFaceHeader->pointSize, false);
    putValue(ui->faceHeader, 1, 1, _ibmfFaceHeader->lineHeight);
    putValue(ui->faceHeader, 2, 1, _ibmfFaceHeader->dpi, false);
    putFix16Value(ui->faceHeader, 3, 1, (float) _ibmfFaceHeader->xHeight / 64.0);
    putFix16Value(ui->faceHeader, 4, 1, (float) _ibmfFaceHeader->emSize / 64.0);
    putFix16Value(ui->faceHeader, 5, 1, (float) _ibmfFaceHeader->slantCorrection / 64.0);
    putValue(ui->faceHeader, 6, 1, _ibmfFaceHeader->maxHeight, false);
    putValue(ui->faceHeader, 7, 1, _ibmfFaceHeader->descenderHeight);
    putValue(ui->faceHeader, 8, 1, _ibmfFaceHeader->spaceSize);
    putValue(ui->faceHeader, 9, 1, _ibmfFaceHeader->glyphCount, false);
    putValue(ui->faceHeader, 10, 1, _ibmfFaceHeader->ligKernStepCount, false);

    _faceReloading = false;

    _ibmfFaceIdx = faceIdx;

    loadGlyph(_ibmfGlyphCode);
  } else {
    return false;
  }

  return true;
}

void MainWindow::saveGlyph() {
  if ((_ibmfFont != nullptr) && _ibmfFont->isInitialized() && _glyphChanged) {
    Bitmap             *theBitmap;
    IBMFDefs::GlyphInfo glyph_info;
    if (_bitmapRenderer->retrieveBitmap(&theBitmap)) {

      glyph_info.bitmapWidth             = theBitmap->dim.width;
      glyph_info.bitmapHeight            = theBitmap->dim.height;
      glyph_info.horizontalOffset        = getValue(ui->characterMetrics, 2, 1).toInt();
      glyph_info.verticalOffset          = getValue(ui->characterMetrics, 3, 1).toInt();
      glyph_info.ligKernPgmIndex         = getValue(ui->characterMetrics, 4, 1).toUInt();
      glyph_info.packetLength            = getValue(ui->characterMetrics, 5, 1).toUInt();
      glyph_info.advance                 = getValue(ui->characterMetrics, 6, 1).toFloat() * 64.0;
      glyph_info.rleMetrics.dynF         = getValue(ui->characterMetrics, 7, 1).toUInt();
      glyph_info.rleMetrics.firstIsBlack = getValue(ui->characterMetrics, 8, 1).toUInt();

      _ibmfFont->saveGlyph(_ibmfFaceIdx, _ibmfGlyphCode, &glyph_info, theBitmap);
      _glyphChanged = false;
    }
  }
}

bool MainWindow::loadGlyph(uint16_t glyphCode) {
  saveGlyph();
  if ((_ibmfFont != nullptr) && (_ibmfFont->isInitialized()) &&
      (_ibmfFaceIdx < _ibmfPreamble.faceCount) && (glyphCode < _ibmfFaceHeader->glyphCount)) {

    if (_ibmfFont->getGlyph(_ibmfFaceIdx, glyphCode, _ibmfGlyphInfo, &_ibmfGlyphBitmap)) {
      _ibmfGlyphCode = glyphCode;

      _glyphChanged   = false;
      _glyphReloading = true;

      putValue(ui->characterMetrics, 0, 1, _ibmfGlyphInfo->bitmapWidth, false);
      putValue(ui->characterMetrics, 1, 1, _ibmfGlyphInfo->bitmapHeight, false);
      putValue(ui->characterMetrics, 2, 1, _ibmfGlyphInfo->horizontalOffset);
      putValue(ui->characterMetrics, 3, 1, _ibmfGlyphInfo->verticalOffset);
      putValue(ui->characterMetrics, 4, 1, _ibmfGlyphInfo->ligKernPgmIndex, false);
      putValue(ui->characterMetrics, 5, 1, _ibmfGlyphInfo->packetLength, false);
      putFix16Value(ui->characterMetrics, 6, 1, (float) _ibmfGlyphInfo->advance / 64.0);
      putValue(ui->characterMetrics, 7, 1, _ibmfGlyphInfo->rleMetrics.dynF, false);
      putValue(ui->characterMetrics, 8, 1, _ibmfGlyphInfo->rleMetrics.firstIsBlack, false);

      centerScrollBarPos();
      _bitmapRenderer->clearAndLoadBitmap(*_ibmfGlyphBitmap, _ibmfPreamble, *_ibmfFaceHeader,
                                          *_ibmfGlyphInfo);
      _smallGlyph->clearAndLoadBitmap(*_ibmfGlyphBitmap, _ibmfPreamble, *_ibmfFaceHeader,
                                      *_ibmfGlyphInfo);
      _largeGlyph->clearAndLoadBitmap(*_ibmfGlyphBitmap, _ibmfPreamble, *_ibmfFaceHeader,
                                      *_ibmfGlyphInfo);

      ui->ligTable->clearContents();
      ui->kernTable->clearContents();

      if (_ibmfFont->getGlyphLigKern(_ibmfFaceIdx, _ibmfGlyphCode, &_ibmfLigKerns)) {
        ui->ligTable->setRowCount(_ibmfLigKerns->lig_steps.size());
        for (int i = 0; i < _ibmfLigKerns->lig_steps.size(); i++) {
          putValue(ui->ligTable, i, 0,
                   QChar(fontFormat0CharacterCodes[_ibmfLigKerns->lig_steps[i]->nextGlyphCode]));
          putValue(ui->ligTable, i, 1,
                   QChar(fontFormat0CharacterCodes[_ibmfLigKerns->lig_steps[i]->glyphCode]));
          int code = _ibmfLigKerns->lig_steps[i]->nextGlyphCode;
          ui->ligTable->item(i, 0)->setToolTip(QString("%1  0o%2  0x%3")
                                                   .arg(code)
                                                   .arg(code, 3, 8, QChar('0'))
                                                   .arg(code, 2, 16, QChar('0')));
          code = _ibmfLigKerns->lig_steps[i]->glyphCode;
          ui->ligTable->item(i, 1)->setToolTip(QString("%1  0o%2  0x%3")
                                                   .arg(code)
                                                   .arg(code, 3, 8, QChar('0'))
                                                   .arg(code, 2, 16, QChar('0')));
          ui->ligTable->item(i, 0)->setTextAlignment(Qt::AlignCenter);
          ui->ligTable->item(i, 1)->setTextAlignment(Qt::AlignCenter);
        }
        ui->kernTable->setRowCount(_ibmfLigKerns->kern_steps.size());
        for (int i = 0; i < _ibmfLigKerns->kern_steps.size(); i++) {
          putValue(ui->kernTable, i, 0,
                   QChar(fontFormat0CharacterCodes[_ibmfLigKerns->kern_steps[i]->nextGlyphCode]));
          putFix16Value(ui->kernTable, i, 1, (float) _ibmfLigKerns->kern_steps[i]->kern / 64.0);
          int code = _ibmfLigKerns->kern_steps[i]->nextGlyphCode;
          ui->kernTable->item(i, 0)->setToolTip(QString("%1  0o%2  0x%3")
                                                    .arg(code)
                                                    .arg(code, 3, 8, QChar('0'))
                                                    .arg(code, 2, 16, QChar('0')));
          ui->kernTable->item(i, 0)->setTextAlignment(Qt::AlignCenter);
          ui->kernTable->item(i, 1)->setTextAlignment(Qt::AlignCenter);
        }
      }

      ui->charactersList->setCurrentCell(glyphCode / 5, glyphCode % 5);

      _glyphReloading = false;
    } else {
      return false;
    }
  } else {
    return false;
  }

  return true;
}

void MainWindow::on_leftButton_clicked() {
  if ((_ibmfFont == nullptr) || !_ibmfFont->isInitialized()) return;

  uint16_t glyphCode = _ibmfGlyphCode;

  if (glyphCode > 0) {
    glyphCode -= 1;
  } else {
    glyphCode = _ibmfFaceHeader->glyphCount - 1;
  }
  loadGlyph(glyphCode);
}

void MainWindow::on_rightButton_clicked() {
  if ((_ibmfFont == nullptr) || !_ibmfFont->isInitialized()) return;

  uint16_t glyphCode = _ibmfGlyphCode;

  if (glyphCode >= _ibmfFaceHeader->glyphCount - 1) {
    glyphCode = 0;
  } else {
    glyphCode += 1;
  }
  loadGlyph(glyphCode);
}

void MainWindow::on_faceIndex_currentIndexChanged(int index) {
  if ((_ibmfFont == nullptr) || !_ibmfFont->isInitialized()) return;

  loadFace(index);
}

void MainWindow::setScrollBarSizes(int pixelSize) {
  ui->bitmapHorizontalScrollBar->setPageStep(
      (ui->bitmapFrame->width() / pixelSize) *
      ((float) ui->bitmapHorizontalScrollBar->maximum() / BitmapRenderer::bitmapWidth));
  ui->bitmapVerticalScrollBar->setPageStep(
      (ui->bitmapFrame->height() / pixelSize) *
      ((float) ui->bitmapVerticalScrollBar->maximum() / BitmapRenderer::bitmapHeight));
}

void MainWindow::centerScrollBarPos() {
  ui->bitmapHorizontalScrollBar->setValue(ui->bitmapHorizontalScrollBar->maximum() / 2);
  ui->bitmapVerticalScrollBar->setValue(ui->bitmapVerticalScrollBar->maximum() / 2);
}

void MainWindow::updateBitmapOffsetPos() {
  QPoint pos =
      QPoint((float) ui->bitmapHorizontalScrollBar->value() /
                     ui->bitmapHorizontalScrollBar->maximum() * BitmapRenderer::bitmapWidth -
                 ((_bitmapRenderer->width() / _bitmapRenderer->getPixelSize()) / 2),
             (float) ui->bitmapVerticalScrollBar->value() / ui->bitmapVerticalScrollBar->maximum() *
                     BitmapRenderer::bitmapHeight -
                 ((_bitmapRenderer->height() / _bitmapRenderer->getPixelSize()) / 2));

  int maxX =
      BitmapRenderer::bitmapWidth - (_bitmapRenderer->width() / _bitmapRenderer->getPixelSize());
  int maxY =
      BitmapRenderer::bitmapHeight - (_bitmapRenderer->height() / _bitmapRenderer->getPixelSize());

  if (pos.x() < 0) pos.setX(0);
  if (pos.y() < 0) pos.setY(0);
  if (pos.x() > maxX) pos.setX(maxX);
  if (pos.y() > maxY) pos.setY(maxY);

#if 0
  std::cout << "Horizontal scrollbar position: "
            << ui->bitmapHorizontalScrollBar->minimum()
            << " < "
            << ui->bitmapHorizontalScrollBar->value()
            << " < "
            << ui->bitmapHorizontalScrollBar->maximum()
            << std::endl;
  std::cout << "Vertical scrollbar position: "
            << ui->bitmapVerticalScrollBar->minimum()
            << " < "
            << ui->bitmapVerticalScrollBar->value()
            << " < "
            << ui->bitmapVerticalScrollBar->maximum()
            << std::endl;

  std::cout << "BitmapRender Size: " << _bitmapRenderer->width() << ", " << _bitmapRenderer->height() << std::endl;
  std::cout << "BitmapRender Pixel Size: " << _bitmapRenderer->getPixelSize() << std::endl;
  std::cout << "BitmapOffsetPos = " << pos.x() << ", " << pos.y() << std::endl;
#endif

  _bitmapRenderer->setBitmapOffsetPos(pos);
}

void MainWindow::on_pixelSize_valueChanged(int value) {
  _bitmapRenderer->setPixelSize(ui->pixelSize->value());

  setScrollBarSizes(value);
}

void MainWindow::on_charactersList_cellClicked(int row, int column) {
  int idx = (row * 5) + column;
  if (_ibmfFont != nullptr) {
    loadGlyph(idx);
  } else {
    _ibmfGlyphCode = idx; // Retain the selection for when a font will be loaded
  }
}

void MainWindow::on_characterMetrics_cellChanged(int row, int column) {
  if ((_ibmfFont != nullptr) && _ibmfFont->isInitialized() && _initialized && !_glyphReloading) {
    _glyphChanged = true;
  }
}

void MainWindow::on_faceHeader_cellChanged(int row, int column) {
  if ((_ibmfFont != nullptr) && _ibmfFont->isInitialized() && _initialized && !_faceReloading) {
    _faceChanged = true;
  }
}

void MainWindow::on_glyphForgetButton_clicked() {
  if (_glyphChanged) {
    QMessageBox::StandardButton button =
        QMessageBox::question(this, "Character has been Modified",
                              "You will loose all changes to the bitmap and metrics. Are-you sure?",
                              QMessageBox::Yes | QMessageBox::No);
    if (button == QMessageBox::Yes) {
      _undoStack->clear();
      _glyphChanged = false;
      loadGlyph(_ibmfGlyphCode);
    }
  }
}

void MainWindow::on_faceForgetButton_clicked() {
  if (_faceChanged) {
    QMessageBox::StandardButton button =
        QMessageBox::question(this, "Face Header Modified",
                              "You will loose all changes to the Face Header. Are-you sure?",
                              QMessageBox::Yes | QMessageBox::No);
    if (button == QMessageBox::Yes) {
      _faceChanged = false;
      loadFace(_ibmfFaceIdx);
    }
  }
}

void MainWindow::on_centerGridButton_clicked() {
  centerScrollBarPos();
  updateBitmapOffsetPos();
  _bitmapRenderer->repaint();
}

void MainWindow::on_leftSplitter_splitterMoved(int pos, int index) {}

void MainWindow::on_bitmapVerticalScrollBar_valueChanged(int value) {
  if (_initialized) {
    updateBitmapOffsetPos();
    _bitmapRenderer->repaint();
  }
}

void MainWindow::on_bitmapHorizontalScrollBar_valueChanged(int value) {
  if (_initialized) {
    updateBitmapOffsetPos();
    _bitmapRenderer->repaint();
  }
}

void MainWindow::on_actionFont_load_save_triggered() {
  if (!checkFontChanged()) return;

  QFile in_file;
  QFile out_file;

  QString filePath = QFileDialog::getOpenFileName(this, "Open IBMF Font File");
  in_file.setFileName(filePath);

  if (!in_file.open(QIODevice::ReadOnly)) {
    QMessageBox::critical(this, "Error", "Unable to open in file " + in_file.errorString());
  } else {
    QByteArray original_content = in_file.readAll();
    in_file.close();
    auto font = IBMFFontModPtr(
        new IBMFFontMod((uint8_t *) original_content.data(), original_content.size()));
    if (font->isInitialized()) {
      filePath.append(".test");
      out_file.setFileName(filePath);
      if (!out_file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Error", "Unable to open out file " + out_file.errorString());
      } else {
        QDataStream out(&out_file);
        if (font->save(out)) {
          out_file.close();
          QMessageBox::information(this, "Completed", "New font file at " + filePath);

        } else {
          out_file.close();
          QMessageBox::critical(this, "Error",
                                "Unable to save IBMF file " + _currentFilePath +
                                    " error: " + QString::number(font->getLastError()));
        }
      }
    } else {
      QMessageBox::critical(this, "Error", "Unable to load IBMF file " + _currentFilePath);
    }
  }
}

void MainWindow::on_actionRLE_Encoder_triggered() {
  if (!checkFontChanged()) return;

  QFile in_file;

  QString filePath = QFileDialog::getOpenFileName(this, "Open IBMF Font File");
  in_file.setFileName(filePath);

  if (!in_file.open(QIODevice::ReadOnly)) {
    QMessageBox::critical(this, "Error", "Unable to open in file " + in_file.errorString());
  } else {
    QByteArray original_content = in_file.readAll();
    in_file.close();
    auto font = IBMFFontModPtr(
        new IBMFFontMod((uint8_t *) original_content.data(), original_content.size()));
    if (font->isInitialized()) {
      IBMFDefs::GlyphInfoPtr glyph_info;
      Bitmap                *bitmapHeightBits;
      //        Bitmap *bitmapOneBit;
      for (int i = 0; i < 174; i++) {
        if (font->getGlyph(0, i, glyph_info, &bitmapHeightBits)) {
          //            if (!font->convertToOneBit(*bitmapHeightBits, &bitmapOneBit)) {
          //              QMessageBox::critical(this, "Conversion error", "convertToOneBit not
          //              working properly!");
          //            }
          auto gen = new RLEGenerator;
          if (gen->encodeBitmap(*bitmapHeightBits)) {
            if ((gen->getData()->size() != glyph_info->packetLength) ||
                (gen->getFirstIsBlack() != glyph_info->rleMetrics.firstIsBlack) ||
                (gen->getDynF() != glyph_info->rleMetrics.dynF)) {
              QMessageBox::critical(
                  this, "Encoder Error",
                  "Encoder error for char code " + QString::number(i) +
                      " Encoder size: " + QString::number(gen->getData()->size()) +
                      " Packet Length: " + QString::number(glyph_info->packetLength));
            }
          }
          delete gen;
          // delete bitmapOneBit;
        }
      }
      QMessageBox::information(this, "Completed", "Completed");
    } else {
      QMessageBox::critical(this, "Error", "Unable to load IBMF file " + _currentFilePath);
    }
  }
}

void MainWindow::on_actionSave_triggered() {
  saveFont(true);
}

void MainWindow::on_actionSaveBackup_triggered() {
  saveFont(false);
}

void MainWindow::on_clearRecentList_triggered() {
  QSettings   settings("ibmf", "IBMFEditor");
  QStringList recentFilePaths = QStringList();
  settings.setValue("recentFiles", recentFilePaths);

  for (int i = 0; i < _recentFileActionList.length(); i++) {
    _recentFileActionList.at(i)->setVisible(false);
    _recentFileActionList.at(i)->setText("");
    _recentFileActionList.at(i)->setData("");
  }
}

void MainWindow::on_actionTest_Dialog_triggered() {
  QString filePath =
      QFileDialog::getOpenFileName(this, "Open TTF Font File", ".", "Font (*.ttf *.otf)");

  if (!filePath.isEmpty()) {
    QFileInfo     fileInfo(filePath);
    BlocksDialog *blocksDialog = new BlocksDialog(filePath, fileInfo.fileName());
    if (blocksDialog->exec() == QDialog::Accepted) {
      auto blockIndexes = blocksDialog->getSelectedBlockIndexes();

      FontParameterDialog *fontDialog = new FontParameterDialog();
      if (fontDialog->exec() == QDialog::Accepted) {
        QMessageBox::information(this, "OK",
                                 QString("Number of accepter blocks: %1").arg(blockIndexes.size()));
      }
    }
  }
}
