#include "MidiOutput.h"
#include <RtMidi.h>
#include <cstdio>
#include <vector>

MidiOutput::MidiOutput() {
    try {
        m_rtmidi = new RtMidiOut();
    } catch (RtMidiError& e) {
        fprintf(stderr, "MidiOutput: RtMidiOut init failed: %s\n",
                e.getMessage().c_str());
        m_rtmidi = nullptr;
    }
}

MidiOutput::~MidiOutput() {
    close();
    delete static_cast<RtMidiOut*>(m_rtmidi);
}

int MidiOutput::portCount() const {
    auto* rt = static_cast<RtMidiOut*>(m_rtmidi);
    return rt ? (int)rt->getPortCount() : 0;
}

std::string MidiOutput::portName(int i) const {
    auto* rt = static_cast<RtMidiOut*>(m_rtmidi);
    if (!rt) return "";
    try { return rt->getPortName((unsigned)i); }
    catch (...) { return ""; }
}

bool MidiOutput::open(int portIndex) {
    auto* rt = static_cast<RtMidiOut*>(m_rtmidi);
    if (!rt) return false;
    close();
    try {
        rt->openPort((unsigned)portIndex);
        m_open      = true;
        m_portIndex = portIndex;
        return true;
    } catch (RtMidiError& e) {
        fprintf(stderr, "MidiOutput: open failed: %s\n", e.getMessage().c_str());
        return false;
    }
}

void MidiOutput::close() {
    if (!m_open) return;
    auto* rt = static_cast<RtMidiOut*>(m_rtmidi);
    if (rt) { try { rt->closePort(); } catch (...) {} }
    m_open      = false;
    m_portIndex = -1;
}

void MidiOutput::send(const MidiInput::Message& msg) {
    if (!m_open) return;
    auto* rt = static_cast<RtMidiOut*>(m_rtmidi);
    if (!rt) return;
    // Determine message length from status byte
    int type   = msg.status & 0xF0;
    int nbytes = (type == 0xC0 || type == 0xD0) ? 2 : 3;  // PC/AT = 2 bytes
    try {
        std::vector<uint8_t> buf;
        buf.reserve(3);
        buf.push_back(msg.status);
        buf.push_back(msg.data1);
        if (nbytes == 3) buf.push_back(msg.data2);
        rt->sendMessage(&buf);
    } catch (...) {}
}

void MidiOutput::sendRaw(uint8_t b0, uint8_t b1, uint8_t b2, int nbytes) {
    if (!m_open) return;
    auto* rt = static_cast<RtMidiOut*>(m_rtmidi);
    if (!rt) return;
    std::vector<uint8_t> buf(nbytes);
    if (nbytes > 0) buf[0] = b0;
    if (nbytes > 1) buf[1] = b1;
    if (nbytes > 2) buf[2] = b2;
    try { rt->sendMessage(&buf); } catch (...) {}
}

void MidiOutput::panic() {
    if (!m_open) return;
    // CC 123 (All Notes Off) on every channel
    for (int ch = 0; ch < 16; ch++)
        sendRaw((uint8_t)(0xB0 | ch), 123, 0);
}
