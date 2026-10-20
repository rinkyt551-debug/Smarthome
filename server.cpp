// ============================================================
//  SMART HOME  -  server.cpp  (the bridge between C++ and the web page)
//
//  Build:  g++ -std=c++11 -o server.exe server.cpp -lws2_32     (Windows)
//  Run:    server.exe      then open  http://127.0.0.1:8080
//
//  What it does, in 3 jobs:
//    1. Serves the dashboard files   (panel/index.html, style.css, script.js)
//    2. GET  /api/state  -> sends the contents of state.txt to the page
//    3. POST /api/state  -> checks what the page sent, then writes state.txt
//
//  state.txt is the SAME file that smarthome.cpp loads and saves, so the
//  console program and the dashboard always see the same settings.
//  The text format is one "key value" pair per line, e.g.  fan_speed 3
// ============================================================
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <cstdlib>
#include <cstring>
using namespace std;

// ---- Sockets: Windows and Linux/Mac use slightly different names ----
#ifdef _WIN32
  #include <winsock2.h>
  typedef SOCKET Socket;
  #define CLOSE_SOCKET closesocket
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <unistd.h>
  typedef int Socket;
  #define INVALID_SOCKET (-1)
  #define CLOSE_SOCKET close
#endif

const int PORT = 8080;
const string STATE_FILE = "state.txt";

// ------------------------------------------------------------
//  The rules. Every setting the dashboard may send, with its limits.
//  (These are the same limits as the C++ classes in smarthome.cpp.)
// ------------------------------------------------------------
struct Rule {
    const char *key;      // name in state.txt
    const char *label;    // name shown in error messages
    int low, high;        // allowed range
};

const Rule RULES[] = {
    { "light_on",    "Light on/off",     0, 1   },
    { "light_level", "Light brightness", 0, 100 },
    { "fan_on",      "Fan on/off",       0, 1   },
    { "fan_speed",   "Fan speed",        0, 5   },
    { "ac_on",       "AC on/off",        0, 1   },
    { "ac_temp",     "AC temperature",   16, 30 },
    { "ac_mode",     "AC mode",          0, 2   },
    { "ac_timer",    "AC timer",         0, 720 },
    { "tv_on",       "TV on/off",        0, 1   },
    { "tv_volume",   "TV volume",        0, 100 },
    { "tv_channel",  "TV channel",       1, 999 }
};
const int RULE_COUNT = sizeof(RULES) / sizeof(RULES[0]);

// Used when state.txt does not exist yet (same as the devices' first values).
const char *DEFAULT_STATE =
    "light_on 1\nlight_level 50\nfan_on 1\nfan_speed 0\n"
    "ac_on 1\nac_temp 24\nac_mode 1\nac_timer 0\n"
    "tv_on 1\ntv_volume 20\ntv_channel 1\n";

// ------------------------------------------------------------
//  File helpers
// ------------------------------------------------------------
string readFile(const string &path) {
    ifstream file(path.c_str(), ios::in | ios::binary);
    if (!file) return "";
    ostringstream text;
    text << file.rdbuf();
    return text.str();
}

// Checks the text sent by the dashboard. Returns the clean text to save.
// Throws a string (the error message) if anything is wrong.
string validate(const string &body) {
    // 1. Split the text into key -> number pairs.
    map<string, int> values;
    istringstream in(body);
    string key;
    while (in >> key) {
        int number;
        if (!(in >> number)) throw string(key + " must be a whole number");
        values[key] = number;
    }

    // 2. Check every rule: the key must exist and be inside its range.
    ostringstream clean;
    for (int i = 0; i < RULE_COUNT; i++) {
        const Rule &r = RULES[i];
        if (!values.count(r.key)) throw string("Missing setting: ") + r.key;
        int v = values[r.key];
        if (v < r.low || v > r.high) {
            ostringstream message;
            message << r.label << " must be from " << r.low << " to " << r.high;
            throw message.str();
        }
        clean << r.key << " " << v << "\n";
    }
    return clean.str();
}

void writeState(const string &text) {
    ofstream file(STATE_FILE.c_str(), ios::out | ios::trunc);
    if (!file) throw string("Cannot open state.txt for writing");
    file << text;
    file.close();
    if (!file) throw string("Failed while writing state.txt");
}

// ------------------------------------------------------------
//  HTTP helpers
// ------------------------------------------------------------
void sendResponse(Socket client, const string &status, const string &type, const string &body) {
    ostringstream head;
    head << "HTTP/1.1 " << status << "\r\n"
         << "Content-Type: " << type << "\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Cache-Control: no-store\r\n"
         << "Connection: close\r\n\r\n";
    string response = head.str() + body;
    send(client, response.c_str(), (int)response.size(), 0);
}

// Waits at most half a second for data, so a browser that opens a
// connection and sends nothing cannot freeze the server.
void setReadTimeout(Socket client) {
#ifdef _WIN32
    DWORD ms = 500;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&ms, sizeof(ms));
#else
    struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 500000;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

// Reads one complete request (headers + body). Returns false if nothing useful arrived.
bool readRequest(Socket client, string &request, size_t &bodyStart, size_t &bodyLength) {
    char buffer[2048];
    bodyStart = string::npos;
    bodyLength = 0;
    while (true) {
        int received = recv(client, buffer, sizeof(buffer), 0);
        if (received <= 0) return false;                    // closed, error or timeout
        request.append(buffer, received);
        if (request.size() > 16384) throw string("Request is too large");

        if (bodyStart == string::npos) {                    // are the headers complete yet?
            size_t end = request.find("\r\n\r\n");
            if (end == string::npos) continue;
            bodyStart = end + 4;
            size_t p = request.find("Content-Length:");
            if (p == string::npos) p = request.find("content-length:");
            if (p != string::npos && p < end) {
                int n = atoi(request.c_str() + p + 15);
                if (n < 0 || n > 8192) throw string("Bad Content-Length");
                bodyLength = (size_t)n;
            }
        }
        if (request.size() >= bodyStart + bodyLength) return true;   // whole body received
    }
}

// ------------------------------------------------------------
//  Handle one browser request
// ------------------------------------------------------------
void handleClient(Socket client) {
    setReadTimeout(client);
    string request;
    size_t bodyStart, bodyLength;
    if (!readRequest(client, request, bodyStart, bodyLength)) return;

    // First line looks like:  POST /api/state HTTP/1.1
    string method, path;
    istringstream firstLine(request.substr(0, request.find("\r\n")));
    firstLine >> method >> path;

    const string text = "text/plain; charset=utf-8";

    if (method == "GET" && path == "/api/state") {            // JOB 2: send the state
        string state = readFile(STATE_FILE);
        if (state.empty()) state = DEFAULT_STATE;
        sendResponse(client, "200 OK", text, state);
    }
    else if (method == "POST" && path == "/api/state") {      // JOB 3: save the state
        try {
            string clean = validate(request.substr(bodyStart, bodyLength));
            writeState(clean);
            sendResponse(client, "200 OK", text, "OK");
            cout << "Dashboard change saved to " << STATE_FILE << "\n";
        } catch (string message) {                            // bad value -> tell the page why
            sendResponse(client, "400 Bad Request", text, message);
            cout << "Rejected: " << message << "\n";
        }
    }
    else if (method == "GET") {                               // JOB 1: send a dashboard file
        string file = "";
        string type = "text/plain; charset=utf-8";
        if (path == "/" || path == "/index.html") { file = "panel/index.html"; type = "text/html; charset=utf-8"; }
        else if (path == "/style.css")            { file = "panel/style.css";  type = "text/css; charset=utf-8"; }
        else if (path == "/script.js")            { file = "panel/script.js";  type = "application/javascript; charset=utf-8"; }

        string body = file.empty() ? "" : readFile(file);     // only these 3 files can be served
        if (body.empty()) sendResponse(client, "404 Not Found", text, "Not found");
        else sendResponse(client, "200 OK", type, body);
    }
    else {
        sendResponse(client, "404 Not Found", text, "Not found");
    }
}

// ------------------------------------------------------------
//  main: open the port and answer requests one after another
// ------------------------------------------------------------
int main() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { cerr << "Could not start Windows networking.\n"; return 1; }
#endif
    Socket server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) { cerr << "Could not create the server socket.\n"; return 1; }

    sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);        // 127.0.0.1: only this computer can connect
    if (bind(server, (sockaddr*)&address, sizeof(address)) != 0 || listen(server, 10) != 0) {
        cerr << "Could not use port " << PORT << ". Close the other program using it and try again.\n";
        return 1;
    }

    cout << "Smart Home server is running at http://127.0.0.1:" << PORT << "\n"
         << "It shares " << STATE_FILE << " with smarthome.exe. Keep this window open. Ctrl+C stops it.\n";

    while (true) {
        Socket client = accept(server, NULL, NULL);
        if (client == INVALID_SOCKET) continue;
        try {
            handleClient(client);
        } catch (string message) {
            sendResponse(client, "400 Bad Request", "text/plain; charset=utf-8", message);
        } catch (...) {
            sendResponse(client, "500 Internal Server Error", "text/plain; charset=utf-8", "Server error");
        }
        CLOSE_SOCKET(client);
    }
}
