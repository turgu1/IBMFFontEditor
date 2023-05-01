#include "mainwindow.h"

#include <iomanip>
#include <iostream>

#include <QColor>
#include <QDateTime>
#include <QRegularExpression>
#include <QSettings>
#include <QTextStream>

#include "./ui_mainwindow.h"
#include "IBMFDriver/IBMFHexImport.hpp"
#include "IBMFDriver/IBMFTTFImport.hpp"
#include "Kerning/kerningDialog.h"
#include "blocksDialog.h"
#include "fix16Delegate.h"
#include "hexFontParameterDialog.h"
#include "pasteSelectionCommand.h"
#include "showResultDialog.h"
#include "ttfFontParameterDialog.h"

//#define TRACE(str) std::cout << str << std::endl;
#define TRACE(str)

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {

  ui->setupUi(this);

  setWindowTitle("IBMF Font Editor");

  // --> Undo / Redo Stack <--

  TRACE("Point 1");

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

  TRACE("Point 2");

  bitmapRenderer_ = new BitmapRenderer(ui->bitmapFrame, 20, false, undoStack_);
  ui->bitmapFrame->layout()->addWidget(bitmapRenderer_);

  QObject::connect(bitmapRenderer_, &BitmapRenderer::bitmapHasChanged, this,
                   &MainWindow::bitmapChanged);
  QObject::connect(bitmapRenderer_, &BitmapRenderer::someSelection, this,
                   &MainWindow::someSelection);
  //  QObject::connect(bitmapRenderer_, &BitmapRenderer::keyPressed, this,
  //                   &MainWindow::rendererKeyPressed);

  smallGlyph_ = new BitmapRenderer(ui->smallGlyphPreview, 2, true, undoStack_);
  ui->smallGlyphPreview->layout()->addWidget(smallGlyph_);
  smallGlyph_->connectTo(bitmapRenderer_);

  largeGlyph_ = new BitmapRenderer(ui->largeGlyphPreview, 4, true, undoStack_);
  ui->largeGlyphPreview->layout()->addWidget(largeGlyph_);
  largeGlyph_->connectTo(bitmapRenderer_);

  // --- Drawing Space ----

  drawingSpace_               = new DrawingSpace(nullptr, 0);
  QVBoxLayout *proofingLayout = new QVBoxLayout();
  proofingLayout->addWidget(drawingSpace_);
  ui->proofingFrame->setLayout(proofingLayout);

  // ---

  TRACE("Point 3");

  centerScrollBarPos();

  TRACE("Point 3.5");

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

  // Set font for characters' list

  QFont font("Tahoma");
  font.setPointSize(16);
  font.setBold(true);

  ui->charactersList->setFont(font);
  ui->ligTable->setFont(font);
  ui->kernTable->setFont(font);

  // --> Tables' Last Column Stretch <--

  TRACE("Point 4");

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

  header = ui->charactersList->horizontalHeader();
  header->setSectionResizeMode(QHeaderView::Stretch);

  // --> Tables columns width <--

  TRACE("Point 5");

  ui->fontHeader->setColumnWidth(0, 120);
  ui->faceHeader->setColumnWidth(0, 120);
  ui->characterMetrics->setColumnWidth(0, 120);
  ui->fontHeader->setColumnWidth(1, 100);
  ui->faceHeader->setColumnWidth(1, 100);
  ui->characterMetrics->setColumnWidth(1, 100);

  // --> Splitters <--

  TRACE("Point 6");

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

  TRACE("Point 7");

  createRecentFileActionsAndConnections();
  readSettings();

  TRACE("Point 8");

  this->clearAll();

  initialized_ = true;

  TRACE("Show...");

  // ui->copyButton->setEnabled(false);
  ui->pasteButton->setEnabled(false);
  ui->pasteMainButton->setEnabled(false);

  ui->addCharacterButton->setEnabled(false);

  ui->actionSave->setEnabled(false);
  ui->actionSaveBackup->setEnabled(false);
  ui->actionProofing_Tool->setEnabled(false);
  ui->menuExport->setEnabled(true);

  ui->actionDump_Font_Content->setEnabled(false);
  ui->actionDump_Modif_Content->setEnabled(false);
  ui->actionDump_Font_Content_With_Glyphs_Bitmap->setEnabled(false);
  ui->actionDump_Modif_Content_With_Glyphs_Bitmap->setEnabled(false);

  ui->actionImport_Modifications_File->setEnabled(false);
  ui->actionBuild_Modifications_File->setEnabled(false);

  show();
  qApp->installEventFilter(this);
  grabKeyboard();
}

MainWindow::~MainWindow() {
  writeSettings();
  delete ui;
}

void MainWindow::updateCharactersList() {
  ui->charactersList->clear();
  ui->charactersList->setColumnCount(5);
  ui->charactersList->setRowCount((ibmfFaceHeader_->glyphCount + 4) / 5);

  int idx = 0;
  for (int row = 0; row < ui->charactersList->rowCount(); row++) {
    ui->charactersList->setRowHeight(row, 50);
    for (int col = 0; col < 5; col++) {
      auto item = new QTableWidgetItem;
      item->setFlags(item->flags() & ~Qt::ItemIsEditable);
      item->setTextAlignment(Qt::AlignCenter);
      if (idx < ibmfFaceHeader_->glyphCount) {
        char32_t codePoint = ibmfFont_->getUTF32(idx);
        item->setData(Qt::EditRole, QChar(codePoint));

        item->setToolTip(QString("%1: U+%2").arg(idx).arg(codePoint, 5, 16, QChar('0')));
      } else {
        item->setFlags(Qt::NoItemFlags);
      }
      ui->charactersList->setItem(row, col, item);
      idx += 1;
    }
  }
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
  if (action && checkFontChanged()) { openFont(action->data().toString()); }
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
  settings.setValue("ProofingText", ui->plainTextEdit->toPlainText());
  settings.setValue("ProofingOpticalKern", ui->autoKernCheckBox->isChecked());
  settings.setValue("ProofingNormalKern", ui->normalKernCheckBox->isChecked());
  settings.setValue("ProofingPixelSize", ui->pixelSizeCombo->currentIndex());
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

  ui->plainTextEdit->setPlainText(settings.value("ProofingText").toString());
  ui->autoKernCheckBox->setChecked(settings.value("ProofingOpticalKern").toBool());
  ui->normalKernCheckBox->setChecked(settings.value("ProofingNormalKern").toBool());
  ui->pixelSizeCombo->setCurrentIndex(settings.value("ProofingPixelSize").toInt());
  settings.endGroup();
}

void MainWindow::adjustRecentsForCurrentFile() {
  QSettings   settings("ibmf", "IBMFEditor");
  QStringList recentFilePaths = settings.value("recentFiles").toStringList();
  recentFilePaths.removeAll(currentFilePath_);
  recentFilePaths.prepend(currentFilePath_);
  while (recentFilePaths.size() > MAX_RECENT_FILES) recentFilePaths.removeLast();
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

void MainWindow::glyphWasChanged(bool initialLoad) {
  IBMFDefs::GlyphInfoPtr glyphInfo = IBMFDefs::GlyphInfoPtr(new IBMFDefs::GlyphInfo);
  IBMFDefs::BitmapPtr    bitmap;

  if (bitmapRenderer_->retrieveBitmap(&bitmap)) {
    glyphInfo->bitmapWidth  = bitmap->dim.width;
    glyphInfo->bitmapHeight = bitmap->dim.height;
  } else {
    glyphInfo->bitmapWidth  = 0;
    glyphInfo->bitmapHeight = 0;
    bitmap                  = BitmapPtr(new Bitmap());
  }
  glyphInfo->horizontalOffset        = getValue(ui->characterMetrics, 2, 1).toInt();
  glyphInfo->verticalOffset          = getValue(ui->characterMetrics, 3, 1).toInt();
  glyphInfo->ligKernPgmIndex         = getValue(ui->characterMetrics, 4, 1).toUInt();
  glyphInfo->packetLength            = getValue(ui->characterMetrics, 5, 1).toUInt();
  glyphInfo->advance                 = getValue(ui->characterMetrics, 6, 1).toFloat() * 64.0;
  glyphInfo->rleMetrics.dynF         = getValue(ui->characterMetrics, 7, 1).toUInt();
  glyphInfo->rleMetrics.firstIsBlack = getValue(ui->characterMetrics, 8, 1).toUInt();
  glyphInfo->mainCode                = getValue(ui->characterMetrics, 9, 1).toUInt();

  drawingSpace_->setBypassGlyph(ibmfGlyphCode_, bitmap, glyphInfo, ibmfGlyphLigKern_);

  if (!initialLoad) glyphChanged_ = true;
}

void MainWindow::bitmapChanged(const Bitmap &bitmap, const QPoint &originOffsets) {
  if (ibmfFont_ != nullptr) {
    if (!fontChanged_) {
      fontChanged_ = true;
      this->setWindowTitle(this->windowTitle() + '*');
    }
    float oldAdvance         = getValue(ui->characterMetrics, 6, 1).toFloat();
    int   oldWidth           = getValue(ui->characterMetrics, 0, 1).toInt();
    int   oldHOffset         = getValue(ui->characterMetrics, 2, 1).toInt();

    int   oldNormalizedWidth = oldWidth - oldHOffset;
    int   newNormalizedWidth = bitmap.dim.width - originOffsets.x();
    float newAdvance         = oldAdvance + (newNormalizedWidth - oldNormalizedWidth);

    putColoredValue(ui->characterMetrics, 0, 1, bitmap.dim.width, false);
    putColoredValue(ui->characterMetrics, 1, 1, bitmap.dim.height, false);
    putColoredValue(ui->characterMetrics, 2, 1, originOffsets.x(), false);
    putColoredValue(ui->characterMetrics, 3, 1, originOffsets.y(), false);
    putColoredFix16Value(ui->characterMetrics, 6, 1, newAdvance, true);
    glyphWasChanged();
    glyphChanged_ = true;
  }
}

bool MainWindow::saveFont(bool askToConfirmName) {
  saveGlyph();
  saveFace();
  QString                   newFilePath;
  static QRegularExpression theDateTimeWithExt(
      "_\\d\\d\\d\\d\\d\\d\\d\\d_\\d\\d\\d\\d\\d\\d\\.ibmf$");
  QRegularExpressionMatch match = theDateTimeWithExt.match(currentFilePath_);
  if (!match.hasMatch()) {
    static QRegularExpression extension("\\.ibmf$");
    match = extension.match(currentFilePath_);
  }
  releaseKeyboard();
  if (match.hasMatch()) {
    // QMessageBox::information(this, "match:", match.captured());
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
  grabKeyboard();

  bool result = true;

  if (newFilePath.isEmpty()) {
    result = false;
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
      } else {
        QMessageBox::critical(
            this, "CRITICAL ERROR",
            "Not able to save font! Please ensure that the following path is writeable:\n\n" +
                newFilePath);
        result = false;
      }
      outFile.close();

      if ((ibmfBackup_ != nullptr) && (ibmfBackup_->isInitialized())) {
        QFileInfo fi(newFilePath);
        QString   backupFilePath = fi.absolutePath() + "/" + fi.completeBaseName() + ".ibmf_mods";
        outFile.setFileName(backupFilePath);
        if (outFile.open(QIODevice::WriteOnly)) {
          QDataStream out(&outFile);
          if (!ibmfBackup_->save(out)) {
            QMessageBox::critical(this, "CRITICAL ERROR",
                                  "Not able to save Font Modifications File! Please ensure that "
                                  "the following path is writeable:\n\n" +
                                      backupFilePath);
          }
          outFile.close();
        }
      } else {
        if (result) {
          QMessageBox::warning(this, "Warning",
                               "Font Modifications File not properly managed by this Editor! "
                               "Unable to save it. The Main Font File has been saved.");
        } else {
          QMessageBox::warning(this, "CRITICAL ERROR",
                               "Font Modifications File not properly managed by this Editor! "
                               "Unable to save it. THE MAIN FONT FILE WAS NOT SAVED EITHER!!!");
        }
      }
    } else {
      QMessageBox::critical(
          this, "CRITICAL ERROR",
          "Not able to save font! Please ensure that the following path is writeable:\n\n" +
              newFilePath);
      result = false;
    }
  }
  return result;
}

bool MainWindow::openFont(QString filePath) {

  QFile     file;
  QSettings settings("ibmf", "IBMFEditor");
  bool      result = true;

  ui->actionDump_Font_Content->setEnabled(false);
  ui->actionDump_Font_Content_With_Glyphs_Bitmap->setEnabled(false);
  ui->actionImport_Modifications_File->setEnabled(false);
  ui->actionBuild_Modifications_File->setEnabled(false);
  ui->addCharacterButton->setEnabled(false);

  file.setFileName(filePath);

  QFileInfo fi(filePath);
  settings.setValue("ibmfFolder", fi.absolutePath());

  if (!file.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, "Warning", "Unable to open file " + file.errorString());
    result = false;
  } else {
    if (loadFont(file)) {
      newFontLoaded(filePath);
      file.close();

      ui->actionDump_Font_Content->setEnabled(true);
      ui->actionDump_Font_Content_With_Glyphs_Bitmap->setEnabled(true);
      ui->actionImport_Modifications_File->setEnabled(true);
      ui->actionBuild_Modifications_File->setEnabled(true);
      ui->addCharacterButton->setEnabled(true);

      ui->actionDump_Modif_Content->setEnabled(false);
      ui->actionDump_Modif_Content_With_Glyphs_Bitmap->setEnabled(false);

      QString backupFilePath = fi.absolutePath() + "/" + fi.completeBaseName() + ".ibmf_mods";
      file.setFileName(backupFilePath);
      if (!file.open(QIODevice::ReadOnly)) {
        ibmfBackup_ = IBMFFontMod::createBackup();
        ui->actionDump_Modif_Content->setEnabled(true);
        ui->actionDump_Modif_Content_With_Glyphs_Bitmap->setEnabled(true);
      } else {
        QByteArray content = file.readAll();
        file.close();
        ibmfBackup_ = IBMFFontModPtr(new IBMFFontMod((uint8_t *) content.data(), content.size()));
        if (ibmfBackup_->isInitialized()) {
          if (ibmfBackup_->getFontFormat() == FontFormat::BACKUP) {
            ui->actionDump_Modif_Content->setEnabled(true);
            ui->actionDump_Modif_Content_With_Glyphs_Bitmap->setEnabled(true);
          } else {
            QMessageBox::warning(this, "Warning",
                                 "Font Modifications File is not of an appropriate Format! Please "
                                 "remove the file and re-initialize it:\n\n" +
                                     backupFilePath);
            ibmfBackup_ = nullptr;
          }
        } else {
          QMessageBox::warning(this, "Warning",
                               "Font Modifications File content error! Please remove the file and "
                               "re-initialize it:\n\n" +
                                   backupFilePath);
          ibmfBackup_ = nullptr;
        }
      }
    } else {
      QMessageBox::warning(this, "Warning", "Unable to load IBMF file " + currentFilePath_);
      result = false;
    }
    file.close();
  }

  return result;
}

bool MainWindow::loadFont(QFile &file) {
  QByteArray content = file.readAll();
  file.close();
  clearAll();
  ibmfFont_ = IBMFFontModPtr(new IBMFFontMod((uint8_t *) content.data(), content.size()));
  if (ibmfFont_->isInitialized()) {
    ibmfPreamble_ = ibmfFont_->getPreamble();

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

    drawingSpace_->setFont(ibmfFont_);
    drawingSpace_->setFaceIdx(0);

    fontChanged_  = false;
    faceChanged_  = false;
    glyphChanged_ = false;

    ui->actionSave->setEnabled(true);
    ui->actionSaveBackup->setEnabled(true);
    ui->actionProofing_Tool->setEnabled(true);
    ui->menuExport->setEnabled(true);

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

void MainWindow::newFontLoaded(QString filePath) {
  currentFilePath_ = filePath;
  adjustRecentsForCurrentFile();
  fontChanged_ = false;
  setWindowTitle("IBMF Font Editor - " + filePath);
}

void MainWindow::on_actionOpen_triggered() {
  if (!checkFontChanged()) return;

  QFile file;

  QSettings settings("ibmf", "IBMFEditor");

  releaseKeyboard();
  QString filePath = QFileDialog::getOpenFileName(
      this, "Open IBMF Font File", settings.value("ibmfFolder").toString(), "*.ibmf");
  grabKeyboard();

  if (!filePath.isEmpty()) { openFont(filePath); }
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
  QTableWidgetItem *item = new QTableWidgetItem();
  item->setData(Qt::EditRole, value);
  if (!editable) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
  w->setItem(row, col, item);
}

void MainWindow::putColoredValue(QTableWidget *w, int row, int col, QVariant value, bool editable) {
  QTableWidgetItem *item     = new QTableWidgetItem();
  QFont             font     = w->item(row, col)->font();
  QVariant          oldValue = w->item(row, col)->data(Qt::EditRole);
  if (oldValue != value) {
    font.setBold(true);
    item->setForeground(QColorConstants::Red);
    item->setFont(font);
  }
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
  w->setItemDelegateForRow(row, new Fix16Delegate(this));
}

void MainWindow::putColoredFix16Value(QTableWidget *w, int row, int col, QVariant value,
                                      bool editable) {
  auto     item     = new QTableWidgetItem();
  QFont    font     = w->item(row, col)->font();
  QVariant oldValue = w->item(row, col)->data(Qt::EditRole);
  if (oldValue != value) {
    font.setBold(true);
    item->setForeground(QColorConstants::Red);
    item->setFont(font);
  }
  item->setData(Qt::EditRole, value);
  if (!editable) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
  w->setItem(row, col, item);
  QAbstractItemDelegate *delegate;
  if ((delegate = w->itemDelegateForRow(row)) != nullptr) delete delegate;
  w->setItemDelegateForRow(row, new Fix16Delegate(this));
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
    face_header.descenderHeight  = getValue(ui->faceHeader, 6, 1).toUInt();
    face_header.spaceSize        = getValue(ui->faceHeader, 7, 1).toUInt();
    face_header.glyphCount       = getValue(ui->faceHeader, 8, 1).toUInt();
    face_header.ligKernStepCount = getValue(ui->faceHeader, 9, 1).toUInt();

    ibmfFont_->saveFaceHeader(ibmfFaceIdx_, face_header);
    faceChanged_ = false;
  }
}

bool MainWindow::loadFace(uint8_t faceIdx) {
  saveGlyph();
  saveFace();
  if ((ibmfFont_ != nullptr) && (ibmfFont_->isInitialized()) &&
      (faceIdx < ibmfPreamble_.faceCount)) {
    ibmfFaceHeader_ = ibmfFont_->getFaceHeader(faceIdx);

    faceReloading_  = true;

    putValue(ui->faceHeader, 0, 1, ibmfFaceHeader_->pointSize, false);
    putValue(ui->faceHeader, 1, 1, ibmfFaceHeader_->lineHeight);
    putValue(ui->faceHeader, 2, 1, ibmfFaceHeader_->dpi, false);
    putFix16Value(ui->faceHeader, 3, 1, (float) ibmfFaceHeader_->xHeight / 64.0);
    putFix16Value(ui->faceHeader, 4, 1, (float) ibmfFaceHeader_->emSize / 64.0);
    putFix16Value(ui->faceHeader, 5, 1, (float) ibmfFaceHeader_->slantCorrection / 64.0);
    putValue(ui->faceHeader, 6, 1, ibmfFaceHeader_->descenderHeight);
    putValue(ui->faceHeader, 7, 1, ibmfFaceHeader_->spaceSize);
    putValue(ui->faceHeader, 8, 1, ibmfFaceHeader_->glyphCount, false);
    putValue(ui->faceHeader, 9, 1, ibmfFaceHeader_->ligKernStepCount, false);

    faceReloading_ = false;

    ibmfFaceIdx_   = faceIdx;

    updateCharactersList();
    loadGlyph(ibmfGlyphCode_);
  } else {
    return false;
  }

  return true;
}

void MainWindow::saveGlyph() {
  if ((ibmfFont_ != nullptr) && ibmfFont_->isInitialized() && glyphChanged_) {
    BitmapPtr              theBitmap;
    IBMFDefs::GlyphInfoPtr glyphInfo = GlyphInfoPtr(new GlyphInfo);
    if (bitmapRenderer_->retrieveBitmap(&theBitmap)) {
      glyphInfo->bitmapWidth  = theBitmap->dim.width;
      glyphInfo->bitmapHeight = theBitmap->dim.height;
    } else {
      glyphInfo->bitmapWidth  = 0;
      glyphInfo->bitmapHeight = 0;
      theBitmap               = BitmapPtr(new Bitmap());
    }
    glyphInfo->horizontalOffset        = getValue(ui->characterMetrics, 2, 1).toInt();
    glyphInfo->verticalOffset          = getValue(ui->characterMetrics, 3, 1).toInt();
    glyphInfo->ligKernPgmIndex         = getValue(ui->characterMetrics, 4, 1).toUInt();
    glyphInfo->packetLength            = getValue(ui->characterMetrics, 5, 1).toUInt();
    glyphInfo->advance                 = getValue(ui->characterMetrics, 6, 1).toFloat() * 64.0;
    glyphInfo->rleMetrics.dynF         = getValue(ui->characterMetrics, 7, 1).toUInt();
    glyphInfo->rleMetrics.firstIsBlack = getValue(ui->characterMetrics, 8, 1).toUInt();
    glyphInfo->mainCode                = getValue(ui->characterMetrics, 9, 1).toUInt();

    if (ibmfFont_->saveGlyph(ibmfFaceIdx_, ibmfGlyphCode_, glyphInfo, theBitmap,
                             ibmfGlyphLigKern_)) {
      glyphChanged_ = false;
      if ((ibmfBackup_ != nullptr) && ibmfBackup_->isInitialized()) {
        ibmfBackup_->saveGlyph(ibmfFaceIdx_, ibmfGlyphCode_, glyphInfo, theBitmap,
                               ibmfGlyphLigKern_, ibmfFont_);
      } else {
        QMessageBox::critical(this, "CRITICAL ERROR",
                              "Unable to save Glyph modification into Font Mofifications File!");
      }
    } else {
      QMessageBox::critical(this, "CRITICAL ERROR", "Unable to save Glyph modification into Font!");
    }
  }

  drawingSpace_->repaint();
}

void MainWindow::populateKernTable() {
  ui->kernTable->setRowCount(ibmfGlyphLigKern_->kernSteps.size());
  for (int i = 0; i < ibmfGlyphLigKern_->kernSteps.size(); i++) {
    putValue(ui->kernTable, i, 0,
             QChar(ibmfFont_->getUTF32(ibmfGlyphLigKern_->kernSteps[i].nextGlyphCode)));
    putFix16Value(ui->kernTable, i, 1, (float) ibmfGlyphLigKern_->kernSteps[i].kern / 64.0);
    int      code      = ibmfGlyphLigKern_->kernSteps[i].nextGlyphCode;
    char32_t codePoint = ibmfFont_->getUTF32(code);
    ui->kernTable->item(i, 0)->setToolTip(
        QString("%1: U+%2").arg(code).arg(codePoint, 4, 16, QChar('0')));
    ui->kernTable->item(i, 0)->setTextAlignment(Qt::AlignCenter);
    ui->kernTable->item(i, 1)->setTextAlignment(Qt::AlignCenter);
  }
}

bool MainWindow::loadGlyph(uint16_t glyphCode) {
  saveGlyph();
  if ((ibmfFont_ != nullptr) && (ibmfFont_->isInitialized()) &&
      (ibmfFaceIdx_ < ibmfPreamble_.faceCount) && (glyphCode < ibmfFaceHeader_->glyphCount)) {

    IBMFDefs::GlyphInfoPtr glyphInfo;
    IBMFDefs::BitmapPtr    bitmap;

    if (ibmfFont_->getGlyph(ibmfFaceIdx_, glyphCode, glyphInfo, bitmap, ibmfGlyphLigKern_)) {
      ibmfGlyphCode_  = glyphCode;

      glyphChanged_   = false;
      glyphReloading_ = true;

      putValue(ui->characterMetrics, 0, 1, glyphInfo->bitmapWidth, false);
      putValue(ui->characterMetrics, 1, 1, glyphInfo->bitmapHeight, false);
      putValue(ui->characterMetrics, 2, 1, glyphInfo->horizontalOffset, false);
      putValue(ui->characterMetrics, 3, 1, glyphInfo->verticalOffset, false);
      putValue(ui->characterMetrics, 4, 1, glyphInfo->ligKernPgmIndex, false);
      putValue(ui->characterMetrics, 5, 1, glyphInfo->packetLength, false);
      putFix16Value(ui->characterMetrics, 6, 1, (float) glyphInfo->advance / 64.0);
      putValue(ui->characterMetrics, 7, 1, glyphInfo->rleMetrics.dynF, false);
      putValue(ui->characterMetrics, 8, 1, glyphInfo->rleMetrics.firstIsBlack, false);
      putValue(ui->characterMetrics, 9, 1, glyphInfo->mainCode, false);

      char32_t codePoint = ibmfFont_->getUTF32(glyphCode);
      ui->currentCodePoint->setText(QString("U+%1").arg(codePoint, 5, 16, QChar('0')));

      centerScrollBarPos();
      bitmapRenderer_->clearAndLoadBitmap(*bitmap, *ibmfFaceHeader_, *glyphInfo);
      smallGlyph_->clearAndLoadBitmap(*bitmap, *ibmfFaceHeader_, *glyphInfo);
      largeGlyph_->clearAndLoadBitmap(*bitmap, *ibmfFaceHeader_, *glyphInfo);

      ui->ligTable->clearContents();
      ui->kernTable->clearContents();

      ui->ligTable->setRowCount(ibmfGlyphLigKern_->ligSteps.size());
      for (int i = 0; i < ibmfGlyphLigKern_->ligSteps.size(); i++) {
        putValue(ui->ligTable, i, 0,
                 QChar(ibmfFont_->getUTF32(ibmfGlyphLigKern_->ligSteps[i].nextGlyphCode)));
        putValue(ui->ligTable, i, 1,
                 QChar(ibmfFont_->getUTF32(ibmfGlyphLigKern_->ligSteps[i].replacementGlyphCode)));
        int      code      = ibmfGlyphLigKern_->ligSteps[i].nextGlyphCode;
        char32_t codePoint = ibmfFont_->getUTF32(code);
        ui->ligTable->item(i, 0)->setToolTip(
            QString("%1: U+%2").arg(code).arg(codePoint, 5, 16, QChar('0')));
        code      = ibmfGlyphLigKern_->ligSteps[i].replacementGlyphCode;
        codePoint = ibmfFont_->getUTF32(code);
        ui->ligTable->item(i, 1)->setToolTip(
            QString("%1: U+%2").arg(code).arg(codePoint, 5, 16, QChar('0')));
        ui->ligTable->item(i, 0)->setTextAlignment(Qt::AlignCenter);
        ui->ligTable->item(i, 1)->setTextAlignment(Qt::AlignCenter);
      }
      populateKernTable();

      ui->charactersList->setCurrentCell(glyphCode / 5, glyphCode % 5);

      ui->pasteMainButton->setEnabled(glyphInfo->mainCode != glyphCode);

      glyphWasChanged(true);
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
  drawingSpace_->setFaceIdx(index);
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
                 ((bitmapRenderer_->width() / bitmapRenderer_->getPixelSize()) / 2),
             (float) ui->bitmapVerticalScrollBar->value() / ui->bitmapVerticalScrollBar->maximum() *
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
    glyphWasChanged();
    if ((row == 6) && (column == 1)) {
      bitmapRenderer_->setAdvance(
          static_cast<IBMFDefs::FIX16>(getValue(ui->characterMetrics, 6, 1).toFloat() * 64.0));
    }
  }
}

void MainWindow::onfaceHeader__cellChanged(int row, int column) {
  if ((ibmfFont_ != nullptr) && ibmfFont_->isInitialized() && initialized_ && !faceReloading_) {
    faceChanged_ = true;
  }
}

void MainWindow::on_glyphForgetButton_clicked() {
  if (glyphChanged_ && (ibmfFont_ != nullptr)) {
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
  bitmapRenderer_->update();
}

void MainWindow::on_leftSplitter_splitterMoved(int pos, int index) {}

void MainWindow::on_bitmapVerticalScrollBar_valueChanged(int value) {
  if (initialized_) {
    updateBitmapOffsetPos();
    bitmapRenderer_->update();
  }
}

void MainWindow::on_bitmapHorizontalScrollBar_valueChanged(int value) {
  if (initialized_) {
    updateBitmapOffsetPos();
    bitmapRenderer_->update();
  }
}

// void MainWindow::on_actionFont_load_save_triggered() {
//   if (!checkFontChanged()) return;

//  QFile in_file;
//  QFile out_file;

//  QString filePath = QFileDialog::getOpenFileName(this, "Open IBMF Font File");
//  in_file.setFileName(filePath);

//  if (!in_file.open(QIODevice::ReadOnly)) {
//    QMessageBox::critical(this, "CRITICAL ERROR", "Unable to open in file " +
//    in_file.errorString());
//  } else {
//    QByteArray original_content = in_file.readAll();
//    in_file.close();
//    auto font = IBMFFontModPtr(
//        new IBMFFontMod((uint8_t *) original_content.data(), original_content.size()));
//    if (font->isInitialized()) {
//      filePath.append(".test");
//      out_file.setFileName(filePath);
//      if (!out_file.open(QIODevice::WriteOnly)) {
//        QMessageBox::critical(this, "CRITICAL ERROR", "Unable to open out file " +
//        out_file.errorString());
//      } else {
//        QDataStream out(&out_file);
//        if (font->save(out)) {
//          out_file.close();
//          QMessageBox::information(this, "Completed", "New font file at " + filePath);

//        } else {
//          out_file.close();
//          QMessageBox::critical(this, "CRITICAL ERROR",
//                                "Unable to save IBMF file " + currentFilePath_ +
//                                    " error: " + QString::number(font->getLastError()));
//        }
//      }
//    } else {
//      QMessageBox::critical(this, "CRITICAL ERROR", "Unable to load IBMF file " +
//      currentFilePath_);
//    }
//  }
//}

// void MainWindow::on_actionRLE_Encoder_triggered() {
//   if (!checkFontChanged()) return;

//  QFile in_file;

//  QString filePath = QFileDialog::getOpenFileName(this, "Open IBMF Font File");
//  in_file.setFileName(filePath);

//  if (!in_file.open(QIODevice::ReadOnly)) {
//    QMessageBox::critical(this, "CRITICAL ERROR", "Unable to open in file " +
//    in_file.errorString());
//  } else {
//    QByteArray original_content = in_file.readAll();
//    in_file.close();
//    auto font = IBMFFontModPtr(
//        new IBMFFontMod((uint8_t *) original_content.data(), original_content.size()));
//    if (font->isInitialized()) {
//      IBMFDefs::GlyphInfoPtr glyphInfo;
//      BitmapPtr              bitmapHeightBits;
//      //        Bitmap *bitmapOneBit;
//      for (int i = 0; i < 174; i++) {
//        if (font->getGlyph(0, i, glyphInfo, &bitmapHeightBits)) {
//          //            if (!font->convertToOneBit(*bitmapHeightBits,
//          //            &bitmapOneBit)) {
//          //              QMessageBox::critical(this, "CRITICAL ERROR",
//          //              "Conversion Error: convertToOneBit() not working properly!");
//          //            }
//          auto gen = new RLEGenerator;
//          if (gen->encodeBitmap(*bitmapHeightBits)) {
//            if ((gen->getData()->size() != glyphInfo->packetLength) ||
//                (gen->getFirstIsBlack() != glyphInfo->rleMetrics.firstIsBlack) ||
//                (gen->getDynF() != glyphInfo->rleMetrics.dynF)) {
//              QMessageBox::critical(
//                  this, "CRITICAL ERROR",
//                  "Encoder error for char code " + QString::number(i) +
//                      " Encoder size: " + QString::number(gen->getData()->size()) +
//                      " Packet Length: " + QString::number(glyphInfo->packetLength));
//            }
//          }
//          delete gen;
//          // delete bitmapOneBit;
//        }
//      }
//      QMessageBox::information(this, "Completed", "Completed");
//    } else {
//      QMessageBox::critical(this, "CRITICAL ERROR", "Unable to load IBMF file " +
//      currentFilePath_);
//    }
//  }
//}

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

  for (int i = 0; i < recentFileActionList_.length(); i++) {
    recentFileActionList_.at(i)->setVisible(false);
    recentFileActionList_.at(i)->setText("");
    recentFileActionList_.at(i)->setData("");
  }
}

// void MainWindow::on_actionTest_Dialog_triggered() {
//   QString filePath =
//       QFileDialog::getOpenFileName(this, "Open TTF Font File", ".", "Font (*.ttf *.otf)");

//  if (!filePath.isEmpty()) {
//    if (ft_ == nullptr) ft_ = new FreeType();
//    QFileInfo     fileInfo(filePath);
//    BlocksDialog *blocksDialog = new BlocksDialog(*ft_, filePath, fileInfo.fileName());
//    if (blocksDialog->exec() == QDialog::Accepted) {
//      auto blockIndexes = blocksDialog->getSelectedBlockIndexes();

//      FontParameterDialog *fontDialog = new FontParameterDialog(*ft_, "TTF Font Import");
//      if (fontDialog->exec() == QDialog::Accepted) {
//        QMessageBox::information(
//            this, "OK", QString("Number of accepter blocks: %1").arg(blockIndexes->size()));
//      }
//    }
//  }
//}

void MainWindow::on_actionImportTrueTypeFont_triggered() {
  if (checkFontChanged()) {
    if (ft_ == nullptr) ft_ = new FreeType();
    TTFFontParameterDialog *fontDialog = new TTFFontParameterDialog(*ft_, "TrueType Font Import");
    if (fontDialog->exec() == QDialog::Accepted) {
      auto fontParameters         = fontDialog->getParameters();

      IBMFTTFImportPtr importFont = IBMFTTFImportPtr(new IBMFTTFImport);

      if (importFont->loadTTF(*ft_, fontParameters)) {
        QFile outFile;
        outFile.setFileName(fontParameters->filename);
        if (outFile.open(QIODevice::WriteOnly)) {
          QDataStream out(&outFile);
          if (importFont->save(out)) {
            outFile.close();
            if (openFont(fontParameters->filename)) {
              QFileInfo info = QFileInfo(fontParameters->filename);
              QMessageBox::information(
                  this, "Import Completed",
                  QString("Import of TTF file to %1 completed!").arg(info.completeBaseName()));
            }
          }
        }
      }
    }
  }
}

void MainWindow::on_editKerningButton_clicked() {
  KerningModel  *model = new KerningModel(ibmfGlyphCode_, ibmfGlyphLigKern_->kernSteps, this);
  KerningDialog *kerningDialog = new KerningDialog(ibmfFont_, ibmfFaceIdx_, model);

  if (kerningDialog->exec() == QDialog::Accepted) {
    model->save(ibmfGlyphLigKern_->kernSteps);
    populateKernTable();
    ui->kernTable->update();
    drawingSpace_->update();
    glyphChanged_ = true;
  }
}

#include "proofingDialog.h"

void MainWindow::on_plainTextEdit_textChanged() {
  drawingSpace_->setText(ui->plainTextEdit->toPlainText());
}

void MainWindow::on_pixelSizeCombo_currentIndexChanged(int index) {
  drawingSpace_->setPixelSize(index + 1);
}

void MainWindow::on_normalKernCheckBox_toggled(bool checked) {
  drawingSpace_->setNormalKerning(checked);
}

void MainWindow::on_autoKernCheckBox_toggled(bool checked) {
  drawingSpace_->setOpticalKerning(checked);
}

void MainWindow::on_actionProofing_Tool_triggered() {
  if (ibmfFont_ != nullptr) {
    ProofingDialog *proofingDialog = new ProofingDialog(ibmfFont_, ibmfFaceIdx_, this);
    proofingDialog->exec();
  }
}

void MainWindow::on_actionC_h_File_triggered() {
  if (ibmfFont_ != nullptr) {
    if (!checkFontChanged()) return;

    releaseKeyboard();
    QString inFilePath = QFileDialog::getOpenFileName(this, "IBMF Font File to Read From",
                                                      currentFilePath_, "*.ibmf");
    grabKeyboard();

    if (!inFilePath.isEmpty()) {

      QFileInfo                 info     = QFileInfo(inFilePath);
      QString                   baseName = info.completeBaseName();
      static QRegularExpression theDateTimeWithExt(
          "_\\d\\d\\d\\d\\d\\d\\d\\d_\\d\\d\\d\\d\\d\\d\\$");
      QRegularExpressionMatch match = theDateTimeWithExt.match(baseName);

      if (match.hasMatch()) { baseName.replace(match.captured(), ""); }

      QString headerFilePath = info.absolutePath() + "/" + baseName + ".h";

      releaseKeyboard();
      QString newFilePath =
          QFileDialog::getSaveFileName(this, "Save C Header Font File", headerFilePath, "*.h");
      grabKeyboard();

      if (!newFilePath.isEmpty()) {
        info                  = QFileInfo(newFilePath);
        QString baseName      = info.completeBaseName();
        QString upperBaseName = info.completeBaseName().toUpper();

        QFile outFile;
        QFile inFile;
        inFile.setFileName(inFilePath);
        outFile.setFileName(newFilePath);

        if (inFile.open(QIODevice::ReadOnly)) {
          QByteArray content = inFile.readAll();
          inFile.close();

          if (outFile.open(QIODevice::WriteOnly)) {
            QTextStream out(&outFile);

            QDateTime UTC(QDateTime::currentDateTimeUtc());

            out << "// ----- IBMF Binary Font " << baseName << " -----" << Qt::endl;
            out << "//" << Qt::endl;
            out << "//  Date: " << UTC.toString() << Qt::endl;
            out << "//" << Qt::endl;
            out << "// Generated from the IBMFFontEditor Version " << IBMF_VERSION << Qt::endl;
            out << "//" << Qt::endl;
            out << Qt::endl;
            out << "#pragma once" << Qt::endl;
            out << Qt::endl;
            out << "const unsigned int " << upperBaseName << "_IBMF_LEN = " << Qt::dec
                << content.size() << ";" << Qt::endl;
            out << "const uint8_t " << upperBaseName << "_IBMF[] = {" << Qt::endl;

            int  count   = 0;
            bool newLine = true;

            for (uint8_t byte : content) {
              if (newLine) {
                out << "   ";
                newLine = false;
              }

              out << " 0x" << QString("%1").arg(+byte, 2, 16, QChar('0')) << ",";

              if (++count == 12) {
                out << Qt::endl;
                newLine = true;
                count   = 0;
              }
            }

            if (count != 0) { out << Qt::endl; }

            out << "};" << Qt::endl;

            outFile.close();

            QMessageBox::information(
                this, "Export Completed",
                QString("Export to a C Header Format of %1 completed!").arg(baseName));

          } else {
            QMessageBox::critical(nullptr, "CRITICAL ERROR",
                                  QString("Unable to write file %1").arg(newFilePath));
          }
        } else {
          QMessageBox::critical(nullptr, "CRITICAL ERROR",
                                QString("Unable to read file %1").arg(inFilePath));
        }
      }
    }
  }
}

void MainWindow::on_zoomToFitButton_clicked() {
  int height = (bitmapRenderer_->height() - 40) / (ibmfFaceHeader_->emSize >> 6);
  int width  = (bitmapRenderer_->width() - 40) / (ibmfFaceHeader_->emSize >> 6);
  int value  = height > width ? width : height;
  ui->pixelSize->setValue(value);
  //  bitmapRenderer_->setPixelSize(value);
  //  setScrollBarSizes(value);

  centerScrollBarPos();
  updateBitmapOffsetPos();
  bitmapRenderer_->update();
}

void MainWindow::on_actionImportHexFont_triggered() {
  if (checkFontChanged()) {
    HexFontParameterDialog *fontDialog = new HexFontParameterDialog("GNU Unifont Import");
    if (fontDialog->exec() == QDialog::Accepted) {
      auto fontParameters         = fontDialog->getParameters();

      IBMFHexImportPtr importFont = IBMFHexImportPtr(new IBMFHexImport);

      if (importFont->loadHex(fontParameters)) {
        QFile outFile;
        outFile.setFileName(fontParameters->filename);
        if (outFile.open(QIODevice::WriteOnly)) {
          QDataStream out(&outFile);
          if (importFont->save(out)) {
            outFile.close();
            if (openFont(fontParameters->filename)) {
              QFileInfo info = QFileInfo(fontParameters->filename);
              QMessageBox::information(
                  this, "Import Completed",
                  QString("Import of GNU Hex file to %1 completed!").arg(info.completeBaseName()));
            }
          }
        }
      }
    }
  }
}

void MainWindow::on_copyButton_clicked() {
  selection_ = bitmapRenderer_->getSelection();
}

void MainWindow::on_pasteButton_clicked() {
  if (selection_ != nullptr) {
    QPoint *atLocation = bitmapRenderer_->getSelectionLocation();
    if (atLocation != nullptr) {
      QRect rect =
          QRect(atLocation->x(), atLocation->y(), selection_->dim.width, selection_->dim.height);
      auto undoSelection = bitmapRenderer_->getSelection(&rect);
      undoStack_->push(
          new PasteSelectionCommand(bitmapRenderer_, selection_, undoSelection, *atLocation));
    }
  }
}

void MainWindow::someSelection(bool some) {
  ui->copyButton->setEnabled(some);
  ui->pasteButton->setEnabled(some && (selection_ != nullptr));
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
  switch (event->key()) {
  case Qt::Key_Left:
    on_leftButton_clicked();
    break;
  case Qt::Key_Right:
    on_rightButton_clicked();
    break;
  case Qt::Key_Copy:
    on_copyButton_clicked();
    break;
  case Qt::Key_Paste:
    on_pasteButton_clicked();
    break;
  default:
    break;
  }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
  if (watched == ui->plainTextEdit) {
    if (event->type() == QEvent::FocusOut) {
      grabKeyboard();
    } else if (event->type() == QEvent::FocusIn) {
      releaseKeyboard();
    }
  }
  return false;
}

void MainWindow::on_pasteMainButton_clicked() {
  if ((ibmfFont_ != nullptr) && ibmfFont_->isInitialized()) {
    IBMFDefs::GlyphInfoPtr    glyphInfo;
    IBMFDefs::BitmapPtr       selection;
    IBMFDefs::GlyphLigKernPtr ligKern;

    int mainCode = getValue(ui->characterMetrics, 9, 1).toUInt();
    // int hOffset  = getValue(ui->characterMetrics, 2, 1).toInt();

    if (ibmfFont_->getGlyph(ibmfFaceIdx_, mainCode, glyphInfo, selection, ligKern)) {
      QPoint origin     = bitmapRenderer_->getOriginPos();
      QPoint atLocation = QPoint(origin.x() - glyphInfo->horizontalOffset,
                                 origin.y() - glyphInfo->verticalOffset - 1);
      QRect  rect =
          QRect(atLocation.x(), atLocation.y(), selection->dim.width, selection->dim.height);
      auto undoSelection = bitmapRenderer_->getSelection(&rect);
      undoStack_->push(
          new PasteSelectionCommand(bitmapRenderer_, selection, undoSelection, atLocation));
    }
  }
}

void MainWindow::on_actionDump_Font_Content_triggered() {
  if ((ibmfFont_ != nullptr) && ibmfFont_->isInitialized()) {
    QString     result;
    QTextStream resultStream(&result);
    QFileInfo   fi(currentFilePath_);
    QString     baseName = fi.baseName();

    ibmfFont_->showFont(resultStream, baseName);

    releaseKeyboard();
    ShowResultDialog *resultDialog =
        new ShowResultDialog("Dump of font " + baseName, baseName, result);
    resultDialog->exec();
    grabKeyboard();
  }
}

void MainWindow::on_actionDump_Modif_Content_triggered() {
  if ((ibmfBackup_ != nullptr) && ibmfBackup_->isInitialized()) {
    QString     result;
    QFileInfo   fi(currentFilePath_);
    QTextStream resultStream(&result);
    QString     baseName = fi.baseName();

    ibmfBackup_->showFont(resultStream, baseName);

    releaseKeyboard();
    ShowResultDialog *resultDialog =
        new ShowResultDialog("Dump of font modifications of " + baseName, baseName, result);
    resultDialog->exec();
    grabKeyboard();
  }
}

void MainWindow::on_actionDump_Font_Content_With_Glyphs_Bitmap_triggered() {
  if ((ibmfFont_ != nullptr) && ibmfFont_->isInitialized()) {
    QString   result;
    QFileInfo fi(currentFilePath_);
    QString   baseName = fi.baseName();

    QTextStream resultStream(&result);
    ibmfFont_->showFont(resultStream, baseName, true);

    releaseKeyboard();
    ShowResultDialog *resultDialog =
        new ShowResultDialog("Dump of font " + baseName, baseName, result);
    resultDialog->exec();
    grabKeyboard();
  }
}

void MainWindow::on_actionDump_Modif_Content_With_Glyphs_Bitmap_triggered() {
  if ((ibmfBackup_ != nullptr) && ibmfBackup_->isInitialized()) {
    QString result;

    QFileInfo fi(currentFilePath_);
    QString   baseName = fi.baseName();

    QTextStream resultStream(&result);
    ibmfBackup_->showFont(resultStream, baseName, true);

    releaseKeyboard();
    ShowResultDialog *resultDialog =
        new ShowResultDialog("Dump of font modifications of " + baseName, baseName, result);
    resultDialog->exec();
    grabKeyboard();
  }
}

void MainWindow::on_actionImport_Modifications_File_triggered() {

  if ((ibmfFont_ != nullptr) && ibmfFont_->isInitialized() && checkFontChanged()) {
    QString   result;
    QFileInfo fi(currentFilePath_);
    releaseKeyboard();
    QString inFilePath = QFileDialog::getOpenFileName(
        this, "IBMF Font Modifications File to Read From",
        fi.absolutePath() + "/" + fi.completeBaseName() + ".ibmf_font", "*.ibmf_mods");
    grabKeyboard();

    if (!inFilePath.isEmpty()) {
      QFile file;
      file.setFileName(inFilePath);
      if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Warning",
                             "Unable to open IBMF Modifications File " + inFilePath);
      } else {
        QByteArray content = file.readAll();
        file.close();
        auto fromBackup =
            IBMFFontModPtr(new IBMFFontMod((uint8_t *) content.data(), content.size()));
        if (fromBackup->isInitialized()) {
          if (fromBackup->getFontFormat() == FontFormat::BACKUP) {
            QTextStream resultStream(&result);
            QFileInfo   fi(currentFilePath_);
            QString     baseName = fi.baseName();

            if (ibmfBackup_ == nullptr) { ibmfBackup_ = IBMFFontMod::createBackup(); }
            ibmfFont_->importModificationsFrom(resultStream, baseName, inFilePath, fromBackup,
                                               ibmfBackup_, ibmfFont_);

            updateCharactersList();

            releaseKeyboard();
            ShowResultDialog *resultDialog =
                new ShowResultDialog("Font Modifications Import Result Log", baseName, result);
            resultDialog->exec();
            grabKeyboard();

            loadGlyph(ibmfGlyphCode_); // It may have changed fromt the import process
            fontChanged_ = true;
          } else {
            QMessageBox::warning(
                this, "Warning",
                "File is not of an appropriate Font Modifications File Format:\n\n" + inFilePath);
          }
        } else {
          QMessageBox::warning(this, "Warning",
                               "Font Modifications File content error:\n\n" + inFilePath);
        }
      }
    }
  }
}

void MainWindow::on_actionBuild_Modifications_File_triggered() {

  if ((ibmfFont_ != nullptr) && ibmfFont_->isInitialized() && checkFontChanged()) {
    QString result;

    releaseKeyboard();
    QString inFilePath = QFileDialog::getOpenFileName(this, "Original IBMF Font File to Read From",
                                                      currentFilePath_, "*.ibmf");
    grabKeyboard();

    if (!inFilePath.isEmpty()) {
      QFile file;
      file.setFileName(inFilePath);
      if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Warning", "Unable to open IBMF Font File " + inFilePath);
      } else {
        QByteArray content = file.readAll();
        file.close();
        auto fromFont = IBMFFontModPtr(new IBMFFontMod((uint8_t *) content.data(), content.size()));
        if (fromFont->isInitialized()) {
          if (fromFont->getFontFormat() == FontFormat::UTF32) {
            QTextStream resultStream(&result);
            QFileInfo   fi(currentFilePath_);
            QString     baseName = fi.baseName();

            ibmfBackup_ = ibmfFont_->buildModificationsFrom(resultStream, fromFont, ibmfFont_);
            if (ibmfBackup_ != nullptr) {
              releaseKeyboard();
              ShowResultDialog *resultDialog =
                  new ShowResultDialog("Font Modifications Import Result Log", baseName, result);
              resultDialog->exec();
              grabKeyboard();

              fontChanged_ = true;
            } else {
              QMessageBox::critical(this, "Critical", "Unable to build Modification Font File!");
            }
          } else {
            QMessageBox::warning(this, "Warning",
                                 "File is not of an appropriate Font File Format:\n\n" +
                                     inFilePath);
          }
        } else {
          QMessageBox::warning(this, "Warning", "Font File content error:\n\n" + inFilePath);
        }
      }
    }
  }
}

void MainWindow::on_addCharacterButton_clicked() {
  if ((ibmfFont_ != nullptr) && ibmfFont_->isInitialized()) {
    char32_t  codePoint = ibmfFont_->addCodePoint(ibmfBackup_, ibmfFont_);
    GlyphCode glyphCode = ibmfFont_->translate(codePoint);

    fontChanged_        = true;
    putValue(ui->faceHeader, 8, 1, ibmfFaceHeader_->glyphCount, false);
    updateCharactersList();
    loadGlyph(glyphCode);
  }
}
