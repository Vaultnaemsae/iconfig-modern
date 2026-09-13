/*
;iConfig source code and documentation is released under a GPLv3 license. 
;
; A copy is available from the Open Source Initiative site at:
;	https://opensource.org/licenses/gpl-3.0.html
*/

#include "MIDIChannelRemapForm.h"
#include "ui_MIDIChannelRemapForm.h"

#include "ACK.h"
#include "MyAlgorithms.h"
#include "MIDIInfo.h"
#include "MIDIPortInfo.h"

#include <QDebug>
#include <QLineEdit>
#include <QSignalBlocker>

#ifndef Q_MOC_RUN
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#endif

using namespace boost;
using namespace MyAlgorithms;
using namespace GeneSysLib;

namespace RemapStatusRow {
typedef enum Enum {
  PitchBendEvents = 1,
  ChannelPressureEvents,
  ProgramChangeEvents,
  ControlChangeEvents,
  PolyKeyPressureEvents,
  NoteEvents
} Enum;
}

const QString MIDIChannelRemapForm::DefaultWindowTitle = tr("MIDI Channel Remap");
const float MIDIChannelRemapForm::kBatchTime = 1500;

QList<CmdEnum> MIDIChannelRemapForm::Query() {
  QList<CmdEnum> query;
  query << Command::RetMIDIInfo << Command::RetMIDIPortInfo
        << Command::RetMIDIPortRemap;
  return query;
}

MIDIChannelRemapForm::MIDIChannelRemapForm(CommPtr _comm, DeviceInfoPtr _device,
                                           QWidget *_parent)
    : RefreshObject(_parent), ui(new Ui::MIDIChannelRemapForm) {
  comm = _comm;
  device = _device;
  Q_ASSERT(comm);
  Q_ASSERT(device);
  ui->setupUi(this);

  tableListener = new TableListener(ui->tableWidget, this);
  tableListener->addIgnoreRow(0);
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

  connect(ui->remapTypeComboBox, SIGNAL(currentIndexChanged(int)), this,
          SLOT(updateChannelRemap()));

  sendTimer = new QTimer(this);
  connect(sendTimer, SIGNAL(timeout()), this, SLOT(sendUpdate()));

  setWindowTitle(MIDIChannelRemapForm::DefaultWindowTitle);

  // Setup Table
  QStringList vertHeaderList;
  vertHeaderList << tr("Target channel") << tr("Pitch Bend")
                 << tr("Channel Pressure") << tr("Program Change")
                 << tr("Control Change") << tr("Poly Key Pressure")
                 << tr("Note On/Off");
  ui->tableWidget->setRowCount(vertHeaderList.count());
  ui->tableWidget->setVerticalHeaderLabels(vertHeaderList);
  ui->tableWidget->verticalHeader()->setDefaultSectionSize(25);

  QStringList horzHeaderList;
  for (auto i = 1; i <= 16; ++i) {
    horzHeaderList << QString::number(i);
  }
  ui->tableWidget->setColumnCount(horzHeaderList.count());
  ui->tableWidget->setHorizontalHeaderLabels(horzHeaderList);
  auto *const horzHeader = ui->tableWidget->horizontalHeader();
  Q_ASSERT(horzHeader);
  horzHeader->setSectionResizeMode(QHeaderView::Stretch);

  tableListener->addCornerLabel(tr("Source channel"));

  // Add the labels
  for (auto row = 1; row < ui->tableWidget->rowCount(); ++row) {
    for (auto col = 0; col < ui->tableWidget->columnCount(); ++col) {
      auto *const label = new QLabel();
      label->setScaledContents(true);
      label->setProperty(kBlockState, BlockState::Empty);
      ui->tableWidget->setCellWidget(row, col, label);
    }
  }

  lineEditSignalMapper = new QSignalMapper(this);

  // Add line edit
  for (auto col = 0; col < ui->tableWidget->columnCount(); ++col) {
    auto *const lineEdit = new QLineEdit();
    lineEdit->setValidator(new QIntValidator(1, 16, lineEdit));
    lineEdit->setText(QString::number(16));

    lineEditSignalMapper->setMapping(lineEdit, col);
    connect(lineEdit, SIGNAL(editingFinished()), lineEditSignalMapper,
            SLOT(map()));

    ui->tableWidget->setCellWidget(0, col, lineEdit);
  }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  connect(lineEditSignalMapper, SIGNAL(mappedInt(int)), this,
#else
  connect(lineEditSignalMapper, SIGNAL(mapped(int)), this,
#endif
          SLOT(lineEditChanged(int)));

  updateChannelRemap();
}

MIDIChannelRemapForm::~MIDIChannelRemapForm() {
  int count = 0;
  updateMutex.lock();
  count = updateList.count();
  updateMutex.unlock();

  if (count > 0) {
    sendUpdate();
  }
}

void MIDIChannelRemapForm::selectedPortIDsChanged(PortIDVector) {
  updateChannelRemap();
}

void MIDIChannelRemapForm::updateChannelRemap() {
  const auto &portID = portSelectionForm->selectedPortID();
  const auto &remapMap = device->midiPortRemap(portID, currentRemapID());

  for (auto col = 0; col < ui->tableWidget->columnCount(); ++col) {
    auto *const lineEdit =
        qobject_cast<QLineEdit *>(ui->tableWidget->cellWidget(0, col));
    Q_ASSERT(lineEdit);

    if (static_cast<size_t>(col) >= remapMap.numRemapStatuses()) {
      qWarning() << "MIDI channel remap column has no corresponding device data:"
                 << col;
      continue;
    }

    const auto &remapStatus = remapMap.remapStatus_at(col);
    const QSignalBlocker blocker(lineEdit);
    lineEdit->setText(QString::number(remapStatus.channelNumber + 1));
  }

  tableListener->updateWidgets(
      bind(&MIDIChannelRemapForm::stateForCell, this, _1, _2));
  updateColumnPresentation(remapMap);
}

void MIDIChannelRemapForm::updateColumnPresentation(
    const MIDIPortRemap &remapMap) {
  for (auto col = 0; col < ui->tableWidget->columnCount(); ++col) {
    if (static_cast<size_t>(col) >= remapMap.numRemapStatuses()) {
      continue;
    }

    const auto &remapStatus = remapMap.remapStatus_at(col);
    bool active = false;
    for (int row = RemapStatusRow::PitchBendEvents;
         row <= RemapStatusRow::NoteEvents; ++row) {
      active = active || rowToRemapStatus(remapStatus, row);
    }

    const auto explanation =
        active
            ? tr("Source MIDI channel %1 remaps enabled event types to MIDI "
                 "channel %2.")
                  .arg(col + 1)
                  .arg(remapStatus.channelNumber + 1)
            : tr("Inactive because no event types are enabled. Stored target "
                 "channel: %1.")
                  .arg(remapStatus.channelNumber + 1);

    auto *const lineEdit = qobject_cast<QLineEdit *>(
        ui->tableWidget->cellWidget(0, col));
    if (lineEdit) {
      lineEdit->setEnabled(active);
      lineEdit->setToolTip(explanation);
    }

    auto *const headerItem = ui->tableWidget->horizontalHeaderItem(col);
    if (headerItem) {
      headerItem->setToolTip(explanation);
    }
  }
}

void MIDIChannelRemapForm::sendUpdate() {
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

  sendTimer->stop();
}

void MIDIChannelRemapForm::cellStateChange(int row, int col,
                                           BlockState::Enum state) {
  if ((row < RemapStatusRow::PitchBendEvents) ||
      (row > RemapStatusRow::NoteEvents) || (col < 0) ||
      (col >= ui->tableWidget->columnCount())) {
    qWarning() << "Ignoring invalid MIDI channel remap event cell:" << row
               << col;
    return;
  }

  const auto &portID = portSelectionForm->selectedPortID();
  auto &remapMap = device->midiPortRemap(portID, currentRemapID());

  if (static_cast<size_t>(col) >= remapMap.numRemapStatuses()) {
    qWarning() << "MIDI channel remap event column has no device data:" << col;
    return;
  }

  auto &remapStatus = remapMap.remapStatus_at(col);
  const auto requestedState = (state == BlockState::Full);

  if (rowToRemapStatus(remapStatus, row) == requestedState) {
    return;
  }

  setRemapStatusBit(remapStatus, row, requestedState);
  updateColumnPresentation(remapMap);

  addToUpdateList(portID);
}

void MIDIChannelRemapForm::lineEditChanged(int col) {
  if ((col < 0) || (col >= ui->tableWidget->columnCount())) {
    qWarning() << "Ignoring invalid MIDI channel remap target column:" << col;
    return;
  }

  auto *const lineEdit =
      qobject_cast<QLineEdit *>(ui->tableWidget->cellWidget(0, col));
  if (!lineEdit) {
    qWarning() << "Missing MIDI channel remap target editor at column:" << col;
    return;
  }

  const auto &portID = portSelectionForm->selectedPortID();
  auto &remapMap = device->midiPortRemap(portID, currentRemapID());

  if (static_cast<size_t>(col) >= remapMap.numRemapStatuses()) {
    qWarning() << "MIDI channel remap target column has no device data:" << col;
    return;
  }

  auto &remapStatus = remapMap.remapStatus_at(col);

  auto text = lineEdit->text();
  int cursorPosition = 0;
  const auto acceptable = lineEdit->validator() &&
                          (lineEdit->validator()->validate(
                               text, cursorPosition) == QValidator::Acceptable);
  bool converted = false;
  const auto displayedChannel = text.toInt(&converted);
  if (!acceptable || !converted || (displayedChannel < 1) ||
      (displayedChannel > 16)) {
    qWarning() << "Ignoring invalid MIDI channel remap target:" << text
               << "at source channel" << (col + 1);
    const QSignalBlocker blocker(lineEdit);
    lineEdit->setText(QString::number(remapStatus.channelNumber + 1));
    return;
  }

  const auto targetChannel = static_cast<Byte>(displayedChannel - 1);
  if (remapStatus.channelNumber == targetChannel) {
    return;
  }

  remapStatus.channelNumber = targetChannel;
  updateColumnPresentation(remapMap);

  addToUpdateList(portID);
}

BlockState::Enum MIDIChannelRemapForm::stateForCell(int row, int col) const {
  if ((row < RemapStatusRow::PitchBendEvents) ||
      (row > RemapStatusRow::NoteEvents) || (col < 0) ||
      (col >= ui->tableWidget->columnCount())) {
    return BlockState::Empty;
  }

  const auto &remapID =
      (ui->remapTypeComboBox->currentIndex() == 0) ? RemapID::InputRemap
                                                   : RemapID::OutputRemap;
  const auto &portID = portSelectionForm->selectedPortID();
  const auto &remapMap = device->midiPortRemap(portID, remapID);

  if (static_cast<size_t>(col) >= remapMap.numRemapStatuses()) {
    return BlockState::Empty;
  }

  const auto &remapStatus = remapMap.remapStatus_at(col);

  return ((rowToRemapStatus(remapStatus, row)) ? (BlockState::Full)
                                               : (BlockState::Empty));
}

bool MIDIChannelRemapForm::rowToRemapStatus(
    const MIDIPortRemap::RemapStatus &remapStatus, int row) const {
  bool statusSet = false;
  switch (row) {
    case RemapStatusRow::PitchBendEvents:
      statusSet = remapStatus.pitchBendEvents;
      break;
    case RemapStatusRow::ChannelPressureEvents:
      statusSet = remapStatus.channelPressureEvents;
      break;
    case RemapStatusRow::ProgramChangeEvents:
      statusSet = remapStatus.programChangeEvents;
      break;
    case RemapStatusRow::ControlChangeEvents:
      statusSet = remapStatus.controlChangeEvents;
      break;
    case RemapStatusRow::PolyKeyPressureEvents:
      statusSet = remapStatus.polyKeyPressureEvents;
      break;
    case RemapStatusRow::NoteEvents:
      statusSet = remapStatus.noteEvents;
      break;
    default:
      break;
  }
  return statusSet;
}

void MIDIChannelRemapForm::setRemapStatusBit(
    MIDIPortRemap::RemapStatus &remapStatus, int row, bool value) const {
  switch (row) {
    case RemapStatusRow::PitchBendEvents:
      remapStatus.pitchBendEvents = value;
      break;
    case RemapStatusRow::ChannelPressureEvents:
      remapStatus.channelPressureEvents = value;
      break;
    case RemapStatusRow::ProgramChangeEvents:
      remapStatus.programChangeEvents = value;
      break;
    case RemapStatusRow::ControlChangeEvents:
      remapStatus.controlChangeEvents = value;
      break;
    case RemapStatusRow::PolyKeyPressureEvents:
      remapStatus.polyKeyPressureEvents = value;
      break;
    case RemapStatusRow::NoteEvents:
      remapStatus.noteEvents = value;
      break;
    default:
      break;
  }
}

RemapTypeEnum MIDIChannelRemapForm::currentRemapID() const {
  return (ui->remapTypeComboBox->currentIndex() == 0) ? RemapID::InputRemap
                                                      : RemapID::OutputRemap;
}

void MIDIChannelRemapForm::refreshWidget() {
  portSelectionForm->updateTree();
  this->updateChannelRemap();
}

void MIDIChannelRemapForm::addToUpdateList(Word portID) {
  sendTimer->stop();
  updateMutex.lock();
  updateList.insert(qMakePair(portID, currentRemapID()));
  updateMutex.unlock();
  sendTimer->start(kBatchTime);
}
