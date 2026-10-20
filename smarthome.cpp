// ============================================================
//  SMART HOME CONTROLLER  -  main program (console version)
//
//  Build:  g++ -std=c++11 -o smarthome.exe smarthome.cpp
//  Run:    smarthome.exe
//
//  The file goes from small things to big things:
//    Part 1  enum                      (named choices)
//    Part 2  simple functions          (inline, default argument, reference)
//    Part 3  templates                 (one code, many data types)
//    Part 4  type conversion           (Temperature)
//    Part 5  operator overloading      (Power)
//    Part 6  Device                    (base class: constructors, static, friend, virtual)
//    Part 7  Light, Fan, AC, Timer, SmartAC, TV   (inheritance)
//    Part 8  files                     (state.txt is shared with the web dashboard)
//    Part 9  main()                    (the menu)
//
//  HOW THE WEB DASHBOARD CONNECTS:
//  This program saves every change in "state.txt" and re-reads that file
//  before every menu. server.cpp reads/writes the SAME file for the web
//  page. So a change in the console shows up on the dashboard, and a
//  click on the dashboard shows up in the console.
// ============================================================
#include <iostream>   // cout, cin
#include <string>     // string
#include <fstream>    // ifstream, ofstream (files)
#include <map>        // map: used only to read the state file
using namespace std;

// ------------------------------------------------------------
//  PART 1: enum  (a list of named choices)
//  ECO = 0, NORMAL = 1, TURBO = 2  (the numbers are saved in state.txt)
// ------------------------------------------------------------
enum Mode { ECO, NORMAL, TURBO };

string modeName(Mode m) {
    if (m == ECO) return "ECO";
    if (m == NORMAL) return "NORMAL";
    return "TURBO";
}

// ------------------------------------------------------------
//  PART 2: simple functions
// ------------------------------------------------------------
inline float toFahrenheit(float c) {          // INLINE function: the code is pasted where it is called
    return c * 9 / 5 + 32;
}

float bill(float units, float rate = 6.5) {   // DEFAULT ARGUMENT: rate is 6.5 if we don't give one
    return units * rate;
}

void increase(int &value, int amount) {       // CALL BY REFERENCE: changes the original variable
    value = value + amount;
}

// ------------------------------------------------------------
//  PART 3: templates  (one code, works for many data types)
// ------------------------------------------------------------
template <class T>
T limit(T value, T low, T high) {             // FUNCTION TEMPLATE: keeps a value between low and high
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

template <class T>
class History {                               // CLASS TEMPLATE: remembers the last 5 values
    T items[5];
    int count;
public:
    History() { count = 0; }
    void add(T v) {
        if (count == 5) {                     // full: throw away the oldest
            for (int i = 1; i < 5; i++) items[i - 1] = items[i];
            count = 4;
        }
        items[count] = v;
        count++;
    }
    void show() {
        for (int i = 0; i < count; i++) cout << items[i] << "  ";
        cout << "\n";
    }
};

// ------------------------------------------------------------
//  PART 4: type conversion
// ------------------------------------------------------------
class Kelvin {
public:
    float value;
    Kelvin(float v = 0) { value = v; }
};

class Temperature {
    float celsius;
public:
    Temperature(float c = 24) { celsius = c; }                // basic type -> class   (Temperature t = 24;)
    operator float() { return toFahrenheit(celsius); }        // class -> basic type   (float f = t;)
    operator Kelvin() { return Kelvin(celsius + 273.15); }    // class -> another class (Kelvin k = t;)
    float getCelsius() { return celsius; }
};

// ------------------------------------------------------------
//  PART 5: operator overloading  (+ - * / ++ -- << >>)
// ------------------------------------------------------------
class Power {
    float watts;
public:
    Power(float w = 0) { watts = w; }
    Power operator+(Power p) { return Power(watts + p.watts); }
    Power operator-(Power p) { return Power(watts - p.watts); }
    Power operator*(Power p) { return Power(watts * p.watts); }
    Power operator/(Power p) {
        if (p.watts == 0) throw 0;                            // exception of type int
        return Power(watts / p.watts);
    }
    Power& operator++() { watts++; return *this; }            // ++a
    Power& operator--() { watts--; return *this; }            // --a
    friend ostream& operator<<(ostream &out, Power p) {       // cout << a
        out << p.watts << " W";
        return out;
    }
    friend istream& operator>>(istream &in, Power &p) {       // cin >> a
        in >> p.watts;
        return in;
    }
};

// ------------------------------------------------------------
//  PART 6: Device  (the base class of every appliance)
// ------------------------------------------------------------
class Device;                                  // "Device exists", it is written below

class Reader {                                 // its function read() will be a friend of Device
public:
    float read(Device &d);
};

class Device {
public:
    string name;                               // PUBLIC: everyone can use it
protected:
    bool on;                                   // PROTECTED: this class and its children
private:
    float watts;                               // PRIVATE: only this class (and friends)
    static int count;                          // STATIC: one shared number for all devices

public:
    Device() {                                 // DEFAULT constructor
        name = "Device"; on = true; watts = 10; count++;
        cout << "  [+] " << name << " switched on\n";
    }
    Device(string n, float w) {                // PARAMETERISED constructor
        name = n; on = true; watts = w; count++;
        cout << "  [+] " << name << " switched on\n";
    }
    Device(const Device &d) {                  // COPY constructor
        name = d.name + " (copy)"; on = d.on; watts = d.watts; count++;
        cout << "  [+] " << name << " created by copying\n";
    }
    virtual ~Device() {                        // VIRTUAL destructor
        count--;
        cout << "  [-] " << name << " switched off\n";
    }

    virtual void show() {                      // VIRTUAL function: children give their own version
        cout << name << " : " << (on ? "ON" : "OFF") << "\n";
    }
    void toggle() { on = !on; }
    void setOn(bool value) { on = value; }     // used when settings are loaded from the file
    bool isOn() { return on; }
    bool operator==(Device &other) {           // OPERATOR OVERLOADING: d1 == d2 (same state and same power usage)
        return on == other.on && watts == other.watts;
    }
    void needOn() {                            // called before every change
        if (!on) throw "Device is OFF";        // exception of type const char*
    }

    static int getCount() { return count; }    // STATIC function: call it as Device::getCount()

    friend void showStatus(Device &d);         // FRIEND function
    friend class Meter;                        // FRIEND class
    friend float Reader::read(Device &d);      // FRIEND member function
};
int Device::count = 0;                         // a static variable gets its first value outside the class

void showStatus(Device &d) {                   // prints the private power usage
    cout << d.name << " : " << (d.on ? "ON" : "OFF") << ", power usage " << d.watts << " W\n";
}

float Reader::read(Device &d) { return d.watts; }

class Meter {
public:
    float total(Device *list[], int n) {       // adds the power of every device that is ON
        float sum = 0;
        for (int i = 0; i < n; i++)
            if (list[i]->on) sum = sum + list[i]->watts;
        return sum;
    }
};

// ------------------------------------------------------------
//  PART 7: the appliances  (INHERITANCE)
//
//      Device -> Light
//      Device -> Fan
//      Device -> TV
//      Device -> AC -> Timer -> SmartAC      (multilevel)
//
//  The timer is a feature of the AC, so Timer is built on top of AC,
//  and SmartAC is built on top of Timer.
//
//  Every setter first calls needOn(), so a device that is OFF refuses
//  changes. Values that are out of range throw an exception (string).
// ------------------------------------------------------------
class Light : public Device {
    int brightness;                            // 0 to 100 %
public:
    Light() : Device("Light", 12) { brightness = 50; }
    Light(string n, float w, int b) : Device(n, w) { brightness = b; }
    ~Light() { cout << "  ~Light called\n"; }

    void set(int b) { needOn(); brightness = limit(b, 0, 100); }
    Light& operator++() {                      // ++light  makes it brighter
        needOn();
        increase(brightness, 10);
        brightness = limit(brightness, 0, 100);
        return *this;
    }
    Light& operator--() {                      // --light  makes it dimmer
        needOn();
        brightness = limit(brightness - 10, 0, 100);
        return *this;
    }
    int& ref() { return brightness; }          // RETURN BY REFERENCE: the caller can change brightness
    int get() { return brightness; }
    void show() {
        cout << name << " : " << (isOn() ? "ON" : "OFF") << ", brightness " << brightness << "%\n";
    }
};

class Fan : public Device {
    int speed;                                 // 0 to 5
public:
    Fan() : Device("Fan", 60) { speed = 0; }
    ~Fan() { cout << "  ~Fan called\n"; }

    void setSpeed(int s) {                     // FUNCTION OVERLOADING: same name ...
        needOn();
        if (s < 0 || s > 5) throw string("Fan speed must be 0 to 5");     // exception of type string
        speed = s;
    }
    void setSpeed(string word) {               // ... different parameter
        if (word == "low") setSpeed(1);
        else if (word == "medium") setSpeed(3);
        else if (word == "high") setSpeed(5);
        else throw string("Use low, medium or high");
    }
    Fan& operator++() { needOn(); speed = limit(speed + 1, 0, 5); return *this; }   // ++fan  faster
    Fan& operator--() { needOn(); speed = limit(speed - 1, 0, 5); return *this; }   // --fan  slower
    int getSpeed() { return speed; }
    void show() {
        cout << name << " : " << (isOn() ? "ON" : "OFF") << ", speed " << speed << "\n";
    }
};

class AC : public Device {
protected:
    Temperature target;                        // an object inside another object
    Mode mode;
public:
    AC(string n = "AC", float w = 1500) : Device(n, w) { mode = NORMAL; }
    ~AC() { cout << "  ~AC called\n"; }

    void setTemp(float c) {
        needOn();
        if (c < 16 || c > 30) throw string("AC temperature must be 16 to 30");
        target = Temperature(c);
    }
    void setMode(Mode m) { needOn(); mode = m; }
    float getTemp() { return target.getCelsius(); }
    int getMode() { return (int)mode; }
    void show() {
        cout << name << " : " << (isOn() ? "ON" : "OFF") << ", " << target.getCelsius()
             << " C (" << (float)target << " F), mode " << modeName(mode) << "\n";
    }
};

class Timer : public AC {                      // Timer is an AC with an extra sleep-timer feature
protected:
    int minutes;                               // 0 = timer off, maximum 720
public:
    Timer(string n = "Timer AC", float w = 1200) : AC(n, w) { minutes = 0; }
    ~Timer() { cout << "  ~Timer called\n"; }
    void setTimer(int m) {
        needOn();
        if (m < 0 || m > 720) throw string("AC timer must be 0 to 720 minutes");
        minutes = m;
    }
    int getTimer() { return minutes; }
};

class SmartAC : public Timer {                 // MULTILEVEL inheritance: Device -> AC -> Timer -> SmartAC
public:
    SmartAC(string n, float w) : Timer(n, w) { }
    ~SmartAC() { cout << "  ~SmartAC called\n"; }
    void show() {
        AC::show();                            // reuse the parent's show(), then add the timer line
        cout << "   timer: " << minutes << " min\n";
    }
};

class TV : public Device {
    int volume;                                // 0 to 100
    int channel;                               // 1 to 999
public:
    TV() : Device("TV", 100) { volume = 20; channel = 1; }
    ~TV() { cout << "  ~TV called\n"; }

    void setVolume(int v) { needOn(); volume = limit(v, 0, 100); }
    void setChannel(int c) {
        needOn();
        if (c < 1 || c > 999) throw string("Channel must be 1 to 999");
        channel = c;
    }
    int getVolume() { return volume; }
    int getChannel() { return channel; }
    void show() {
        cout << name << " : " << (isOn() ? "ON" : "OFF") << ", volume " << volume
             << ", channel " << channel << "\n";
    }
};

// ------------------------------------------------------------
//  PART 8: files
//
//  state.txt has one "key value" pair per line:
//      light_on 1          (1 = ON, 0 = OFF)
//      light_level 50
//      fan_on 1
//      fan_speed 0
//      ac_on 1
//      ac_temp 24
//      ac_mode 1           (0 ECO, 1 NORMAL, 2 TURBO)
//      ac_timer 0
//      tv_on 1
//      tv_volume 20
//      tv_channel 1
//  server.cpp reads and writes exactly this format for the dashboard.
// ------------------------------------------------------------
const string STATE_FILE = "state.txt";

void saveState(Light &l, Fan &f, SmartAC &a, TV &t) {
    ofstream file(STATE_FILE.c_str());         // write to a file
    if (!file) throw string("Cannot open state.txt for writing");
    file << "light_on " << l.isOn() << "\n"
         << "light_level " << l.get() << "\n"
         << "fan_on " << f.isOn() << "\n"
         << "fan_speed " << f.getSpeed() << "\n"
         << "ac_on " << a.isOn() << "\n"
         << "ac_temp " << (int)a.getTemp() << "\n"
         << "ac_mode " << a.getMode() << "\n"
         << "ac_timer " << a.getTimer() << "\n"
         << "tv_on " << t.isOn() << "\n"
         << "tv_volume " << t.getVolume() << "\n"
         << "tv_channel " << t.getChannel() << "\n";
    if (!file) throw string("Failed while writing state.txt");
}

// Returns the number saved under "key", or "fallback" if the key is missing.
int pick(map<string, int> &values, string key, int fallback) {
    return values.count(key) ? values[key] : fallback;
}

bool loadState(Light &l, Fan &f, SmartAC &a, TV &t) {
    ifstream file(STATE_FILE.c_str());         // read from a file
    map<string, int> values;                   // key -> number, e.g. "fan_speed" -> 3
    string key;
    int number;
    while (file >> key >> number) values[key] = number;
    if (values.empty()) return false;          // no file yet, or it is damaged

    // A device that is OFF refuses changes, so switch all ON first,
    // set the values, and then restore the saved ON/OFF position.
    l.setOn(true); f.setOn(true); a.setOn(true); t.setOn(true);
    l.set(pick(values, "light_level", 50));
    f.setSpeed(pick(values, "fan_speed", 0));
    a.setTemp(pick(values, "ac_temp", 24));
    a.setMode((Mode)pick(values, "ac_mode", 1));
    a.setTimer(pick(values, "ac_timer", 0));
    t.setVolume(pick(values, "tv_volume", 20));
    t.setChannel(pick(values, "tv_channel", 1));
    l.setOn(pick(values, "light_on", 1) == 1);
    f.setOn(pick(values, "fan_on", 1) == 1);
    a.setOn(pick(values, "ac_on", 1) == 1);
    t.setOn(pick(values, "tv_on", 1) == 1);
    return true;
}

void copyFile(string from, string to) {       // copy one file into another
    ifstream in(from.c_str());
    if (!in) throw string("Cannot open " + from);
    ofstream out(to.c_str());
    if (!out) throw string("Cannot open " + to + " for writing");
    char ch;
    while (in.get(ch)) out.put(ch);
    if (!out) throw string("Failed while writing " + to);
}

void lampsDemo() {
    Light lamps[3];                            // ARRAY OF OBJECTS: the default constructor runs 3 times
    for (int i = 0; i < 3; i++) {
        lamps[i].name = "Lamp " + to_string(i + 1);
        lamps[i].set(30 * (i + 1));
        lamps[i].show();
    }
}                                              // all 3 lamps are destroyed here

// ------------------------------------------------------------
//  PART 9: main  (the program starts here)
// ------------------------------------------------------------
void printMenu() {
    cout << "\n========== SMART HOME ==========\n"
         << " 1  Switch a device on/off\n"
         << " 2  Light: brighter / dimmer\n"
         << " 3  Fan: speed\n"
         << " 4  Smart AC: temperature, mode and timer\n"
         << " 5  TV: volume and channel\n"
         << " 6  Show all devices\n"
         << " 7  Power calculator  (operators)\n"
         << " 8  Device status     (friend, static, default argument)\n"
         << " 9  Temperature conversions\n"
         << "10  Copy a device     (copy constructor)\n"
         << "11  Three lamps       (array of objects)\n"
         << "12  Backup            (files)\n"
         << "13  Delete a SmartAC through a Device pointer (virtual destructor)\n"
         << "14  Compare two devices (operator ==)\n"
         << " 0  Exit\n"
         << "Choice: ";
}

int main() {
    cout << "Starting Smart Home...\n";

    // The four devices of the house. These are the same four the dashboard shows.
    Light light("Living Room Light", 12, 50);
    Fan fan;
    SmartAC ac("Smart AC", 1200);
    TV tv;
    fan.name = "Ceiling Fan";                  // name is public, so we can change it directly
    tv.name = "Living Room TV";

    const int DEVICES = 4;
    Device *home[DEVICES] = { &light, &fan, &ac, &tv };   // POINTERS: one array, four kinds of device

    History<float> temps;                      // class template used with float
    History<int> levels;                       // class template used with int

    try {
        if (loadState(light, fan, ac, tv)) cout << "Loaded your last saved settings.\n";
    } catch (string msg) {
        cout << "Saved file ignored: " << msg << "\n";
    }
    temps.add(ac.getTemp());
    levels.add(light.get());

    int choice;
    do {
        // Read state.txt again, so changes made on the web dashboard appear here.
        try { loadState(light, fan, ac, tv); } catch (...) { }

        printMenu();
        if (!(cin >> choice)) {                // the user typed letters instead of a number
            if (cin.eof()) break;
            cin.clear();
            cin.ignore(1000, '\n');
            choice = -1;
        }

        try {                                  // EXCEPTION HANDLING: try the action ...
            switch (choice) {
            case 1: {
                int d;
                cout << "1 Light, 2 Fan, 3 Smart AC, 4 TV: ";
                cin >> d;
                if (d < 1 || d > DEVICES) { cout << "No such device\n"; break; }
                home[d - 1]->toggle();
                home[d - 1]->show();           // each device shows itself in its own way (virtual)
                break;
            }
            case 2: {
                char c;
                cout << "+ brighter, - dimmer, f full brightness: ";
                cin >> c;
                if (c == '+') ++light;
                else if (c == '-') --light;
                else if (c == 'f') { light.needOn(); light.ref() = 100; }
                else { cout << "Unknown option\n"; break; }
                levels.add(light.get());
                light.show();
                break;
            }
            case 3: {
                string word;
                cout << "Speed (0 to 5, low / medium / high, + faster, - slower): ";
                cin >> word;
                if (word == "+") ++fan;                                            // operator++
                else if (word == "-") --fan;                                       // operator--
                else if (word[0] >= '0' && word[0] <= '9') fan.setSpeed(stoi(word));   // number version
                else fan.setSpeed(word);                                           // word version
                fan.show();
                break;
            }
            case 4: {
                int t, m, minutes;
                cout << "Temperature (16 to 30): ";
                cin >> t;
                ac.setTemp(t);
                temps.add(t);
                cout << "Mode (0 ECO, 1 NORMAL, 2 TURBO): ";
                cin >> m;
                if (m >= 0 && m <= 2) ac.setMode((Mode)m);
                else cout << "Mode not changed (use 0, 1 or 2)\n";
                cout << "Sleep timer in minutes (0 = off, max 720): ";
                cin >> minutes;
                ac.setTimer(minutes);
                ac.show();
                break;
            }
            case 5: {
                int v, c;
                cout << "Volume (0 to 100): ";
                cin >> v;
                cout << "Channel (1 to 999): ";
                cin >> c;
                tv.setVolume(v);
                tv.setChannel(c);
                tv.show();
                break;
            }
            case 6:
                for (int i = 0; i < DEVICES; i++) home[i]->show();
                cout << "Devices alive: " << Device::getCount() << "\n";
                cout << "Recent AC temperatures: ";
                temps.show();
                cout << "Recent light brightness: ";
                levels.show();
                break;
            case 7: {
                Power a, b;
                cout << "First power in watts: ";
                cin >> a;
                cout << "Second power in watts: ";
                cin >> b;
                cout << "a + b = " << a + b << "\n";
                cout << "a - b = " << a - b << "\n";
                cout << "a * b = " << a * b << "\n";
                cout << "++a   = " << ++a << "\n";
                cout << "--a   = " << --a << "\n";
                cout << "a / b = " << a / b << "\n";           // throws an int if b is 0
                break;
            }
            case 8: {
                Reader reader;
                Meter meter;
                for (int i = 0; i < DEVICES; i++) showStatus(*home[i]);        // friend function
                cout << "Reader sees the fan uses " << reader.read(fan) << " W\n";   // friend member function
                cout << "Meter says " << meter.total(home, DEVICES) << " W is being used now\n";   // friend class
                cout << "10 units cost Rs " << bill(10) << " (default rate)\n";
                cout << "10 units cost Rs " << bill(10, 8) << " (rate given)\n";
                break;
            }
            case 9: {
                Temperature t = 24;                // basic -> class
                float f = t;                       // class -> basic
                Kelvin k = t;                      // class -> class
                cout << t.getCelsius() << " C = " << f << " F = " << k.value << " K\n";
                break;
            }
            case 10: {
                Light copy = light;                // copy constructor
                copy.show();
                break;                             // the copy is destroyed here
            }
            case 11:
                lampsDemo();
                break;
            case 12:
                saveState(light, fan, ac, tv);
                copyFile(STATE_FILE, "backup.txt");
                cout << "Saved to state.txt and copied to backup.txt\n";
                break;
            case 13: {
                Device *p = new SmartAC("Temporary AC", 900);
                p->show();
                delete p;                          // virtual destructor: the whole chain runs
                break;
            }
            case 14: {
                int a, b;
                cout << "First device (1 Light, 2 Fan, 3 Smart AC, 4 TV): ";
                cin >> a;
                cout << "Second device (1 Light, 2 Fan, 3 Smart AC, 4 TV): ";
                cin >> b;
                if (a < 1 || a > DEVICES || b < 1 || b > DEVICES) { cout << "No such device\n"; break; }
                if (*home[a - 1] == *home[b - 1]) cout << "Same state and same power usage\n";   // operator==
                else cout << "They are different\n";
                break;
            }
            case 0:
                cout << "Closing the program...\n";
                break;
            default:
                cout << "Please choose a number from the menu\n";
            }

            // Options 1 to 5 change a device, so save: the dashboard will see it.
            if (choice >= 1 && choice <= 5) saveState(light, fan, ac, tv);
        }
        catch (const char *msg) {                  // ... and CATCH what goes wrong
            cout << "Error: " << msg << "\n";
        }
        catch (string msg) {
            cout << "Error: " << msg << "\n";
        }
        catch (int code) {
            cout << "Error: cannot divide by zero (code " << code << ")\n";
        }
        catch (...) {
            cout << "Error: something went wrong\n";
        }
    } while (choice != 0);

    return 0;                                      // every device is destroyed here, so each prints "switched off"
}
