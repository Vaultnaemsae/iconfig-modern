/*
;iConfig source code and documentation is released under a GPLv3 license. 
;
; A copy is available from the Open Source Initiative site at:
;	https://opensource.org/licenses/gpl-3.0.html
*/

#include "MainWindow.h"
#include "ui_MainWindow.h"

// Audio Related
#include "./AudioRelated/AudioInfoForm.h"
#include "./AudioRelated/AudioPatchbayForm.h"
#include "./AudioRelated/PatchbayV2Form.h"
#include "./AudioRelated/AudioPatchbaySourceV1.h"
#include "./AudioRelated/AudioPatchbaySourceV2.h"
#include "./AudioRelated/AudioControlWidget.h"

#include "./MixerRelated/MixerWidget.h"

// Firmware Related
#include "./FirmwareRelated/FirmwareUpgradeDialog.h"

// MIDI Related
#include "./MIDIRelated/MIDIChannelRemapForm.h"
#include "./MIDIRelated/MIDIControllerFilterForm.h"
#include "./MIDIRelated/MIDIControllerRemapForm.h"
#include "./MIDIRelated/MIDIInfoForm.h"
#include "./MIDIRelated/MIDIPortFiltersForm.h"
#include "./MIDIRelated/MIDIPortRoutingForm.h"
#include "./MIDIRelated/MIDIPortSelectionForm.h"

#include "ACK.h"
#include "CommandList.h"
#include "Device.h"
#include "DeviceID.h"
#include "DeviceInfoForm.h"
#include "DeviceInformationDialog.h"
#include "DeviceSelectionDialog.h"
#include "LegacyDataImport.h"
#include "MyAlgorithms.h"
#include "Reset.h"
#include "ResetList.h"
#include "SaveRestore.h"
#include "Presets/ICRestoreDialog.h"
#include "Presets/ICSaveDialog.h"
#include "FirmwareRelated/FirmwareCheckDialog.h"
#include "Version.h"

#ifdef Q_OS_MAC
#include "MacAppearance.h"
#endif

#ifndef Q_MOC_RUN
#include <boost/bind.hpp>
#include <boost/range/adaptors.hpp>
#endif

#include <QProgressBar>
#include <QPointer>
#include <QActionGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QGuiApplication>
#include <QLabel>
#include <QMenu>
#include <QScreen>
#include <QString>
#include <QSettings>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QDateTime>
#include <QDebug>
#include <QFileDialog>
#include <QToolButton>
#include <QVBoxLayout>

using namespace GeneSysLib;
using namespace MyAlgorithms;
using namespace boost::adaptors;
using namespace boost;
using namespace std;

namespace {

const QSize kDefaultMainWindowSize(1000, 720);

void populateToolBarOverflow(QToolBar* toolBar, QMenu* overflowMenu) {
  overflowMenu->clear();
  const QList<QAction*> actions = toolBar->actions();
  for (QAction* action : actions) {
    QWidget* actionWidget = toolBar->widgetForAction(action);
    if (!actionWidget || !actionWidget->isVisible()) {
      overflowMenu->addAction(action);
    }
  }

  // A platform style may keep overflow widgets technically visible while
  // placing them outside the toolbar. Never present an empty overflow.
  if (overflowMenu->isEmpty()) {
    overflowMenu->addActions(actions);
  }
}

void installStableToolBarOverflow(QToolBar* toolBar) {
  QToolButton* extensionButton =
      toolBar->findChild<QToolButton*>("qt_toolbar_ext_button");
  if (!extensionButton) {
    return;
  }

  QMenu* overflowMenu = extensionButton->findChild<QMenu*>(
      "iConfigToolBarOverflowMenu", Qt::FindDirectChildrenOnly);
  if (!overflowMenu) {
    overflowMenu = new QMenu(extensionButton);
    overflowMenu->setObjectName("iConfigToolBarOverflowMenu");
    QObject::connect(overflowMenu, &QMenu::aboutToShow, toolBar,
                     [toolBar, overflowMenu]() {
      populateToolBarOverflow(toolBar, overflowMenu);
    });
  }

  // QToolButton will not open a menu that is empty before showMenu() starts.
  // Populate once now, then refresh again in aboutToShow after future resizes.
  populateToolBarOverflow(toolBar, overflowMenu);

  // Qt's private clicked() connection expands the whole toolbar as a transient
  // popup. That popup closes prematurely on current macOS. Keep the extension
  // button, but give it ordinary QMenu lifetime semantics instead.
  QObject::disconnect(extensionButton, nullptr, nullptr, nullptr);
  extensionButton->setChecked(false);
  extensionButton->setCheckable(false);
  extensionButton->setMenu(overflowMenu);
  extensionButton->setPopupMode(QToolButton::DelayedPopup);
  QObject::connect(extensionButton, &QToolButton::clicked, extensionButton,
                   [toolBar, extensionButton, overflowMenu]() {
    populateToolBarOverflow(toolBar, overflowMenu);
    const QPoint menuPosition = extensionButton->mapToGlobal(
        QPoint(extensionButton->width() - overflowMenu->sizeHint().width(),
               extensionButton->height()));
    overflowMenu->popup(menuPosition);
  });
}

void configureStableToolBarOverflow(QToolBar* toolBar) {
  QTimer::singleShot(0, toolBar, [toolBar]() {
    installStableToolBarOverflow(toolBar);
  });
  QTimer::singleShot(250, toolBar, [toolBar]() {
    installStableToolBarOverflow(toolBar);
  });
}

}  // namespace

const int MainWindow::kButtonDisableTime = 10000;
const int MainWindow::kShortButtonDisableTime = 330;

MainWindow::MainWindow(QWidget* parent)
  : QMainWindow(parent),
    comm(new Communicator()),
    currentDevice(new DeviceInfo(comm)),
    ccFilterAction(nullptr),
    ccRemapAction(nullptr),
    channelRemapAction(nullptr),
    devInfoAction(nullptr),
    portFilterAction(nullptr),
    portRoutingAction(nullptr),
    ui(new Ui::MainWindow()),
    basicMode(false),
    connected(false) {
  ui->setupUi(this);

  // The retired online firmware service and its automatic-check preferences
  // are not presented as working release features. Manual local firmware
  // loading remains available as an explicitly advanced operation.
  ui->actionFirmwareConfig->setVisible(false);
  ui->actionUpgrade_Firmware->setVisible(false);
  ui->actionUpgrade_Firmware_From_Local_Drive->setText(
      tr("Load Local Firmware File... (Advanced)"));
  //Re-factory device selection UI functions, zx, 2017-04-06
  m_FirstSelectedDevice = false;
  continuedOpeningFileName = "";
  rebootMe = false;

  progressBar = new QProgressBar(ui->statusBar);
  progressBar->setMinimum(0);
  progressBar->setMaximum(0);
  progressBar->setMaximumWidth(120);

  progressBar->hide();

  ui->statusBar->addPermanentWidget(progressBar, 0);

  createActions();

#ifdef Q_OS_MAC
  QMenu* appearanceMenu = new QMenu(tr("Appearance"), this);
  ui->menuBar->insertMenu(ui->menuHelp->menuAction(), appearanceMenu);

  QActionGroup* appearanceGroup = new QActionGroup(appearanceMenu);
  appearanceGroup->setExclusive(true);

  QSettings appearanceSettings;
  appearanceSettings.beginGroup("appearance");
  const MacAppearance currentAppearance = macAppearanceFromString(
      appearanceSettings.value("mode", "light").toString());
  appearanceSettings.endGroup();

  const auto addAppearanceAction =
      [this, appearanceMenu, appearanceGroup, currentAppearance](
          const QString& text, MacAppearance appearance) {
    QAction* action = appearanceMenu->addAction(text);
    action->setCheckable(true);
    action->setChecked(currentAppearance == appearance);
    appearanceGroup->addAction(action);
    connect(action, &QAction::triggered, this, [appearance]() {
      QSettings settings;
      settings.beginGroup("appearance");
      const QString newMode = macAppearanceToString(appearance);
      const QString oldMode = settings.value("mode", "light").toString();
      if (oldMode.compare(newMode, Qt::CaseInsensitive) == 0) {
        settings.endGroup();
        return;
      }
      settings.setValue("mode", newMode);
      settings.endGroup();
      QCoreApplication::exit(kMacAppearanceRestartExitCode);
    });
  };

  addAppearanceAction(tr("Light"), MacAppearance::Light);
  addAppearanceAction(tr("Dark"), MacAppearance::Dark);
  addAppearanceAction(tr("System"), MacAppearance::System);
#endif

  // open up the selection dialog
  QTimer::singleShot(500, this, SLOT(on_actionClose_triggered()));

  connect(this, SIGNAL(closeAll()), this, SLOT(closeAllInTime()),
          Qt::QueuedConnection);

  updateDeviceMenu();
  connect(ui->menuDevice, SIGNAL(aboutToShow()), this,
          SLOT(updateDeviceMenu()));

  connect(ui->menuFile, SIGNAL(aboutToShow()), this, SLOT(updateFileMenu()));

  auto handler = boost::bind(&MainWindow::ackCallback, this, _1, _2, _3, _4);
  ackHandler = comm->registerHandler(Command::ACK, handler);

  refreshMetersTimer = refreshAudioControlsTimer = 0;
  devInfoDialog = 0;

  readSettings();
  if (comm->timerThread) {
    connect(comm->timerThread.get(), SIGNAL(timedOut()), this,
            SLOT(onTimeout()));
  }
  ui->toolBar->setStyleSheet("QToolBar { border: 0px; background: #404040; color: #979797; margin: 0px; padding: 0px; spacing: 0px;} QToolButton {height: 25px; width: 128px; border: 1px solid black; border-left:0; color: #ffffff; } QToolButton#qt_toolbar_ext_button { width: 28px; min-width: 28px; max-width: 28px; border-left: 1px solid black; } QToolButton:checked { color: #404040; background-color: #C2C2C2; } QToolButton:hover { color: #404040; background-color: #C2C2C2; } QToolButton:pressed { color: #404040; background-color: #C2C2C2; }");
//  ui->toolBar->setMinimumHeight(50);
  ui->toolBar->layout()->setContentsMargins(0,0,0,0);
  ui->toolBar->show();
  configureStableToolBarOverflow(ui->toolBar);

  //Re-factory warning message UI, zx, 2017-04-21
  m_WarningState = WMSG_UNSPECIFIED;
}

MainWindow::~MainWindow() {
  if (refreshAudioControlsTimer != 0)
    refreshAudioControlsTimer->stop();

  if (refreshMetersTimer != 0)
    refreshMetersTimer->stop();

  comm->unRegisterHandler(Command::ACK, ackHandler);
  comm->m_parser = 0; // fix crash on exit off audio controls screen.
}

void MainWindow::generateToolBarMidi(GeneSysLib::CommandList commandList) {
  if (commandList.contains(Command::GetMIDIInfo)) {
//    ui->toolBar->addSeparator();
    ui->toolBar->addAction(midiInfoAction);

    if (commandList.contains(Command::GetMIDIPortRoute)) {
      ui->toolBar->addAction(portRoutingAction);
    }

    if (commandList.contains(Command::GetMIDIPortFilter)) {
      ui->toolBar->addAction(portFilterAction);
    }

    if (commandList.contains(Command::GetMIDIPortRemap)) {
      ui->toolBar->addAction(channelRemapAction);
    }

    if (commandList.contains(Command::GetMIDIPortFilter) && !basicMode) {
      ui->toolBar->addAction(ccFilterAction);
    }

    if (commandList.contains(Command::GetMIDIPortRemap) && !basicMode) {
      ui->toolBar->addAction(ccRemapAction);
    }
  }
}

void MainWindow::generateToolBarAudio(GeneSysLib::CommandList commandList) {
  if ((commandList.contains(Command::GetAudioInfo)) ||
      (commandList.contains(Command::GetAudioGlobalParm))) {
//    ui->toolBar->addSeparator();

    /*if ((commandList.contains(Command::GetAudioControlParm)) &&
        (commandList.contains(Command::GetAudioControlDetail)) &&
        (commandList.contains(Command::GetAudioControlDetailValue))) {
      ui->toolBar->addAction(audioControlAction);
    }*/

    if ((commandList.contains(Command::GetMixerParm))) {
      ui->toolBar->addAction(audioMixerAction);
    }

    if ((commandList.contains(Command::GetAudioPortPatchbay)) ||
        (commandList.contains(Command::GetAudioPatchbayParm))) {
      ui->toolBar->addAction(audioPatchbayAction);
    }


    ui->toolBar->addAction(audioInfoAction);
  }
}

void MainWindow::generateToolBar(GeneSysLib::CommandList commandList) {
  clearToolBar();

  if ((commandList.contains(Command::GetInfoList)) ||
      (commandList.contains(Command::GetInfo))) {
    ui->toolBar->addAction(devInfoAction);
  }

  switch ((DevicePID::Enum) currentDevice->getPID()) {
  case DevicePID::iConnect4Audio:
  case DevicePID::iConnect2Audio:
  case DevicePID::PlayAudio12:
    generateToolBarAudio(commandList);
    generateToolBarMidi(commandList);
    break;
  default:
    generateToolBarMidi(commandList);
    generateToolBarAudio(commandList);
    break;
  }

  configureStableToolBarOverflow(ui->toolBar);
}

void MainWindow::clearToolBar() { ui->toolBar->clear(); }

void MainWindow::ReconnectPreviousOpenDevice(DeviceInfoPtr prevDevice) {
  currentDevice = prevDevice;

  if ((currentDevice) || (!connected)) {
    const auto& transID = currentDevice->getTransID();

    if (currentDevice->contains<CommandList>()) {
      generateToolBar(currentDevice->get<CommandList>());
    } else {
      clearToolBar();
    }

    // Open the ports after selection
    if (comm->openOutput(transID)) {
      if (comm->openAllInputs()) {
        connect(currentDevice.get(),
                SIGNAL(queryCompleted(Screen, CommandQList)), this,
                SLOT(queryCompleted(Screen, CommandQList)),
                Qt::QueuedConnection);
        connect(currentDevice.get(), SIGNAL(writingStarted(int)), this,
                SLOT(writingStarted(int)));
        connect(currentDevice.get(), SIGNAL(writingProgress(int)), this,
                SLOT(writingProgress(int)));
        connect(currentDevice.get(), SIGNAL(writeCompleted()), this,
                SLOT(writingCompleted()));
        connect(currentDevice.get(), SIGNAL(writeFailed(QString,int,int)), this,
                SLOT(writingFailed(QString,int,int)));
        connect(currentDevice.get(), SIGNAL(queryStarted()), progressBar,
                SLOT(show()));
        connect(currentDevice.get(),
                SIGNAL(queryCompleted(Screen, CommandQList)), progressBar,
                SLOT(hide()));

        const auto& devInfo = currentDevice->get<Device>();

        if (devInfo.mode() == BootMode::BootLoaderMode) {
          /*m_WarningState = WMSG_POORNETWORK4FIRMWAREUPDATE; //zx, 2017-04-25
          QPointer<FirmwareUpgradeDialog> firmwareDialog(
                new FirmwareUpgradeDialog(this->comm, this->currentDevice,
                                          FirmwareMode::RecoveryMode, this));
          connect(firmwareDialog, SIGNAL(rejected()), this,
                  SLOT(on_actionClose_triggered()));
          firmwareDialog->showNormal(); */
           showErrorWithTitleAndMsg("Bootloader Mode Device", "Bootloader Moder device needs firmware upgrade either from iConfig or manually.", true);
           QApplication::quit();
        } else {
          if (audioMixerAction) {
            audioMixerControl_triggered();
          } else {
            deviceInfo_triggered();
          }

          if (continuedOpeningFileName == "") {

            QSettings settings(QCoreApplication::organizationName(),
                               QCoreApplication::applicationName());

            settings.beginGroup("informationScreen");
            bool show = settings.value("show" + QString::number(currentDevice->getDeviceID().pid()), true).toBool();
            settings.endGroup();

            if ((DevicePID::Enum) currentDevice->getDeviceID().pid() == DevicePID::iConnect4Audio) {
              if (show) {
                devInfoDialog = new DeviceInformationDialog(currentDevice, this);
                devInfoDialog->show();
              }
            }
          }
        }
        //Re-factory device selection UI functions, zx, 2017-04-06
        m_FirstSelectedDevice = true;
        connected = true;
        if (continuedOpeningFileName != "") {
          CommandQList query;
          currentDevice->startQuery(OpenScreen, query);
        }
      } else {
        //showConnectionError();
        //zx, 2017-04-19
         showErrorWithTitleAndMsg("Device Connection Lost", "Previous connected device is missing! Please check device connection", true);
      }
    } else {
      //showConnectionError();
      //zx, 2017-04-19
      showErrorWithTitleAndMsg("Device Connection Lost", "Previous connected device is missing! Please check device connection", true);
    }
  }

}

void MainWindow::on_actionClose_triggered() {
  qDebug() << "MainWindow::on_actionClose_triggered() call begin";
  DeviceID prevDevice;
  if(m_FirstSelectedDevice == true) {
    prevDevice = currentDevice->getDeviceID();
  }
  connected = false;

  if (devInfoAction)
    devInfoAction->setChecked(true);

  if (devInfoDialog)
    devInfoDialog->close();

  clearLayout(ui->centralWidget->layout());

  if (refreshAudioControlsTimer)
    refreshAudioControlsTimer->stop();

  if (refreshMetersTimer)
    refreshMetersTimer->stop();

  // Clear the selection
  currentDevice.reset(new DeviceInfo(comm));
  fileName.clear();

  DeviceSelectionDialog dialog(comm);
  m_WarningState = WMSG_DEVICECONNECTIONLOST; //zx, 2017-04-26
  if(m_FirstSelectedDevice == true) {
    dialog.SetPreviousCachedDeviceID(prevDevice);
  }
  if (dialog.exec())
  {
    currentDevice = dialog.getSelectedDevice();

    if ((currentDevice) || (!connected)) {
      const auto& transID = currentDevice->getTransID();

      if (currentDevice->contains<CommandList>()) {
        generateToolBar(currentDevice->get<CommandList>());
      } else {
        clearToolBar();
      }

      // Open the ports after selection
      if (comm->openOutput(transID)) {
        if (comm->openAllInputs()) {
          connect(currentDevice.get(),
                  SIGNAL(queryCompleted(Screen, CommandQList)), this,
                  SLOT(queryCompleted(Screen, CommandQList)),
                  Qt::QueuedConnection);
          connect(currentDevice.get(), SIGNAL(writingStarted(int)), this,
                  SLOT(writingStarted(int)));
          connect(currentDevice.get(), SIGNAL(writingProgress(int)), this,
                  SLOT(writingProgress(int)));
          connect(currentDevice.get(), SIGNAL(writeCompleted()), this,
                  SLOT(writingCompleted()));
          connect(currentDevice.get(), SIGNAL(writeFailed(QString,int,int)), this,
                  SLOT(writingFailed(QString,int,int)));
          connect(currentDevice.get(), SIGNAL(queryStarted()), progressBar,
                  SLOT(show()));
          connect(currentDevice.get(),
                  SIGNAL(queryCompleted(Screen, CommandQList)), progressBar,
                  SLOT(hide()));

          const auto& devInfo = currentDevice->get<Device>();

          if (devInfo.mode() == BootMode::BootLoaderMode) {
            m_WarningState = WMSG_POORNETWORK4FIRMWAREUPDATE; //zx, 2017-04-25
            QPointer<FirmwareUpgradeDialog> firmwareDialog(
                  new FirmwareUpgradeDialog(this->comm, this->currentDevice,
                                            FirmwareMode::RecoveryMode, this));
            connect(firmwareDialog, SIGNAL(rejected()), this,
                    SLOT(on_actionClose_triggered()));
            firmwareDialog->showNormal();
          } else {
            if (audioMixerAction) {
              audioMixerControl_triggered();
            } else {
              deviceInfo_triggered();
            }

            if (continuedOpeningFileName == "") {

              QSettings settings(QCoreApplication::organizationName(),
                                 QCoreApplication::applicationName());

              // The retired online firmware service is no longer queried
              // automatically. Manual and local-file firmware actions remain.

              settings.beginGroup("informationScreen");
              bool show = settings.value("show" + QString::number(currentDevice->getDeviceID().pid()), true).toBool();
              settings.endGroup();

              if ((DevicePID::Enum) currentDevice->getDeviceID().pid() == DevicePID::iConnect4Audio) {
                if (show) {
                  devInfoDialog = new DeviceInformationDialog(currentDevice, this);
                  devInfoDialog->show();
                }
              }
            }
          }
          //Re-factory device selection UI functions, zx, 2017-04-06
          m_FirstSelectedDevice = true;
          connected = true;
          if (continuedOpeningFileName != "") {
            CommandQList query;
            currentDevice->startQuery(OpenScreen, query);
          }
        } else {
          //showConnectionError();
          //zx, 2017-04-20
          showErrorWithTitleAndMsg("Device Connection Lost", "Selected device is missing! Please check device connection", true);
        }
      } else {
        //showConnectionError();
        //zx, 2017-04-20
        showErrorWithTitleAndMsg("Device Connection Lost", "Selected device is missing! Please check device connection", true);
      }
    }
  }
  else
  {
    //Re-factory device selection UI functions, zx, 2017-04-06
    qDebug() << "QApplication::quit() by close";
    if(m_FirstSelectedDevice == false) {
      // Close app
      QApplication::quit();
    }
    else
    {
      DeviceInfoPtr prevDevicePtr = dialog.GetPreviousOpenDevice();
      if(prevDevicePtr.get())
      {
          ReconnectPreviousOpenDevice(prevDevicePtr);
      }
      else
      {
        QApplication::quit();
      }
    }
  }
}

void MainWindow::on_actionCommit_Changes_to_FLASH_triggered() {
  if ((currentDevice) || (!connected)) {
    QMessageBox msgBox;
    msgBox.setText(tr("All settings will be committed to memory."));
    msgBox.setInformativeText(tr("Do you want to continue?"));
    msgBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
    msgBox.setIcon(QMessageBox::Warning);

    msgBox.setDefaultButton(QMessageBox::Yes);

    if (msgBox.exec() == QMessageBox::Yes) {
      currentDevice->send<SaveRestoreCommand>(SaveRestoreID::SaveToFlash);
    }
  } else {
    showConnectionError();
  }
}

void MainWindow::on_actionRestore_from_FLASH_triggered() {
  if ((currentDevice) || (!connected)) {
    QMessageBox msgBox;
    msgBox.setText(tr("All settings will be restored from memory."));
    msgBox.setInformativeText(tr("Device will be reset. Do you want to continue?"));
    msgBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
    msgBox.setIcon(QMessageBox::Warning);

    msgBox.setDefaultButton(QMessageBox::Yes);

    if (msgBox.exec() == QMessageBox::Yes) {
      currentDevice->send<SaveRestoreCommand>(SaveRestoreID::RestoreFromFlash);

      QTimer::singleShot(100,this,SLOT(doReset()));
    }
  } else {
    showConnectionError();
  }
}

void MainWindow::on_actionRestore_to_Factory_Defaults_triggered() {
  if ((currentDevice) || (!connected)) {
    QMessageBox msgBox;
    msgBox.setText(tr("All settings will be set to factory defaults."));
    msgBox.setInformativeText(tr("Device will be reset. Do you want to continue?"));
    msgBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
    msgBox.setIcon(QMessageBox::Warning);

    msgBox.setDefaultButton(QMessageBox::Yes);

    if (msgBox.exec() == QMessageBox::Yes) {
      ui->statusBar->showMessage(tr("Restoring factory defaults."), 2000);
      currentDevice->send<SaveRestoreCommand>(SaveRestoreID::FactoryDefault);
      //currentDevice->send<SaveRestoreCommand>(SaveRestoreID::SaveToFlash);
      //currentDevice->send<ResetCommand>(BootMode::AppMode);

      QTimer::singleShot(100,this,SLOT(doReset()));
    }
  } else {
    showConnectionError();
  }
}

void MainWindow::on_actionReset_triggered() {
  if ((currentDevice) || (!connected)) {
    QMessageBox msgBox;
    msgBox.setText(tr("All uncommitted changes will be lost."));
    msgBox.setInformativeText(tr("Do you want to reset the device?"));
    msgBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
    msgBox.setIcon(QMessageBox::Warning);

    msgBox.setDefaultButton(QMessageBox::Yes);

    if (msgBox.exec() == QMessageBox::Yes) {
      QTimer::singleShot(100,this,SLOT(doReset()));
    }
  } else {
    showConnectionError();
  }
}

void MainWindow::doReset() {
  ui->statusBar->showMessage(tr("Resetting device."), 2000);
  currentDevice->send<ResetCommand>(BootMode::AppMode);
  if (devInfoAction)
    devInfoAction->setChecked(true);
}

void MainWindow::doResetForOpen() {
  ui->statusBar->showMessage(tr("Resetting device and continuing preset open."), 2000);
  auto bytes = sysex(ResetCommand(currentDevice->getDeviceID(), currentDevice->getTransID(),
                                  BootMode::AppMode));
  sysexMessages.push(bytes);

  // Send the commit to flash message
  currentDevice->send<SaveRestoreCommand>(SaveRestoreID::SaveToFlash);
  clearLayout(ui->centralWidget->layout());

  if (devInfoAction)
    devInfoAction->setChecked(true);
}

void MainWindow::doResetBootloader() {
  ui->statusBar->showMessage(tr("Resetting device."), 2000);
  currentDevice->send<ResetCommand>(BootMode::BootLoaderMode);
  if (devInfoAction)
    devInfoAction->setChecked(true);
}


void MainWindow::on_actionReset_to_Bootloader_Mode_triggered() {
  if ((currentDevice) || (!connected)) {
    QMessageBox msgBox;
    msgBox.setText(tr("All uncommitted changes will be lost."));
    msgBox.setInformativeText(
          tr("Do you want to reset the device to bootloader mode?"));
    msgBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
    msgBox.setIcon(QMessageBox::Warning);

    msgBox.setDefaultButton(QMessageBox::Yes);

    if (msgBox.exec() == QMessageBox::Yes) {
      QTimer::singleShot(100,this,SLOT(doResetBootloader()));
    }
  } else {
    showConnectionError();
  }
}

void MainWindow::on_actionSave_triggered() {
  startSave(SaveScreen);
}

void MainWindow::startSave(Screen screen) {
  if ((currentDevice) || (!connected)) {
    if (currentDevice->contains<CommandList>()) {
      auto& commandListData = currentDevice->get<CommandList>();

      // Query everything
      CommandQList query;
      for (const auto& cmd : commandListData.commandList) {
        query << (CmdEnum)(QUERY_BIT | cmd);
      }
      currentDevice->startQuery(screen, query);
    }
  } else {
    showConnectionError();
  }
}

void MainWindow::on_actionFirmwareConfig_triggered() {
  CommandQList query;
  currentDevice->startQuery(FirmwareConfigScreen, query);
}

void MainWindow::on_actionOpen_triggered() {
  if ((currentDevice) || (!connected)) {
    startSave(OpenScreen);
  } else {
    showConnectionError();
  }
}

void MainWindow::on_actionOpenPresetsFolder_triggered() {
  QDesktopServices::openUrl(QUrl::fromLocalFile(
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
      "/presets"));
}

void MainWindow::on_actionImportLegacyData_triggered() {
  const LegacyDataImport::NamespacePaths legacyPaths =
      LegacyDataImport::legacyNamespacePaths();
  const LegacyDataImport::NamespacePaths modernPaths =
      LegacyDataImport::activeNamespacePaths();

  QDialog dialog(this);
  dialog.setWindowTitle(tr("Import Legacy iConfig Data"));
  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  QLabel *description = new QLabel(
      tr("Import copies selected legacy data into iConfig Modern. The legacy "
         "files are opened read-only and are never changed."),
      &dialog);
  description->setWordWrap(true);
  layout->addWidget(description);

  QCheckBox *presets = new QCheckBox(tr("Presets for the connected device"),
                                     &dialog);
  const bool canImportPresets = connected && currentDevice &&
      QDir(legacyPaths.presetDirectory).exists();
  presets->setEnabled(canImportPresets);
  presets->setChecked(canImportPresets);
  if (!connected) {
    presets->setToolTip(
        tr("Connect and select a device so preset compatibility can be validated."));
  }
  layout->addWidget(presets);

  QCheckBox *preferences = new QCheckBox(
      tr("Appearance preference (Light, Dark, or System)"), &dialog);
  preferences->setChecked(false);
  layout->addWidget(preferences);

  layout->addWidget(new QLabel(
      tr("If a modern preset has the same name:"), &dialog));
  QComboBox *conflict = new QComboBox(&dialog);
  conflict->addItem(tr("Skip (recommended)"),
                    static_cast<int>(LegacyDataImport::ConflictPolicy::Skip));
  conflict->addItem(tr("Rename imported copy"),
                    static_cast<int>(LegacyDataImport::ConflictPolicy::Rename));
  conflict->addItem(tr("Replace modern copy"),
                    static_cast<int>(LegacyDataImport::ConflictPolicy::Replace));
  layout->addWidget(conflict);

  QLabel *source = new QLabel(
      tr("Legacy source:\n%1").arg(legacyPaths.appDataDirectory), &dialog);
  source->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(source);

  QDialogButtonBox *buttons = new QDialogButtonBox(
      QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dialog);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);

  if (dialog.exec() != QDialog::Accepted) {
    return;
  }
  if (!presets->isChecked() && !preferences->isChecked()) {
    QMessageBox::information(this, tr("Import Legacy iConfig Data"),
                             tr("No data was selected."));
    return;
  }

  QStringList summary;
  if (presets->isChecked()) {
    const auto validator = [this](const QByteArray &contents, QString *error) {
      const Bytes data(contents.begin(), contents.end());
      const DeviceInfo::RestorePlan plan =
          currentDevice->buildRestorePlan(data, tr("legacy import validation"));
      if (!plan.valid && error) {
        *error = plan.error;
      }
      return plan.valid;
    };
    const auto policy = static_cast<LegacyDataImport::ConflictPolicy>(
        conflict->currentData().toInt());
    const LegacyDataImport::PresetImportResult result =
        LegacyDataImport::importPresets(
            legacyPaths.presetDirectory, modernPaths.presetDirectory,
            extensionForPID(currentDevice->getPID()),
            static_cast<quint8>(currentDevice->getPID()), policy, validator);
    summary << tr("Presets imported: %1 (%2 files)\n"
                  "Presets skipped: %3\nPresets rejected: %4")
                   .arg(result.importedPresets)
                   .arg(result.importedFiles)
                   .arg(result.skippedPresets)
                   .arg(result.rejectedPresets);
    summary << result.messages;
  }

  if (preferences->isChecked()) {
    QSettings legacySettings(QSettings::NativeFormat, QSettings::UserScope,
                             QStringLiteral("iConnectivity"),
                             QStringLiteral("iConnectivity iConfig"));
    QSettings modernSettings;
    const auto result = LegacyDataImport::importWhitelistedPreferences(
        legacySettings, modernSettings);
    summary << tr("Preferences imported: %1\nPreferences rejected: %2")
                   .arg(result.importedKeys)
                   .arg(result.rejectedKeys);
    summary << result.messages;
  }

  QMessageBox::information(this, tr("Import Legacy iConfig Data"),
                           summary.join(QStringLiteral("\n")));
}

void MainWindow::clearLayout(QLayout* layout)
{
  QLayoutItem* child;
  while((child = layout->takeAt(0)) != 0)
  {
    if(child->layout() != 0)
    {
      clearLayout(child->layout());
    }

    if(child->widget() != 0)
    {
      delete child->widget();
    }

    delete child;
  }
}

void MainWindow::deviceInfo_triggered() {
  if (ensureAudioInfoSave()) {
    clearLayout(ui->centralWidget->layout());
    DeviceInfoForm* devInfoForm = new DeviceInfoForm(currentDevice);

    connect(devInfoForm, SIGNAL(requestRefresh()), this, SLOT(requestRefresh()),
            Qt::QueuedConnection);
    connect(this, SIGNAL(refreshAll()), devInfoForm, SLOT(refreshWidget()),
            Qt::QueuedConnection);
    connect(devInfoForm, SIGNAL(updateMessage(QString, int)), ui->statusBar,
            SLOT(showMessage(QString, int)), Qt::QueuedConnection);

    ui->centralWidget->layout()->addWidget(devInfoForm);

    devInfoAction->setEnabled(false);
    QTimer::singleShot(kShortButtonDisableTime, this,
                       SLOT(enableDevInfoAction()));
  }
}

void MainWindow::midiInfo_triggered() {
  if (ensureAudioInfoSave()) {
    clearLayout(ui->centralWidget->layout());

    MIDIInfoForm* midiInfoForm = new MIDIInfoForm(comm, currentDevice);

    connect(midiInfoForm, SIGNAL(requestRefresh()), this,
            SLOT(requestRefresh()), Qt::QueuedConnection);
    connect(this, SIGNAL(refreshAll()), midiInfoForm, SLOT(refreshWidget()),
            Qt::QueuedConnection);
    connect(midiInfoForm, SIGNAL(updateMessage(QString, int)), ui->statusBar,
            SLOT(showMessage(QString, int)), Qt::QueuedConnection);

    ui->centralWidget->layout()->addWidget(midiInfoForm);
    midiInfoAction->setEnabled(false);
    QTimer::singleShot(kShortButtonDisableTime, this,
                       SLOT(enableMidiInfoAction()));
  }
}

void MainWindow::portRouting_triggered() {
  if (ensureAudioInfoSave()) {

    clearLayout(ui->centralWidget->layout());

    if ((currentDevice) || (!connected)) {
      currentDevice->startQuery(PortRoutingScreen, MIDIPortRoutingForm::Query());
    }
    portRoutingAction->setEnabled(false);
    QTimer::singleShot(kButtonDisableTime, this, SLOT(enablePortRoutingAction()));
  }
}

void MainWindow::portFilter_triggered() {
  if (ensureAudioInfoSave()) {

    clearLayout(ui->centralWidget->layout());

    if ((currentDevice) || (!connected)) {
      currentDevice->startQuery(PortFiltersScreen, MIDIPortFiltersForm::Query());
    }
    portFilterAction->setEnabled(false);
    QTimer::singleShot(kButtonDisableTime, this, SLOT(enablePortFilterAction()));
  }
}

void MainWindow::channelRemap_triggered() {
  if (ensureAudioInfoSave()) {

    clearLayout(ui->centralWidget->layout());

    if ((currentDevice) || (!connected)) {
      currentDevice->startQuery(ChannelRemapScreen,
                                MIDIChannelRemapForm::Query());
    }
    channelRemapAction->setEnabled(false);
    QTimer::singleShot(kButtonDisableTime, this,
                       SLOT(enableChannelRemapAction()));
  }
}

void MainWindow::ccFilter_triggered() {
  if (ensureAudioInfoSave()) {

    clearLayout(ui->centralWidget->layout());

    if ((currentDevice) || (!connected)) {
      currentDevice->startQuery(CCFiltersScreen,
                                MIDIControllerFilterForm::Query());
    }
    ccFilterAction->setEnabled(false);
    QTimer::singleShot(kButtonDisableTime, this, SLOT(enableCCFilterAction()));
  }
}

void MainWindow::ccRemap_triggered() {
  if (ensureAudioInfoSave()) {
    clearLayout(ui->centralWidget->layout());

    if ((currentDevice) || (!connected)) {
      currentDevice->startQuery(CCRemapScreen, MIDIControllerRemapForm::Query());
    }
    ccRemapAction->setEnabled(false);
    QTimer::singleShot(kButtonDisableTime, this, SLOT(enableCCRemapAction()));
  }
}

void MainWindow::audioInfo_triggered() {
  if (ensureAudioInfoSave()) {

    clearLayout(ui->centralWidget->layout());

    AudioInfoForm* audioInfoForm = new AudioInfoForm(currentDevice);

    connect(audioInfoForm, SIGNAL(requestRefresh()), this,
            SLOT(requestRefresh()), Qt::QueuedConnection);
    connect(this, SIGNAL(refreshAll()), audioInfoForm, SLOT(refreshWidget()),
            Qt::QueuedConnection);
    connect(audioInfoForm, SIGNAL(updateMessage(QString, int)), ui->statusBar,
            SLOT(showMessage(QString, int)), Qt::QueuedConnection);

    ui->centralWidget->layout()->addWidget(audioInfoForm);

    QTimer::singleShot(kShortButtonDisableTime, this,
                       SLOT(enableAudioInfoAction()));
  }
}

void MainWindow::audioPatchbay_triggered() {
  if (ensureAudioInfoSave()) {
    clearLayout(ui->centralWidget->layout());

    if (currentDevice) {
      const auto& cmdList = currentDevice->get<CommandList>();
      if (cmdList.contains(Command::GetAudioPortPatchbay)) {
        currentDevice->startQuery(Screen::AudioPatchbayScreen,
                                  AudioPatchbaySourceV1::Query()); ///????AudioPatchbaySourceV1::Query());  //????????
      } else {
        currentDevice->startQuery(Screen::AudioPatchbayScreen,
                                  AudioPatchbaySourceV2::Query());
      }
      audioPatchbayAction->setEnabled(false);
      QTimer::singleShot(kShortButtonDisableTime, this,
                         SLOT(enableAudioPatchbayAction()));
    }
  }
}

void MainWindow::audioMixerControl_triggered() {
  if (ensureAudioInfoSave()) {
    if (audioMixerAction) {
      audioMixerAction->setChecked(true);
    }
    clearLayout(ui->centralWidget->layout());


    if (currentDevice) {
      currentDevice->startQuery(Screen::MixerScreen,
                                MixerWidget::Query());
      audioMixerAction->setEnabled(false);
      QTimer::singleShot(kShortButtonDisableTime, this,
                         SLOT(enableAudioMixerAction()));
    }
  }
}

void MainWindow::audioControl_triggered() {
  if (ensureAudioInfoSave()) {
    clearLayout(ui->centralWidget->layout());

    if (currentDevice) {
      currentDevice->startQuery(Screen::AudioControlScreen,
                                AudioControlWidget::Query());
      audioControlAction->setEnabled(false);
      QTimer::singleShot(kShortButtonDisableTime, this,
                         SLOT(enableAudioControlAction()));
    }
  }
}

std::set<Command::Enum> MainWindow::getPreRebootCommands() {
  std::set<Command::Enum> toReturn;
  toReturn.insert(Command::RetAudioInfo);
  toReturn.insert(Command::RetAudioCfgInfo);
  toReturn.insert(Command::RetAudioPortInfo);
  toReturn.insert(Command::RetAudioPortCfgInfo);
  toReturn.insert(Command::RetAudioClockInfo);

  toReturn.insert(Command::RetAudioGlobalParm);
  toReturn.insert(Command::RetAudioPortParm);
  toReturn.insert(Command::RetAudioDeviceParm);
  toReturn.insert(Command::RetAudioClockParm);

  toReturn.insert(Command::RetMixerParm);
  toReturn.insert(Command::RetMixerPortParm);
  return toReturn;
}

void MainWindow::queryCompleted(Screen screen, CommandQList) {
  if (connected) {
    switch (screen) {
    case PortRoutingScreen: {
      auto* const portRouting = new MIDIPortRoutingForm(currentDevice);

      connect(portRouting, SIGNAL(requestRefresh()), this,
              SLOT(requestRefresh()), Qt::QueuedConnection);
      connect(this, SIGNAL(refreshAll()), portRouting, SLOT(refreshWidget()),
              Qt::QueuedConnection);

      ui->centralWidget->layout()->addWidget(portRouting);

      enablePortRoutingAction();
      m_WarningState = WMSG_UNSPECIFIED;//zx, 2017-04-26
      break;
    }

    case PortFiltersScreen: {
      MIDIPortFiltersForm* filters =
          new MIDIPortFiltersForm(comm, currentDevice);

      connect(filters, SIGNAL(requestRefresh()), this, SLOT(requestRefresh()),
              Qt::QueuedConnection);
      connect(this, SIGNAL(refreshAll()), filters, SLOT(refreshWidget()),
              Qt::QueuedConnection);

      ui->centralWidget->layout()->addWidget(filters);


      enablePortFilterAction();
      m_WarningState = WMSG_UNSPECIFIED;//zx, 2017-04-26
      break;
    }

    case ChannelRemapScreen: {
      MIDIChannelRemapForm* remap =
          new MIDIChannelRemapForm(comm, currentDevice);

      connect(remap, SIGNAL(requestRefresh()), this, SLOT(requestRefresh()),
              Qt::QueuedConnection);
      connect(this, SIGNAL(refreshAll()), remap, SLOT(refreshWidget()),
              Qt::QueuedConnection);

      ui->centralWidget->layout()->addWidget(remap);

      enableChannelRemapAction();
      m_WarningState = WMSG_UNSPECIFIED;//zx, 2017-04-26
      break;
    }

    case CCFiltersScreen: {
      auto* const ccFilterForm =
          new MIDIControllerFilterForm(comm, currentDevice);

      connect(ccFilterForm, SIGNAL(requestRefresh()), this,
              SLOT(requestRefresh()), Qt::QueuedConnection);
      connect(this, SIGNAL(refreshAll()), ccFilterForm, SLOT(refreshWidget()),
              Qt::QueuedConnection);

      ui->centralWidget->layout()->addWidget(ccFilterForm);


      enableCCFilterAction();
      m_WarningState = WMSG_UNSPECIFIED;//zx, 2017-04-26
      break;
    }

    case CCRemapScreen: {
      auto* const ccRemapForm = new MIDIControllerRemapForm(currentDevice);

      connect(ccRemapForm, SIGNAL(requestRefresh()), this,
              SLOT(requestRefresh()), Qt::QueuedConnection);
      connect(this, SIGNAL(refreshAll()), ccRemapForm, SLOT(refreshWidget()),
              Qt::QueuedConnection);

      ui->centralWidget->layout()->addWidget(ccRemapForm);

      enableCCRemapAction();
      m_WarningState = WMSG_UNSPECIFIED;//zx, 2017-04-26
      break;
    }

    case OpenScreen: {
      if ((currentDevice) || (!connected)) {
        bool restoringAuxiliary = false;
        clearLayout(ui->centralWidget->layout());
        DeviceInfoForm* devInfoForm = new DeviceInfoForm(currentDevice);

        connect(devInfoForm, SIGNAL(requestRefresh()), this, SLOT(requestRefresh()),
                Qt::QueuedConnection);
        connect(this, SIGNAL(refreshAll()), devInfoForm, SLOT(refreshWidget()),
                Qt::QueuedConnection);
        connect(devInfoForm, SIGNAL(updateMessage(QString, int)), ui->statusBar,
                SLOT(showMessage(QString, int)), Qt::QueuedConnection);

        ui->centralWidget->layout()->addWidget(devInfoForm);
        devInfoAction->setChecked(true);

        if (continuedOpeningFileName != "") {
          fileName = continuedOpeningFileName;
          continuedOpeningFileName = "";
          rebootMe = false;
          restoringAuxiliary = true;
        }
        else {
          //Bugfixing, zx-03-27
          qDebug() << "Open preset UI to load";
          ICRestoreDialog* restore = new ICRestoreDialog(currentDevice, this);
          if (restore->exec() == QDialog::Accepted) {
            fileName = restore->getFileName();
          }
          else {
            fileName = "";
          }
        }

        if (!fileName.isEmpty()) {
          QFile file(fileName);
          QFile file2(fileName + ".aux");
          if (file2.exists()) {
            continuedOpeningFileName = fileName + ".aux";
            rebootMe = true;
            printf("setting rebootMe to true!\n");
          }

          //Bugfixing, zx-03-27
          qDebug() << "Preset file to open:" << continuedOpeningFileName;

          if (!file.open(QFile::ReadOnly)) {
            QMessageBox::warning(this, tr("Open Failed"),
                                 tr("Cannot read file %1:\n%2.").arg(fileName)
                                 .arg(file.errorString()));
            return;
          }

          QByteArray qData = file.readAll();
          file.close();

          Bytes data = Bytes(qData.begin(), qData.end());

          ui->statusBar->showMessage(tr("Opening file..."), 3000);
          const DeviceInfo::RestorePlan restorePlan =
              currentDevice->buildRestorePlan(
                  data, restoringAuxiliary ? tr("auxiliary") : tr("primary"));
          if (!restorePlan.valid) {
            continuedOpeningFileName.clear();
            rebootMe = false;
            ui->statusBar->showMessage(tr("File read error."), 3000);
            QMessageBox msgBox;
            msgBox.setText(tr("Could not read file"));
            msgBox.setInformativeText(restorePlan.error);
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.setIcon(QMessageBox::Critical);

            msgBox.exec();
          }
          else if (restorePlan.entries.empty()) {
            continuedOpeningFileName.clear();
            rebootMe = false;
            ui->statusBar->showMessage(
                tr("Preset contains no restorable configuration."), 3000);
            QMessageBox::information(
                this, tr("Nothing to Restore"),
                tr("The preset contains no writable configuration records. "
                   "No device changes were started."));
          }
          else if (!restoringAuxiliary &&
                   QMessageBox::warning(
                       this, tr("Experimental Preset Restore"),
                       tr("Preset Restore is experimental in this "
                          "community-maintained beta. It may change many "
                          "device settings and can continue through an "
                          "auxiliary stage. Back up the current configuration "
                          "and use only a preset intended for this device.\n\n"
                          "This stage contains %1 validated configuration "
                          "writes. Continue?")
                           .arg(restorePlan.entries.size()),
                       QMessageBox::Yes | QMessageBox::Cancel,
                       QMessageBox::Cancel) != QMessageBox::Yes) {
            continuedOpeningFileName.clear();
            rebootMe = false;
            ui->statusBar->showMessage(tr("Preset restore cancelled."), 3000);
          }
          else if (!currentDevice->dispatchRestorePlan(restorePlan)) {
            continuedOpeningFileName.clear();
            rebootMe = false;
            ui->statusBar->showMessage(tr("Preset restore could not start."),
                                       3000);
            QMessageBox::critical(
                this, tr("Restore Failed"),
                tr("The device is busy or another restore is already active."));
          } else {
            qDebug() << "Restore plan stage:" << restorePlan.sourceStage
                     << "validated:" << restorePlan.validatedRecordCount
                     << "writable:" << restorePlan.entries.size()
                     << "not restorable:"
                     << restorePlan.rejectedRecords.size();
            if (!restorePlan.rejectedRecords.empty()) {
              ui->statusBar->showMessage(
                  tr("Opening preset (%1 read-only records skipped)...")
                      .arg(restorePlan.rejectedRecords.size()),
                  3000);
            }
            ui->statusBar->showMessage(tr("File opened, please wait"), 2000);
          }

        }
        m_WarningState = WMSG_UNSPECIFIED;//zx, 2017-04-26
      }
      else
      {
        showConnectionError();
      }
    }
    break;

    case SaveAsScreen:
      fileName.clear();
      // FALL THROUGH
    case SaveScreen: {
      m_WarningState = WMSG_UNSPECIFIED;//zx, 2017-04-26
      ICSaveDialog* saveDialog = new ICSaveDialog(currentDevice, this);
      saveDialog->show();

      break;
    }

    case FirmwareConfigScreen:
    {
      FirmwareCheckDialog* firmwareCheckDialog = new FirmwareCheckDialog(this);
      firmwareCheckDialog->show();

      break;
    }

    case RereadAllScreen:
      m_WarningState = WMSG_UNSPECIFIED;//zx, 2017-04-26
      emit refreshAll();
      break;
    case RereadAudioControls:
      m_WarningState = WMSG_UNSPECIFIED;//zx, 2017-04-26
      emit refreshAudio();
      break;
    case RereadMeters:
      m_WarningState = WMSG_UNSPECIFIED;//zx, 2017-04-26
      emit refreshMeters();
      break;

    case AudioPatchbayScreen: {
      qDebug() << "AudioPatchbayScreen is called";
      if (connected) {
        m_WarningState = WMSG_UNSPECIFIED;//zx, 2017-04-26
        qDebug() << "Is connected in AudioPatchbayScreen";
        const auto& cmdList = currentDevice->get<CommandList>();

        boost::shared_ptr<IAudioPatchbaySource> source;
        if (cmdList.contains(Command::GetAudioPortPatchbay)) {
          source = boost::shared_ptr<IAudioPatchbaySource>(new AudioPatchbaySourceV1(currentDevice));///?????AudioPatchbaySourceV1(currentDevice));
          qDebug() << "AudioPatchbayScreen using AudioPatchbaySourceV1 as source";
        } else {
          source = boost::shared_ptr<IAudioPatchbaySource>(new AudioPatchbaySourceV2(currentDevice));
          qDebug() << "AudioPatchbayScreen using AudioPatchbaySourceV2 as source";
        }
        auto* patchbayForm = new PatchbayV2Form(source);

        connect(patchbayForm, SIGNAL(requestRefresh()), this,
                SLOT(requestRefresh()), Qt::QueuedConnection);
        connect(this, SIGNAL(refreshAll()), patchbayForm, SLOT(refreshWidget()),
                Qt::QueuedConnection);

        ui->centralWidget->layout()->addWidget(patchbayForm);
        enableAudioPatchbayAction();

        break;
      }
    }

    case MixerScreen: {
      auto *widget = new MixerWidget(currentDevice);

      connect(widget, SIGNAL(requestRefresh()), this,
              SLOT(requestRefresh()), Qt::QueuedConnection);
      connect(this, SIGNAL(refreshAll()), widget, SLOT(refreshWidget()),
              Qt::QueuedConnection);
      connect(this, SIGNAL(refreshAudio()), widget, SLOT(refreshWidget()),
              Qt::QueuedConnection);
      connect(this, SIGNAL(refreshMeters()), widget, SLOT(refreshMeters()),
              Qt::QueuedConnection);
      connect(widget, SIGNAL(doneAudioRefresh()), this, SLOT(doneAudioRefresh()),
              Qt::DirectConnection);
      connect(widget, SIGNAL(doneMetersRefresh()), this, SLOT(doneMetersRefresh()),
              Qt::DirectConnection);

      ui->centralWidget->layout()->addWidget(widget);
      //ui->centralWidget->setStyleSheet("QWidget#centralWidget { background-color: #202020; }");

      if (refreshAudioControlsTimer == 0) {
        refreshAudioControlsTimer = new QTimer(this);
        refreshAudioControlsTimer->setSingleShot(true);
        connect(refreshAudioControlsTimer, SIGNAL(timeout()), this, SLOT(rereadAudioControls()));
      }
      refreshAudioControlsTimer->start(500);

      if (refreshMetersTimer == 0) {
        refreshMetersTimer = new QTimer(this);
        refreshMetersTimer->setSingleShot(true);
        connect(refreshMetersTimer, SIGNAL(timeout()), this, SLOT(rereadMeters()));
      }
      refreshMetersTimer->start(150);

      enableAudioMixerAction();
      m_WarningState = WMSG_UNSPECIFIED;//zx, 2017-04-26
      break;
    }

    case UnknownScreen:

    default:
      break;
    }
  }
}

void MainWindow::doneAudioRefresh() {
  if (refreshAudioControlsTimer)
    refreshAudioControlsTimer->start(500);
  if (refreshMetersTimer)
    if (!refreshMetersTimer->isActive())
      refreshMetersTimer->start(150);
}

void MainWindow::doneMetersRefresh() {
  if (refreshMetersTimer)
    refreshMetersTimer->start(150);
}

void MainWindow::requestRefresh() { emit refreshAll(); }

void MainWindow::enableDevInfoAction() {
  if (devInfoAction) {
    devInfoAction->setEnabled(true);
  }
}

void MainWindow::enableMidiInfoAction() {
  if (midiInfoAction) {
    midiInfoAction->setEnabled(true);
  }
}

void MainWindow::enablePortRoutingAction() {
  if (portRoutingAction) {
    portRoutingAction->setEnabled(true);
  }
}

void MainWindow::enablePortFilterAction() {
  if (portFilterAction) {
    portFilterAction->setEnabled(true);
  }
}

void MainWindow::enableChannelRemapAction() {
  if (channelRemapAction) {
    channelRemapAction->setEnabled(true);
  }
}

void MainWindow::enableCCFilterAction() {
  if (ccFilterAction) {
    ccFilterAction->setEnabled(true);
  }
}

void MainWindow::enableCCRemapAction() {
  if (ccRemapAction) {
    ccRemapAction->setEnabled(true);
  }
}

void MainWindow::enableAudioInfoAction() {
  if (audioInfoAction) {
    audioInfoAction->setEnabled(true);
  }
}

void MainWindow::enableAudioPatchbayAction() {
  if (audioPatchbayAction) {
    audioPatchbayAction->setEnabled(true);
  }
}

void MainWindow::enableAudioMixerAction() {
  if (audioMixerAction) {
    audioMixerAction->setEnabled(true);
  }
}

void MainWindow::enableAudioControlAction() {
  if (audioControlAction) {
    audioControlAction->setEnabled(true);
  }
}

bool MainWindow::ensureAudioInfoSave() {
  for (int i = 0; i < ui->centralWidget->layout()->count(); ++i)
  {
    if (!strcmp(ui->centralWidget->layout()->itemAt(i)->widget()->metaObject()->className(),"AudioInfoForm")) {
      AudioInfoForm *audioInfoForm = qobject_cast<AudioInfoForm *>(ui->centralWidget->layout()->itemAt(i)->widget());
      if (audioInfoForm) {
        return audioInfoForm->promptForClose();
      }
      else {
        return true;
      }
    }
  }
  return true;
}

void MainWindow::updateFileMenu() {
  bool savableNew = false;

  if ((currentDevice) || (!connected)) {
    const auto& deviceID = currentDevice->getDeviceID();

    savableNew = ((deviceID.pid() == (Byte)(DevicePID::iConnect2Plus)) ||
               (deviceID.pid() == (Byte)(DevicePID::iConnect4Plus)) ||
               (deviceID.pid() == (Byte)(DevicePID::iConnect4Audio)) ||
               (deviceID.pid() == (Byte)(DevicePID::MIO2)) ||
               (deviceID.pid() == (Byte)(DevicePID::MIO4)) ||
               (deviceID.pid() == (Byte)(DevicePID::MIO10)) ||
               (deviceID.pid() == (Byte)(DevicePID::PlayAudio12)) ||
               //(deviceID.pid() == (Byte)(DevicePID::xxx)) ||
               (deviceID.pid() == (Byte)(DevicePID::iConnect2Audio)));
  }

  ui->actionOpen->setEnabled(savableNew);
  ui->actionOpenPresetsFolder->setEnabled(savableNew);
  ui->actionSave->setEnabled(savableNew);
}

void MainWindow::updateDeviceMenu() {
  bool commitToFlashSupported = false;
  bool restoreFromFlashSupported = false;
  bool restoreFactorySupported = false;
  bool resetSupported = false;
  bool resetToBootloaderSupported = false;

  if ((currentDevice) || (!connected)) {
    if (currentDevice->contains<SaveRestoreList>()) {
      auto& saveRestoreListData = currentDevice->get<SaveRestoreList>();
      commitToFlashSupported =
          saveRestoreListData.contains(SaveRestoreID::SaveToFlash);
      restoreFromFlashSupported =
          saveRestoreListData.contains(SaveRestoreID::RestoreFromFlash);
      restoreFactorySupported =
          saveRestoreListData.contains(SaveRestoreID::FactoryDefault);
    }

    if (currentDevice->contains<ResetList>()) {
      const auto& resetList = currentDevice->get<ResetList>();
      resetSupported = resetList.contains(BootMode::AppMode);
      resetToBootloaderSupported = resetList.contains(BootMode::BootLoaderMode);
    }
  }

  ui->actionCommit_Changes_to_FLASH->setEnabled(commitToFlashSupported);
  ui->actionRestore_from_FLASH->setEnabled(restoreFromFlashSupported);
  ui->actionRestore_to_Factory_Defaults->setEnabled(restoreFactorySupported);
  ui->actionReset->setEnabled(resetSupported);
  ui->actionReset_to_Bootloader_Mode->setEnabled(resetToBootloaderSupported);
}

void MainWindow::createActions() {
  QActionGroup *anActionGroup = new QActionGroup(this);
  // Gate off the first action
  if (!devInfoAction) {
    //TODO: change the status tips
    devInfoAction = new QAction(DeviceInfoForm::DefaultWindowTitle, this);
    devInfoAction->setCheckable(true);
    devInfoAction->setChecked(true);
    anActionGroup->addAction(devInfoAction);
    devInfoAction->setStatusTip(
          tr("Get/Set the information for the current iConnectivity device."));
    connect(devInfoAction, SIGNAL(triggered()), this,
            SLOT(deviceInfo_triggered()), Qt::QueuedConnection);

    midiInfoAction = new QAction(MIDIInfoForm::DefaultWindowTitle, this);
    midiInfoAction->setCheckable(true);
    anActionGroup->addAction(midiInfoAction);
    midiInfoAction->setStatusTip(tr(
                                   "Get/Set the MIDI information for the current iConnectivity device."));
    connect(midiInfoAction, SIGNAL(triggered()), this,
            SLOT(midiInfo_triggered()), Qt::QueuedConnection);

    portRoutingAction =
        new QAction(MIDIPortRoutingForm::DefaultWindowTitle, this);
    portRoutingAction->setCheckable(true);
    anActionGroup->addAction(portRoutingAction);
    portRoutingAction->setStatusTip(tr("Configure the MIDI Port routing."));
    connect(portRoutingAction, SIGNAL(triggered()), this,
            SLOT(portRouting_triggered()), Qt::QueuedConnection);

    portFilterAction =
        new QAction(MIDIPortFiltersForm::DefaultWindowTitle, this);
    portFilterAction->setStatusTip(tr("Configure the MIDI Port filters."));
    portFilterAction->setCheckable(true);
    anActionGroup->addAction(portFilterAction);
    connect(portFilterAction, SIGNAL(triggered()), this,
            SLOT(portFilter_triggered()), Qt::QueuedConnection);

    channelRemapAction =
        new QAction(MIDIChannelRemapForm::DefaultWindowTitle, this);
    channelRemapAction->setCheckable(true);
    anActionGroup->addAction(channelRemapAction);
    channelRemapAction->setStatusTip(
          tr("Configure the channel remap for a port."));
    connect(channelRemapAction, SIGNAL(triggered()), this,
            SLOT(channelRemap_triggered()), Qt::QueuedConnection);

    ccFilterAction =
        new QAction(MIDIControllerFilterForm::DefaultWindowTitle, this);
    ccFilterAction->setCheckable(true);
    anActionGroup->addAction(ccFilterAction);
    ccFilterAction->setStatusTip(
          tr("Configure the continuous controller remapping."));
    connect(ccFilterAction, SIGNAL(triggered()), this,
            SLOT(ccFilter_triggered()), Qt::QueuedConnection);

    ccRemapAction =
        new QAction(MIDIControllerRemapForm::DefaultWindowTitle, this);
    ccRemapAction->setCheckable(true);
    anActionGroup->addAction(ccRemapAction);
    ccRemapAction->setStatusTip(
          tr("Configure the continuous controller filters."));
    connect(ccRemapAction, SIGNAL(triggered()), this, SLOT(ccRemap_triggered()),
            Qt::QueuedConnection);

    audioInfoAction = new QAction(AudioInfoForm::DefaultWindowTitle, this);
    audioInfoAction->setCheckable(true);
    anActionGroup->addAction(audioInfoAction);
    audioInfoAction->setStatusTip(tr(
                                    "Get/Set the audio information for the current iConnectivity device."));
    connect(audioInfoAction, SIGNAL(triggered()), this,
            SLOT(audioInfo_triggered()), Qt::QueuedConnection);

    audioPatchbayAction =
        new QAction(AudioPatchbayForm::DefaultWindowTitle, this);
    audioPatchbayAction->setCheckable(true);
    anActionGroup->addAction(audioPatchbayAction);
    audioPatchbayAction->setStatusTip(tr("Configure the Audio Patchbay."));
    connect(audioPatchbayAction, SIGNAL(triggered()), this,
            SLOT(audioPatchbay_triggered()));

    audioControlAction =
        new QAction(AudioControlWidget::DefaultWindowTitle, this);
    audioControlAction->setCheckable(true);
    anActionGroup->addAction(audioControlAction);
    audioControlAction->setStatusTip(tr("Adjust the audio controls."));
    connect(audioControlAction, SIGNAL(triggered()), this,
            SLOT(audioControl_triggered()));

    audioMixerAction =
        new QAction(MixerWidget::DefaultWindowTitle, this);
    audioMixerAction->setCheckable(true);
    anActionGroup->addAction(audioMixerAction);
    audioMixerAction->setStatusTip(tr("Control the mixer."));
    connect(audioMixerAction, SIGNAL(triggered()), this,
            SLOT(audioMixerControl_triggered()));

    // File menus
    connect(ui->actionQuit, SIGNAL(triggered()), this, SLOT(close()));
  }
}

void MainWindow::writingStarted(int max) {
  if (progressDialog) {
    progressDialog.clear();
    progressDialog = QSharedPointer<QProgressDialog>(new QProgressDialog());

    progressDialog->setLabelText(tr("Writing settings to device"));
    progressDialog->setMaximum(max);
    progressDialog->setValue(0);
    progressDialog->show();
    progressDialog->setCancelButton(0);
  }
  ui->statusBar->showMessage(tr("Writing settings to device"));
}

void MainWindow::writingProgress(int value) {
  if (progressDialog) {
    progressDialog->setValue(value);
  }
}

void MainWindow::writingCompleted() {
  if (progressDialog) {
    progressDialog->hide();
  }
  ui->statusBar->showMessage(tr("Open file complete"), 2000);
  clearLayout(ui->centralWidget->layout());

  devInfoAction->setChecked(true);

  if (rebootMe) {
    doResetForOpen();
  }
  else {
//    currentDevice->send<SaveRestoreCommand>(SaveRestoreID::SaveToFlash);

    deviceInfo_triggered();
  }
}

void MainWindow::writingFailed(QString reason, int completed, int total) {
  continuedOpeningFileName.clear();
  rebootMe = false;
  if (progressDialog) {
    progressDialog->hide();
  }
  ui->statusBar->showMessage(tr("Preset restore stopped after %1 of %2 writes.")
                                 .arg(completed)
                                 .arg(total),
                             5000);
  QMessageBox::critical(
      this, tr("Restore Failed"),
      tr("The restore stopped after %1 of %2 acknowledged writes.\n\n%3\n\n"
         "No Save to Flash or reset operation was started.")
          .arg(completed)
          .arg(total)
          .arg(reason));
  deviceInfo_triggered();
}

void MainWindow::showConnectionError() {
  //zx, 2017-04-26
  QString szTitle = tr("Timeout");
  QString szMsg = tr("There was a communication error. Please check device connection.");
  if(m_WarningState == WMSG_POORNETWORK4FIRMWAREUPDATE)
  {
    QString szTitle = tr("Firmware Update Failed");
    QString szMsg = tr("Firmware update failed due to poor network connection.");
  }
  else if(m_WarningState == WMSG_DEVICECONNECTIONLOST)
  {
    QString szTitle = tr("Device Connection Lost");
    QString szMsg = tr("Selected device is missing. Please check device connection.");
  }
  QMessageBox::critical(this, szTitle, szMsg);
  m_WarningState = WMSG_UNSPECIFIED;

  if (refreshAudioControlsTimer != 0)
    refreshAudioControlsTimer->stop();

  if (refreshMetersTimer != 0)
    refreshMetersTimer->stop();

  if (currentDevice) {
    QTimer::singleShot(10, currentDevice.get(), SLOT(timeout()));
  }
  QTimer::singleShot(10, this, SLOT(on_actionClose_triggered()));
  if (progressDialog) {
    progressDialog->hide();
  }
  clearToolBar();
}

//zx, 2017-04-18
void MainWindow::showErrorWithTitleAndMsg(QString title, QString msg, bool bShutdownApp)
{
  QMessageBox::critical(this, title, msg);

  if(bShutdownApp == false)
     return;

  if (refreshAudioControlsTimer != 0)
    refreshAudioControlsTimer->stop();

  if (refreshMetersTimer != 0)
    refreshMetersTimer->stop();

  if (currentDevice) {
    QTimer::singleShot(10, currentDevice.get(), SLOT(timeout()));
  }
  QTimer::singleShot(10, this, SLOT(on_actionClose_triggered()));
  if (progressDialog) {
    progressDialog->hide();
  }
  clearToolBar();
}

QString MainWindow::extensionForPID(Word pid) {
  QString extension = ".ic";
  switch ((DevicePID::Enum) pid) {
  case DevicePID::iConnect2Plus:
    extension += tr("m2");
    break;

  case DevicePID::iConnect4Plus:
    extension += tr("m4");
    break;

  case DevicePID::iConnect4Audio:
    extension += tr("a4");
    break;

  case DevicePID::iConnect2Audio:
    extension += tr("a2");
    break;

  default:
    extension += tr("_unk");
    break;
  }

  return extension;
}

void MainWindow::ackCallback(CmdEnum, DeviceID, Word,
                             commandData_t commandData) {
  const auto& ackData = commandData.get<ACK>();
  switch (ackData.commandID()) {
  case Command::SaveRestore: {
    // update save restore
    if (ackData.errorCode() == ErrorCode::NoError) {

      // send reset
      if (!sysexMessages.empty()) {
        auto sysexBytes = sysexMessages.front();
        sysexMessages.pop();

        currentDevice->send(sysexBytes);
      }
      else {
        /*if ((currentDevice) && (connected)) {
          currentDevice->rereadStored();
        } else {
          showConnectionError();
        }*/
      }
    }
    break;
  }

  case Command::Reset: {
    emit closeAll();
  } break;

  default:
    break;
  }
}

void MainWindow::rereadAudioControls() {
  progressBar->hide();
  if (currentDevice) {
    currentDevice->rereadAudioControls();
  }
}

void MainWindow::rereadMeters() {
  progressBar->hide();
  if (currentDevice) {
    currentDevice->rereadMeters();
  }
}

void MainWindow::on_actionReread_Settings_triggered() {
  if ((!connected) || (currentDevice)) {
    currentDevice->rereadStored();
  } else {
    showConnectionError();
  }
}

void MainWindow::onTimeout() {
  if (currentDevice && currentDevice->restoreFailureRecorded()) {
    return;
  }
  if (connected) {
    showConnectionError();
    m_WarningState = WMSG_UNSPECIFIED; //zx. 2017-04-26

  }
}

void MainWindow::closeEvent(QCloseEvent* event) {
  writeSettings();
  event->accept();
}

void MainWindow::writeSettings() {
  QSettings settings(QCoreApplication::organizationName(),
                     QCoreApplication::applicationName());

  settings.beginGroup("mainwindow");
  settings.setValue("geometry-v2", saveGeometry());
  settings.setValue("windowstate", saveState());
  settings.endGroup();
}

void MainWindow::readSettings() {
  QSettings settings(QCoreApplication::organizationName(),
                     QCoreApplication::applicationName());

  settings.beginGroup("mainwindow");
  const QByteArray geometry = settings.value("geometry-v2").toByteArray();
  if (!geometry.isEmpty()) {
    restoreGeometry(geometry);
  } else {
    QSize initialSize = kDefaultMainWindowSize;
    if (QScreen* screen = QGuiApplication::primaryScreen()) {
      initialSize = initialSize.boundedTo(screen->availableGeometry().size());
    }
    resize(initialSize);
  }
  restoreState(settings.value("windowstate").toByteArray());
  settings.endGroup();
}

void MainWindow::on_actionAbout_triggered() {
  QString aboutString =
      QStringLiteral("<H1>iConfig Modern</H1>") +
      "<center>"
      "<table>"
      "<tr>"
      "   <th>Version: " + QCoreApplication::applicationVersion() +
      "</th>"
      "</tr>"
      "<tr><th>Based on iConnectivity iConfig " ICONFIG_BASE_VERSION "</th></tr>"
      "<tr/><tr>"
      "<th>Original software copyright &#169; iKingdom Corp. 2017</th>"
      "<tr><th>Community-maintained by Vaultnaemsae Software</th></tr>"
      "<tr><td>This is not an official current iConnectivity product.</td></tr>"
      "</table>"
      "</center>";

  QMessageBox::about(this, QCoreApplication::applicationName(), aboutString);
}

void MainWindow::closeAllInTime() {
  // Show dialog here
  if (currentDevice) {
    QTimer::singleShot(6900, currentDevice.get(), SLOT(timeout()));
  }

  if (resettingMessageBox) {
    resettingMessageBox.clear();
  }

  if (refreshAudioControlsTimer)
    refreshAudioControlsTimer->stop();

  if (refreshMetersTimer)
    refreshMetersTimer->stop();

  clearLayout(ui->centralWidget->layout());
  clearToolBar();

  resettingMessageBox = QSharedPointer<QMessageBox>(new QMessageBox());
  if (continuedOpeningFileName == "")
    resettingMessageBox->setText(tr("Resetting device."));
  else
    resettingMessageBox->setText(tr("Resetting device and continuing preset open."));

  resettingMessageBox->setInformativeText(tr("Please wait."));
  resettingMessageBox->setStandardButtons(QMessageBox::NoButton);
  resettingMessageBox->setIcon(QMessageBox::Information);
  resettingMessageBox->setWindowFlags(Qt::FramelessWindowHint);
  resettingMessageBox->show();
  QTimer::singleShot(10200, resettingMessageBox.data(), SLOT(hide()));
  QTimer::singleShot(10400, this, SLOT(on_actionClose_triggered()));
}

void MainWindow::on_actionIConnectivity_Website_triggered() {
  QDesktopServices::openUrl(
        QUrl("http://www.iconnectivity.com/", QUrl::TolerantMode));
}

void MainWindow::on_actionSoftware_Manual_triggered() {
  if (currentDevice) {
    const auto& deviceID = currentDevice->getDeviceID();

    const QString formattedString = QString::asprintf(
        "https://support.iconnectivity.com/support/iconfig/%04d/macPC/",
        deviceID.pid());
    QDesktopServices::openUrl(QUrl(formattedString));
  }
}

void MainWindow::on_actionRegister_triggered() {
  if (currentDevice) {
      QDesktopServices::openUrl(
          QUrl("https://www.iconnectivity.com/registration", QUrl::TolerantMode));
  }
}

void MainWindow::on_actionUpgrade_Firmware_triggered() {
  m_WarningState = WMSG_POORNETWORK4FIRMWAREUPDATE; //zx, 2017-04-25
  QPointer<FirmwareUpgradeDialog> firmwareDialog(new FirmwareUpgradeDialog(
                                                   this->comm, this->currentDevice, FirmwareMode::UpgradeMode, this));
  firmwareDialog->showNormal();
}

//zx, 2017-05-05
void MainWindow::on_actionUpgrade_Firmware_From_Local_Drive_triggered()
{
  QString filename = QFileDialog::getOpenFileName(this, tr("Load Firmware File"), "", tr("MIDI Files (*.mid)"));
  if(!filename.isEmpty())
  {
     QPointer<FirmwareUpgradeDialog> firmwareDialog(new FirmwareUpgradeDialog(this->comm, this->currentDevice, filename, this));
     firmwareDialog->ShowFWUpdateDialogForLocalFile();
  }
}
