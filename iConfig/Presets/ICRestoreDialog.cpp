/*
;iConfig source code and documentation is released under a GPLv3 license. 
;
; A copy is available from the Open Source Initiative site at:
;	https://opensource.org/licenses/gpl-3.0.html
*/

#include "ICRestoreDialog.h"
#include "ui_ICRestoreDialog.h"
#include "DeviceInfo.h"
#include "MainWindow.h"

#include <QFileDialog>
#include <QDesktopServices>
#include <QCryptographicHash>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDebug>


ICRestoreDialog::ICRestoreDialog(DeviceInfoPtr device, QWidget *parent) :
  currentDevice(device),
  QDialog(parent),
  ui(new Ui::ICRestoreDialog)
{
  qDebug() << "ICRestoreDialog is open";
  ui->setupUi(this);
  listModel = new QStandardItemModel();
  fileName = "";
  loadFiles();
  setModal(true);
}

ICRestoreDialog::~ICRestoreDialog()
{
  delete ui;
}

void ICRestoreDialog::loadFiles() {
  QDir::root().mkpath(
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
      "/presets");
  QDir presetsDir = QDir(
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
      "/presets");

  qDebug() << "Preset Device:" << MainWindow::extensionForPID(currentDevice->getPID());

  presetsDir.setNameFilters(QStringList()<<("*" + MainWindow::extensionForPID(currentDevice->getPID())));

  QStringList fileList = presetsDir.entryList();

  foreach (QString file, fileList) {
    ui->listFiles->addItem(file);
  }

  connect(ui->listFiles,SIGNAL(itemSelectionChanged()),
          this, SLOT(handleSelectionChanged()));
}

void ICRestoreDialog::handleSelectionChanged() {
  if(ui->listFiles->selectedItems().isEmpty()) {
    ui->textEditDescription->setText("");
  }
  else {
    loadDescription(ui->listFiles->selectedItems().first()->text());
  }
}

void ICRestoreDialog::loadDescription(const QString index) {
  const QString selectedFile =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
      "/presets/" + index;

  QFile file(selectedFile);
  if (!file.open(QFile::ReadOnly)) {
    QMessageBox::warning(this, tr("Read Failed"),
                         tr("Cannot read file %1:\n%2.").arg(selectedFile)
                         .arg(file.errorString()));
    return;
  }
  else {
    QByteArray qData = file.readAll();
    file.close();

    bool valid = qData.size() >= 21 && qData.left(3) == "iCM" &&
        static_cast<unsigned char>(qData.at(3)) == currentDevice->getPID();
    const unsigned char version =
        valid ? static_cast<unsigned char>(qData.at(4)) : 0;
    valid = valid && (version == 1 || version == 2);

    if (valid) {
      const QByteArray expectedHash = qData.right(16);
      const QByteArray actualHash = QCryptographicHash::hash(
          qData.left(qData.size() - 16), QCryptographicHash::Md5);
      valid = expectedHash == actualHash;
    }

    QString description;
    if (valid && version == 2) {
      valid = qData.size() >= 22;
      const int descSize = valid
          ? static_cast<unsigned char>(qData.at(5)) : 0;
      valid = valid && (6 + descSize <= qData.size() - 16);
      if (valid) {
        description = QString::fromLatin1(qData.constData() + 6, descSize);
      }
    }

    if (valid) {
      fileName = selectedFile;
      ui->textEditDescription->setText(description);
    } else {
      fileName.clear();
      ui->textEditDescription->setText(
          tr("Invalid or incompatible preset."));
    }
  }
}

void ICRestoreDialog::accept() {
  qDebug() << "Open Preset Filename:" << fileName;
  if (fileName == "") {
    QMessageBox::warning(this, "Open Preset", "You need to select a preset to open");
    return;
  }
  QDialog::accept();
}

void ICRestoreDialog::reject() {
  QDialog::reject();
}

QString ICRestoreDialog::getFileName() {
  return fileName;
}
