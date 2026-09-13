#include <QApplication>
#include <QComboBox>
#include <QTableWidget>
#include <QTextStream>

#define private public
#include "MIDIRelated/MIDIChannelRemapForm.h"
#include "MIDIRelated/MIDIControllerRemapForm.h"
#undef private

#include "CommandData.h"
#include "MIDIInfo.h"
#include "MIDIPortInfo.h"
#include "MIDIPortRemap.h"
#include "PortType.h"

#include <cstring>
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

MIDIPortInfo makePortInfo(Word portID, const char *name) {
  Bytes data;
  data += 0x01;
  appendMidiWord(data, portID);
  data += static_cast<Byte>(PortType::DIN);
  data += 0x01;
  data += 0x00;
  data += 0x00;
  data += 0x00;
  data += 0x20;
  data += 0x03;
  data.insert(data.end(), name, name + std::strlen(name));

  MIDIPortInfo result;
  auto begin = data.begin();
  auto end = data.end();
  result.parse(begin, end);
  return result;
}

MIDIPortRemap makeRemap(Word portID, RemapTypeEnum remapID,
                        Byte source, Byte destination) {
  MIDIPortRemap::RemapStatues statuses = {};
  for (int index = 0; index < 16; ++index) {
    statuses[index].channelNumber = static_cast<Byte>(index);
  }

  MIDIPortRemap::RemapFlagsVector controllers(8);
  controllers[0].controllerSource = source;
  controllers[0].controllerDestination = destination;
  return MIDIPortRemap(portID, remapID, 8, statuses, controllers);
}

template <typename Form>
QComboBox *directionCombo(Form *form) {
  return form->template findChild<QComboBox *>("remapTypeComboBox");
}

template <typename Form>
void clearPending(Form *form) {
  form->sendTimer->stop();
  form->updateList.clear();
}

template <typename Form>
void selectDirection(Form *form, RemapTypeEnum remapID) {
  directionCombo(form)->setCurrentIndex(remapID == RemapID::InputRemap ? 0 : 1);
}

template <typename Form>
void verifyBatching(Form *form, const QString &name) {
  clearPending(form);
  selectDirection(form, RemapID::OutputRemap);
  form->addToUpdateList(5);
  form->addToUpdateList(23);
  check(form->updateList.count() == 2,
        name + ": P5/Output and P23/Output both survive");
  check(form->updateList.contains(qMakePair(Word(5), RemapID::OutputRemap)) &&
            form->updateList.contains(
                qMakePair(Word(23), RemapID::OutputRemap)),
        name + ": same-direction pending identities are exact");
  check(form->sendTimer->isActive(),
        name + ": latest edit leaves batching timer active");

  clearPending(form);
  selectDirection(form, RemapID::OutputRemap);
  form->addToUpdateList(23);
  form->addToUpdateList(5);
  check(form->updateList.count() == 2,
        name + ": reverse same-direction ordering preserves both ports");
  check(form->updateList.contains(qMakePair(Word(5), RemapID::OutputRemap)) &&
            form->updateList.contains(
                qMakePair(Word(23), RemapID::OutputRemap)),
        name + ": reverse ordering preserves exact identities");

  clearPending(form);
  selectDirection(form, RemapID::InputRemap);
  form->addToUpdateList(5);
  selectDirection(form, RemapID::OutputRemap);
  form->addToUpdateList(5);
  check(form->updateList.count() == 2 &&
            form->updateList.contains(qMakePair(Word(5), RemapID::InputRemap)) &&
            form->updateList.contains(qMakePair(Word(5), RemapID::OutputRemap)),
        name + ": P5/Input and P5/Output remain distinct");

  clearPending(form);
  selectDirection(form, RemapID::InputRemap);
  form->addToUpdateList(5);
  form->addToUpdateList(23);
  check(form->updateList.count() == 2 &&
            form->updateList.contains(qMakePair(Word(5), RemapID::InputRemap)) &&
            form->updateList.contains(qMakePair(Word(23), RemapID::InputRemap)),
        name + ": P5/Input and P23/Input remain distinct");

  clearPending(form);
  selectDirection(form, RemapID::OutputRemap);
  form->addToUpdateList(5);
  form->addToUpdateList(5);
  check(form->updateList.count() == 1,
        name + ": repeated P5/Output edits coalesce");

  selectDirection(form, RemapID::InputRemap);
  check(form->updateList.contains(qMakePair(Word(5), RemapID::OutputRemap)),
        name + ": direction selection change does not retarget pending P5/Output");
  clearPending(form);
}

template <typename Form>
void verifyPendingPayloadIdentities(Form *form, DeviceInfoPtr device,
                                    const QString &name) {
  clearPending(form);
  selectDirection(form, RemapID::OutputRemap);
  form->addToUpdateList(5);
  form->addToUpdateList(23);

  bool saw5 = false;
  bool saw23 = false;
  for (const auto &update : form->updateList) {
    const auto &remap = device->midiPortRemap(update.first, update.second);
    check(remap.portID() == update.first && remap.remapID() == update.second,
          name + ": pending key retrieves matching full-object identity");
    if (update.first == 5) {
      saw5 = remap.controller_at(0).controllerSource == 11 &&
             remap.controller_at(0).controllerDestination == 7;
    } else if (update.first == 23) {
      saw23 = remap.controller_at(0).controllerSource == 56 &&
              remap.controller_at(0).controllerDestination == 32;
    }
  }
  check(saw5 && saw23,
        name + ": isolated pending payloads retain distinctive object data");
  clearPending(form);
}

template <typename Form>
void verifySameObjectCoalescing(Form *form, DeviceInfoPtr device,
                                const QString &name) {
  clearPending(form);
  selectDirection(form, RemapID::OutputRemap);
  auto &remap = device->midiPortRemap(5, RemapID::OutputRemap);
  auto &controller = remap.controller_at(0);

  controller.controllerSource = 2;
  form->addToUpdateList(5);
  controller.controllerDestination = 71;
  form->addToUpdateList(5);
  controller.channelBitmap.reset();
  form->addToUpdateList(5);
  controller.channelBitmap.set(MIDIPortRemap::ChannelBitmapBits::channel16);
  form->addToUpdateList(5);

  check(form->updateList.count() == 1 &&
            form->updateList.contains(qMakePair(Word(5), RemapID::OutputRemap)),
        name + ": four P5/Output edits coalesce to one identity");
  const auto &finalObject = device->midiPortRemap(5, RemapID::OutputRemap);
  check(finalObject.controller_at(0).controllerSource == 2 &&
            finalObject.controller_at(0).controllerDestination == 71 &&
            finalObject.controller_at(0).channelBitmap.test(
                MIDIPortRemap::ChannelBitmapBits::channel16) &&
            finalObject.controller_at(0).channelBitmap.count() == 1,
        name + ": coalesced object contains final combined state");
  clearPending(form);
}

void verifyNoOpAndInvalidEdits(MIDIChannelRemapForm *channelForm,
                               MIDIControllerRemapForm *controllerForm) {
  clearPending(channelForm);
  selectDirection(channelForm, RemapID::InputRemap);
  channelForm->cellStateChange(6, 0, BlockState::Empty);
  channelForm->lineEditChanged(0);
  check(channelForm->updateList.isEmpty(),
        "Channel Remap: unchanged edits create no pending update");
  channelForm->cellStateChange(-1, 0, BlockState::Full);
  channelForm->lineEditChanged(-1);
  check(channelForm->updateList.isEmpty(),
        "Channel Remap: invalid edits create no pending update");

  clearPending(controllerForm);
  selectDirection(controllerForm, RemapID::InputRemap);
  controllerForm->cellStateChange(0, 0, BlockState::Empty);
  controllerForm->srcComboBoxChanged(0);
  controllerForm->dstComboBoxChanged(0);
  check(controllerForm->updateList.isEmpty(),
        "Controller Remap: unchanged edits create no pending update");
  controllerForm->cellStateChange(-1, 0, BlockState::Full);
  controllerForm->srcComboBoxChanged(-1);
  controllerForm->dstComboBoxChanged(-1);
  check(controllerForm->updateList.isEmpty(),
        "Controller Remap: invalid edits create no pending update");
}

void verifyProtocolIdentity(DeviceInfoPtr device) {
  const auto key5In = MIDIPortRemap::queryKey(5, RemapID::InputRemap);
  const auto key5Out = MIDIPortRemap::queryKey(5, RemapID::OutputRemap);
  const auto key23In = MIDIPortRemap::queryKey(23, RemapID::InputRemap);
  const auto key23Out = MIDIPortRemap::queryKey(23, RemapID::OutputRemap);
  check(key5In != key5Out, "identity P5/Input differs from P5/Output");
  check(key5Out != key23Out, "identity P5/Output differs from P23/Output");
  check(key5In != key23In, "identity P5/Input differs from P23/Input");

  check(device->midiPortRemap(5, RemapID::InputRemap).portID() == 5 &&
            device->midiPortRemap(5, RemapID::InputRemap).remapID() ==
                RemapID::InputRemap,
        "cached P5/Input object preserves protocol identity");
  check(device->midiPortRemap(23, RemapID::OutputRemap).portID() == 23 &&
            device->midiPortRemap(23, RemapID::OutputRemap).remapID() ==
                RemapID::OutputRemap,
        "cached P23/Output object preserves protocol identity");
}

void verifySharedFullObject(MIDIChannelRemapForm *channelForm,
                            MIDIControllerRemapForm *controllerForm,
                            DeviceInfoPtr device) {
  clearPending(channelForm);
  clearPending(controllerForm);
  selectDirection(channelForm, RemapID::InputRemap);
  selectDirection(controllerForm, RemapID::InputRemap);

  auto &canonical = device->midiPortRemap(5, RemapID::InputRemap);
  canonical.remapStatus_at(0).noteEvents = false;
  canonical.controller_at(0).controllerSource = 2;
  canonical.controller_at(0).controllerDestination = 74;

  channelForm->cellStateChange(6, 0, BlockState::Full);

  auto *table = controllerForm->findChild<QTableWidget *>("tableWidget");
  auto *source = qobject_cast<QComboBox *>(table->cellWidget(0, 16));
  source->setCurrentIndex(64);
  controllerForm->srcComboBoxChanged(0);

  check(&channelForm->device->midiPortRemap(5, RemapID::InputRemap) ==
            &controllerForm->device->midiPortRemap(5, RemapID::InputRemap),
        "Channel and Controller forms reference one canonical cached object");
  check(canonical.remapStatus_at(0).noteEvents,
        "cross-editor object retains Channel Remap edit");
  check(canonical.controller_at(0).controllerSource == 64 &&
            canonical.controller_at(0).controllerDestination == 74,
        "cross-editor object retains Controller Remap edit and other fields");
  check(canonical.generate() ==
            controllerForm->device->midiPortRemap(5, RemapID::InputRemap)
                .generate(),
        "both forms serialize the same final full object");
  clearPending(channelForm);
  clearPending(controllerForm);
}

}  // namespace

int main(int argc, char **argv) {
  QApplication app(argc, argv);

  CommPtr comm(new Communicator());
  DeviceInfoPtr device(new DeviceInfo(comm));
  device->addCommandData(commandData_t(
      MIDIInfo(23, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0)));
  device->addCommandData(commandData_t(makePortInfo(5, "Synthetic DIN 5")));
  device->addCommandData(commandData_t(makePortInfo(23, "Synthetic DIN 23")));

  device->addCommandData(commandData_t(
      makeRemap(5, RemapID::InputRemap, 2, 74)));
  device->addCommandData(commandData_t(
      makeRemap(5, RemapID::OutputRemap, 11, 7)));
  device->addCommandData(commandData_t(
      makeRemap(23, RemapID::InputRemap, 64, 1)));
  device->addCommandData(commandData_t(
      makeRemap(23, RemapID::OutputRemap, 56, 32)));

  verifyProtocolIdentity(device);

  auto *channelForm = new MIDIChannelRemapForm(comm, device);
  auto *controllerForm = new MIDIControllerRemapForm(device);
  verifyBatching(channelForm, "Channel Remap");
  verifyBatching(controllerForm, "Controller Remap");
  verifyNoOpAndInvalidEdits(channelForm, controllerForm);
  verifyPendingPayloadIdentities(channelForm, device, "Channel Remap");
  verifyPendingPayloadIdentities(controllerForm, device, "Controller Remap");
  verifySameObjectCoalescing(channelForm, device, "Channel Remap");
  verifySameObjectCoalescing(controllerForm, device, "Controller Remap");
  verifySharedFullObject(channelForm, controllerForm, device);

  QTextStream(stdout) << "RESULT failures=" << failures << Qt::endl;
  std::_Exit(failures == 0 ? 0 : 1);
}
