#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <iostream>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
  initialized = false;
  ui->setupUi(this);

  currentFilename = "";
  fontChanged     = false;
  glyphChanged    = false;
  faceChanged     = false;
  glyphReloading  = false;
  faceReloading   = false;

  setWindowTitle("IBMF Font Editor");

  bitmapRenderer = new BitmapRenderer(ui->bitmapFrame, 20, false);
  ui->bitmapFrame->layout()->addWidget(bitmapRenderer);

  QObject::connect(bitmapRenderer, &BitmapRenderer::bitmapHasChanged , this, &MainWindow::bitmapChanged);

  smallGlyph = new BitmapRenderer(ui->smallGlyphPreview, 2, true);
  ui->smallGlyphPreview->layout()->addWidget(smallGlyph);
  smallGlyph->connectTo(bitmapRenderer);

  largeGlyph = new BitmapRenderer(ui->largeGlyphPreview, 4, true);
  ui->largeGlyphPreview->layout()->addWidget(largeGlyph);
  largeGlyph->connectTo(bitmapRenderer);

  centerScrollBarPos();

  ibmfFont = nullptr;
  ibmfGlyphCode = 0;

  // All captions in the tables are not editable
  for (int row = 0; row < ui->fontHeader->rowCount(); row++) {
    this->clearEditable(ui->fontHeader, row, 0);
  }

  for (int row = 0; row < ui->faceHeader->rowCount(); row++) {
    this->clearEditable(ui->faceHeader, row, 0);
  }

  for (int row = 0; row < ui->characterMetrics->rowCount(); row++) {
    this->clearEditable(ui->characterMetrics, row, 0);
  }

  // Initialize the characters table
  ui->charactersList->setColumnCount(5);
  ui->charactersList->setRowCount((174 + 4) / 5);

  int idx = 0;
  for (int row = 0; row < ui->charactersList->rowCount(); row++) {
    ui->charactersList->setRowHeight(row, 50);
    for (int col = 0; col < 5; col++) {
      auto item = new QTableWidgetItem;
      item->setFlags(item->flags() & ~ Qt::ItemIsEditable);
      item->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
      if (idx < 174) {
          item->setData(Qt::EditRole, characterCodes[idx]);
          item->setToolTip(QString::number(idx));
      }
      else {
          item->setFlags(Qt::NoItemFlags);
      }
      ui->charactersList->setItem(row, col, item);
      idx += 1;
    }
  }

  // Set tables columns widths
  ui->fontHeader->setColumnWidth(0, 120);
  ui->faceHeader->setColumnWidth(0, 120);
  ui->characterMetrics->setColumnWidth(0, 120);
  ui->fontHeader->setColumnWidth(1, 100);
  ui->faceHeader->setColumnWidth(1, 100);
  ui->characterMetrics->setColumnWidth(1, 100);

  // clear all data
  this->clearAll();

  initialized = true;
}

MainWindow::~MainWindow()
{
  delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  if (!checkFontChanged()) event->ignore();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
  setScrollBarSizes(bitmapRenderer->getPixelSize());
  updateBitmapOffsetPos();

  adjustCharactersListColumns();
}

void MainWindow::bitmapChanged(Bitmap & bitmap)
{
  if (ibmfFont != nullptr) {
    fontChanged = true;
    glyphChanged = true;
  }
}

bool MainWindow::checkFontChanged()
{
  if (fontChanged) {
    saveGlyph();
    QMessageBox::StandardButton button = QMessageBox::question(
            this, "File Changed", "File was changed. Do you want to save it ?",
            QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel);
    if (button == QMessageBox::Yes) {
      QMessageBox::information(this, "Saving....", "Saving the file...");
    }
    else {
      return button != QMessageBox::Cancel;
    }

    fontChanged = false;
    currentFilename = "";
    setWindowTitle("IBMF Font Editor");
  }
  return true;
}

void MainWindow::on_actionOpen_triggered()
{
  if (!checkFontChanged()) return;

  QString filename = QFileDialog::getOpenFileName(this, "Open IBMF Font File");
  file.setFileName(filename);
  currentFilename = filename;
  fontChanged = false;

  if (!file.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, "Warning", "Unable to open file " + file.errorString());
  }
  else {
    if (loadFont()) {
      setWindowTitle("IBMF Font Editor - " + filename);
    }
    else {
      QMessageBox::warning(this, "Warning", "Unable to load IBMF file " + currentFilename);
    }
    file.close();
  }
}

void MainWindow::clearAll()
{
  ui->faceIndex->clear();
  bitmapRenderer->clear_and_emit(); // Will clear other renderers through signaling
}

void MainWindow::on_actionExit_triggered()
{
  if (checkFontChanged()) QApplication::exit();
}

void MainWindow::clearEditable(QTableWidget * w, int row, int col )
{
  auto item = w->item(row, col);
  item->setFlags(item->flags() & ~ Qt::ItemIsEditable);
}

void MainWindow::putValue(QTableWidget * w, int row, int col, QVariant value, bool editable)
{
  auto item = new QTableWidgetItem();
  item->setData(Qt::EditRole, value);
  if (!editable) item->setFlags(item->flags() & ~ Qt::ItemIsEditable);
  w->setItem(row, col, item);
}

void MainWindow::putFix16Value(QTableWidget * w, int row, int col, QVariant value, bool editable)
{
  auto item = new QTableWidgetItem();
  item->setData(Qt::EditRole, value);
  if (!editable) item->setFlags(item->flags() & ~ Qt::ItemIsEditable);
  w->setItem(row, col, item);
  QAbstractItemDelegate * delegate;
  if ((delegate = w->itemDelegateForRow(row)) != nullptr) delete delegate;
  w->setItemDelegateForRow(row, new Fix16Delegate);
}

QVariant MainWindow::getValue(QTableWidget * w, int row, int col)
{
  return w->item(row, col)->data(Qt::DisplayRole);
}

bool MainWindow::loadFont()
{
  QByteArray content = file.readAll();
  file.close();
  clearAll();
  ibmfFont = IBMFFontModPtr(new IBMFFontMod((uint8_t *) content.data(), content.size()));
  if (ibmfFont->is_initialized()) {
    ibmfPreamble = ibmfFont->get_preample();

    char marker[5];
    memcpy(marker, ibmfPreamble.marker, 4);
    marker[4] = 0;

    putValue(ui->fontHeader, 0, 1, QByteArray(marker),         false);
    putValue(ui->fontHeader, 1, 1, ibmfPreamble.face_count,    false);
    putValue(ui->fontHeader, 2, 1, ibmfPreamble.bits.version,  false);
    putValue(ui->fontHeader, 3, 1, ibmfPreamble.bits.char_set, false);

    for (int i = 0; i < ibmfPreamble.face_count; i++) {
      IBMFFontMod::FaceHeaderPtr face_header = ibmfFont->get_face_header(i);
      ui->faceIndex->addItem(QString::number(face_header->point_size).append(" pts"));
    }

    loadFace(0);

    fontChanged = false;
    faceChanged = false;
    glyphChanged = false;
  }
  return ibmfFont->is_initialized();
}

void MainWindow::saveFace()
{
  if ((ibmfFont != nullptr) && ibmfFont->is_initialized() && faceChanged) {
    IBMFFontMod::FaceHeader face_header;

    face_header.point_size         = getValue(ui->faceHeader,  0, 1).toUInt();
    face_header.line_height        = getValue(ui->faceHeader,  1, 1).toUInt();
    face_header.dpi                = getValue(ui->faceHeader,  2, 1).toUInt();
    face_header.x_height           = getValue(ui->faceHeader,  3, 1).toFloat() * 64.0;
    face_header.em_size            = getValue(ui->faceHeader,  4, 1).toFloat() * 64.0;
    face_header.slant_correction   = getValue(ui->faceHeader,  5, 1).toFloat() * 64.0;
    face_header.descender_height   = getValue(ui->faceHeader,  6, 1).toUInt();
    face_header.space_size         = getValue(ui->faceHeader,  7, 1).toUInt();
    face_header.glyph_count        = getValue(ui->faceHeader,  8, 1).toUInt();
    face_header.lig_kern_pgm_count = getValue(ui->faceHeader,  9, 1).toUInt();
    face_header.kern_count         = getValue(ui->faceHeader, 10, 1).toUInt();

    ibmfFont->save_face_header(ibmfFaceIdx, face_header);
    faceChanged = false;
  }
}

bool MainWindow::loadFace(uint8_t face_idx)
{
  saveFace();
  if ((ibmfFont != nullptr) && (ibmfFont->is_initialized()) && (face_idx < ibmfPreamble.face_count)) {
    ibmfFaceHeader = ibmfFont->get_face_header(face_idx);

    faceReloading = true;

    putValue(ui->faceHeader,  0, 1, ibmfFaceHeader->point_size,                       false);
    putValue(ui->faceHeader,  1, 1, ibmfFaceHeader->line_height);
    putValue(ui->faceHeader,  2, 1, ibmfFaceHeader->dpi);
    putFix16Value(ui->faceHeader,  3, 1, (float) ibmfFaceHeader->x_height / 64.0);
    putFix16Value(ui->faceHeader,  4, 1, (float) ibmfFaceHeader->em_size / 64.0);
    putFix16Value(ui->faceHeader,  5, 1, (float) ibmfFaceHeader->slant_correction / 64.0);
    putValue(ui->faceHeader,  6, 1, ibmfFaceHeader->descender_height);
    putValue(ui->faceHeader,  7, 1, ibmfFaceHeader->space_size);
    putValue(ui->faceHeader,  8, 1, ibmfFaceHeader->glyph_count,                      false);
    putValue(ui->faceHeader,  9, 1, ibmfFaceHeader->lig_kern_pgm_count,               false);
    putValue(ui->faceHeader, 10, 1, ibmfFaceHeader->kern_count,                       false);

    faceReloading = false;

    ibmfFaceIdx = face_idx;

    loadGlyph(ibmfGlyphCode);
  }
  else {
    return false;
  }

  return true;
}

void MainWindow::saveGlyph()
{
  if ((ibmfFont != nullptr) && ibmfFont->is_initialized() && glyphChanged) {
    Bitmap * theBitmap;
    IBMFFontMod::GlyphInfo glyph_info;
    if (bitmapRenderer->retrieveBitmap(&theBitmap)) {

      glyph_info.char_code                   = getValue(ui->characterMetrics,  0, 1).toUInt();
      glyph_info.bitmap_width                = theBitmap->dim.width;
      glyph_info.bitmap_height               = theBitmap->dim.height;
      glyph_info.horizontal_offset           = getValue(ui->characterMetrics,  3, 1).toInt();
      glyph_info.vertical_offset             = getValue(ui->characterMetrics,  4, 1).toInt();
      glyph_info.lig_kern_pgm_index          = getValue(ui->characterMetrics,  5, 1).toUInt();
      glyph_info.packet_length               = getValue(ui->characterMetrics,  6, 1).toUInt();
      glyph_info.advance                     = getValue(ui->characterMetrics,  7, 1).toFloat() * 64.0;
      glyph_info.glyph_metric.dyn_f          = getValue(ui->characterMetrics,  8, 1).toUInt();
      glyph_info.glyph_metric.first_is_black = getValue(ui->characterMetrics,  9, 1).toUInt();

      ibmfFont->save_glyph(ibmfFaceIdx, ibmfGlyphCode, &glyph_info, theBitmap);
      glyphChanged = false;
    }
  }
}

bool MainWindow::loadGlyph(uint16_t glyph_code)
{
  saveGlyph();
  if ((ibmfFont != nullptr) &&
      (ibmfFont->is_initialized()) &&
      (ibmfFaceIdx < ibmfPreamble.face_count) &&
      (glyph_code < ibmfFaceHeader->glyph_count)) {

    if (ibmfFont->get_glyph(ibmfFaceIdx, glyph_code, ibmfGlyphInfo, &ibmfGlyphBitmap)) {
      ibmfGlyphCode = glyph_code;

      glyphChanged = false;
      glyphReloading = true;

      putValue(ui->characterMetrics,  0, 1, ibmfGlyphInfo->char_code,                   false);
      putValue(ui->characterMetrics,  1, 1, ibmfGlyphInfo->bitmap_width,                false);
      putValue(ui->characterMetrics,  2, 1, ibmfGlyphInfo->bitmap_height,               false);
      putValue(ui->characterMetrics,  3, 1, ibmfGlyphInfo->horizontal_offset);
      putValue(ui->characterMetrics,  4, 1, ibmfGlyphInfo->vertical_offset);
      putValue(ui->characterMetrics,  5, 1, ibmfGlyphInfo->lig_kern_pgm_index,          false);
      putValue(ui->characterMetrics,  6, 1, ibmfGlyphInfo->packet_length,               false);
      putFix16Value(ui->characterMetrics,  7, 1, (float) ibmfGlyphInfo->advance / 64.0);
      putValue(ui->characterMetrics,  8, 1, ibmfGlyphInfo->glyph_metric.dyn_f,          false);
      putValue(ui->characterMetrics,  9, 1, ibmfGlyphInfo->glyph_metric.first_is_black, false);

      centerScrollBarPos();
      bitmapRenderer->clearAndLoadBitmap(*ibmfGlyphBitmap);
      smallGlyph->clearAndLoadBitmap(*ibmfGlyphBitmap);
      largeGlyph->clearAndLoadBitmap(*ibmfGlyphBitmap);

      ui->ligTable->clearContents();
      ui->kernTable->clearContents();

      if (ibmfFont->get_glyph_lig_kern(ibmfFaceIdx, ibmfGlyphCode, &ibmfLigKerns)) {
        ui->ligTable->setRowCount(ibmfLigKerns->lig_steps.size());
        for (int i = 0; i < ibmfLigKerns->lig_steps.size(); i++) {
          putValue(ui->ligTable, i, 0, QChar(characterCodes[ibmfLigKerns->lig_steps[i]->next_char_code]));
          putValue(ui->ligTable, i, 1, QChar(characterCodes[ibmfLigKerns->lig_steps[i]->char_code]));
          ui->ligTable->item(i, 0)->setToolTip(QString::number(ibmfLigKerns->lig_steps[i]->next_char_code));
          ui->ligTable->item(i, 1)->setToolTip(QString::number(ibmfLigKerns->lig_steps[i]->char_code));
        }
        ui->kernTable->setRowCount(ibmfLigKerns->kern_steps.size());
        for (int i = 0; i < ibmfLigKerns->kern_steps.size(); i++) {
          putValue(ui->kernTable, i, 0, QChar(characterCodes[ibmfLigKerns->kern_steps[i]->next_char_code]));
          putFix16Value(ui->kernTable, i, 1, (float) ibmfLigKerns->kern_steps[i]->kern / 64.0);
          ui->kernTable->item(i, 0)->setToolTip(QString::number(ibmfLigKerns->kern_steps[i]->next_char_code));
        }
      }

      ui->charactersList->setCurrentCell(glyph_code / 5, glyph_code % 5);

      glyphReloading = false;
    }
    else {
      return false;
    }
  }
  else {
    return false;
  }

  return true;
}

void MainWindow::on_leftButton_clicked()
{
  if ((ibmfFont == nullptr) || !ibmfFont->is_initialized()) return;

  uint16_t glyph_code = ibmfGlyphCode;

  if (glyph_code > 0) {
    glyph_code -= 1;
  }
  else {
    glyph_code = ibmfFaceHeader->glyph_count - 1;
  }
  loadGlyph(glyph_code);
}


void MainWindow::on_rightButton_clicked()
{
  if ((ibmfFont == nullptr) || !ibmfFont->is_initialized()) return;

  uint16_t glyphCode = ibmfGlyphCode;

  if (glyphCode >= ibmfFaceHeader->glyph_count - 1) {
    glyphCode = 0;
  }
  else {
    glyphCode += 1;
  }
  loadGlyph(glyphCode);
}


void MainWindow::on_faceIndex_currentIndexChanged(int index)
{
  if ((ibmfFont == nullptr) || !ibmfFont->is_initialized()) return;

  loadFace(index);
}

void MainWindow::setScrollBarSizes(int pixelSize)
{
    ui->bitmapHorizontalScrollBar->setPageStep((ui->bitmapFrame->width() / pixelSize) * ((float)ui->bitmapHorizontalScrollBar->maximum() / BitmapRenderer::BITMAP_WIDTH));
    ui->bitmapVerticalScrollBar->setPageStep((ui->bitmapFrame->height() / pixelSize) * ((float)ui->bitmapVerticalScrollBar->maximum() / BitmapRenderer::BITMAP_HEIGHT));
}

void MainWindow::centerScrollBarPos()
{
    ui->bitmapHorizontalScrollBar->setValue(ui->bitmapHorizontalScrollBar->maximum() / 2);
    ui->bitmapVerticalScrollBar->setValue(ui->bitmapVerticalScrollBar->maximum() / 2);
}

void MainWindow::updateBitmapOffsetPos()
{
  QPoint pos = QPoint((float) ui->bitmapHorizontalScrollBar->value()
                          / ui->bitmapHorizontalScrollBar->maximum()
                          * BitmapRenderer::BITMAP_WIDTH
                          - ((bitmapRenderer->width() / bitmapRenderer->getPixelSize()) / 2),
                      (float) ui->bitmapVerticalScrollBar->value()
                          / ui->bitmapVerticalScrollBar->maximum()
                          * BitmapRenderer::BITMAP_HEIGHT
                          - ((bitmapRenderer->height() / bitmapRenderer->getPixelSize()) / 2) );

  int maxX = BitmapRenderer::BITMAP_WIDTH - (bitmapRenderer->width() / bitmapRenderer->getPixelSize());
  int maxY = BitmapRenderer::BITMAP_HEIGHT - (bitmapRenderer->height() / bitmapRenderer->getPixelSize());

  if (pos.x() < 0) pos.setX(0);
  if (pos.y() < 0) pos.setY(0);
  if (pos.x() > maxX) pos.setX(maxX);
  if (pos.y() > maxY) pos.setY(maxY);

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

  std::cout << "BitmapRender Size: " << bitmapRenderer->width() << ", " << bitmapRenderer->height() << std::endl;
  std::cout << "BitmapRender Pixel Size: " << bitmapRenderer->getPixelSize() << std::endl;
  std::cout << "BitmapOffsetPos = " << pos.x() << ", " << pos.y() << std::endl;

  bitmapRenderer->setBitmapOffsetPos(pos);
}

void MainWindow::on_pixelSize_valueChanged(int value)
{
  bitmapRenderer->setPixelSize(ui->pixelSize->value());

  setScrollBarSizes(value);
}


void MainWindow::on_charactersList_cellClicked(int row, int column)
{
  int idx = (row * 5) + column;
  if (ibmfFont != nullptr) {
    loadGlyph(idx);
  }
  else {
    ibmfGlyphCode = idx; // Retain the selection for when a font will be loaded
  }
}

void MainWindow::on_characterMetrics_cellChanged(int row, int column)
{
  if ((ibmfFont != nullptr) && ibmfFont->is_initialized() && initialized && !glyphReloading) {
    glyphChanged = true;
  }
}

void MainWindow::on_faceHeader_cellChanged(int row, int column)
{
  if ((ibmfFont != nullptr) && ibmfFont->is_initialized() && initialized && !faceReloading) {
    faceChanged = true;
  }

}

void MainWindow::on_glyphForgetButton_clicked()
{
  if (glyphChanged) {
    QMessageBox::StandardButton button = QMessageBox::question(
            this, "Character has been Modified",
            "You will loose all changes to the bitmap and metrics. Are-you sure?",
            QMessageBox::Yes|QMessageBox::No);
    if (button == QMessageBox::Yes) {
      glyphChanged = false;
      loadGlyph(ibmfGlyphCode);
    }
  }
}

void MainWindow::on_faceForgetButton_clicked()
{
  if (faceChanged) {
    QMessageBox::StandardButton button = QMessageBox::question(
            this, "Face Header Modified",
            "You will loose all changes to the Face Header. Are-you sure?",
            QMessageBox::Yes|QMessageBox::No);
    if (button == QMessageBox::Yes) {
      faceChanged = false;
      loadFace(ibmfFaceIdx);
    }
  }
}

void MainWindow::adjustCharactersListColumns() {
    for (int col = 0; col < 5; col++) {
        ui->charactersList->setColumnWidth(col, (ui->charactersList->width() - 22) / 5);
    }
}

void MainWindow::on_pushButton_clicked()
{
    centerScrollBarPos();
    updateBitmapOffsetPos();
}


void MainWindow::on_leftSplitter_splitterMoved(int pos, int index)
{
    adjustCharactersListColumns();
}


void MainWindow::on_bitmapVerticalScrollBar_valueChanged(int value)
{
    updateBitmapOffsetPos();
}

void MainWindow::on_bitmapHorizontalScrollBar_valueChanged(int value)
{
    updateBitmapOffsetPos();
}


