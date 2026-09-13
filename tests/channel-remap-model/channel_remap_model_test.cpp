#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QTextStream>

#define private public
#include "MIDIRelated/MIDIChannelRemapForm.h"
#undef private

#include "CommandData.h"
#include "MIDIInfo.h"
#include "MIDIPortInfo.h"
#include "MIDIPortRemap.h"
#include "PortType.h"

#include <cstdlib>

using namespace GeneSysLib;

namespace {

enum EventRow {
  TargetRow = 0,
  PitchBendRow = 1,
  ChannelPressureRow = 2,
  ProgramChangeRow = 3,
  ControlChangeRow = 4,
  PolyPressureRow = 5,
  NoteRow = 6
};

int failures = 0;

void check(bool condition, const QString &message) {
  QTextStream out(stdout);
  out << (condition ? "PASS " : "FAIL ") << message << Qt::endl;
  if (!condition) {
    ++failures;
  }
}

QList<int> changedByteIndexes(const Bytes &before, const Bytes &after) {
  QList<int> result;
  for (int index = 0; index < static_cast<int>(before.size()); ++index) {
    if (before[index] != after[index]) {
      result << index;
    }
  }
  return result;
}

MIDIPortInfo makePortInfo() {
  Bytes data;
  data += 0x01;
  appendMidiWord(data, 1);
  data += static_cast<Byte>(PortType::DIN);
  data += 0x01;
  data += 0x00;
  data += 0x00;
  data += 0x00;
  data += 0x20;
  data += 0x03;
  const char name[] = "Synthetic DIN 1";
  data.insert(data.end(), name, name + sizeof(name) - 1);

  MIDIPortInfo result;
  auto begin = data.begin();
  auto end = data.end();
  result.parse(begin, end);
  return result;
}

void setFlags(MIDIPortRemap::RemapStatus &status, Byte mask) {
  status.pitchBendEvents = (mask & 0x20) != 0;
  status.channelPressureEvents = (mask & 0x10) != 0;
  status.programChangeEvents = (mask & 0x08) != 0;
  status.controlChangeEvents = (mask & 0x04) != 0;
  status.polyKeyPressureEvents = (mask & 0x02) != 0;
  status.noteEvents = (mask & 0x01) != 0;
}

Byte flagMask(const MIDIPortRemap::RemapStatus &status) {
  return status.generate().at(0);
}

MIDIPortRemap makeRemap(RemapTypeEnum type, bool outputVariant) {
  const Byte inputMasks[16] = {
      0x01, 0x04, 0x20, 0x08, 0x10, 0x02, 0x05, 0x3f,
      0x00, 0x2a, 0x15, 0x01, 0x04, 0x18, 0x22, 0x3f};
  MIDIPortRemap::RemapStatues statuses = {};
  for (int source = 0; source < 16; ++source) {
    statuses[source].channelNumber = static_cast<Byte>(
        outputVariant ? source : (15 - source));
    const Byte mask = outputVariant
                          ? static_cast<Byte>(inputMasks[15 - source])
                          : inputMasks[source];
    setFlags(statuses[source], mask);
  }

  MIDIPortRemap::RemapFlagsVector controllers;
  return MIDIPortRemap(1, type, 0, statuses, controllers);
}

QLineEdit *targetEditor(QTableWidget *table, int sourceChannelIndex) {
  return qobject_cast<QLineEdit *>(table->cellWidget(TargetRow,
                                                      sourceChannelIndex));
}

bool eventEnabled(QTableWidget *table, int row, int sourceChannelIndex) {
  auto *label = qobject_cast<QLabel *>(table->cellWidget(row,
                                                         sourceChannelIndex));
  return label && label->property(kBlockState).toInt() == BlockState::Full;
}

bool expectedEvent(const MIDIPortRemap::RemapStatus &status, int row) {
  switch (row) {
    case PitchBendRow: return status.pitchBendEvents;
    case ChannelPressureRow: return status.channelPressureEvents;
    case ProgramChangeRow: return status.programChangeEvents;
    case ControlChangeRow: return status.controlChangeEvents;
    case PolyPressureRow: return status.polyKeyPressureEvents;
    case NoteRow: return status.noteEvents;
    default: return false;
  }
}

void clearPending(MIDIChannelRemapForm *form) {
  form->updateList.clear();
  form->sendTimer->stop();
}

void verifyDisplayedRemap(MIDIChannelRemapForm *form,
                          const MIDIPortRemap &expected,
                          const QString &name) {
  auto *table = form->findChild<QTableWidget *>("tableWidget");
  check(table != nullptr, name + ": table exists");
  if (!table) {
    return;
  }
  check(table->rowCount() == 7,
        name + ": target row plus six event rows");
  check(table->columnCount() == 16,
        name + ": sixteen source-channel columns");
  for (int source = 0; source < 16; ++source) {
    const auto &status = expected.remapStatus_at(source);
    auto *editor = targetEditor(table, source);
    check(editor && editor->text().toInt() == status.channelNumber + 1,
          QString("%1: source channel %2 displays target %3")
              .arg(name).arg(source + 1).arg(status.channelNumber + 1));
    for (int row = PitchBendRow; row <= NoteRow; ++row) {
      check(eventEnabled(table, row, source) == expectedEvent(status, row),
            QString("%1: source channel %2 event row %3")
                .arg(name).arg(source + 1).arg(row));
    }
    const bool active = flagMask(status) != 0;
    check(editor && editor->isEnabled() == active,
          QString("%1: source channel %2 target editor active state")
              .arg(name).arg(source + 1));
  }
}

void verifyStatusEncoding() {
  struct Case { const char *name; Byte mask; };
  const Case cases[] = {
      {"none", 0x00}, {"note", 0x01}, {"poly pressure", 0x02},
      {"control change", 0x04}, {"program change", 0x08},
      {"channel pressure", 0x10}, {"pitch bend", 0x20},
      {"note + control change", 0x05},
      {"program change + channel pressure", 0x18},
      {"all six", 0x3f}};

  for (const auto &test : cases) {
    MIDIPortRemap::RemapStatus status = {};
    status.channelNumber = 15;
    setFlags(status, test.mask);
    const Bytes bytes = status.generate();
    check(bytes == Bytes({test.mask, 0x0f}),
          QString("status bytes %1").arg(test.name));
  }
}

void verifyRoundTrip(const MIDIPortRemap &original, const QString &name) {
  Bytes generated = original.generate();
  MIDIPortRemap parsed;
  auto begin = generated.begin();
  auto end = generated.end();
  parsed.parse(begin, end);
  check(begin == end, name + ": parser consumes complete payload");
  check(parsed.generate() == generated,
        name + ": parse/generate round trip is byte-exact");
}

}  // namespace

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  verifyStatusEncoding();

  CommPtr comm(new Communicator());
  DeviceInfoPtr device(new DeviceInfo(comm));
  device->addCommandData(commandData_t(
      MIDIInfo(1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0)));
  device->addCommandData(commandData_t(makePortInfo()));
  const auto input = makeRemap(RemapID::InputRemap, false);
  const auto output = makeRemap(RemapID::OutputRemap, true);
  verifyRoundTrip(input, "input channel remap");
  verifyRoundTrip(output, "output channel remap");
  device->addCommandData(commandData_t(input));
  device->addCommandData(commandData_t(output));

  auto *form = new MIDIChannelRemapForm(comm, device);
  auto *table = form->findChild<QTableWidget *>("tableWidget");
  auto *typeCombo = form->findChild<QComboBox *>("remapTypeComboBox");
  check(typeCombo && typeCombo->currentIndex() == 0,
        "Input Remap is selector index 0");
  check(typeCombo && typeCombo->itemText(0) ==
                         "Input (entering this port)" &&
            typeCombo->itemText(1) == "Output (leaving this port)",
        "direction labels state semantics relative to selected port");
  check(table && table->verticalHeaderItem(TargetRow)->text() ==
                     "Target channel",
        "target row is named unambiguously");
  verifyDisplayedRemap(form, input, "input population");
  check(form->updateList.isEmpty() && !form->sendTimer->isActive(),
        "input population schedules no SetMIDIPortRemap");

  typeCombo->setCurrentIndex(1);
  verifyDisplayedRemap(form, output, "output population");
  check(form->updateList.isEmpty() && !form->sendTimer->isActive(),
        "output population schedules no SetMIDIPortRemap");

  typeCombo->setCurrentIndex(0);
  auto &inactive = device->midiPortRemap(1, RemapID::InputRemap);
  for (int source = 0; source < 16; ++source) {
    setFlags(inactive.remapStatus_at(source), 0x00);
  }
  form->updateChannelRemap();
  verifyDisplayedRemap(form, inactive, "all-inactive population");
  check(form->updateList.isEmpty() && !form->sendTimer->isActive(),
        "all-inactive population schedules no SetMIDIPortRemap");

  setFlags(inactive.remapStatus_at(0), 0x01);
  form->updateChannelRemap();
  verifyDisplayedRemap(form, inactive, "one-active population");

  for (int source = 0; source < 16; ++source) {
    setFlags(inactive.remapStatus_at(source), 0x3f);
  }
  form->updateChannelRemap();
  verifyDisplayedRemap(form, inactive, "all-active population");

  device->addCommandData(commandData_t(input));
  form->updateChannelRemap();
  auto &editable = device->midiPortRemap(1, RemapID::InputRemap);

  // Exhaustive model -> UI target conversion.
  for (int target = 0; target < 16; ++target) {
    editable.remapStatus_at(0).channelNumber = static_cast<Byte>(target);
    form->updateChannelRemap();
    check(targetEditor(table, 0)->text().toInt() == target + 1,
          QString("model target %1 displays MIDI channel %2")
              .arg(target).arg(target + 1));
    check(form->updateList.isEmpty() && !form->sendTimer->isActive(),
          QString("model target %1 population schedules no write").arg(target));
  }

  // Exhaustive UI -> model target conversion.
  for (int displayed = 1; displayed <= 16; ++displayed) {
    editable.remapStatus_at(0).channelNumber =
        static_cast<Byte>((displayed == 1) ? 1 : 0);
    targetEditor(table, 0)->setEnabled(true);
    targetEditor(table, 0)->setText(QString::number(displayed));
    clearPending(form);
    form->lineEditChanged(0);
    check(editable.remapStatus_at(0).channelNumber == displayed - 1,
          QString("UI target %1 stores model value %2")
              .arg(displayed).arg(displayed - 1));
    clearPending(form);
  }

  // Invalid and no-op edits must not mutate or schedule writes.
  editable.remapStatus_at(0).channelNumber = 7;
  form->updateChannelRemap();
  clearPending(form);
  form->lineEditChanged(0);
  check(editable.remapStatus_at(0).channelNumber == 7 &&
            form->updateList.isEmpty() && !form->sendTimer->isActive(),
        "unchanged target schedules no write");

  const QString invalidTargets[] = {"", "0", "17", "x"};
  for (const auto &invalid : invalidTargets) {
    editable.remapStatus_at(0).channelNumber = 7;
    targetEditor(table, 0)->setText(invalid);
    clearPending(form);
    form->lineEditChanged(0);
    check(editable.remapStatus_at(0).channelNumber == 7 &&
              form->updateList.isEmpty() && !form->sendTimer->isActive(),
          QString("invalid target '%1' is rejected without write")
              .arg(invalid));
  }

  // A single target edit changes only its source channel's target byte.
  form->updateChannelRemap();
  editable.remapStatus_at(0).channelNumber = 15;
  targetEditor(table, 0)->setEnabled(true);
  targetEditor(table, 0)->setText("1");
  const Bytes beforeTarget = editable.generate();
  clearPending(form);
  form->lineEditChanged(0);
  const Bytes afterTarget = editable.generate();
  check(changedByteIndexes(beforeTarget, afterTarget) == QList<int>({6}) &&
            editable.remapStatus_at(0).channelNumber == 0,
        "source channel 1 target edit changes only payload byte 6");
  clearPending(form);

  // Each UI event row changes only the matching bit in source channel 1 byte 5.
  const Byte eventMasks[6] = {0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
  for (int index = 0; index < 6; ++index) {
    const int row = index + 1;
    setFlags(editable.remapStatus_at(0), 0x00);
    form->updateChannelRemap();
    const Bytes before = editable.generate();
    clearPending(form);
    form->cellStateChange(row, 0, BlockState::Full);
    const Bytes after = editable.generate();
    check(changedByteIndexes(before, after) == QList<int>({5}) &&
              after.at(5) == eventMasks[index],
          QString("event row %1 changes only expected mask 0x%2")
              .arg(row).arg(eventMasks[index], 2, 16, QChar('0')));
    clearPending(form);
  }

  // Repeating the current event state is a no-op.
  setFlags(editable.remapStatus_at(1), 0x01);
  form->updateChannelRemap();
  clearPending(form);
  form->cellStateChange(NoteRow, 1, BlockState::Full);
  check(flagMask(editable.remapStatus_at(1)) == 0x01 &&
            form->updateList.isEmpty() && !form->sendTimer->isActive(),
        "unchanged event flag schedules no write");

  const Bytes beforeInvalid = editable.generate();
  form->lineEditChanged(-1);
  form->lineEditChanged(16);
  form->cellStateChange(TargetRow, 0, BlockState::Full);
  form->cellStateChange(NoteRow + 1, 0, BlockState::Full);
  form->cellStateChange(NoteRow, -1, BlockState::Full);
  form->cellStateChange(NoteRow, 16, BlockState::Full);
  check(editable.generate() == beforeInvalid && form->updateList.isEmpty() &&
            !form->sendTimer->isActive(),
        "invalid target/event coordinates are rejected without mutation");

  verifyRoundTrip(editable, "edited cached channel remap");

  check(failures == 0, "complete channel-remap offline UI/model suite");
  QTextStream(stdout) << "RESULT failures=" << failures << Qt::endl;

  // Deliberately avoid the legacy destructor's pending-edit flush. The test
  // never enters the event loop and no synthetic edit can reach sendSysex.
  std::_Exit(failures == 0 ? 0 : 1);
}
