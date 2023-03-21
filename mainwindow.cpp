#include "mainwindow.h"

#include <iostream>

#include <QDateTime>
#include <QRegularExpression>
#include <QSettings>

#include "./ui_mainwindow.h"
#include "blocksDialog.h"
#include "fontParameterDialog.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
  initialized_ = false;
  ui->setupUi(this);

  setWindowTitle("IBMF Font Editor");

  // --> Undo / Redo Stack <--

  undoStack_  = new QUndoStack(this);

  undoAction_ = undoStack_->createUndoAction(this, tr("&Undo"));
  undoAction_->setShortcuts(QKeySequence::Undo);

  redoAction_ = undoStack_->createRedoAction(this, tr("&Redo"));
  redoAction_->setShortcuts(QKeySequence::Redo);

  ui->editMenu->addAction(undoAction_);
  ui->editMenu->addAction(redoAction_);

  ui->undoButton->setAction(undoAction_);
  ui->redoButton->setAction(redoAction_);

  // --> Bitmap Renderers <--

  bitmapRenderer_ = new BitmapRenderer(ui->bitmapFrame, 20, false, undoStack_);
  ui->bitmapFrame->layout()->addWidget(bitmapRenderer_);

  QObject::connect(bitmapRenderer_, &BitmapRenderer::bitmapHasChanged, this,
                   &MainWindow::bitmapChanged);

  smallGlyph_ = new BitmapRenderer(ui->smallGlyphPreview, 2, true, undoStack_);
  ui->smallGlyphPreview->layout()->addWidget(smallGlyph_);
  smallGlyph_->connectTo(bitmapRenderer_);

  largeGlyph_ = new BitmapRenderer(ui->largeGlyphPreview, 4, true, undoStack_);
  ui->largeGlyphPreview->layout()->addWidget(largeGlyph_);
  largeGlyph_->connectTo(bitmapRenderer_);

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

  ibmfFont_        = nullptr;
  ibmfGlyphCode_   = 0;
  currentFilePath_ = "";
  fontChanged_     = false;
  glyphChanged_    = false;
  faceChanged_     = false;
  glyphReloading_  = false;
  faceReloading_   = false;

  this->clearAll();

  initialized_ = true;

  show();
}

MainWindow::~MainWindow() {
  writeSettings();
  delete ui;
}

void MainWindow::createUndoView() {
  undoView_ = new QUndoView(undoStack_);
  undoView_->setWindowTitle(tr("Command List"));
  undoView_->show();
  undoView_->setAttribute(Qt::WA_QuitOnClose, false);
}

void MainWindow::createRecentFileActionsAndConnections() {
  QAction *recentFileAction;
  QMenu   *recentFilesMenu = ui->menuOpenRecent;
  for (auto i = 0; i < MAX_RECENT_FILES; ++i) {
    recentFileAction = new QAction(this);
    recentFileAction->setVisible(false);
    QObject::connect(recentFileAction, &QAction::triggered, this, &MainWindow::openRecent);
    recentFileActionList_.append(recentFileAction);
    recentFilesMenu->addAction(recentFileActionList_.at(i));
  }

  updateRecentActionList();
}

void MainWindow::updateRecentActionList() {
  QSettings   settings("ibmf", "IBMFEditor");
  QStringList recentFilePaths = settings.value("recentFiles").toStringList();

  auto itEnd                  = 0u;

  if (recentFilePaths.size() <= MAX_RECENT_FILES) {
    itEnd = recentFilePaths.size();
  } else {
    itEnd = MAX_RECENT_FILES;
  }
  for (auto i = 0u; i < itEnd; ++i) {
    QString strippedName = QFileInfo(recentFilePaths.at(i)).fileName();
    recentFileActionList_.at(i)->setText(strippedName);
    recentFileActionList_.at(i)->setData(recentFilePaths.at(i));
    recentFileActionList_.at(i)->setVisible(true);
  }

  for (auto i = itEnd; i < MAX_RECENT_FILES; ++i) {
    recentFileActionList_.at(i)->setVisible(false);
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
        currentFilePath_ = action->data().toString();
        adjustRecentsForCurrentFile();
        fontChanged_ = false;
        setWindowTitle("IBMF Font Editor - " + currentFilePath_);
      } else {
        QMessageBox::warning(this, "Warning", "Unable to load IBMF file " + currentFilePath_);
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
  if (geometry.isEmpty())
    setGeometry(200, 200, 800, 800);
  else
    restoreGeometry(geometry);

  const auto rightSplitterState = settings.value("RightSplitter", QByteArray()).toByteArray();
  if (!rightSplitterState.isEmpty()) {
    ui->rightSplitter->restoreState(rightSplitterState);
  }

  const auto leftSplitterState = settings.value("LeftSplitter", QByteArray()).toByteArray();
  if (!leftSplitterState.isEmpty()) {
    ui->leftSplitter->restoreState(leftSplitterState);
  }

  const auto leftFrameState = settings.value("LeftFrame", QByteArray()).toByteArray();
  if (!leftFrameState.isEmpty()) {
    ui->leftFrame->restoreState(leftFrameState);
  }

  const auto faceCharsSplitterState =
      settings.value("FaceCharsSplitter", QByteArray()).toByteArray();
  if (!faceCharsSplitterState.isEmpty()) {
    ui->FaceCharsSplitter->restoreState(faceCharsSplitterState);
  }

  const auto rightFrameState = settings.value("RightFrame", QByteArray()).toByteArray();
  if (!rightFrameState.isEmpty()) {
    ui->rightFrame->restoreState(rightFrameState);
  }

  settings.endGroup();
}

void MainWindow::adjustRecentsForCurrentFile() {
  QSettings   settings("ibmf", "IBMFEditor");
  QStringList recentFilePaths = settings.value("recentFiles").toStringList();
  recentFilePaths.removeAll(currentFilePath_);
  recentFilePaths.prepend(currentFilePath_);
  while (recentFilePaths.size() > MAX_RECENT_FILES)
    recentFilePaths.removeLast();
  settings.setValue("recentFiles", recentFilePaths);
  updateRecentActionList();
}

void MainWindow::closeEvent(QCloseEvent *event) {
  if (!checkFontChanged()) event->ignore();
}

void MainWindow::resizeEvent(QResizeEvent *event) {}

void MainWindow::paintEvent(QPaintEvent *event) {
  setScrollBarSizes(bitmapRenderer_->getPixelSize());
  updateBitmapOffsetPos();
}

void MainWindow::bitmapChanged(Bitmap &bitmap) {
  if (ibmfFont_ != nullptr) {
    if (!fontChanged_) {
      fontChanged_ = true;
      this->setWindowTitle(this->windowTitle() + '*');
    }
    glyphChanged_ = true;
  }
}

bool MainWindow::saveFont(bool askToConfirmName) {
  QString            newFilePath;
  QRegularExpression theDateTimeWithExt("_\\d\\d\\d\\d\\d\\d\\d\\d_\\d\\d\\d\\d\\d\\d\\.ibmf$");
  QRegularExpressionMatch match = theDateTimeWithExt.match(currentFilePath_);
  if (!match.hasMatch()) {
    QRegularExpression extension("\\.ibmf$");
    match = extension.match(currentFilePath_);
  }
  if (match.hasMatch()) {
    QMessageBox::information(this, "match:", match.captured());
    QString newExt   = "_" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd_hhmmss") + ".ibmf";
    QString filePath = currentFilePath_;
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
      if (ibmfFont_->save(out)) {
        currentFilePath_ = newFilePath;
        adjustRecentsForCurrentFile();
        fontChanged_ = false;
        setWindowTitle("IBMF Font Editor - " + currentFilePath_);
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
  ibmfFont_ = IBMFFontModPtr(new IBMFFontMod((uint8_t *)content.data(), content.size()));
  if (ibmfFont_->isInitialized()) {
    ibmfPreamble_ = ibmfFont_->getPreample();

    char marker[5];
    memcpy(marker, ibmfPreamble_.marker, 4);
    marker[4] = 0;

    putValue(ui->fontHeader, 0, 1, QByteArray(marker), false);
    putValue(ui->fontHeader, 1, 1, ibmfPreamble_.faceCount, false);
    putValue(ui->fontHeader, 2, 1, ibmfPreamble_.bits.version, false);
    putValue(ui->fontHeader, 3, 1, ibmfPreamble_.bits.fontFormat, false);

    for (int i = 0; i < ibmfPreamble_.faceCount; i++) {
      IBMFDefs::FaceHeaderPtr face_header = ibmfFont_->getFaceHeader(i);
      ui->faceIndex->addItem(QString::number(face_header->pointSize).append(" pts"));
    }

    loadFace(0);

    fontChanged_  = false;
    faceChanged_  = false;
    glyphChanged_ = false;
    undoStack_->clear();
  }
  return ibmfFont_->isInitialized();
}

bool MainWindow::checkFontChanged() {
  if (fontChanged_) {
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
        currentFilePath_ = filePath;
        adjustRecentsForCurrentFile();
        fontChanged_ = false;
        setWindowTitle("IBMF Font Editor - " + filePath);
      } else {
        QMessageBox::warning(this, "Warning", "Unable to load IBMF file " + currentFilePath_);
      }
      file.close();
    }
  }
}

void MainWindow::clearAll() {
  ui->faceIndex->clear();
  bitmapRenderer_->clearAndEmit(); // Will clear other renderers through signaling
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
  if ((ibmfFont_ != nullptr) && ibmfFont_->isInitialized() && faceChanged_) {
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

    ibmfFont_->saveFaceHeader(ibmfFaceIdx_, face_header);
    faceChanged_ = false;
  }
}

bool MainWindow::loadFace(uint8_t faceIdx) {
  saveFace();
  if ((ibmfFont_ != nullptr) && (ibmfFont_->isInitialized()) &&
      (faceIdx < ibmfPreamble_.faceCount)) {
    ibmfFaceHeader_ = ibmfFont_->getFaceHeader(faceIdx);

    faceReloading_  = true;

    putValue(ui->faceHeader, 0, 1, ibmfFaceHeader_->pointSize, false);
    putValue(ui->faceHeader, 1, 1, ibmfFaceHeader_->lineHeight);
    putValue(ui->faceHeader, 2, 1, ibmfFaceHeader_->dpi, false);
    putFix16Value(ui->faceHeader, 3, 1, (float)ibmfFaceHeader_->xHeight / 64.0);
    putFix16Value(ui->faceHeader, 4, 1, (float)ibmfFaceHeader_->emSize / 64.0);
    putFix16Value(ui->faceHeader, 5, 1, (float)ibmfFaceHeader_->slantCorrection / 64.0);
    putValue(ui->faceHeader, 6, 1, ibmfFaceHeader_->maxHeight, false);
    putValue(ui->faceHeader, 7, 1, ibmfFaceHeader_->descenderHeight);
    putValue(ui->faceHeader, 8, 1, ibmfFaceHeader_->spaceSize);
    putValue(ui->faceHeader, 9, 1, ibmfFaceHeader_->glyphCount, false);
    putValue(ui->faceHeader, 10, 1, ibmfFaceHeader_->ligKernStepCount, false);

    faceReloading_ = false;

    ibmfFaceIdx_   = faceIdx;

    loadGlyph(ibmfGlyphCode_);
  } else {
    return false;
  }

  return true;
}

void MainWindow::saveGlyph() {
  if ((ibmfFont_ != nullptr) && ibmfFont_->isInitialized() && glyphChanged_) {
    Bitmap             *theBitmap;
    IBMFDefs::GlyphInfo glyph_info;
    if (bitmapRenderer_->retrieveBitmap(&theBitmap)) {

      glyph_info.bitmapWidth             = theBitmap->dim.width;
      glyph_info.bitmapHeight            = theBitmap->dim.height;
      glyph_info.horizontalOffset        = getValue(ui->characterMetrics, 2, 1).toInt();
      glyph_info.verticalOffset          = getValue(ui->characterMetrics, 3, 1).toInt();
      glyph_info.ligKernPgmIndex         = getValue(ui->characterMetrics, 4, 1).toUInt();
      glyph_info.packetLength            = getValue(ui->characterMetrics, 5, 1).toUInt();
      glyph_info.advance                 = getValue(ui->characterMetrics, 6, 1).toFloat() * 64.0;
      glyph_info.rleMetrics.dynF         = getValue(ui->characterMetrics, 7, 1).toUInt();
      glyph_info.rleMetrics.firstIsBlack = getValue(ui->characterMetrics, 8, 1).toUInt();

      ibmfFont_->saveGlyph(ibmfFaceIdx_, ibmfGlyphCode_, &glyph_info, theBitmap);
      glyphChanged_ = false;
    }
  }
}

bool MainWindow::loadGlyph(uint16_t glyphCode) {
  saveGlyph();
  if ((ibmfFont_ != nullptr) && (ibmfFont_->isInitialized()) &&
      (ibmfFaceIdx_ < ibmfPreamble_.faceCount) && (glyphCode < ibmfFaceHeader_->glyphCount)) {

    if (ibmfFont_->getGlyph(ibmfFaceIdx_, glyphCode, ibmfGlyphInfo_, &ibmfGlyphBitmap_)) {
      ibmfGlyphCode_  = glyphCode;

      glyphChanged_   = false;
      glyphReloading_ = true;

      putValue(ui->characterMetrics, 0, 1, ibmfGlyphInfo_->bitmapWidth, false);
      putValue(ui->characterMetrics, 1, 1, ibmfGlyphInfo_->bitmapHeight, false);
      putValue(ui->characterMetrics, 2, 1, ibmfGlyphInfo_->horizontalOffset);
      putValue(ui->characterMetrics, 3, 1, ibmfGlyphInfo_->verticalOffset);
      putValue(ui->characterMetrics, 4, 1, ibmfGlyphInfo_->ligKernPgmIndex, false);
      putValue(ui->characterMetrics, 5, 1, ibmfGlyphInfo_->packetLength, false);
      putFix16Value(ui->characterMetrics, 6, 1, (float)ibmfGlyphInfo_->advance / 64.0);
      putValue(ui->characterMetrics, 7, 1, ibmfGlyphInfo_->rleMetrics.dynF, false);
      putValue(ui->characterMetrics, 8, 1, ibmfGlyphInfo_->rleMetrics.firstIsBlack, false);

      centerScrollBarPos();
      bitmapRenderer_->clearAndLoadBitmap(*ibmfGlyphBitmap_, ibmfPreamble_, *ibmfFaceHeader_,
                                          *ibmfGlyphInfo_);
      smallGlyph_->clearAndLoadBitmap(*ibmfGlyphBitmap_, ibmfPreamble_, *ibmfFaceHeader_,
                                      *ibmfGlyphInfo_);
      largeGlyph_->clearAndLoadBitmap(*ibmfGlyphBitmap_, ibmfPreamble_, *ibmfFaceHeader_,
                                      *ibmfGlyphInfo_);

      ui->ligTable->clearContents();
      ui->kernTable->clearContents();

      if (ibmfFont_->getGlyphLigKern(ibmfFaceIdx_, ibmfGlyphCode_, &ibmfLigKerns_)) {
        ui->ligTable->setRowCount(ibmfLigKerns_->ligSteps.size());
        for (int i = 0; i < ibmfLigKerns_->ligSteps.size(); i++) {
          putValue(ui->ligTable, i, 0,
                   QChar(fontFormat0CharacterCodes[ibmfLigKerns_->ligSteps[i]->nextGlyphCode]));
          putValue(ui->ligTable, i, 1,
                   QChar(fontFormat0CharacterCodes[ibmfLigKerns_->ligSteps[i]->glyphCode]));
          int code = ibmfLigKerns_->ligSteps[i]->nextGlyphCode;
          ui->ligTable->item(i, 0)->setToolTip(QString("%1  0o%2  0x%3")
                                                   .arg(code)
                                                   .arg(code, 3, 8, QChar('0'))
                                                   .arg(code, 2, 16, QChar('0')));
          code = ibmfLigKerns_->ligSteps[i]->glyphCode;
          ui->ligTable->item(i, 1)->setToolTip(QString("%1  0o%2  0x%3")
                                                   .arg(code)
                                                   .arg(code, 3, 8, QChar('0'))
                                                   .arg(code, 2, 16, QChar('0')));
          ui->ligTable->item(i, 0)->setTextAlignment(Qt::AlignCenter);
          ui->ligTable->item(i, 1)->setTextAlignment(Qt::AlignCenter);
        }
        ui->kernTable->setRowCount(ibmfLigKerns_->kernSteps.size());
        for (int i = 0; i < ibmfLigKerns_->kernSteps.size(); i++) {
          putValue(ui->kernTable, i, 0,
                   QChar(fontFormat0CharacterCodes[ibmfLigKerns_->kernSteps[i]->nextGlyphCode]));
          putFix16Value(ui->kernTable, i, 1, (float)ibmfLigKerns_->kernSteps[i]->kern / 64.0);
          int code = ibmfLigKerns_->kernSteps[i]->nextGlyphCode;
          ui->kernTable->item(i, 0)->setToolTip(QString("%1  0o%2  0x%3")
                                                    .arg(code)
                                                    .arg(code, 3, 8, QChar('0'))
                                                    .arg(code, 2, 16, QChar('0')));
          ui->kernTable->item(i, 0)->setTextAlignment(Qt::AlignCenter);
          ui->kernTable->item(i, 1)->setTextAlignment(Qt::AlignCenter);
        }
      }

      ui->charactersList->setCurrentCell(glyphCode / 5, glyphCode % 5);

      glyphReloading_ = false;
    } else {
      return false;
    }
  } else {
    return false;
  }

  return true;
}

void MainWindow::on_leftButton_clicked() {
  if ((ibmfFont_ == nullptr) || !ibmfFont_->isInitialized()) return;

  uint16_t glyphCode = ibmfGlyphCode_;

  if (glyphCode > 0) {
    glyphCode -= 1;
  } else {
    glyphCode = ibmfFaceHeader_->glyphCount - 1;
  }
  loadGlyph(glyphCode);
}

void MainWindow::on_rightButton_clicked() {
  if ((ibmfFont_ == nullptr) || !ibmfFont_->isInitialized()) return;

  uint16_t glyphCode = ibmfGlyphCode_;

  if (glyphCode >= ibmfFaceHeader_->glyphCount - 1) {
    glyphCode = 0;
  } else {
    glyphCode += 1;
  }
  loadGlyph(glyphCode);
}

void MainWindow::on_faceIndex_currentIndexChanged(int index) {
  if ((ibmfFont_ == nullptr) || !ibmfFont_->isInitialized()) return;

  loadFace(index);
}

void MainWindow::setScrollBarSizes(int pixelSize) {
  ui->bitmapHorizontalScrollBar->setPageStep(
      (ui->bitmapFrame->width() / pixelSize) *
      ((float)ui->bitmapHorizontalScrollBar->maximum() / BitmapRenderer::bitmapWidth));
  ui->bitmapVerticalScrollBar->setPageStep(
      (ui->bitmapFrame->height() / pixelSize) *
      ((float)ui->bitmapVerticalScrollBar->maximum() / BitmapRenderer::bitmapHeight));
}

void MainWindow::centerScrollBarPos() {
  ui->bitmapHorizontalScrollBar->setValue(ui->bitmapHorizontalScrollBar->maximum() / 2);
  ui->bitmapVerticalScrollBar->setValue(ui->bitmapVerticalScrollBar->maximum() / 2);
}

void MainWindow::updateBitmapOffsetPos() {
  QPoint pos =
      QPoint((float)ui->bitmapHorizontalScrollBar->value() /
                     ui->bitmapHorizontalScrollBar->maximum() * BitmapRenderer::bitmapWidth -
                 ((bitmapRenderer_->width() / bitmapRenderer_->getPixelSize()) / 2),
             (float)ui->bitmapVerticalScrollBar->value() / ui->bitmapVerticalScrollBar->maximum() *
                     BitmapRenderer::bitmapHeight -
                 ((bitmapRenderer_->height() / bitmapRenderer_->getPixelSize()) / 2));

  int maxX =
      BitmapRenderer::bitmapWidth - (bitmapRenderer_->width() / bitmapRenderer_->getPixelSize());
  int maxY =
      BitmapRenderer::bitmapHeight - (bitmapRenderer_->height() / bitmapRenderer_->getPixelSize());

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

  std::cout << "BitmapRender Size: " << bitmapRenderer_->width() << ", " << bitmapRenderer_->height() << std::endl;
  std::cout << "BitmapRender Pixel Size: " << bitmapRenderer_->getPixelSize() << std::endl;
  std::cout << "BitmapOffsetPos = " << pos.x() << ", " << pos.y() << std::endl;
#endif

  bitmapRenderer_->setBitmapOffsetPos(pos);
}

void MainWindow::on_pixelSize_valueChanged(int value) {
  bitmapRenderer_->setPixelSize(ui->pixelSize->value());

  setScrollBarSizes(value);
}

void MainWindow::on_charactersList_cellClicked(int row, int column) {
  int idx = (row * 5) + column;
  if (ibmfFont_ != nullptr) {
    loadGlyph(idx);
  } else {
    ibmfGlyphCode_ = idx; // Retain the selection for when a font will be loaded
  }
}

void MainWindow::on_characterMetrics_cellChanged(int row, int column) {
  if ((ibmfFont_ != nullptr) && ibmfFont_->isInitialized() && initialized_ && !glyphReloading_) {
    glyphChanged_ = true;
  }
}

void MainWindow::on_faceHeader_cellChanged(int row, int column) {
  if ((ibmfFont_ != nullptr) && ibmfFont_->isInitialized() && initialized_ && !faceReloading_) {
    faceChanged_ = true;
  }
}

void MainWindow::on_glyphForgetButton_clicked() {
  if (glyphChanged_) {
    QMessageBox::StandardButton button =
        QMessageBox::question(this, "Character has been Modified",
                              "You will loose all changes to the bitmap and metrics. Are-you sure?",
                              QMessageBox::Yes | QMessageBox::No);
    if (button == QMessageBox::Yes) {
      undoStack_->clear();
      glyphChanged_ = false;
      loadGlyph(ibmfGlyphCode_);
    }
  }
}

void MainWindow::on_faceForgetButton_clicked() {
  if (faceChanged_) {
    QMessageBox::StandardButton button =
        QMessageBox::question(this, "Face Header Modified",
                              "You will loose all changes to the Face Header. Are-you sure?",
                              QMessageBox::Yes | QMessageBox::No);
    if (button == QMessageBox::Yes) {
      faceChanged_ = false;
      loadFace(ibmfFaceIdx_);
    }
  }
}

void MainWindow::on_centerGridButton_clicked() {
  centerScrollBarPos();
  updateBitmapOffsetPos();
  bitmapRenderer_->repaint();
}

void MainWindow::on_leftSplitter_splitterMoved(int pos, int index) {}

void MainWindow::on_bitmapVerticalScrollBar_valueChanged(int value) {
  if (initialized_) {
    updateBitmapOffsetPos();
    bitmapRenderer_->repaint();
  }
}

void MainWindow::on_bitmapHorizontalScrollBar_valueChanged(int value) {
  if (initialized_) {
    updateBitmapOffsetPos();
    bitmapRenderer_->repaint();
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
        new IBMFFontMod((uint8_t *)original_content.data(), original_content.size()));
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
                                "Unable to save IBMF file " + currentFilePath_ +
                                    " error: " + QString::number(font->getLastError()));
        }
      }
    } else {
      QMessageBox::critical(this, "Error", "Unable to load IBMF file " + currentFilePath_);
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
        new IBMFFontMod((uint8_t *)original_content.data(), original_content.size()));
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
      QMessageBox::critical(this, "Error", "Unable to load IBMF file " + currentFilePath_);
    }
  }
}

void MainWindow::on_actionSave_triggered() { saveFont(true); }

void MainWindow::on_actionSaveBackup_triggered() { saveFont(false); }

void MainWindow::on_clearRecentList_triggered() {
  QSettings   settings("ibmf", "IBMFEditor");
  QStringList recentFilePaths = QStringList();
  settings.setValue("recentFiles", recentFilePaths);

  for (int i = 0; i < recentFileActionList_.length(); i++) {
    recentFileActionList_.at(i)->setVisible(false);
    recentFileActionList_.at(i)->setText("");
    recentFileActionList_.at(i)->setData("");
  }
}

void MainWindow::on_actionTest_Dialog_triggered() {
  QString filePath =
      QFileDialog::getOpenFileName(this, "Open TTF Font File", ".", "Font (*.ttf *.otf)");

  if (!filePath.isEmpty()) {
    QFileInfo     fileInfo(filePath);
    BlocksDialog *blocksDialog = new BlocksDialog(filePath, fileInfo.fileName());
    if (blocksDialog->exec() == QDialog::Accepted) {
      auto blockIndexes               = blocksDialog->getSelectedBlockIndexes();

      FontParameterDialog *fontDialog = new FontParameterDialog();
      if (fontDialog->exec() == QDialog::Accepted) {
        QMessageBox::information(this, "OK",
                                 QString("Number of accepter blocks: %1").arg(blockIndexes.size()));
      }
    }
  }
}
