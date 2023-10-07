#pragma once

#include <QAction>
#include <QByteArray>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QList>
#include <QMainWindow>
#include <QMessageBox>
#include <QString>
#include <QStringLiteral>
#include <QTableWidgetItem>
#include <QUndoStack>
#include <QUndoView>

#include "IBMFDriver/IBMFFontMod.hpp"
#include "bitmapRenderer.h"
#include "drawingSpace.h"
#include "freeType.h"

#define IBMF_VERSION "0.90.0"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

protected:
  void closeEvent(QCloseEvent *event);
  void resizeEvent(QResizeEvent *event);
  void paintEvent(QPaintEvent *even);
  void keyPressEvent(QKeyEvent *event);
  bool eventFilter(QObject *watched, QEvent *event);

private slots:
  void on_actionOpen_triggered();
  void on_actionExit_triggered();
  void on_leftButton_clicked();
  void on_rightButton_clicked();
  void on_faceIndex_currentIndexChanged(int index);
  void on_pixelSize_valueChanged(int value);
  void on_charactersList_cellClicked(int row, int column);
  void on_characterMetrics_cellChanged(int row, int column);
  void onfaceHeader__cellChanged(int row, int column);
  void on_glyphForgetButton_clicked();
  void on_faceForgetButton_clicked();
  void on_centerGridButton_clicked();
  void on_leftSplitter_splitterMoved(int pos, int index);
  void on_bitmapVerticalScrollBar_valueChanged(int value);
  void on_bitmapHorizontalScrollBar_valueChanged(int value);
  // void on_actionFont_load_save_triggered();
  // void on_actionRLE_Encoder_triggered();
  void on_actionSave_triggered();
  void on_actionSaveBackup_triggered();
  void on_clearRecentList_triggered();
  void bitmapChanged(const Bitmap &bitmap, const QPoint &originOffsets);
  void setScrollBarSizes(int value);
  void centerScrollBarPos();
  void updateBitmapOffsetPos();
  void openRecent();
  // void on_actionTest_Dialog_triggered();
  void on_actionImportTrueTypeFont_triggered();
  void on_editKerningButton_clicked();
  void on_plainTextEdit_textChanged();
  void on_pixelSizeCombo_currentIndexChanged(int index);
  void on_normalKernCheckBox_toggled(bool checked);
  void on_autoKernCheckBox_toggled(bool checked);
  void on_actionProofing_Tool_triggered();
  void on_actionC_h_File_triggered();
  void on_zoomToFitButton_clicked();
  void on_actionImportHexFont_triggered();
  void on_copyButton_clicked();
  void on_pasteButton_clicked();
  void someSelection(bool some);
  //  void rendererKeyPressed(QKeyEvent *event);
  void on_pasteMainButton_clicked();
  void on_actionDump_Font_Content_triggered();
  void on_actionDump_Modif_Content_triggered();
  void on_actionDump_Font_Content_With_Glyphs_Bitmap_triggered();
  void on_actionDump_Modif_Content_With_Glyphs_Bitmap_triggered();
  void on_actionImport_Modifications_File_triggered();
  void on_actionBuild_Modifications_File_triggered();
  void on_addCharacterButton_clicked();

  void on_beforeMinus1Radio_toggled(bool checked);

  void on_before0Radio_toggled(bool checked);

  void on_before1Radio_toggled(bool checked);

  void on_before2Radio_toggled(bool checked);

  void on_after0Radio_toggled(bool checked);

  void on_after1Radio_toggled(bool checked);

  void on_actionRecompute_Ligatures_triggered();

  private:
  const int MAX_RECENT_FILES = 10;

  Ui::MainWindow *ui;
  QString         currentFilePath_{""};

  FreeType   *ft_{nullptr};
  QUndoStack *undoStack_;
  QUndoView  *undoView_;
  QAction    *undoAction_;
  QAction    *redoAction_;

  bool fontChanged_{false};
  bool faceChanged_{false};
  bool glyphChanged_{false};
  bool initialized_{false};
  bool glyphReloading_{false};
  bool faceReloading_{false};

  BitmapRenderer *bitmapRenderer_;
  BitmapRenderer *smallGlyph_;
  BitmapRenderer *largeGlyph_;

  DrawingSpace *drawingSpace_;

  IBMFFontModPtr            ibmfFont_{nullptr};
  IBMFFontModPtr            ibmfBackup_{nullptr};
  IBMFDefs::Preamble        ibmfPreamble_;
  IBMFDefs::FaceHeaderPtr   ibmfFaceHeader_{nullptr};
  IBMFDefs::GlyphLigKernPtr ibmfGlyphLigKern_{nullptr};

  IBMFDefs::BitmapPtr selection_{nullptr};

  int              ibmfFaceIdx_{0};
  int              ibmfGlyphCode_{0};
  QList<QAction *> recentFileActionList_;

  void     updateCharactersList();
  void     writeSettings();
  void     readSettings();
  void     createUndoView();
  void     createRecentFileActionsAndConnections();
  void     updateRecentActionList();
  void     adjustRecentsForCurrentFile();
  bool     checkFontChanged();
  bool     saveFont(bool askToConfirmName);
  void     newFontLoaded(QString filePath);
  bool     openFont(QString filePath);
  bool     loadFont(QFile &file);
  bool     loadFace(uint8_t faceIdx);
  void     saveFace();
  void     saveGlyph();
  bool     loadGlyph(uint16_t glyphCode);
  void     clearAll();
  void     putValue(QTableWidget *w, int row, int col, QVariant value, bool editable = true);
  void     putColoredValue(QTableWidget *w, int row, int col, QVariant value, bool editable = true);
  void     putFix16Value(QTableWidget *w, int row, int col, QVariant value, bool editable = true);
  void     putColoredFix16Value(QTableWidget *w, int row, int col, QVariant value,
                                bool editable = true);
  QVariant getValue(QTableWidget *w, int row, int col);
  void     clearEditable(QTableWidget *w, int row, int col);
  void     glyphWasChanged(bool initialLoad = false);
  void     populateKernTable();
};
