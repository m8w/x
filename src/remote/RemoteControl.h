#pragma once
#include "fractal/FractalEngine.h"
#include "fractal/BlendController.h"
#include <thread>
#include <atomic>
#include <string>
#ifdef _WIN32
#  include <winsock2.h>
using SocketFd = SOCKET;
static constexpr SocketFd kInvalidSocket = INVALID_SOCKET;
#else
using SocketFd = int;
static constexpr SocketFd kInvalidSocket = -1;
#endif

// Tiny embedded HTTP server that serves a mobile-friendly fractal control page.
// Usage:
//   RemoteControl remote(engine, blend);
//   remote.start(7777);
//   // Open http://<machine-ip>:7777 on your phone
class RemoteControl {
public:
    RemoteControl(FractalEngine& engine, BlendController& blend);
    ~RemoteControl();

    bool start(int port = 7777);
    void stop();
    bool isRunning() const { return m_running; }
    int  port()      const { return m_port;    }

private:
    FractalEngine&   m_engine;
    BlendController& m_blend;
    std::thread      m_thread;
    std::atomic<bool> m_running{false};
    SocketFd         m_serverFd = kInvalidSocket;
    int              m_port     = 7777;

    void serverLoop();
    void handleClient(SocketFd fd);
    void applyParam(const std::string& key, const std::string& val);
    static std::string urlDecode(const std::string& s);
};
