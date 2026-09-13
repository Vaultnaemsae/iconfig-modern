#include <QApplication>
#include <QComboBox>
#include <QSignalBlocker>
#include <QTextStream>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#define private public
#include "MIDIRelated/MIDIPortFiltersForm.h"
#include "MIDIRelated/MIDIControllerFilterForm.h"
#undef private

#include "CommandData.h"
#include "MIDIInfo.h"
#include "MIDIPortFilter.h"
#include "MIDIPortInfo.h"
#include "PortType.h"

#include <cstdlib>
#include <cstring>
#include <initializer_list>

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

MIDIPortFilter makeFilter(Word portID, FilterIDEnum filterID,
                          Byte channelStatus, Byte controllerID,
                          Byte controllerBitmap) {
  Bytes data;
  data += 0x01;
  appendMidiWord(data, portID);
  data += static_cast<Byte>(filterID);
  data += 0x08;
  data += 0x00;
  data += 0x00;
  for (int channel = 0; channel < 16; ++channel) {
    data += (channel == 0) ? channelStatus : 0x00;
  }
  for (int controller = 0; controller < 8; ++controller) {
    data += 0x00;
    data += 0x00;
    data += 0x00;
    data += (controller == 0) ? controllerBitmap : 0x00;
    data += (controller == 0) ? controllerID : 0x00;
  }

  MIDIPortFilter result;
  auto begin = data.begin();
  auto end = data.end();
  result.parse(begin, end);
  return result;
}

template <typename Form>
QComboBox *directionCombo(Form *form) {
  return form->template findChild<QComboBox *>("filterTypeComboBox");
}

template <typename Form>
void selectDirection(Form *form, FilterIDEnum filterID) {
  directionCombo(form)->setCurrentIndex(filterID == FilterID::InputFilter ? 0
                                                                         : 1);
}

void selectPorts(MIDIPortSelectionForm *selection,
                 std::initializer_list<Word> portIDs) {
  auto *const tree = selection->findChild<QTreeWidget *>("portTreeWidget");
  const QSignalBlocker blocker(tree);
  for (const auto &entry : selection->portItemMap) {
    entry.second->setSelected(false);
  }
  for (const auto portID : portIDs) {
    selection->portItemMap.at(portID)->setSelected(true);
  }
  selection->selectionChanged();
}

template <typename Form>
void clearPending(Form *form) {
  form->sendTimer->stop();
  form->updateList.clear();
}

void verifyProtocolIdentity(DeviceInfoPtr device) {
  const auto key5In = MIDIPortFilter::queryKey(5, FilterID::InputFilter);
  const auto key5Out = MIDIPortFilter::queryKey(5, FilterID::OutputFilter);
  const auto key23In = MIDIPortFilter::queryKey(23, FilterID::InputFilter);
  const auto key23Out = MIDIPortFilter::queryKey(23, FilterID::OutputFilter);
  check(key5In != key5Out, "identity P5/Input differs from P5/Output");
  check(key5In != key23In, "identity P5/Input differs from P23/Input");
  check(key5Out != key23Out, "identity P5/Output differs from P23/Output");
  check(device->midiPortFilter(5, FilterID::InputFilter).portID() == 5 &&
            device->midiPortFilter(5, FilterID::InputFilter).filterID() ==
                FilterID::InputFilter,
        "canonical cache preserves P5/Input identity");
}

void verifyPortFilterBatching(MIDIPortFiltersForm *form, DeviceInfoPtr device) {
  clearPending(form);
  selectDirection(form, FilterID::InputFilter);
  selectPorts(form->portSelectionForm, {5});
  form->cellStateChange(0, 1, BlockState::Full);
  selectPorts(form->portSelectionForm, {23});
  form->cellStateChange(0, 1, BlockState::Full);
  check(form->updateList.count() == 2,
        "Port Filters: P5/Input then P23/Input both survive");
  check(form->updateList.contains(qMakePair(Word(5), FilterID::InputFilter)) &&
            form->updateList.contains(
                qMakePair(Word(23), FilterID::InputFilter)),
        "Port Filters: same-direction Input identities are exact");

  clearPending(form);
  selectPorts(form->portSelectionForm, {23});
  form->cellStateChange(0, 2, BlockState::Full);
  selectPorts(form->portSelectionForm, {5});
  form->cellStateChange(0, 2, BlockState::Full);
  check(form->updateList.count() == 2,
        "Port Filters: reverse Input ordering preserves both ports");

  clearPending(form);
  selectDirection(form, FilterID::OutputFilter);
  selectPorts(form->portSelectionForm, {5});
  form->cellStateChange(0, 3, BlockState::Full);
  selectPorts(form->portSelectionForm, {23});
  form->cellStateChange(0, 3, BlockState::Full);
  check(form->updateList.count() == 2,
        "Port Filters: P5/Output then P23/Output both survive");
  check(form->updateList.contains(qMakePair(Word(5), FilterID::OutputFilter)) &&
            form->updateList.contains(
                qMakePair(Word(23), FilterID::OutputFilter)),
        "Port Filters: same-direction Output identities are exact");

  clearPending(form);
  selectPorts(form->portSelectionForm, {23});
  form->cellStateChange(0, 4, BlockState::Full);
  selectPorts(form->portSelectionForm, {5});
  form->cellStateChange(0, 4, BlockState::Full);
  check(form->updateList.count() == 2,
        "Port Filters: reverse Output ordering preserves both ports");

  clearPending(form);
  selectPorts(form->portSelectionForm, {5});
  selectDirection(form, FilterID::InputFilter);
  form->cellStateChange(1, 5, BlockState::Full);
  selectDirection(form, FilterID::OutputFilter);
  form->cellStateChange(1, 5, BlockState::Full);
  check(form->updateList.count() == 2,
        "Port Filters: P5/Input and P5/Output remain distinct");
  check(form->updateList.contains(qMakePair(Word(5), FilterID::InputFilter)) &&
            form->updateList.contains(
                qMakePair(Word(5), FilterID::OutputFilter)),
        "Port Filters: opposite-direction identities are exact");

  clearPending(form);
  selectDirection(form, FilterID::InputFilter);
  selectPorts(form->portSelectionForm, {5});
  form->cellStateChange(2, 6, BlockState::Full);
  form->cellStateChange(3, 6, BlockState::Full);
  check(form->updateList.count() == 1 &&
            device->midiPortFilter(5, FilterID::InputFilter)
                .channelFilterStatus_at(6)
                .test(ChannelFilterStatusBit::programChangeEvents) &&
            device->midiPortFilter(5, FilterID::InputFilter)
                .channelFilterStatus_at(6)
                .test(ChannelFilterStatusBit::controlChangeEvents),
        "Port Filters: repeated P5/Input edits coalesce with final state");

  selectPorts(form->portSelectionForm, {23});
  selectDirection(form, FilterID::OutputFilter);
  check(form->updateList.count() == 1 &&
            form->updateList.contains(
                qMakePair(Word(5), FilterID::InputFilter)),
        "Port Filters: selection changes do not retarget pending P5/Input");

  clearPending(form);
  selectDirection(form, FilterID::InputFilter);
  selectPorts(form->portSelectionForm, {5});
  const auto current = device->midiPortFilter(5, FilterID::InputFilter)
                           .channelFilterStatus_at(0)
                           .test(ChannelFilterStatusBit::pitchBendEvents);
  form->cellStateChange(0, 0,
                        current ? BlockState::Full : BlockState::Empty);
  check(form->updateList.isEmpty(),
        "Port Filters: unchanged edit creates no pending update");
  clearPending(form);
  form->cellStateChange(99, 0, BlockState::Full);
  check(form->updateList.isEmpty(),
        "Port Filters: invalid edit creates no pending update");
  clearPending(form);
}

void verifyControllerFilterBatching(MIDIControllerFilterForm *form,
                                    DeviceInfoPtr device) {
  clearPending(form);
  selectPorts(form->portSelectionForm, {5});
  selectDirection(form, FilterID::InputFilter);
  clearPending(form);
  form->cellStateChange(0, 8, BlockState::Full);
  selectDirection(form, FilterID::OutputFilter);
  form->cellStateChange(0, 8, BlockState::Full);
  check(form->updateList.count() == 2,
        "Controller Filters: P5/Input then P5/Output both survive");
  check(form->updateList.contains(qMakePair(Word(5), FilterID::InputFilter)) &&
            form->updateList.contains(
                qMakePair(Word(5), FilterID::OutputFilter)),
        "Controller Filters: opposite-direction identities are exact");

  clearPending(form);
  selectDirection(form, FilterID::OutputFilter);
  form->cellStateChange(0, 9, BlockState::Full);
  selectDirection(form, FilterID::InputFilter);
  form->cellStateChange(0, 9, BlockState::Full);
  check(form->updateList.count() == 2,
        "Controller Filters: reverse P5 direction ordering preserves both");

  clearPending(form);
  selectPorts(form->portSelectionForm, {23});
  selectDirection(form, FilterID::InputFilter);
  clearPending(form);
  form->cellStateChange(0, 10, BlockState::Full);
  selectDirection(form, FilterID::OutputFilter);
  form->cellStateChange(0, 10, BlockState::Full);
  check(form->updateList.count() == 2,
        "Controller Filters: P23/Input and P23/Output both survive");

  clearPending(form);
  selectDirection(form, FilterID::InputFilter);
  selectPorts(form->portSelectionForm, {5});
  clearPending(form);
  form->cellStateChange(0, 11, BlockState::Full);
  selectPorts(form->portSelectionForm, {23});
  form->cellStateChange(0, 11, BlockState::Full);
  check(form->updateList.count() == 2,
        "Controller Filters: different ports with same direction survive");
  check(form->updateList.contains(qMakePair(Word(5), FilterID::InputFilter)) &&
            form->updateList.contains(
                qMakePair(Word(23), FilterID::InputFilter)),
        "Controller Filters: same-direction port identities are exact");

  clearPending(form);
  selectPorts(form->portSelectionForm, {5});
  selectDirection(form, FilterID::InputFilter);
  clearPending(form);
  form->cellStateChange(0, 12, BlockState::Full);
  form->cellStateChange(0, 13, BlockState::Full);
  check(form->updateList.count() == 1 &&
            device->midiPortFilter(5, FilterID::InputFilter)
                .controllerFilter_at(0)
                .channelBitmap.test(ChannelBitmapBit::channel13) &&
            device->midiPortFilter(5, FilterID::InputFilter)
                .controllerFilter_at(0)
                .channelBitmap.test(ChannelBitmapBit::channel14),
        "Controller Filters: repeated P5/Input edits coalesce with final state");

  selectPorts(form->portSelectionForm, {23});
  selectDirection(form, FilterID::OutputFilter);
  check(form->updateList.count() == 1 &&
            form->updateList.contains(
                qMakePair(Word(5), FilterID::InputFilter)),
        "Controller Filters: selection changes do not retarget P5/Input");

  clearPending(form);
  selectPorts(form->portSelectionForm, {5});
  selectDirection(form, FilterID::InputFilter);
  clearPending(form);
  const auto current = device->midiPortFilter(5, FilterID::InputFilter)
                           .controllerFilter_at(0)
                           .channelBitmap.test(ChannelBitmapBit::channel1);
  form->cellStateChange(0, 0,
                        current ? BlockState::Full : BlockState::Empty);
  check(form->updateList.isEmpty(),
        "Controller Filters: unchanged edit creates no pending update");
  form->cellStateChange(-1, 0, BlockState::Full);
  check(form->updateList.isEmpty(),
        "Controller Filters: invalid edit creates no pending update");
  clearPending(form);
}

void verifyCrossEditor(MIDIPortFiltersForm *portForm,
                       MIDIControllerFilterForm *controllerForm,
                       DeviceInfoPtr device) {
  clearPending(portForm);
  clearPending(controllerForm);
  selectPorts(portForm->portSelectionForm, {5});
  selectPorts(controllerForm->portSelectionForm, {5});
  selectDirection(portForm, FilterID::InputFilter);
  selectDirection(controllerForm, FilterID::InputFilter);

  auto &canonical = device->midiPortFilter(5, FilterID::InputFilter);
  canonical.channelFilterStatus_at(7).reset();
  canonical.controllerFilter_at(0).channelBitmap.reset();
  portForm->cellStateChange(5, 7, BlockState::Full);
  controllerForm->cellStateChange(0, 7, BlockState::Full);

  check(&portForm->device->midiPortFilter(5, FilterID::InputFilter) ==
            &controllerForm->device->midiPortFilter(5,
                                                     FilterID::InputFilter),
        "Port and Controller filter forms share one canonical object");
  check(canonical.channelFilterStatus_at(7).test(
            ChannelFilterStatusBit::noteEvents),
        "cross-editor object retains port/event edit");
  check(canonical.controllerFilter_at(0).channelBitmap.test(
            ChannelBitmapBit::channel8),
        "cross-editor object retains controller-filter edit");
  check(canonical.generate() ==
            controllerForm->device->midiPortFilter(5, FilterID::InputFilter)
                .generate(),
        "cross-editor full serialization contains both edits");
  check(portForm->updateList.contains(
            qMakePair(Word(5), FilterID::InputFilter)) &&
            controllerForm->updateList.contains(
                qMakePair(Word(5), FilterID::InputFilter)),
        "both editors retain the exact shared-object pending identity");
  clearPending(portForm);
  clearPending(controllerForm);
}

}  // namespace

int main(int argc, char **argv) {
  QApplication app(argc, argv);

  CommPtr comm(new Communicator());
  DeviceInfoPtr device(new DeviceInfo(comm));
  device->addCommandData(commandData_t(
      MIDIInfo(23, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0)));
  device->addCommandData(commandData_t(makePortInfo(1, "Synthetic DIN 1")));
  device->addCommandData(commandData_t(makePortInfo(5, "Synthetic DIN 5")));
  device->addCommandData(commandData_t(makePortInfo(23, "Synthetic DIN 23")));

  device->addCommandData(commandData_t(
      makeFilter(1, FilterID::InputFilter, 0x00, 0, 0x00)));
  device->addCommandData(commandData_t(
      makeFilter(1, FilterID::OutputFilter, 0x00, 0, 0x00)));
  device->addCommandData(commandData_t(
      makeFilter(5, FilterID::InputFilter, 0x01, 2, 0x01)));
  device->addCommandData(commandData_t(
      makeFilter(5, FilterID::OutputFilter, 0x02, 11, 0x02)));
  device->addCommandData(commandData_t(
      makeFilter(23, FilterID::InputFilter, 0x04, 64, 0x04)));
  device->addCommandData(commandData_t(
      makeFilter(23, FilterID::OutputFilter, 0x08, 56, 0x08)));

  verifyProtocolIdentity(device);

  auto *portForm = new MIDIPortFiltersForm(comm, device);
  auto *controllerForm = new MIDIControllerFilterForm(comm, device);
  verifyPortFilterBatching(portForm, device);
  verifyControllerFilterBatching(controllerForm, device);
  verifyCrossEditor(portForm, controllerForm, device);

  QTextStream(stdout) << "RESULT failures=" << failures << Qt::endl;
  std::_Exit(failures == 0 ? 0 : 1);
}
