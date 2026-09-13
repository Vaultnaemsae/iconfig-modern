#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QTableWidget>
#include <QTextStream>

#define private public
#include "MIDIRelated/MIDIControllerRemapForm.h"
#undef private

#include "CCList.h"
#include "CommandData.h"
#include "MIDIInfo.h"
#include "MIDIPortInfo.h"
#include "MIDIPortRemap.h"
#include "PortType.h"

#include <cstdlib>

using namespace GeneSysLib;

namespace {

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

MIDIPortRemap makeRemap(RemapTypeEnum type, int variant) {
  MIDIPortRemap::RemapStatues statuses = {};
  for (int channel = 0; channel < 16; ++channel) {
    statuses[channel].channelNumber = static_cast<Byte>(channel);
  }

  const int sources[8] = {2, 11, 64, 0, 127, 56, 7, 32};
  const int destinations[8] = {74, 7, 1, 127, 0, 64, 11, 56};
  const MIDIPortRemap::ChannelBitmapBits::Enum bitA[8] = {
      MIDIPortRemap::ChannelBitmapBits::channel1,
      MIDIPortRemap::ChannelBitmapBits::channel2,
      MIDIPortRemap::ChannelBitmapBits::channel3,
      MIDIPortRemap::ChannelBitmapBits::channel1,
      MIDIPortRemap::ChannelBitmapBits::channel1,
      MIDIPortRemap::ChannelBitmapBits::channel8,
      MIDIPortRemap::ChannelBitmapBits::channel5,
      MIDIPortRemap::ChannelBitmapBits::channel15};

  MIDIPortRemap::RemapFlagsVector controllers;
  for (int slot = 0; slot < 8; ++slot) {
    MIDIPortRemap::RemapFlags flags = {};
    flags.controllerSource = static_cast<Byte>((sources[slot] + variant) & 0x7f);
    flags.controllerDestination =
        static_cast<Byte>((destinations[slot] + variant) & 0x7f);
    if (slot != 3) {
      flags.channelBitmap.set(bitA[slot]);
    }
    if (slot == 0) {
      flags.channelBitmap.set(MIDIPortRemap::ChannelBitmapBits::channel16);
    } else if (slot == 2) {
      flags.channelBitmap.set(MIDIPortRemap::ChannelBitmapBits::channel4);
    } else if (slot == 4) {
      const MIDIPortRemap::ChannelBitmapBits::Enum allBits[16] = {
          MIDIPortRemap::ChannelBitmapBits::channel1,
          MIDIPortRemap::ChannelBitmapBits::channel2,
          MIDIPortRemap::ChannelBitmapBits::channel3,
          MIDIPortRemap::ChannelBitmapBits::channel4,
          MIDIPortRemap::ChannelBitmapBits::channel5,
          MIDIPortRemap::ChannelBitmapBits::channel6,
          MIDIPortRemap::ChannelBitmapBits::channel7,
          MIDIPortRemap::ChannelBitmapBits::channel8,
          MIDIPortRemap::ChannelBitmapBits::channel9,
          MIDIPortRemap::ChannelBitmapBits::channel10,
          MIDIPortRemap::ChannelBitmapBits::channel11,
          MIDIPortRemap::ChannelBitmapBits::channel12,
          MIDIPortRemap::ChannelBitmapBits::channel13,
          MIDIPortRemap::ChannelBitmapBits::channel14,
          MIDIPortRemap::ChannelBitmapBits::channel15,
          MIDIPortRemap::ChannelBitmapBits::channel16};
      for (auto bit : allBits) {
        flags.channelBitmap.set(bit);
      }
    } else if (slot == 5) {
      flags.channelBitmap.set(MIDIPortRemap::ChannelBitmapBits::channel9);
    }
    controllers.push_back(flags);
  }

  return MIDIPortRemap(1, type, 8, statuses, controllers);
}

QComboBox *cellCombo(QTableWidget *table, int row, int column) {
  return qobject_cast<QComboBox *>(table->cellWidget(row, column));
}

bool cellEnabled(QTableWidget *table, int row, int column) {
  auto *label = qobject_cast<QLabel *>(table->cellWidget(row, column));
  return label && label->property(kBlockState).toInt() == BlockState::Full;
}

void verifyDisplayedRemap(MIDIControllerRemapForm *form,
                          const MIDIPortRemap &expected,
                          const QString &name) {
  auto *table = form->findChild<QTableWidget *>("tableWidget");
  check(table != nullptr, name + ": table exists");
  if (!table) {
    return;
  }
  check(table->rowCount() == 8, name + ": eight controller rows");
  check(table->columnCount() == 18, name + ": sixteen channels plus source/destination");
  for (int row = 0; row < 8; ++row) {
    const auto &entry = expected.controller_at(row);
    check(cellCombo(table, row, 16)->currentIndex() == entry.controllerSource,
          QString("%1: row %2 source CC%3").arg(name).arg(row + 1).arg(entry.controllerSource));
    check(cellCombo(table, row, 17)->currentIndex() == entry.controllerDestination,
          QString("%1: row %2 destination CC%3").arg(name).arg(row + 1).arg(entry.controllerDestination));
    check(cellCombo(table, row, 16)->count() == 128 &&
              cellCombo(table, row, 17)->count() == 128,
          QString("%1: row %2 exposes complete CC0-127 range").arg(name).arg(row + 1));
    check(cellCombo(table, row, 16)->itemText(entry.controllerSource) ==
              CCList().at(entry.controllerSource) &&
              cellCombo(table, row, 17)->itemText(entry.controllerDestination) ==
                  CCList().at(entry.controllerDestination),
          QString("%1: row %2 source/destination names use the same indexed list")
              .arg(name)
              .arg(row + 1));
    for (int channel = 0; channel < 16; ++channel) {
      const auto bit = form->colChBitmapMap.at(channel);
      check(cellEnabled(table, row, channel) == entry.channelBitmap.test(bit),
            QString("%1: row %2 channel %3").arg(name).arg(row + 1).arg(channel + 1));
    }
  }
}

void verifyBitmapBytes() {
  struct Case {
    QString name;
    QList<MIDIPortRemap::ChannelBitmapBits::Enum> bits;
    Bytes expected;
  };
  const QList<Case> cases = {
      {"none", {}, {0x00, 0x00, 0x00, 0x00, 0x02, 0x4a}},
      {"channel 1", {MIDIPortRemap::ChannelBitmapBits::channel1}, {0x00, 0x00, 0x00, 0x01, 0x02, 0x4a}},
      {"channel 2", {MIDIPortRemap::ChannelBitmapBits::channel2}, {0x00, 0x00, 0x00, 0x02, 0x02, 0x4a}},
      {"channel 16", {MIDIPortRemap::ChannelBitmapBits::channel16}, {0x08, 0x00, 0x00, 0x00, 0x02, 0x4a}},
      {"channels 1+16", {MIDIPortRemap::ChannelBitmapBits::channel1,
                          MIDIPortRemap::ChannelBitmapBits::channel16},
                         {0x08, 0x00, 0x00, 0x01, 0x02, 0x4a}},
      {"channels 1+2", {MIDIPortRemap::ChannelBitmapBits::channel1,
                         MIDIPortRemap::ChannelBitmapBits::channel2},
                        {0x00, 0x00, 0x00, 0x03, 0x02, 0x4a}},
      {"all channels", {MIDIPortRemap::ChannelBitmapBits::channel1,
                         MIDIPortRemap::ChannelBitmapBits::channel2,
                         MIDIPortRemap::ChannelBitmapBits::channel3,
                         MIDIPortRemap::ChannelBitmapBits::channel4,
                         MIDIPortRemap::ChannelBitmapBits::channel5,
                         MIDIPortRemap::ChannelBitmapBits::channel6,
                         MIDIPortRemap::ChannelBitmapBits::channel7,
                         MIDIPortRemap::ChannelBitmapBits::channel8,
                         MIDIPortRemap::ChannelBitmapBits::channel9,
                         MIDIPortRemap::ChannelBitmapBits::channel10,
                         MIDIPortRemap::ChannelBitmapBits::channel11,
                         MIDIPortRemap::ChannelBitmapBits::channel12,
                         MIDIPortRemap::ChannelBitmapBits::channel13,
                         MIDIPortRemap::ChannelBitmapBits::channel14,
                         MIDIPortRemap::ChannelBitmapBits::channel15,
                         MIDIPortRemap::ChannelBitmapBits::channel16},
                        {0x0f, 0x0f, 0x0f, 0x0f, 0x02, 0x4a}}};

  for (const auto &test : cases) {
    MIDIPortRemap::RemapFlags flags = {};
    flags.controllerSource = 2;
    flags.controllerDestination = 74;
    for (auto bit : test.bits) {
      flags.channelBitmap.set(bit);
    }
    check(flags.generate() == test.expected,
          "bitmap bytes " + test.name);
  }
}

void verifyRoundTrip(const MIDIPortRemap &original, const QString &name) {
  Bytes generated = original.generate();
  MIDIPortRemap parsed;
  auto begin = generated.begin();
  auto end = generated.end();
  parsed.parse(begin, end);

  check(begin == end, name + ": parser consumes the complete payload");
  check(parsed.generate() == generated,
        name + ": parse/generate round trip is byte-exact");
}

}  // namespace

int main(int argc, char **argv) {
  QApplication app(argc, argv);

  const auto &cc = CCList();
  check(cc.size() == 128, "CC list contains 128 entries");
  for (int index = 0; index < cc.size(); ++index) {
    check(cc.at(index).startsWith(QString::number(index) + " - "),
          QString("CC list index %1 has matching numeric prefix").arg(index));
  }
  const QList<int> representativeCCs = {0, 1, 2, 7, 11, 32, 56, 64, 74, 127};
  for (const auto ccNumber : representativeCCs) {
    check(cc.at(ccNumber).startsWith(QString::number(ccNumber) + " - "),
          QString("representative CC%1 name/value alignment").arg(ccNumber));
  }
  check(cc.at(66) == "66 - Sostenuto Pedal on/off", "CC66 name corrected");
  check(cc.at(80) == "80 - General Purpose Controller 5", "CC80 name corrected");
  check(cc.at(84) == "84 - Portamento Control", "CC84 name corrected");
  check(cc.at(88) == "88 - High Resolution Velocity Prefix", "CC88 name corrected");
  check(cc.at(98).endsWith("NRPN) LSB") && cc.at(99).endsWith("NRPN) MSB"),
        "NRPN LSB/MSB names aligned with CC98/99");
  check(cc.at(100).endsWith("RPN) LSB") && cc.at(101).endsWith("RPN) MSB"),
        "RPN LSB/MSB names aligned with CC100/101");
  check(cc.at(121) == "121 - Reset All Controllers", "CC121 name corrected");
  verifyBitmapBytes();

  CommPtr comm(new Communicator());
  DeviceInfoPtr device(new DeviceInfo(comm));
  device->addCommandData(commandData_t(MIDIInfo(1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0)));
  device->addCommandData(commandData_t(makePortInfo()));
  const auto input = makeRemap(RemapID::InputRemap, 0);
  const auto output = makeRemap(RemapID::OutputRemap, 3);
  verifyRoundTrip(input, "input remap");
  verifyRoundTrip(output, "output remap");
  device->addCommandData(commandData_t(input));
  device->addCommandData(commandData_t(output));

  auto *form = new MIDIControllerRemapForm(device);
  auto *typeCombo = form->findChild<QComboBox *>("remapTypeComboBox");
  auto *table = form->findChild<QTableWidget *>("tableWidget");
  check(typeCombo && typeCombo->currentIndex() == 0,
        "Input Remap is selector index 0");
  check(typeCombo && typeCombo->itemText(0) == "Input (entering this port)" &&
            typeCombo->itemText(1) == "Output (leaving this port)",
        "direction labels state semantics relative to selected port");
  verifyDisplayedRemap(form, input, "input population");
  check(table->verticalHeaderItem(0)->text() == "Mapping 1",
        "active slot is labeled as a mapping");
  check(table->verticalHeaderItem(3)->text() == "Mapping 4 (unused)",
        "zero-channel slot is explicitly labeled unused");
  check(cellCombo(table, 3, 16)->currentIndex() == 0,
        "unused slot still faithfully represents valid source CC0");
  check(form->updateList.isEmpty() && !form->sendTimer->isActive(),
        "input population schedules no SetMIDIPortRemap");

  typeCombo->setCurrentIndex(1);
  verifyDisplayedRemap(form, output, "output population");
  check(form->updateList.isEmpty() && !form->sendTimer->isActive(),
        "output population schedules no SetMIDIPortRemap");

  typeCombo->setCurrentIndex(0);
  auto &editable = device->midiPortRemap(1, RemapID::InputRemap);

  const Bytes beforeSource = editable.generate();
  cellCombo(table, 0, 16)->setCurrentIndex(11);
  const Bytes afterSource = editable.generate();
  auto changedIndexes = changedByteIndexes(beforeSource, afterSource);
  check(changedIndexes == QList<int>({41}) &&
            editable.controller_at(0).controllerSource == 11,
        "editing row 1 source changes only slot 0 source byte");
  form->updateList.clear();
  form->sendTimer->stop();

  const Bytes beforeDestination = editable.generate();
  cellCombo(table, 0, 17)->setCurrentIndex(71);
  const Bytes afterDestination = editable.generate();
  changedIndexes = changedByteIndexes(beforeDestination, afterDestination);
  check(changedIndexes == QList<int>({42}) &&
            editable.controller_at(0).controllerDestination == 71,
        "editing row 1 destination changes only slot 0 destination byte");
  form->updateList.clear();
  form->sendTimer->stop();

  const auto channel1 = MIDIPortRemap::ChannelBitmapBits::channel1;
  const auto channel16 = MIDIPortRemap::ChannelBitmapBits::channel16;
  const Bytes beforeCh1 = editable.generate();
  form->cellStateChange(0, 0, BlockState::Empty);
  const Bytes afterCh1 = editable.generate();
  changedIndexes = changedByteIndexes(beforeCh1, afterCh1);
  check(changedIndexes == QList<int>({40}) &&
            !editable.controller_at(0).channelBitmap.test(channel1),
        "editing row 1 channel 1 changes only slot 0 bitmap byte 4");
  form->updateList.clear();
  form->sendTimer->stop();

  const Bytes beforeCh16 = editable.generate();
  form->cellStateChange(0, 15, BlockState::Empty);
  const Bytes afterCh16 = editable.generate();
  changedIndexes = changedByteIndexes(beforeCh16, afterCh16);
  check(changedIndexes == QList<int>({37}) &&
            !editable.controller_at(0).channelBitmap.test(channel16),
        "editing row 1 channel 16 changes only slot 0 bitmap byte 1");
  form->updateList.clear();
  form->sendTimer->stop();

  const Bytes beforeSlot2 = editable.generate();
  cellCombo(table, 1, 16)->setCurrentIndex(64);
  const Bytes afterSlot2 = editable.generate();
  changedIndexes = changedByteIndexes(beforeSlot2, afterSlot2);
  check(changedIndexes == QList<int>({47}) &&
            editable.controller_at(1).controllerSource == 64,
        "editing row 2 source changes only slot 1 source byte");
  form->updateList.clear();
  form->sendTimer->stop();

  check(failures == 0, "complete controller-remap offline UI/model suite");
  QTextStream(stdout) << "RESULT failures=" << failures << Qt::endl;

  // The form is deliberately not destroyed: its legacy destructor flushes pending
  // edits. This test process never enters the event loop and exits immediately, so
  // no synthetic edit can reach Communicator::sendSysex.
  std::_Exit(failures == 0 ? 0 : 1);
}
