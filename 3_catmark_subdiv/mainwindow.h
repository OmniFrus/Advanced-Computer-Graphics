#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QFileDialog>
#include <QMainWindow>

#include "mesh/mesh.h"
#include "subdivision/subdivider.h"

namespace Ui {
class MainWindow;
}

/**
 * @brief The MainWindow class represents the main window.
 */
class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

 private slots:
  void on_LoadOBJ_pressed();
  void on_MeshPresetComboBox_currentTextChanged(const QString &meshName);
  void on_SubdivSteps_valueChanged(int subdivLevel);
  void on_TessellationCheckBox_toggled(bool checked);
  void on_EdgeSharpness_valueChanged(double sharpness);

  void on_HideMeshCheckBox_toggled(bool checked);
  void on_LimitPositionCheckBox_toggled(bool checked);
  void on_ShowSharpEdgesCheckBox_toggled(bool checked);
  void on_ShowVerticesCheckBox_toggled(bool checked);
  void on_BezierRadio_toggled(bool checked);
  void on_BSplineRadio_toggled(bool checked);
  
  void onEdgeSelected(float sharpness);  // Slot for edge selection signal
  void onVertexSelected(int sharpEdgeCount);  // Slot for vertex selection signal

 private:
  void importOBJ(const QString &fileName);
  void setupCreaseCube(Mesh &mesh);  // Sets up crease edges on a cube model
  void setupCreaseSquare(Mesh &mesh);  // Sets up crease edges on a 2D square model
  void setupCreaseOctahedron(Mesh &mesh);
  Ui::MainWindow *ui;
  Subdivider *subdivider;
  QVector<Mesh> meshes;
};

#endif  // MAINWINDOW_H
