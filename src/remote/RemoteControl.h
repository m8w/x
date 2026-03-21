#pragma once
#include "fractal/FractalEngine.h"
#include "fractal/BlendController.h"
#include <thread>
#include <atomic>
#include <string>

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
    int              m_serverFd = -1;
    int              m_port     = 7777;

    void serverLoop();
    void handleClient(int fd);
    void applyParam(const std::string& key, const std::string& val);
    static std::string urlDecode(const std::string& s);
};
