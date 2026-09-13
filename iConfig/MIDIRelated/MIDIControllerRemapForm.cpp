/*
;iConfig source code and documentation is released under a GPLv3 license. 
;
; A copy is available from the Open Source Initiative site at:
;	https://opensource.org/licenses/gpl-3.0.html
*/

#include "MIDIControllerRemapForm.h"
#include "ui_MIDIControllerRemapForm.h"

#include "ACK.h"
#include "CCList.h"
#include "DeviceID.h"
#include "MyAlgorithms.h"

#include <QComboBox>
#include <QDebug>
#include <QSignalBlocker>
#ifndef Q_MOC_RUN
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#endif


using namespace GeneSysLib;
using namespace MyAlgorithms;
using namespace boost;

namespace RemapControllerColumns {
typedef enum Enum {
  Channel1 = 0,
  Channel2,
  Channel3,
  Channel4,
  Channel5,
  Channel6,
  Channel7,
  Channel8,
  Channel9,
  Channel10,
  Channel11,
  Channel12,
  Channel13,
  Channel14,
  Channel15,
  Channel16,
  Source,
  Destination
} Enum;
}

const QString MIDIControllerRemapForm::DefaultWindowTitle =
    tr("MIDI Controller Remap");
const float MIDIControllerRemapForm::kBatchTime = 1500;

QList<CmdEnum> MIDIControllerRemapForm::Query() {
  QList<CmdEnum> query;
  query << Command::RetMIDIInfo << Command::RetMIDIPortInfo
        << Command::RetMIDIPortRemap;
  return query;
}

MIDIControllerRemapForm::MIDIControllerRemapForm(DeviceInfoPtr _device,
                                                 QWidget *_parent)
    : RefreshObject(_parent),
      ackHandlerID(-1),
      ui(new Ui::MIDIControllerRemapForm),
      device(_device) {
  Q_ASSERT(device);
  ui->setupUi(this);

  tableListener = new TableListener(ui->tableWidget, this);
  tableListener->addIgnoreCol(RemapControllerColumns::Source);
  tableListener->addIgnoreCol(RemapControllerColumns::Destination);
  connect(tableListener, SIGNAL(cellStateChanged(int, int, BlockState::Enum)),
          this, SLOT(cellStateChange(int, int, BlockState::Enum)));

  portSelectionForm = new MIDIPortSelectionForm(device, this);

  auto *const gridLayout = new QGridLayout(ui->portSelectionContainer);
  gridLayout->addWidget(portSelectionForm, 0, 0, 1, 1);
  gridLayout->setContentsMargins(0, 0, 0, 0);
  gridLayout->setSpacing(0);
  gridLayout->setVerticalSpacing(0);

  connect(portSelectionForm, SIGNAL(selectedPortIDsChanged(PortIDVector)), this,
          SLOT(selectedPortIDsChanged(PortIDVector)));
  portSelectionForm->setSelectionMode(QAbstractItemView::SingleSelection);

  sendTimer = new QTimer(this);
  connect(sendTimer, SIGNAL(timeout()), this, SLOT(sendUpdate()));

  auto ackHandler =
      bind(&MIDIControllerRemapForm::ackCallback, this, _1, _2, _3, _4);
  ackHandlerID = device->registerHandler(Command::ACK, ackHandler);

  connect(ui->remapTypeComboBox, SIGNAL(currentIndexChanged(int)), this,
          SLOT(updateCCRemap()));

  const auto &portID = portSelectionForm->selectedPortID();

  const auto &remapMap = device->midiPortRemap(portID, currentRemapID());
  const auto &maxControllers = remapMap.maxControllerSupported();

  // Setup col map
  colChBitmapMap[RemapControllerColumns::Channel1] =
      MIDIPortRemap::ChannelBitmapBits::channel1;
  colChBitmapMap[RemapControllerColumns::Channel2] =
      MIDIPortRemap::ChannelBitmapBits::channel2;
  colChBitmapMap[RemapControllerColumns::Channel3] =
      MIDIPortRemap::ChannelBitmapBits::channel3;
  colChBitmapMap[RemapControllerColumns::Channel4] =
      MIDIPortRemap::ChannelBitmapBits::channel4;
  colChBitmapMap[RemapControllerColumns::Channel5] =
      MIDIPortRemap::ChannelBitmapBits::channel5;
  colChBitmapMap[RemapControllerColumns::Channel6] =
      MIDIPortRemap::ChannelBitmapBits::channel6;
  colChBitmapMap[RemapControllerColumns::Channel7] =
      MIDIPortRemap::ChannelBitmapBits::channel7;
  colChBitmapMap[RemapControllerColumns::Channel8] =
      MIDIPortRemap::ChannelBitmapBits::channel8;
  colChBitmapMap[RemapControllerColumns::Channel9] =
      MIDIPortRemap::ChannelBitmapBits::channel9;
  colChBitmapMap[RemapControllerColumns::Channel10] =
      MIDIPortRemap::ChannelBitmapBits::channel10;
  colChBitmapMap[RemapControllerColumns::Channel11] =
      MIDIPortRemap::ChannelBitmapBits::channel11;
  colChBitmapMap[RemapControllerColumns::Channel12] =
      MIDIPortRemap::ChannelBitmapBits::channel12;
  colChBitmapMap[RemapControllerColumns::Channel13] =
      MIDIPortRemap::ChannelBitmapBits::channel13;
  colChBitmapMap[RemapControllerColumns::Channel14] =
      MIDIPortRemap::ChannelBitmapBits::channel14;
  colChBitmapMap[RemapControllerColumns::Channel15] =
      MIDIPortRemap::ChannelBitmapBits::channel15;
  colChBitmapMap[RemapControllerColumns::Channel16] =
      MIDIPortRemap::ChannelBitmapBits::channel16;

  // Setup Table
  QStringList vertHeaderList;
  for (auto i = 1; i <= maxControllers; ++i) {
    vertHeaderList << tr("Mapping %1").arg(QString::number(i));
  }
  ui->tableWidget->setRowCount(vertHeaderList.count());
  ui->tableWidget->setVerticalHeaderLabels(vertHeaderList);
  ui->tableWidget->verticalHeader()->setDefaultSectionSize(25);

  QStringList horzHeaderList;
  for (auto i = 1; i <= 16; ++i) {
    horzHeaderList << QString::number(i);
  }
  horzHeaderList << tr("Source CC");
  horzHeaderList << tr("Destination CC");
  ui->tableWidget->setColumnCount(horzHeaderList.count());
  ui->tableWidget->setHorizontalHeaderLabels(horzHeaderList);

  auto *const horzHeader = ui->tableWidget->horizontalHeader();
  horzHeader->setSectionResizeMode(QHeaderView::Fixed);
  horzHeader->setDefaultSectionSize(25);
  horzHeader->setSectionResizeMode(horzHeaderList.count() - 2,
                                   QHeaderView::Stretch);
  horzHeader->setSectionResizeMode(horzHeaderList.count() - 1,
                                   QHeaderView::Stretch);

  // add comboBoxes
  srcSignalMapper = new QSignalMapper(this);
  for (auto row = 0; row < ui->tableWidget->rowCount(); ++row) {
    auto *const comboBox = new QComboBox();
    comboBox->addItems(CCList());
    srcSignalMapper->setMapping(comboBox, row);
    connect(comboBox, SIGNAL(currentIndexChanged(int)), srcSignalMapper,
            SLOT(map()));
    ui->tableWidget
        ->setCellWidget(row, RemapControllerColumns::Source, comboBox);
  }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  connect(srcSignalMapper, SIGNAL(mappedInt(int)), this,
#else
  connect(srcSignalMapper, SIGNAL(mapped(int)), this,
#endif
          SLOT(srcComboBoxChanged(int)));

  dstSignalMapper = new QSignalMapper(this);
  for (auto row = 0; row < ui->tableWidget->rowCount(); ++row) {
    auto *const comboBox = new QComboBox();
    comboBox->addItems(CCList());
    dstSignalMapper->setMapping(comboBox, row);
    connect(comboBox, SIGNAL(currentIndexChanged(int)), dstSignalMapper,
            SLOT(map()));
    ui->tableWidget
        ->setCellWidget(row, RemapControllerColumns::Destination, comboBox);
  }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  connect(dstSignalMapper, SIGNAL(mappedInt(int)), this,
#else
  connect(dstSignalMapper, SIGNAL(mapped(int)), this,
#endif
          SLOT(dstComboBoxChanged(int)));

  tableListener->addCornerLabel(tr("Channels"));

  for (auto row = 0; row < ui->tableWidget->rowCount(); ++row) {
    for (auto col = 0; col < ui->tableWidget->columnCount() - 2; ++col) {
      auto *const label = new QLabel();
      label->setScaledContents(true);
      label->setProperty(kBlockState, BlockState::Empty);
      ui->tableWidget->setCellWidget(row, col, label);
    }
  }

  updateCCRemap();

  setWindowTitle(MIDIControllerRemapForm::DefaultWindowTitle);
}

MIDIControllerRemapForm::~MIDIControllerRemapForm() {
  int count = 0;
  updateMutex.lock();
  count = updateList.count();
  updateMutex.unlock();

  if (count > 0) {
    sendUpdate();
  }

  device->unRegisterHandler(Command::ACK, ackHandlerID);
}

void MIDIControllerRemapForm::selectedPortIDsChanged(PortIDVector) {
  updateCCRemap();
}

void MIDIControllerRemapForm::updateCCRemap() {
  const auto &portID = portSelectionForm->selectedPortID();
  const auto &remapMap = device->midiPortRemap(portID, currentRemapID());

  // Update source and Destination
  for (auto row = 0; row < (int) ui->tableWidget->rowCount(); ++row) {
    auto *const srcComboBox = qobject_cast<QComboBox *>(
        ui->tableWidget->cellWidget(row, RemapControllerColumns::Source));
    Q_ASSERT(srcComboBox);
    auto *const dstComboBox = qobject_cast<QComboBox *>(
        ui->tableWidget->cellWidget(row, RemapControllerColumns::Destination));
    Q_ASSERT(dstComboBox);

    if (static_cast<size_t>(row) >= remapMap.numControllers()) {
      qWarning() << "MIDI controller remap row has no corresponding device data:"
                 << row;
      continue;
    }

    const auto &remapController = remapMap.controller_at(row);
    const auto source = static_cast<int>(remapController.controllerSource);
    const auto destination =
        static_cast<int>(remapController.controllerDestination);
    const QSignalBlocker sourceBlocker(srcComboBox);
    const QSignalBlocker destinationBlocker(dstComboBox);

    if (source >= srcComboBox->count()) {
      srcComboBox->setCurrentIndex(-1);
      qWarning() << "Invalid MIDI controller remap source from device:"
                 << source << "at row" << row;
    } else {
      srcComboBox->setCurrentIndex(source);
    }

    if (destination >= dstComboBox->count()) {
      dstComboBox->setCurrentIndex(-1);
      qWarning() << "Invalid MIDI controller remap destination from device:"
                 << destination << "at row" << row;
    } else {
      dstComboBox->setCurrentIndex(destination);
    }
  }

  tableListener->updateWidgets(
      bind(&MIDIControllerRemapForm::stateForCell, this, _1, _2));
  updateRowPresentation(remapMap);
}

void MIDIControllerRemapForm::updateRowPresentation(
    const MIDIPortRemap &remapMap) {
  for (auto row = 0; row < ui->tableWidget->rowCount(); ++row) {
    if (static_cast<size_t>(row) >= remapMap.numControllers()) {
      continue;
    }

    const auto &entry = remapMap.controller_at(row);
    QStringList enabledChannels;
    for (int col = RemapControllerColumns::Channel1;
         col <= RemapControllerColumns::Channel16; ++col) {
      const auto channel = colChBitmapMap.at(col);
      if (entry.channelBitmap.test(channel)) {
        enabledChannels << QString::number(col + 1);
      }
    }

    const auto active = !enabledChannels.isEmpty();
    const auto rowName = active
                             ? tr("Mapping %1").arg(row + 1)
                             : tr("Mapping %1 (unused)").arg(row + 1);
    const auto explanation =
        active
            ? tr("Active on MIDI channel(s): %1.")
                  .arg(enabledChannels.join(", "))
            : tr("Unused because no MIDI channels are enabled. CC 0 remains "
                 "a valid controller value.");

    auto *const headerItem = ui->tableWidget->verticalHeaderItem(row);
    if (headerItem) {
      headerItem->setText(rowName);
      headerItem->setToolTip(explanation);
    }

    for (const auto column : {RemapControllerColumns::Source,
                              RemapControllerColumns::Destination}) {
      auto *const comboBox = qobject_cast<QComboBox *>(
          ui->tableWidget->cellWidget(row, column));
      if (comboBox) {
        comboBox->setToolTip(explanation);
      }
    }
  }
}

void MIDIControllerRemapForm::sendUpdate() {
  sendTimer->stop();

  updateMutex.lock();
  QSetIterator<RemapUpdateKey> i(updateList);
  while (i.hasNext()) {
    const auto update = i.next();
    const auto &portID = update.first;
    const auto &remapID = update.second;
    const auto &remapData = device->midiPortRemap(portID, remapID);

    device->send<SetMIDIPortRemapCommand>(remapData);
  }
  updateList.clear();
  updateMutex.unlock();
}

void MIDIControllerRemapForm::cellStateChange(int row, int col,
                                              BlockState::Enum state) {
  if ((row < 0) || (row >= ui->tableWidget->rowCount()) ||
      (colChBitmapMap.count(col) == 0)) {
    return;
  }

  const auto &portID = portSelectionForm->selectedPortID();
  auto &remapMap = device->midiPortRemap(portID, currentRemapID());

  if (static_cast<size_t>(row) >= remapMap.numControllers()) {
    qWarning() << "MIDI controller remap edit row has no device data:" << row;
    return;
  }

  auto &remapController = remapMap.controller_at(row);

  const auto channel = colChBitmapMap.at(col);
  const auto requestedState = (state == BlockState::Full);
  if (remapController.channelBitmap[channel] == requestedState) {
    return;
  }

  remapController.channelBitmap[channel] = requestedState;
  updateRowPresentation(remapMap);
  addToUpdateList(portID);
}

void MIDIControllerRemapForm::srcComboBoxChanged(int row) {
  if ((row < 0) || (row >= ui->tableWidget->rowCount())) {
    qWarning() << "Ignoring invalid MIDI controller remap source row:" << row;
    return;
  }

  auto *const comboBox = qobject_cast<QComboBox *>(
      ui->tableWidget->cellWidget(row, RemapControllerColumns::Source));
  if (!comboBox || (comboBox->currentIndex() < 0) ||
      (comboBox->currentIndex() >= comboBox->count())) {
    qWarning() << "Ignoring invalid MIDI controller remap source selection at row"
               << row;
    return;
  }

  const auto &portID = portSelectionForm->selectedPortID();
  auto &remapMap = device->midiPortRemap(portID, currentRemapID());

  if (static_cast<size_t>(row) >= remapMap.numControllers()) {
    qWarning() << "MIDI controller remap source row has no device data:" << row;
    return;
  }

  auto &remapController = remapMap.controller_at(row);

  if (remapController.controllerSource ==
      static_cast<Byte>(comboBox->currentIndex())) {
    return;
  }

  remapController.controllerSource = comboBox->currentIndex();
  addToUpdateList(portID);
}

void MIDIControllerRemapForm::dstComboBoxChanged(int row) {
  if ((row < 0) || (row >= ui->tableWidget->rowCount())) {
    qWarning() << "Ignoring invalid MIDI controller remap destination row:"
               << row;
    return;
  }

  auto *const comboBox = qobject_cast<QComboBox *>(
      ui->tableWidget->cellWidget(row, RemapControllerColumns::Destination));
  if (!comboBox || (comboBox->currentIndex() < 0) ||
      (comboBox->currentIndex() >= comboBox->count())) {
    qWarning()
        << "Ignoring invalid MIDI controller remap destination selection at row"
        << row;
    return;
  }

  const auto &portID = portSelectionForm->selectedPortID();
  auto &remapMap = device->midiPortRemap(portID, currentRemapID());

  if (static_cast<size_t>(row) >= remapMap.numControllers()) {
    qWarning() << "MIDI controller remap destination row has no device data:"
               << row;
    return;
  }

  auto &remapController = remapMap.controller_at(row);

  if (remapController.controllerDestination ==
      static_cast<Byte>(comboBox->currentIndex())) {
    return;
  }

  remapController.controllerDestination = comboBox->currentIndex();
  addToUpdateList(portID);
}

void MIDIControllerRemapForm::ackCallback(CmdEnum, DeviceID, Word,
                                          commandData_t commandData) {
  const auto &ackData = commandData.get<ACK>();
  if (ackData.commandID() == Command::SetMIDIPortRemap) {
    emit requestRefresh();
  }
}

BlockState::Enum MIDIControllerRemapForm::stateForCell(int row, int col) const {
  const auto &portID = portSelectionForm->selectedPortID();
  const auto &remapMap = device->midiPortRemap(portID, currentRemapID());
  const auto &remapFlags = remapMap.controller_at(row);

  return (remapFlags.channelBitmap[colChBitmapMap.at(col)])
             ? (BlockState::Full)
             : (BlockState::Empty);
}

RemapTypeEnum MIDIControllerRemapForm::currentRemapID() const {
  return (ui->remapTypeComboBox->currentIndex() == 0) ? RemapID::InputRemap
                                                      : RemapID::OutputRemap;
}

void MIDIControllerRemapForm::refreshWidget() {
  portSelectionForm->updateTree();
  this->updateCCRemap();
}

void MIDIControllerRemapForm::addToUpdateList(Word portID) {
  sendTimer->stop();
  updateMutex.lock();
  updateList.insert(qMakePair(portID, currentRemapID()));
  updateMutex.unlock();
  sendTimer->start(kBatchTime);
}
