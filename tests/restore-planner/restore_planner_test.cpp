#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QTextStream>

#define private public
#include "DeviceInfo.h"
#undef private

#include "ACK.h"
#include "Device.h"
#include "DevicePID.h"
#include "ErrorCode.h"
#include "FilterID.h"
#include "MIDIPortFilter.h"
#include "MIDIPortRemap.h"
#include "RemapID.h"
#include "StreamHelpers.h"

#include <algorithm>
#include <set>

using namespace GeneSysLib;

namespace {

int assertions = 0;
int failures = 0;

void check(bool condition, const QString &message) {
  ++assertions;
  QTextStream(stdout) << (condition ? "PASS " : "FAIL ") << message
                      << Qt::endl;
  if (!condition) {
    ++failures;
  }
}

Bytes readFile(const QString &path) {
  QFile file(path);
  if (!file.open(QFile::ReadOnly)) {
    return Bytes();
  }
  const QByteArray data = file.readAll();
  return Bytes(data.begin(), data.end());
}

void replaceEnvelopeHash(Bytes &preset) {
  QCryptographicHash hash(QCryptographicHash::Md5);
  hash.addData(reinterpret_cast<const char *>(preset.data()),
               preset.size() - 16);
  const QByteArray digest = hash.result();
  std::copy(digest.begin(), digest.end(), preset.end() - 16);
}

size_t payloadOffset(const Bytes &preset) {
  return preset[4] == 2 ? 6 + preset[5] : 5;
}

size_t firstFrameEnd(const Bytes &preset) {
  return std::distance(
      preset.begin(),
      std::find(preset.begin() + payloadOffset(preset), preset.end() - 16,
                static_cast<Byte>(0xF7)));
}

void repairFirstFrameChecksum(Bytes &preset) {
  const size_t start = payloadOffset(preset);
  const size_t end = firstFrameEnd(preset);
  preset[end - 1] = static_cast<Byte>(
      (~std::accumulate(preset.begin() + start + 5,
                        preset.begin() + end - 1, 0x00) + 1) & 0x7F);
  replaceEnvelopeHash(preset);
}

commandData_t makeACK(CmdEnum command, ErrorCodeEnum error) {
  Bytes payload;
  appendMidiWord(payload, static_cast<Word>(command));
  payload += static_cast<Byte>(error);
  ACK ack;
  auto begin = payload.begin();
  auto end = payload.end();
  ack.parse(begin, end);
  return commandData_t(ack);
}

DeviceInfo::RestorePlan prefixPlan(const DeviceInfo::RestorePlan &source,
                                   size_t count) {
  DeviceInfo::RestorePlan result = source;
  result.entries.resize(std::min(count, result.entries.size()));
  return result;
}

void acknowledge(DeviceInfo &device, CmdEnum command, ErrorCodeEnum error,
                 Word transIDOffset = 0, bool wrongDevice = false) {
  DeviceID id = device.getDeviceID();
  if (wrongDevice) {
    SerialNumber serial = id.serialNumber();
    serial[4] = static_cast<Byte>((serial[4] + 1) & 0x7F);
    id = DeviceID(id.pid(), serial);
  }
  device.handleACKData(Command::ACK, id,
                       static_cast<Word>(device.getTransID() + transIDOffset),
                       makeACK(command, error));
}

MIDIPortRemap makeRemap(Word portID, RemapTypeEnum remapID) {
  MIDIPortRemap::RemapStatues statuses = {};
  MIDIPortRemap::RemapFlagsVector controllers;
  for (int slot = 0; slot < 8; ++slot) {
    MIDIPortRemap::RemapFlags flags = {};
    flags.channelBitmap = MIDIPortRemap::ChannelBitmap(slot == 0 ? 1 : 0);
    flags.controllerSource = static_cast<Byte>(slot == 0 ? 2 : 0);
    flags.controllerDestination = static_cast<Byte>(slot == 0 ? 74 : 0);
    controllers.push_back(flags);
  }
  return MIDIPortRemap(portID, remapID, 8, statuses, controllers);
}

MIDIPortFilter makeFilter(Word portID, FilterIDEnum filterID) {
  Bytes data;
  data += 0x01;
  appendMidiWord(data, portID);
  data += static_cast<Byte>(filterID);
  data += 0x08, 0x00, 0x00;
  for (int channel = 0; channel < 16; ++channel) {
    data += 0x00;
  }
  for (int slot = 0; slot < 8; ++slot) {
    data += 0x00, 0x00, 0x00, 0x00, static_cast<Byte>(slot);
  }
  MIDIPortFilter filter;
  auto begin = data.begin();
  auto end = data.end();
  filter.parse(begin, end);
  return filter;
}

void verifyKnownPresets(DeviceInfo &device, const QString &primaryPath,
                        const QString &auxiliaryPath) {
  const Bytes before = device.serialize();
  const auto primary = device.buildRestorePlan(readFile(primaryPath), "primary");
  const auto auxiliary =
      device.buildRestorePlan(readFile(auxiliaryPath), "auxiliary");
  const Bytes after = device.serialize();

  check(primary.valid, "known primary preset validates");
  check(primary.validatedRecordCount == 17 && primary.entries.size() == 17 &&
            primary.rejectedRecords.empty(),
        "primary plan is exactly 17 validated preset records and 17 writes");
  check(auxiliary.valid, "known auxiliary preset validates");
  check(auxiliary.validatedRecordCount == 364 &&
            auxiliary.entries.size() == 364 &&
            auxiliary.rejectedRecords.empty(),
        "auxiliary plan is exactly 364 records and 364 writes, not old 400");
  check(before == after,
        "building primary and auxiliary plans does not mutate live cache");

  bool directTrace = true;
  bool noMeters = true;
  bool noUndefined = true;
  for (const auto &entry : auxiliary.entries) {
    directTrace = directTrace && entry.sourceStage == "auxiliary" &&
                  !entry.key.empty() && entry.returnCommand == keyToCommand(entry.key);
    noMeters = noMeters && entry.returnCommand != Command::RetMixerMeterValue &&
               entry.returnCommand != Command::RetAudioPortMeterValue;
    noUndefined = noUndefined && entry.setterCommand != Command::Unknown &&
                  (static_cast<Word>(entry.setterCommand) & WRITE_BIT) == WRITE_BIT;
  }
  check(directTrace, "every auxiliary write traces to its parsed preset record");
  check(noMeters, "auxiliary plan contains no transient meter writes");
  check(noUndefined, "auxiliary plan contains no undefined WRITE_BIT command");
}

void verifyNarrowPresets(DeviceInfo &device) {
  DeviceInfo source(CommPtr(new Communicator()),
                    DeviceID(DevicePID::iConnect4Audio), 7);
  source.addCommandData(commandData_t(makeRemap(5, RemapID::InputRemap)));
  source.addCommandData(commandData_t(makeRemap(5, RemapID::OutputRemap)));
  Bytes remapPreset = source.serialize2({Command::RetMIDIPortRemap}, "MIDI only");
  auto remapPlan = device.buildRestorePlan(remapPreset, "auxiliary");
  check(remapPlan.valid && remapPlan.entries.size() == 2,
        "narrow remap preset creates exactly two writes");
  check(std::all_of(remapPlan.entries.begin(), remapPlan.entries.end(),
                    [](const DeviceInfo::RestorePlanEntry &entry) {
                      return entry.setterCommand == Command::SetMIDIPortRemap;
                    }),
        "narrow remap preset contains no unrelated setter");

  DeviceInfo filterSource(CommPtr(new Communicator()),
                          DeviceID(DevicePID::iConnect4Audio), 7);
  filterSource.addCommandData(
      commandData_t(makeFilter(5, FilterID::InputFilter)));
  filterSource.addCommandData(
      commandData_t(makeFilter(5, FilterID::OutputFilter)));
  Bytes filterPreset =
      filterSource.serialize2({Command::RetMIDIPortFilter}, "Filter only");
  auto filterPlan = device.buildRestorePlan(filterPreset, "auxiliary");
  check(filterPlan.valid && filterPlan.entries.size() == 2,
        "narrow filter preset creates exactly two writes");
  check(std::all_of(filterPlan.entries.begin(), filterPlan.entries.end(),
                    [](const DeviceInfo::RestorePlanEntry &entry) {
                      return entry.setterCommand == Command::SetMIDIPortFilter;
                    }),
        "narrow filter preset contains no unrelated setter");
}

void verifyMalformed(DeviceInfo &device, const Bytes &validPreset) {
  Bytes invalidMD5 = validPreset;
  invalidMD5.back() ^= 1;
  check(!device.buildRestorePlan(invalidMD5, "primary").valid,
        "invalid MD5 fails closed");

  Bytes wrongPID = validPreset;
  wrongPID[3] ^= 1;
  replaceEnvelopeHash(wrongPID);
  check(!device.buildRestorePlan(wrongPID, "primary").valid,
        "wrong PID fails closed");

  Bytes badVersion = validPreset;
  badVersion[4] = 3;
  replaceEnvelopeHash(badVersion);
  check(!device.buildRestorePlan(badVersion, "primary").valid,
        "unsupported version fails closed");

  Bytes truncated = validPreset;
  const size_t removeAt = firstFrameEnd(truncated) - 2;
  truncated.erase(truncated.begin() + removeAt);
  replaceEnvelopeHash(truncated);
  check(!device.buildRestorePlan(truncated, "primary").valid,
        "truncated frame fails closed");

  Bytes badChecksum = validPreset;
  badChecksum[payloadOffset(badChecksum) + 18] ^= 1;
  replaceEnvelopeHash(badChecksum);
  check(!device.buildRestorePlan(badChecksum, "primary").valid,
        "invalid frame checksum fails closed");

  Bytes unknown = validPreset;
  const size_t first = payloadOffset(unknown);
  unknown[first + 14] = 0;
  unknown[first + 15] = 0x62;
  repairFirstFrameChecksum(unknown);
  check(!device.buildRestorePlan(unknown, "primary").valid,
        "unknown return command fails closed");

  DeviceInfo queryOnlySource(CommPtr(new Communicator()),
                             DeviceID(DevicePID::iConnect4Audio), 7);
  queryOnlySource.addCommandData(commandData_t(Device(1, 1, 1024)));
  const Bytes queryOnlyPreset =
      queryOnlySource.serialize2({Command::RetDevice}, "query only");
  const auto queryOnlyPlan =
      device.buildRestorePlan(queryOnlyPreset, "primary");
  check(queryOnlyPlan.valid && queryOnlyPlan.validatedRecordCount == 1 &&
            queryOnlyPlan.entries.empty() &&
            queryOnlyPlan.rejectedRecords.size() == 1,
        "known query-only record is explicit and produces zero writes");
}

struct SignalCounts {
  int completed = 0;
  int failed = 0;
  int lastCompleted = -1;
  int lastTotal = -1;
};

void connectSignals(DeviceInfo &device, SignalCounts &counts) {
  QObject::connect(&device, &DeviceInfo::writeCompleted,
                   [&counts]() { ++counts.completed; });
  QObject::connect(&device, &DeviceInfo::writeFailed,
                   [&counts](const QString &, int completed, int total) {
                     ++counts.failed;
                     counts.lastCompleted = completed;
                     counts.lastTotal = total;
                   });
}

void verifyACKStateMachine(const DeviceInfo::RestorePlan &fullPlan) {
  const auto plan = prefixPlan(fullPlan, 3);

  {
    DeviceInfo device(CommPtr(new Communicator()),
                      DeviceID(DevicePID::iConnect4Audio), 7);
    SignalCounts counts;
    connectSignals(device, counts);
    check(device.dispatchRestorePlan(plan), "success sequence starts");
    for (const auto &entry : plan.entries) {
      acknowledge(device, entry.setterCommand, ErrorCode::NoError);
    }
    check(counts.completed == 1 && counts.failed == 0 &&
              !device.restoreInProgress() && device.restoreCompletedCount == 3,
          "all NoError ACKs complete exactly once");
    acknowledge(device, plan.entries.back().setterCommand, ErrorCode::NoError);
    check(counts.completed == 1 && counts.failed == 0,
          "duplicate late ACK after completion is ignored");
  }

  for (size_t failIndex = 0; failIndex < plan.entries.size(); ++failIndex) {
    DeviceInfo device(CommPtr(new Communicator()),
                      DeviceID(DevicePID::iConnect4Audio), 7);
    SignalCounts counts;
    connectSignals(device, counts);
    device.dispatchRestorePlan(plan);
    for (size_t index = 0; index < failIndex; ++index) {
      acknowledge(device, plan.entries[index].setterCommand,
                  ErrorCode::NoError);
    }
    acknowledge(device, plan.entries[failIndex].setterCommand,
                ErrorCode::CommandFailed);
    check(counts.completed == 0 && counts.failed == 1 &&
              counts.lastCompleted == static_cast<int>(failIndex) &&
              !device.restoreInProgress() &&
              device.restoreCompletedCount == failIndex,
          QString("nonzero ACK at position %1 stops without false completion")
              .arg(failIndex + 1));
  }

  {
    DeviceInfo device(CommPtr(new Communicator()),
                      DeviceID(DevicePID::iConnect4Audio), 7);
    SignalCounts counts;
    connectSignals(device, counts);
    device.dispatchRestorePlan(plan);
    device.timeout();
    check(counts.completed == 0 && counts.failed == 1 &&
              !device.restoreInProgress(),
          "timeout stops restore");
  }

  {
    DeviceInfo device(CommPtr(new Communicator()),
                      DeviceID(DevicePID::iConnect4Audio), 7);
    SignalCounts counts;
    connectSignals(device, counts);
    device.dispatchRestorePlan(plan);
    acknowledge(device, Command::Reset, ErrorCode::NoError);
    check(counts.failed == 1 && counts.completed == 0,
          "wrong ACK command stops restore");
  }

  {
    DeviceInfo device(CommPtr(new Communicator()),
                      DeviceID(DevicePID::iConnect4Audio), 7);
    SignalCounts counts;
    connectSignals(device, counts);
    device.dispatchRestorePlan(plan);
    acknowledge(device, plan.entries.front().setterCommand,
                ErrorCode::NoError, 1);
    check(counts.failed == 1 && counts.completed == 0,
          "wrong transaction/output identity stops restore");
  }

  {
    DeviceInfo device(CommPtr(new Communicator()),
                      DeviceID(DevicePID::iConnect4Audio), 7);
    SignalCounts counts;
    connectSignals(device, counts);
    device.dispatchRestorePlan(plan);
    acknowledge(device, plan.entries.front().setterCommand,
                ErrorCode::NoError, 0, true);
    check(counts.failed == 1 && counts.completed == 0,
          "wrong device identity stops restore");
    acknowledge(device, plan.entries.front().setterCommand,
                ErrorCode::NoError);
    check(counts.failed == 1 && counts.completed == 0,
          "late ACK after failure cannot restart or complete restore");
  }
}

}  // namespace

int main(int argc, char **argv) {
  QCoreApplication application(argc, argv);
  if (argc != 1 && argc != 3) {
    QTextStream(stderr)
        << "usage: restore_planner_test [PRIMARY AUXILIARY]" << Qt::endl;
    return 2;
  }

  DeviceInfo device(CommPtr(new Communicator()),
                    DeviceID(DevicePID::iConnect4Audio), 7);
  device.addCommandData(commandData_t(makeRemap(99, RemapID::InputRemap)));

  verifyNarrowPresets(device);

  DeviceInfo syntheticSource(CommPtr(new Communicator()),
                             DeviceID(DevicePID::iConnect4Audio), 7);
  syntheticSource.addCommandData(
      commandData_t(makeRemap(5, RemapID::InputRemap)));
  syntheticSource.addCommandData(
      commandData_t(makeRemap(5, RemapID::OutputRemap)));
  syntheticSource.addCommandData(
      commandData_t(makeFilter(5, FilterID::InputFilter)));
  syntheticSource.addCommandData(
      commandData_t(makeFilter(5, FilterID::OutputFilter)));
  const Bytes syntheticPreset = syntheticSource.serialize2(
      {Command::RetMIDIPortRemap, Command::RetMIDIPortFilter},
      "Synthetic restore-planner fixture");
  const auto syntheticPlan =
      device.buildRestorePlan(syntheticPreset, "synthetic");
  check(syntheticPlan.valid && syntheticPlan.entries.size() == 4,
        "self-contained synthetic preset creates four planned writes");
  verifyMalformed(device, syntheticPreset);
  verifyACKStateMachine(syntheticPlan);

  if (argc == 3) {
    const QString primaryPath = QString::fromLocal8Bit(argv[1]);
    const QString auxiliaryPath = QString::fromLocal8Bit(argv[2]);
    const Bytes primaryBytes = readFile(primaryPath);
    const Bytes auxiliaryBytes = readFile(auxiliaryPath);
    check(!primaryBytes.empty() && !auxiliaryBytes.empty(),
          "optional known preset fixtures are readable");
    verifyKnownPresets(device, primaryPath, auxiliaryPath);
  }

  QTextStream(stdout) << "RESULT " << (assertions - failures) << "/"
                      << assertions << " assertions passed" << Qt::endl;
  return failures == 0 ? 0 : 1;
}
