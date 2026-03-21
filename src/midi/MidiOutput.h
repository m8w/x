#pragma once
#include "MidiInput.h"
#include <string>
#include <vector>
#include <cstdint>

// MIDI output port — wraps RtMidiOut.
// Generated notes and optional MIDI-thru are sent here so a DAW/VST
// receives real MIDI and plays along while the fractal visuals react.
class MidiOutput {
public:
    MidiOutput();
    ~MidiOutput();

    // Port enumeration
    int         portCount() const;
    std::string portName(int i) const;

    // Open / close
    bool open(int portIndex);
    void close();
    bool isOpen()       const { return m_open; }
    int  openedPort()   const { return m_portIndex; }

    // Send a three-byte message.  Safe to call even if not open (no-op).
    void send(const MidiInput::Message& msg);

    // Convenience: send raw bytes directly
    void sendRaw(uint8_t b0, uint8_t b1 = 0, uint8_t b2 = 0, int nbytes = 3);

    // All-notes-off on every channel (panic)
    void panic();

private:
    void*  m_rtmidi    = nullptr;  // RtMidiOut* (hidden to avoid header dep)
    bool   m_open      = false;
    int    m_portIndex = -1;
};
