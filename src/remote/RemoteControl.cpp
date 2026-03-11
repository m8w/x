#include "RemoteControl.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <stdexcept>

// ── Embedded mobile UI ────────────────────────────────────────────────────────
static const char kHtmlPage[] = R"HTML(<!DOCTYPE html>
<html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Fractal Remote</title><style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0d0d0d;color:#e0e0e0;font-family:-apple-system,BlinkMacSystemFont,sans-serif;padding:14px;padding-bottom:40px}
h1{color:#88ccff;font-size:22px;margin-bottom:14px;letter-spacing:-0.5px}
.card{background:#1a1a1a;border-radius:12px;padding:14px;margin-bottom:10px}
.card h2{color:#ffcc66;font-size:11px;text-transform:uppercase;letter-spacing:1.5px;margin-bottom:12px}
.row{margin-bottom:14px}
.row:last-child{margin-bottom:0}
.row label{display:flex;justify-content:space-between;align-items:baseline;font-size:14px;margin-bottom:5px}
.v{color:#88ccff;font-weight:600;min-width:46px;text-align:right;font-size:13px}
input[type=range]{width:100%;height:34px;accent-color:#55aaff;cursor:pointer}
#dot{position:fixed;top:14px;right:14px;width:10px;height:10px;border-radius:50%;background:#444;transition:background .3s}
#dot.ok{background:#44ee88}
#dot.err{background:#ee4444}
</style></head><body>
<div id="dot"></div>
<h1>&#127801; Fractal Remote</h1>

<div class="card">
  <h2>Blend</h2>
  <div class="row"><label>Mandelbrot <span class="v" id="lv_blend_m">-</span></label><input type=range id="blend_m" min=0 max=1 step=0.01></div>
  <div class="row"><label>Julia <span class="v" id="lv_blend_j">-</span></label><input type=range id="blend_j" min=0 max=1 step=0.01></div>
  <div class="row"><label>Mandelbulb <span class="v" id="lv_blend_mb">-</span></label><input type=range id="blend_mb" min=0 max=1 step=0.01></div>
  <div class="row"><label>Euclidean <span class="v" id="lv_blend_e">-</span></label><input type=range id="blend_e" min=0 max=1 step=0.01></div>
</div>

<div class="card">
  <h2>Julia Set</h2>
  <div class="row"><label>C real <span class="v" id="lv_julia_re">-</span></label><input type=range id="julia_re" min=-2 max=2 step=0.001></div>
  <div class="row"><label>C imaginary <span class="v" id="lv_julia_im">-</span></label><input type=range id="julia_im" min=-2 max=2 step=0.001></div>
</div>

<div class="card">
  <h2>Parameters</h2>
  <div class="row"><label>Mandelbulb power <span class="v" id="lv_power">-</span></label><input type=range id="power" min=2 max=16 step=0.1></div>
  <div class="row"><label>Max iterations <span class="v" id="lv_max_iter">-</span></label><input type=range id="max_iter" min=16 max=512 step=1></div>
  <div class="row"><label>Bailout <span class="v" id="lv_bailout">-</span></label><input type=range id="bailout" min=2 max=10 step=0.1></div>
</div>

<div class="card">
  <h2>View</h2>
  <div class="row"><label>Zoom <span class="v" id="lv_zoom">-</span></label><input type=range id="zoom" min=0.05 max=100 step=0.01></div>
  <div class="row"><label>Pan X <span class="v" id="lv_offset_x">-</span></label><input type=range id="offset_x" min=-3 max=3 step=0.001></div>
  <div class="row"><label>Pan Y <span class="v" id="lv_offset_y">-</span></label><input type=range id="offset_y" min=-3 max=3 step=0.001></div>
</div>

<script>
const ids=['blend_m','blend_j','blend_mb','blend_e','julia_re','julia_im',
           'power','max_iter','bailout','zoom','offset_x','offset_y'];
const dot=document.getElementById('dot');

function fmt(id,v){
  const f=parseFloat(v);
  if(id==='max_iter') return String(Math.round(f));
  if(id.startsWith('julia')||id.startsWith('offset')) return f.toFixed(3);
  return f.toFixed(2);
}

function send(key,val){
  fetch('/set?'+key+'='+encodeURIComponent(val))
    .then(()=>{dot.className='ok'})
    .catch(()=>{dot.className='err'});
}

ids.forEach(id=>{
  const el=document.getElementById(id);
  if(!el) return;
  el.addEventListener('input',()=>{
    const lv=document.getElementById('lv_'+id);
    if(lv) lv.textContent=fmt(id,el.value);
    send(id,el.value);
  });
});

fetch('/state')
  .then(r=>r.json())
  .then(s=>{
    ids.forEach(id=>{
      if(s[id]===undefined) return;
      const el=document.getElementById(id);
      if(!el) return;
      el.value=s[id];
      const lv=document.getElementById('lv_'+id);
      if(lv) lv.textContent=fmt(id,s[id]);
    });
    dot.className='ok';
  })
  .catch(()=>{dot.className='err'});
</script>
</body></html>
)HTML";

// ── Helpers ───────────────────────────────────────────────────────────────────

std::string RemoteControl::urlDecode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int v = 0;
            sscanf(s.c_str() + i + 1, "%2x", &v);
            out += (char)v;
            i += 2;
        } else if (s[i] == '+') {
            out += ' ';
        } else {
            out += s[i];
        }
    }
    return out;
}

void RemoteControl::applyParam(const std::string& key, const std::string& val) {
    try {
        float f = std::stof(val);
        if      (key == "blend_m")   m_blend.mandelbrot = f;
        else if (key == "blend_j")   m_blend.julia      = f;
        else if (key == "blend_mb")  m_blend.mandelbulb = f;
        else if (key == "blend_e")   m_blend.euclidean  = f;
        else if (key == "julia_re")  m_engine.juliaC.x  = f;
        else if (key == "julia_im")  m_engine.juliaC.y  = f;
        else if (key == "power")     m_engine.power     = f;
        else if (key == "max_iter")  m_engine.maxIter   = (int)f;
        else if (key == "bailout")   m_engine.bailout   = f;
        else if (key == "zoom")      m_engine.zoom      = f;
        else if (key == "offset_x")  m_engine.offset.x  = f;
        else if (key == "offset_y")  m_engine.offset.y  = f;
    } catch (...) {}
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

RemoteControl::RemoteControl(FractalEngine& engine, BlendController& blend)
    : m_engine(engine), m_blend(blend) {}

RemoteControl::~RemoteControl() { stop(); }

bool RemoteControl::start(int port) {
    m_port     = port;
    m_serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverFd < 0) { perror("RemoteControl: socket"); return false; }

    int opt = 1;
    setsockopt(m_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)port);

    if (bind(m_serverFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("RemoteControl: bind"); close(m_serverFd); m_serverFd = -1; return false;
    }
    if (listen(m_serverFd, 8) < 0) {
        perror("RemoteControl: listen"); close(m_serverFd); m_serverFd = -1; return false;
    }

    m_running = true;
    m_thread  = std::thread(&RemoteControl::serverLoop, this);
    return true;
}

void RemoteControl::stop() {
    m_running = false;
    if (m_serverFd >= 0) { shutdown(m_serverFd, SHUT_RDWR); close(m_serverFd); m_serverFd = -1; }
    if (m_thread.joinable()) m_thread.join();
}

// ── Server loop ───────────────────────────────────────────────────────────────

void RemoteControl::serverLoop() {
    while (m_running) {
        int clientFd = accept(m_serverFd, nullptr, nullptr);
        if (clientFd < 0) break;
        handleClient(clientFd);
        close(clientFd);
    }
}

void RemoteControl::handleClient(int fd) {
    char buf[4096] = {};
    int  n = (int)recv(fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return;

    // Parse request line: "GET /path?query HTTP/1.x"
    std::string req(buf, (size_t)n);
    auto nl = req.find('\n');
    std::string line = (nl != std::string::npos) ? req.substr(0, nl) : req;

    std::string pathQuery;
    size_t s1 = line.find(' ');
    if (s1 != std::string::npos) {
        size_t s2 = line.find(' ', s1 + 1);
        pathQuery  = line.substr(s1 + 1, (s2 != std::string::npos) ? s2 - s1 - 1 : std::string::npos);
    }
    while (!pathQuery.empty() && (pathQuery.back() == '\r' || pathQuery.back() == ' '))
        pathQuery.pop_back();

    auto   qm    = pathQuery.find('?');
    std::string path  = (qm != std::string::npos) ? pathQuery.substr(0, qm)  : pathQuery;
    std::string query = (qm != std::string::npos) ? pathQuery.substr(qm + 1) : "";

    // ── Route ─────────────────────────────────────────────────────────────────
    int         statusCode  = 200;
    std::string contentType = "text/plain";
    std::string body;

    if (path == "/" || path.empty()) {
        body        = kHtmlPage;
        contentType = "text/html; charset=utf-8";

    } else if (path == "/set") {
        size_t pos = 0;
        while (pos < query.size()) {
            auto amp  = query.find('&', pos);
            std::string pair = query.substr(pos, (amp != std::string::npos) ? amp - pos : std::string::npos);
            auto eq   = pair.find('=');
            if (eq != std::string::npos)
                applyParam(urlDecode(pair.substr(0, eq)), urlDecode(pair.substr(eq + 1)));
            if (amp == std::string::npos) break;
            pos = amp + 1;
        }
        body = "OK";

    } else if (path == "/state") {
        char json[512];
        snprintf(json, sizeof(json),
            "{\"blend_m\":%.3f,\"blend_j\":%.3f,\"blend_mb\":%.3f,\"blend_e\":%.3f,"
            "\"julia_re\":%.4f,\"julia_im\":%.4f,\"power\":%.2f,\"max_iter\":%d,"
            "\"bailout\":%.2f,\"zoom\":%.4f,\"offset_x\":%.4f,\"offset_y\":%.4f}",
            m_blend.mandelbrot, m_blend.julia, m_blend.mandelbulb, m_blend.euclidean,
            m_engine.juliaC.x, m_engine.juliaC.y, m_engine.power, m_engine.maxIter,
            m_engine.bailout, m_engine.zoom, m_engine.offset.x, m_engine.offset.y);
        body        = json;
        contentType = "application/json";

    } else {
        statusCode = 404;
        body       = "Not found";
    }

    // ── Send response ─────────────────────────────────────────────────────────
    char header[256];
    int  hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n\r\n",
        statusCode, contentType.c_str(), body.size());

    ::send(fd, header, (size_t)hlen, 0);
    ::send(fd, body.c_str(), body.size(), 0);
}
