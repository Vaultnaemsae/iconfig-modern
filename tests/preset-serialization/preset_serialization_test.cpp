#include <QApplication>
#include <QCheckBox>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#define private public
#include "DeviceInfo.h"
#include "Presets/ICSaveDialog.h"
#undef private
#include "ui_ICSaveDialog.h"

#include "CommandData.h"
#include "DevicePID.h"
#include "FilterID.h"
#include "Generator.h"
#include "MIDIPortFilter.h"
#include "MIDIPortRemap.h"
#include "MainWindow.h"
#include "RemapID.h"
#include "StreamHelpers.h"

#include <cstdlib>

using namespace GeneSysLib;

namespace {

int assertions = 0;
int failures = 0;

void check(bool condition, const QString &message) {
  ++assertions;
  QTextStream(stdout) << (condition ? "PASS " : "FAIL ") << message << Qt::endl;
  if (!condition) {
    ++failures;
  }
}

QByteArray byteArray(const Bytes &bytes) {
  return QByteArray(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}

QByteArray hex(const Bytes &bytes) {
  return byteArray(bytes).toHex();
}

MIDIPortFilter makeFilter(Word portID, FilterIDEnum filterID) {
  Bytes data;
  data += 0x01;
  appendMidiWord(data, portID);
  data += static_cast<Byte>(filterID);
  data += 0x08;
  data += 0x00;
  data += 0x7F;
  for (int channel = 0; channel < 16; ++channel) {
    data += static_cast<Byte>(channel == 0 ? 0x7F : channel & 0x3F);
  }
  for (int slot = 0; slot < 8; ++slot) {
    const bool full = slot == 0;
    data += full ? 0x0F : 0x00;
    data += full ? 0x0F : 0x00;
    data += full ? 0x0F : 0x00;
    data += full ? 0x0F : 0x00;
    data += static_cast<Byte>(slot == 0 ? 56 : (slot == 1 ? 0 : 127));
  }
  MIDIPortFilter result;
  auto begin = data.begin();
  auto end = data.end();
  result.parse(begin, end);
  return result;
}

MIDIPortRemap makeRemap(Word portID, RemapTypeEnum remapID) {
  MIDIPortRemap::RemapStatues statuses = {};
  for (int channel = 0; channel < 16; ++channel) {
    auto &status = statuses.at(channel);
    status.pitchBendEvents = (channel & 1) != 0;
    status.channelPressureEvents = (channel & 2) != 0;
    status.programChangeEvents = (channel & 4) != 0;
    status.controlChangeEvents = (channel & 8) != 0;
    status.polyKeyPressureEvents = channel == 0 || channel == 15;
    status.noteEvents = channel == 0 || channel == 15;
    status.channelNumber = static_cast<Byte>(15 - channel);
  }
  statuses.at(0).pitchBendEvents = true;
  statuses.at(0).channelPressureEvents = true;
  statuses.at(0).programChangeEvents = true;
  statuses.at(0).controlChangeEvents = true;
  statuses.at(0).polyKeyPressureEvents = true;
  statuses.at(0).noteEvents = true;
  statuses.at(15).pitchBendEvents = false;
  statuses.at(15).channelPressureEvents = false;
  statuses.at(15).programChangeEvents = false;
  statuses.at(15).controlChangeEvents = false;
  statuses.at(15).polyKeyPressureEvents = false;
  statuses.at(15).noteEvents = false;

  MIDIPortRemap::RemapFlagsVector controllers;
  for (int slot = 0; slot < 8; ++slot) {
    MIDIPortRemap::RemapFlags flags = {};
    if (slot == 0) {
      flags.channelBitmap = MIDIPortRemap::ChannelBitmap(0x0F0F0F0FUL);
      flags.controllerSource = 0;
      flags.controllerDestination = 127;
    } else if (slot == 1) {
      flags.channelBitmap.reset();
      flags.controllerSource = 127;
      flags.controllerDestination = 0;
    } else {
      flags.channelBitmap = MIDIPortRemap::ChannelBitmap(1UL << (slot - 2));
      flags.controllerSource = static_cast<Byte>(slot);
      flags.controllerDestination = static_cast<Byte>(127 - slot);
    }
    controllers.push_back(flags);
  }
  return MIDIPortRemap(portID, remapID, 8, statuses, controllers);
}

bool validEnvelope(const Bytes &preset, Byte expectedPID, Byte expectedVersion,
                   const QString &description) {
  if (preset.size() < 21 || preset[0] != 'i' || preset[1] != 'C' ||
      preset[2] != 'M' || preset[3] != expectedPID ||
      preset[4] != expectedVersion) {
    return false;
  }
  size_t offset = 5;
  if (expectedVersion == 2) {
    const QByteArray expectedDescription = description.toLatin1().left(239);
    if (preset[5] != expectedDescription.size()) {
      return false;
    }
    offset = 6 + preset[5];
    if (QByteArray(reinterpret_cast<const char *>(preset.data() + 6), preset[5]) !=
        expectedDescription) {
      return false;
    }
  }
  if (offset > preset.size() - 16) {
    return false;
  }
  const QByteArray contents(reinterpret_cast<const char *>(preset.data()),
                            preset.size() - 16);
  const QByteArray expected(reinterpret_cast<const char *>(preset.data() + preset.size() - 16), 16);
  return QCryptographicHash::hash(contents, QCryptographicHash::Md5) == expected;
}

void verifySelection(DeviceInfoPtr device) {
  ICSaveDialog dialog(device);
  check(dialog.getPreRebootCommands().empty(),
        "all-unchecked selection has no pre-reboot commands");
  check(dialog.getPostRebootCommands().empty(),
        "all-unchecked selection has no post-reboot commands");

  dialog.ui->checkMidiRemap->blockSignals(true);
  dialog.ui->checkMidiRemap->setChecked(true);
  dialog.ui->checkMidiRemap->blockSignals(false);
  const auto post = dialog.getPostRebootCommands();
  check(post.size() == 1 && post.count(Command::RetMIDIPortRemap) == 1,
        "MIDI Remap checkbox selects only MIDIPortRemap");
  check(dialog.getPreRebootCommands().empty(),
        "MIDI Remap selection does not add reboot-stage data");

  dialog.ui->checkMidiRemap->setChecked(false);
  dialog.ui->checkAudioInfo->blockSignals(true);
  dialog.ui->checkAudioInfo->setChecked(true);
  dialog.ui->checkAudioInfo->blockSignals(false);
  const auto pre = dialog.getPreRebootCommands();
  check(pre.size() == 11 && pre.count(Command::RetAudioInfo) == 1 &&
            pre.count(Command::RetMixerPortParm) == 1,
        "Audio Info selects the documented 11 pre-reboot command types");
}

void verifyRoundTrip() {
  CommPtr sourceComm(new Communicator());
  DeviceInfoPtr source(new DeviceInfo(sourceComm, DeviceID(DevicePID::iConnect4Audio), 7));
  source->addCommandData(commandData_t(makeFilter(5, FilterID::InputFilter)));
  source->addCommandData(commandData_t(makeFilter(23, FilterID::OutputFilter)));
  source->addCommandData(commandData_t(makeRemap(5, RemapID::InputRemap)));
  source->addCommandData(commandData_t(makeRemap(23, RemapID::OutputRemap)));

  const std::set<Command::Enum> commands = {
      Command::RetMIDIPortFilter, Command::RetMIDIPortRemap};
  const QString description = QString::fromLatin1("Boundary \xE9 description");
  Bytes serialized = source->serialize2(commands, description);
  check(validEnvelope(serialized, DevicePID::iConnect4Audio, 2, description),
        "version-2 envelope, Latin-1 description, and MD5 are valid");

  CommPtr parsedComm(new Communicator());
  DeviceInfoPtr parsed(new DeviceInfo(parsedComm, DeviceID(DevicePID::iConnect4Audio), 7));
  auto begin = serialized.begin() + 6 + serialized[5];
  auto end = serialized.end() - 16;
  parsedComm->parseBytes(begin, end, parsed->getDeviceID());
  const Bytes reserialized = parsed->serialize2(commands, description);
  check(serialized == reserialized,
        "model to preset to parser cache to preset is byte-exact");

  Bytes serializedV1 = source->serialize();
  check(validEnvelope(serializedV1, DevicePID::iConnect4Audio, 1, QString()),
        "legacy version-1 envelope and MD5 are valid");
  CommPtr parsedV1Comm(new Communicator());
  DeviceInfoPtr parsedV1(
      new DeviceInfo(parsedV1Comm, DeviceID(DevicePID::iConnect4Audio), 7));
  auto v1Begin = serializedV1.begin() + 5;
  auto v1End = serializedV1.end() - 16;
  parsedV1Comm->parseBytes(v1Begin, v1End, parsedV1->getDeviceID());
  check(parsedV1->serialize() == serializedV1,
        "legacy version-1 model parse and reserialization are byte-exact");
  check(parsed->midiPortRemap(5, RemapID::InputRemap).remapStatus_at(0).channelNumber == 15,
        "target MIDI channel 16 survives as model value 15");
  check(parsed->midiPortRemap(5, RemapID::InputRemap).remapStatus_at(0).generate()[0] == 0x3F,
        "all six channel-remap event flags survive");
  check(parsed->midiPortRemap(5, RemapID::InputRemap).remapStatus_at(15).generate()[0] == 0,
        "no channel-remap event flags survives");
  const auto &controller = parsed->midiPortRemap(5, RemapID::InputRemap).controller_at(0);
  check(controller.controllerSource == 0 && controller.controllerDestination == 127,
        "controller remap CC0 to CC127 survives");
  check(controller.channelBitmap.to_ulong() == 0x0F0F0F0FUL,
        "controller-remap full 16-channel bitmap survives");
  const auto &filter = parsed->midiPortFilter(5, FilterID::InputFilter);
  check(filter.controllerFilter_at(0).controllerID == 56 &&
            filter.controllerFilter_at(0).channelBitmap.to_ulong() == 0x0F0F0F0FUL,
        "controller filter CC56 and full bitmap survive");
  check(filter.controllerFilter_at(1).channelBitmap.none(),
        "zero controller-filter bitmap survives");
  check(parsed->midiPortFilter(23, FilterID::OutputFilter).portID() == 23 &&
            parsed->midiPortRemap(23, RemapID::OutputRemap).portID() == 23,
        "multiple ports and both Input/Output variants survive");

  DeviceInfoPtr invalidTarget(new DeviceInfo(CommPtr(new Communicator()),
                                             DeviceID(DevicePID::iConnect4Audio), 7));
  Bytes truncated = {0x69, 0x43, 0x4D};
  check(!invalidTarget->deserialize(truncated), "truncated preset is rejected safely");
  Bytes wrongPID = serialized;
  wrongPID[3] = DevicePID::iConnect2Audio;
  check(!invalidTarget->deserialize(wrongPID), "wrong-device preset is rejected");
  Bytes badHash = serialized;
  badHash.back() ^= 0x01;
  check(!invalidTarget->deserialize(badHash), "MD5 mismatch is rejected");
  Bytes badVersion = serialized;
  badVersion[4] = 3;
  check(!invalidTarget->deserialize(badVersion), "unsupported preset version is rejected");
  Bytes badDescription = serialized;
  badDescription[5] = 0xEF;
  check(!invalidTarget->deserialize(badDescription), "overrunning description length is rejected");
  Bytes junk = serialized;
  junk.insert(junk.end() - 16, 0x01);
  const QByteArray junkBody(reinterpret_cast<const char *>(junk.data()), junk.size() - 16);
  const QByteArray junkHash = QCryptographicHash::hash(junkBody, QCryptographicHash::Md5);
  std::copy(junkHash.begin(), junkHash.end(), junk.end() - 16);
  check(!invalidTarget->deserialize(junk), "authenticated trailing junk is rejected");
}

void writeDryRun(const QString &rawPath, const QString &presetPath,
                 const QString &outputPath) {
  CommPtr comm(new Communicator());
  DeviceInfoPtr device(new DeviceInfo(comm, DeviceID(DevicePID::iConnect4Audio), 7));

  QFile raw(rawPath);
  check(raw.open(QFile::ReadOnly), "raw CoreMIDI baseline opens");
  while (!raw.atEnd()) {
    const auto document = QJsonDocument::fromJson(raw.readLine());
    const auto array = document.object().value("bytes").toArray();
    if (array.isEmpty()) {
      continue;
    }
    Bytes frame;
    for (const auto &value : array) {
      frame.push_back(static_cast<Byte>(value.toInt()));
    }
    comm->m_parser->parse(frame);
  }

  QFile preset(presetPath);
  check(preset.open(QFile::ReadOnly), "disposable preset opens for dry-run parse");
  const QByteArray presetData = preset.readAll();
  Bytes bytes(presetData.begin(), presetData.end());
  auto begin = bytes.begin() + 6 + bytes[5];
  auto end = bytes.end() - 16;
  comm->parseBytes(begin, end, device->getDeviceID());

  QJsonArray plan;
  int order = 0;
  for (const auto &entry : device->storedCommandData) {
    const CmdEnum ret = keyToCommand(entry.first);
    const CmdEnum setter = static_cast<CmdEnum>(WRITE_BIT | keyToCommandID(entry.first));
    const Bytes frame = GeneSysLib::generate(device->deviceID, device->transID,
                                             setter, entry.second);
    QJsonObject item;
    item["order"] = ++order;
    item["keyHex"] = QString::fromLatin1(hex(entry.first));
    item["returnCommandID"] = static_cast<int>(ret);
    item["setterCommandID"] = static_cast<int>(setter);
    item["payloadHex"] = QString::fromLatin1(hex(entry.second.generate()));
    item["frameHex"] = QString::fromLatin1(hex(frame));
    plan.append(item);
  }
  QJsonObject root;
  root["sourceRawCapture"] = rawPath;
  root["sourcePreset"] = presetPath;
  root["wouldAttemptCount"] = plan.size();
  root["orderedWrites"] = plan;
  QFile output(outputPath);
  check(output.open(QFile::WriteOnly | QFile::Truncate), "restore dry-run output opens");
  output.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  output.close();
  check(plan.size() > 381,
        "current restore would attempt cached objects beyond preset contents");
}

}  // namespace

QString MainWindow::extensionForPID(Word pid) {
  return pid == DevicePID::iConnect4Audio ? ".ica4" : ".ic_unk";
}

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  CommPtr comm(new Communicator());
  DeviceInfoPtr device(new DeviceInfo(comm, DeviceID(DevicePID::iConnect4Audio), 7));
  verifySelection(device);
  verifyRoundTrip();
  if (argc == 4) {
    writeDryRun(QString::fromLocal8Bit(argv[1]), QString::fromLocal8Bit(argv[2]),
                QString::fromLocal8Bit(argv[3]));
  }
  QTextStream(stdout) << "RESULT assertions=" << assertions
                      << " failures=" << failures << Qt::endl;
  std::_Exit(failures == 0 ? 0 : 1);
}
