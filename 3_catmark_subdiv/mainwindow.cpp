#include "mainwindow.h"

#include "initialization/meshinitializer.h"
#include "initialization/objfile.h"
#include "subdivision/catmullclarksubdivider.h"
#include "subdivision/subdivider.h"
#include "ui_mainwindow.h"
#include <QSignalBlocker>

/**
 * @brief MainWindow::MainWindow Creates a new Main Window UI.
 * @param parent Qt parent widget.
 */
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);
  ui->MeshGroupBox->setEnabled(ui->MainDisplay->settings.modelLoaded);
  ui->tessSettingsGroupBox->setEnabled(
      ui->MainDisplay->settings.tesselationMode);

  // Initialize tessellation patch type toggle based on UI defaults
  ui->MainDisplay->settings.useBezierPatch = ui->BezierRadio->isChecked();
  ui->ShowSharpEdgesCheckBox->setChecked(ui->MainDisplay->settings.showSharpEdges);
  
  // Connect edge selection signal
  connect(ui->MainDisplay, &MainView::edgeSelected, this, &MainWindow::onEdgeSelected);
  // Connect vertex selection signal
  connect(ui->MainDisplay, &MainView::vertexSelected, this, &MainWindow::onVertexSelected);
}

/**
 * @brief MainWindow::~MainWindow Deconstructs the main window.
 */
MainWindow::~MainWindow() {
  delete ui;

  meshes.clear();
  meshes.squeeze();
}

/**
 * @brief MainWindow::importOBJ Imports an obj file and adds the constructed
 * half-edge to the collection of meshes.
 * @param fileName Path of the .obj file.
 */
void MainWindow::importOBJ(const QString& fileName) {
  OBJFile newModel = OBJFile(fileName);
  meshes.clear();
  meshes.squeeze();

  if (newModel.loadedSuccessfully()) {
    MeshInitializer meshInitializer;
    meshes.append(meshInitializer.constructHalfEdgeMesh(newModel));
    
    // Set crease edges for specific models
    if (fileName.contains("CreaseCube", Qt::CaseInsensitive)) {
      setupCreaseCube(meshes[0]);
    } else if (fileName.contains("CreaseSquare", Qt::CaseInsensitive)) {
      setupCreaseSquare(meshes[0]);
    } else if (fileName.contains("CreaseOctahedron", Qt::CaseInsensitive)) {
      setupCreaseOctahedron(meshes[0]);
    }
    
    ui->MainDisplay->updateBuffers(meshes[0]);
    ui->MainDisplay->setCurrentMesh(&meshes[0]);
    ui->MainDisplay->settings.modelLoaded = true;
  } else {
    ui->MainDisplay->settings.modelLoaded = false;
  }

  ui->MeshGroupBox->setEnabled(ui->MainDisplay->settings.modelLoaded);
  ui->SubdivSteps->setValue(0);
  ui->MainDisplay->update();
}

// Don't worry about adding documentation for the UI-related functions.

void MainWindow::on_LoadOBJ_pressed() {
  QString filename = QFileDialog::getOpenFileName(
      this, "Import OBJ File", "../", tr("Obj Files (*.obj)"));
  importOBJ(filename);
}

void MainWindow::on_MeshPresetComboBox_currentTextChanged(
    const QString& meshName) {
  importOBJ(":/models/" + meshName + ".obj");
}

  void MainWindow::on_SubdivSteps_valueChanged(int value) {
    ui->MainDisplay->settings.subdivisionLevel = value;
    // Clear edge and vertex selection when subdividing (selected edges/vertices may point to different mesh)
    const QSignalBlocker edgeSharpnessBlocker(ui->EdgeSharpness);
    ui->MainDisplay->clearEdgeSelection();
    ui->MainDisplay->clearVertexSelection();
    Subdivider* subdivider = new CatmullClarkSubdivider();
    for (int k = meshes.size() - 1; k < value; k++) {
      meshes.append(subdivider->subdivide(meshes[k]));
    }
    ui->MainDisplay->updateBuffers(meshes[value]);
    ui->MainDisplay->setCurrentMesh(&meshes[value]);
    delete subdivider;
  }

void MainWindow::on_TessellationCheckBox_toggled(bool checked) {
  ui->MainDisplay->settings.tesselationMode = checked;
  ui->tessSettingsGroupBox->setEnabled(checked);
  ui->MainDisplay->settings.uniformUpdateRequired = true;
  ui->MainDisplay->update();
}

void MainWindow::on_HideMeshCheckBox_toggled(bool checked) {
  // Useful for clearly seeing only the patches rendered by the Tessellation
  // shaders.
  ui->MainDisplay->settings.showCpuMesh = !checked;
  ui->MainDisplay->settings.uniformUpdateRequired = true;
  ui->MainDisplay->update();
}

void MainWindow::on_LimitPositionCheckBox_toggled(bool checked) {
    ui->MainDisplay->settings.showLimitPosition = checked;
    ui->MainDisplay->updateBuffers(meshes[ui->SubdivSteps->value()]);
    ui->MainDisplay->update();
}

void MainWindow::on_ShowSharpEdgesCheckBox_toggled(bool checked) {
    ui->MainDisplay->settings.showSharpEdges = checked;
    ui->MainDisplay->settings.uniformUpdateRequired = true;
    ui->MainDisplay->update();
}

void MainWindow::on_ShowVerticesCheckBox_toggled(bool checked) {
    ui->MainDisplay->settings.showVertices = checked;
    ui->MainDisplay->settings.uniformUpdateRequired = true;
    ui->MainDisplay->update();
}

void MainWindow::onEdgeSelected(float sharpness) {
  if (sharpness == -1.0f) {
    // Infinite sharpness
    ui->EdgeSharpness->setValue(-1.0);
  } else if (sharpness >= 0.0f) {
    ui->EdgeSharpness->setValue(sharpness);
  } else {
    ui->EdgeSharpness->setValue(0.0);
  }
}

void MainWindow::on_EdgeSharpness_valueChanged(double sharpness) {
    ui->MainDisplay->updateSharpness(static_cast<float>(sharpness));
    meshes.resize(ui->MainDisplay->settings.subdivisionLevel + 1);
}

void MainWindow::onVertexSelected(int sharpEdgeCount) {
  if (sharpEdgeCount == -1) {
    // No selection
    ui->VertexSharpEdgeCountLabel->setText("-");
  } else if (sharpEdgeCount == -999) {
    // Boundary vertex
    ui->VertexSharpEdgeCountLabel->setText("boundary");
  } else {
    ui->VertexSharpEdgeCountLabel->setText(QString::number(sharpEdgeCount));
  }
}

void MainWindow::on_BezierRadio_toggled(bool checked) {
  if (checked) {
    ui->MainDisplay->settings.useBezierPatch = true;
    ui->MainDisplay->settings.uniformUpdateRequired = true;
    ui->MainDisplay->update();
  }
}

void MainWindow::on_BSplineRadio_toggled(bool checked) {
  if (checked) {
    ui->MainDisplay->settings.useBezierPatch = false;
    ui->MainDisplay->settings.uniformUpdateRequired = true;
    ui->MainDisplay->update();
  }
}

/**
 * @brief MainWindow::setupCreaseCube Sets up crease edges on a cube model.
 * Sets the top face edges (around z=0.5) as creases with different sharpness values
 * to demonstrate semi-sharp creases. This matches the example from Figure 7 in the paper.
 * @param mesh The cube mesh to set crease edges on.
 */
void MainWindow::setupCreaseCube(Mesh &mesh) {
  // Cube vertex layout (0-indexed after OBJ 1-based to 0-based conversion):
  // 0: (-0.5, -0.5, -0.5) bottom-left-back
  // 1: (-0.5, -0.5,  0.5) bottom-left-front
  // 2: (-0.5,  0.5, -0.5) top-left-back
  // 3: (-0.5,  0.5,  0.5) top-left-front
  // 4: ( 0.5, -0.5, -0.5) bottom-right-back
  // 5: ( 0.5, -0.5,  0.5) bottom-right-front
  // 6: ( 0.5,  0.5, -0.5) top-right-back
  // 7: ( 0.5,  0.5,  0.5) top-right-front
  
  // Set top face edges as creases (edges connecting vertices 2, 3, 6, 7)
  // Top face edges form a square: 2-3-7-6
  // (top-left-back, top-left-front, top-right-front, top-right-back)
  
  // Top front edge (3-7): sharpness 2
  float sharpness = 3.0f;
  mesh.setCreaseEdge(3, 2, sharpness);
  mesh.setCreaseEdge(2, 6, sharpness);
  mesh.setCreaseEdge(6, 7, sharpness);
  mesh.setCreaseEdge(7, 3, sharpness);

  mesh.setCreaseEdge(0, 1, sharpness);
  mesh.setCreaseEdge(1, 5, sharpness);
  mesh.setCreaseEdge(5, 4, sharpness);
  mesh.setCreaseEdge(4, 0, sharpness);

}

/**
 * @brief MainWindow::setupCreaseSquare Sets up crease edges on a 2D square model
 * for easy visualization of crease rules. The square is flat (z=0) making it easy
 * to see the subdivision behavior.
 * @param mesh The square mesh to set crease edges on.
 */
void MainWindow::setupCreaseSquare(Mesh &mesh) {
  // Set top edge (2-3) as crease with sharpness 2
  // This edge will use sharp rules for 2 subdivision steps, then become smooth
  mesh.setCreaseEdge(1, 2, -1);
  mesh.setCreaseEdge(2, 3, -1);
  mesh.setCreaseEdge(3, 0, -1);
  mesh.setCreaseEdge(0, 1, -1);
}


/**
 * @brief MainWindow::setupCreaseOctahedron Sets up crease edges on a 3D octahedron model
 * for visualization of crossing crease rules.
 * @param mesh The octahedron mesh to set crease edges on.
 */
void MainWindow::setupCreaseOctahedron(Mesh &mesh) {
    // Set top edge (2-3) as crease with sharpness 2
    // This edge will use sharp rules for 2 subdivision steps, then become smooth
    mesh.setCreaseEdge(1, 2, 4);
    mesh.setCreaseEdge(3, 0, 4);
    mesh.setCreaseEdge(3, 1, 4);
    mesh.setCreaseEdge(2, 0, 4);

    mesh.setCreaseEdge(0, 4, 2);
    mesh.setCreaseEdge(4, 1, 2);
    mesh.setCreaseEdge(1, 5, 2);
    mesh.setCreaseEdge(5, 0, 2);
}
