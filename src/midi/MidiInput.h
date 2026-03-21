#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

// Non-blocking MIDI input wrapper around RtMidi.
// Messages arrive via callback and are queued; call poll() each frame to drain.
class MidiInput {
public:
    MidiInput();
    ~MidiInput();

    // Port enumeration
    int         portCount() const;
    std::string portName(int i) const;

    // Open/close
    bool open(int portIndex);
    void close();
    bool isOpen() const { return m_open; }
    int  openedPort() const { return m_portIndex; }

    // Three-byte MIDI message
    struct Message {
        uint8_t status;  // high nibble: type  low nibble: channel (0-based)
        uint8_t data1;   // note / CC number
        uint8_t data2;   // velocity / CC value
    };

    // Drain all messages received since last call.  Thread-safe.
    std::vector<Message> poll();

    // Most-recently received message (for the activity indicator in the UI)
    Message lastMessage() const;

private:
    static void rtCallback(double ts, std::vector<uint8_t>* msg, void* userdata);

    void*               m_rtmidi    = nullptr;  // RtMidiIn* hidden to avoid header dep
    mutable std::mutex  m_mtx;
    std::vector<Message> m_queue;
    Message             m_last      = {0,0,0};
    bool                m_open      = false;
    int                 m_portIndex = -1;
};
