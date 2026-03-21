#include "MidiInput.h"
#include <RtMidi.h>
#include <cstdio>

// ── Callback called from RtMidi's internal thread ────────────────────────────
void MidiInput::rtCallback(double /*ts*/, std::vector<uint8_t>* msg, void* userdata) {
    auto* self = static_cast<MidiInput*>(userdata);
    if (!msg || msg->size() < 2) return;

    Message m;
    m.status = (*msg)[0];
    m.data1  = msg->size() > 1 ? (*msg)[1] : 0;
    m.data2  = msg->size() > 2 ? (*msg)[2] : 0;

    std::lock_guard<std::mutex> lk(self->m_mtx);
    self->m_queue.push_back(m);
    self->m_last = m;
}

MidiInput::MidiInput() {
    try {
        m_rtmidi = new RtMidiIn();
    } catch (RtMidiError& e) {
        fprintf(stderr, "MidiInput: RtMidiIn init failed: %s\n", e.getMessage().c_str());
        m_rtmidi = nullptr;
    }
}

MidiInput::~MidiInput() {
    close();
    delete static_cast<RtMidiIn*>(m_rtmidi);
}

int MidiInput::portCount() const {
    auto* rt = static_cast<RtMidiIn*>(m_rtmidi);
    return rt ? (int)rt->getPortCount() : 0;
}

std::string MidiInput::portName(int i) const {
    auto* rt = static_cast<RtMidiIn*>(m_rtmidi);
    if (!rt) return "";
    try { return rt->getPortName((unsigned)i); }
    catch (...) { return ""; }
}

bool MidiInput::open(int portIndex) {
    auto* rt = static_cast<RtMidiIn*>(m_rtmidi);
    if (!rt) return false;
    close();
    try {
        rt->openPort((unsigned)portIndex);
        rt->ignoreTypes(true, true, true);  // ignore sysex/time/sense
        rt->setCallback(rtCallback, this);
        m_open      = true;
        m_portIndex = portIndex;
        return true;
    } catch (RtMidiError& e) {
        fprintf(stderr, "MidiInput: open failed: %s\n", e.getMessage().c_str());
        return false;
    }
}

void MidiInput::close() {
    if (!m_open) return;
    auto* rt = static_cast<RtMidiIn*>(m_rtmidi);
    if (rt) {
        try {
            rt->cancelCallback();
            rt->closePort();
        } catch (...) {}
    }
    m_open      = false;
    m_portIndex = -1;
}

std::vector<MidiInput::Message> MidiInput::poll() {
    std::lock_guard<std::mutex> lk(m_mtx);
    std::vector<Message> out;
    out.swap(m_queue);
    return out;
}

MidiInput::Message MidiInput::lastMessage() const {
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_last;
}
