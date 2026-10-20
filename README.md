# Smart Home Controller

A small house-automation project in C++ with a web dashboard.
Four appliances (light, fan, smart AC, TV) can be controlled either from a
**console menu** or from a **web page**, and both always show the same settings.

---

## 1. The project in 3 sentences

Every appliance is a C++ class, and all of them come from one base class, `Device`.
`smarthome.cpp` is the real program: it holds the classes and a menu, and uses every C++ topic from class.
`server.cpp` is a tiny web server that lets the HTML/CSS/JS dashboard control the same devices through one shared file, `state.txt`.

---

## 2. What is in the folder

```
smarthome/
├── README.md        <- this file (the only document you need)
├── build.bat        <- compiles smarthome.cpp and server.cpp (run first)
├── run.bat          <- starts the console program or the web dashboard
├── smarthome.cpp    <- MAIN SOURCE: all classes + the console menu
├── server.cpp       <- the bridge: connects the dashboard to the C++ side
└── panel/           <- the web dashboard
    ├── index.html   <- page structure (the 4 device cards)
    ├── style.css    <- colours, layout, phone layout
    └── script.js    <- button logic + talking to server.cpp
```

These files appear **after you run** :

| File | Made by | What it is |
|---|---|---|
| `smarthome.exe`, `server.exe` | `build.bat` | the compiled programs |
| `state.txt` | console or server | the current settings of all devices (shared) |
| `backup.txt` | console menu option 12 | a copy of `state.txt` |

---

## 3. How to run (Windows)

**You need:** `g++` (MinGW / MSYS2 / WinLibs). Check with `g++ --version` in a terminal.

1. Extract the ZIP into a normal folder (do not run from inside the ZIP).
2. Double-click **`build.bat`** (or type `.\build.bat` in the VS Code terminal).
   It compiles both programs. Wait for *Build OK*. If it says *Build failed*, read the compiler message above it.
   Run it again whenever you change a `.cpp` file.
3. Double-click **`run.bat`** (or type `.\run.bat` in the VS Code terminal) and choose what to start:
   - **1 → Console program.** A menu appears. Type a number and press Enter. `0` exits.
   - **2 → Web dashboard.** Your browser opens `http://127.0.0.1:8080`. **Keep the black window open**; it is the server. Press `Ctrl+C` in it to stop.

**Important**

- Always open the dashboard through `http://127.0.0.1:8080`. Double-clicking `panel/index.html` will **not** work, because the page needs the server.
- You can use the console and the dashboard **at the same time** (open `run.bat` twice). Change something in one and it appears in the other: the console re-reads the file before every menu, and the web page asks the server every 3 seconds.
- If the browser says *Cannot reach the C++ server*, the server window is closed. Start it again with option 2.
- If it says *Could not use port 8080*, another program is using that port. Close it and try again.

To build by hand instead of `build.bat`:

```
g++ -std=c++11 -o smarthome.exe smarthome.cpp
g++ -std=c++11 -o server.exe server.cpp -lws2_32
```

---

## 4. How the parts fit together

```
 ┌────────────────────┐      HTTP (text)       ┌──────────────┐
 │  Browser           │  GET/POST /api/state   │  server.exe  │
 │  index.html        │ ─────────────────────► │  (server.cpp)│
 │  style.css         │ ◄───────────────────── │  checks the  │
 │  script.js         │      "OK" or error     │  values      │
 └────────────────────┘                        └──────┬───────┘
                                                      │ reads / writes
                                                      ▼
                                               ┌─────────────┐
                                               │  state.txt  │   <- ONE shared file
                                               └──────▲──────┘
                                                      │ reads / writes
                                               ┌──────┴───────┐
                                               │smarthome.exe │
                                               │(smarthome.cpp│
                                               │ console menu)│
                                               └──────────────┘
```

Nothing is sent between the two C++ programs directly. They only share **`state.txt`**.
That is why the design stays simple: each program has one job.

### The `state.txt` format

One `key value` pair per line. `1` = ON, `0` = OFF.

```
light_on 1          light_level 50        (0 to 100)
fan_on 1            fan_speed 0           (0 to 5)
ac_on 1             ac_temp 24            (16 to 30)
ac_mode 1           ac_timer 0            (mode: 0 ECO, 1 NORMAL, 2 TURBO; timer: 0 to 720 minutes)
tv_on 1             tv_volume 20          (0 to 100)
tv_channel 1                              (1 to 999)
```

The dashboard sends and receives exactly this text, so there is no JSON to parse.

---

## 5. `smarthome.cpp` - the main program, part by part

The file reads from small things to big things. Each part has a banner comment (`PART 1`, `PART 2`, ...).

| Part | Contains | What to say |
|---|---|---|
| 1 | `enum Mode` | A list of named choices for the AC: ECO, NORMAL, TURBO. |
| 2 | `toFahrenheit`, `bill`, `increase` | Normal functions. `toFahrenheit` is **inline**, `bill` has a **default argument**, `increase` is **call by reference**. |
| 3 | `limit`, `History` | **Templates.** One piece of code works for int, float and more. `limit` keeps a number between a low and a high value. |
| 4 | `Temperature`, `Kelvin` | **Type conversion**: number to class, class to number, class to class. |
| 5 | `Power` | **Operator overloading**: `a + b`, `++a`, `cout << a`, `cin >> a` work on our own class. |
| 6 | `Device`, `Reader`, `Meter` | The **base class**: 3 kinds of constructor, a virtual destructor, a static counter, friends, access specifiers. |
| 7 | `Light`, `Fan`, `AC`, `Timer`, `SmartAC`, `TV` | **Inheritance.** Each appliance is a child of `Device`. `Timer` derives from `AC` and `SmartAC` from `Timer` (multilevel). |
| 8 | `saveState`, `loadState`, `copyFile` | **File handling.** Settings go to `state.txt`; `copyFile` makes `backup.txt`. |
| 9 | `main` | Creates the 4 devices, shows the menu, catches errors with `try / catch`. |

### The inheritance tree

```
Device ─┬─ Light
        ├─ Fan
        ├─ TV
        └─ AC ── Timer ── SmartAC
```

The timer is a feature of the AC, not of every appliance, so `Timer` is built on `AC`, and `SmartAC` on `Timer`.

### How a device refuses changes when it is OFF

Every setter starts with `needOn()`. If the device is OFF it throws `"Device is OFF"`, and `main` catches it and prints `Error: ...`.
Values outside the allowed range throw a `string` the same way (for example `"Fan speed must be 0 to 5"`).

### The console menu

| Option | What it does | Concept shown |
|---|---|---|
| 1 | Switch a device on/off | virtual `show()` through a `Device*` |
| 2 | Light brighter / dimmer / full | `operator++`, `operator--`, return by reference |
| 3 | Fan speed (0-5, low/medium/high, `+` / `-`) | function overloading, `operator++` / `operator--` on `Fan` |
| 4 | Smart AC: temperature, mode, timer | exceptions for bad values |
| 5 | TV volume and channel | |
| 6 | Show all devices | static counter, class template `History` |
| 7 | Power calculator | operator overloading, `throw 0` on divide by zero |
| 8 | Device status (power usage of every device) and bill | friend function `showStatus`, friend class / member function, default argument |
| 9 | Temperature conversions | type conversion |
| 10 | Copy a device | copy constructor |
| 11 | Three lamps | array of objects |
| 12 | Backup | `state.txt` copied to `backup.txt` |
| 13 | Delete a SmartAC through a `Device*` | virtual destructor chain |
| 14 | Compare two devices | `operator==` |
| 0 | Exit | |

Options 1-5 **save automatically** to `state.txt`, which is how the dashboard sees the change.

### Where to find each concept (Ctrl+F in VS Code)

| Concept | Search for |
|---|---|
| Function overloading | `void setSpeed` (two of them in `Fan`) |
| Inline function | `inline float toFahrenheit` |
| Default argument | `float bill(float units, float rate = 6.5)` |
| Call by reference | `void increase(int &value` |
| Return by reference | `int& ref()` |
| Access specifiers | `public:`, `protected:`, `private:` in `Device` |
| Array of objects | `Light lamps[3]` |
| Enum | `enum Mode` |
| Friend function / class / member function | `friend void showStatus`, `friend class Meter`, `friend float Reader::read` |
| Static member | `static int count`, `static int getCount` |
| Constructors | `Device()`, `Device(string n, float w)`, `Device(const Device &d)` |
| Destructor | `virtual ~Device` |
| Operator overloading | `Power operator+`, `Light& operator++`, `Fan& operator++`, `bool operator==` |
| `<<` and `>>` | `friend ostream& operator<<`, `friend istream& operator>>` |
| Type conversion | `Temperature(float c`, `operator float()`, `operator Kelvin()` |
| Single and multilevel inheritance | `class Light : public Device`, `class Timer : public AC`, `class SmartAC : public Timer` |
| Virtual function | `virtual void show()` |
| Templates | `T limit(`, `class History` |
| File handling | `void saveState`, `bool loadState`, `void copyFile` |
| Exception handling | `throw` in `needOn`, `setSpeed`, `Power::operator/` and the `catch` blocks in `main` |
| Pointers | `Device *home[DEVICES]` |
| STL | `map<string, int>` in `loadState` (only used to read the file) |

---

## 6. `server.cpp` - the bridge

It is about 250 lines and does **three jobs**:

1. **Serves the dashboard.** `GET /`, `/style.css`, `/script.js` send the files from `panel/`. Only these three files can be requested.
2. **`GET /api/state`** sends the contents of `state.txt` (or the default settings if the file does not exist yet).
3. **`POST /api/state`** receives the settings from the page, **checks every value against the table `RULES`**, and only then writes `state.txt`.
   If something is wrong (for example brightness 700) it answers `400` with a message such as *Light brightness must be from 0 to 100*, and the page shows that message in red.

Good to know:

- It listens on `127.0.0.1` only, so **only your own computer** can connect.
- It handles one request at a time and gives up on silent connections after half a second, so it cannot get stuck.
- The same code compiles on Windows (Winsock) and Linux/Mac; the `#ifdef _WIN32` lines handle the small differences.

---

## 7. The dashboard (`panel/`)

| File | Role |
|---|---|
| `index.html` | The structure: top bar, message bar, 4 device cards. Each element that the script changes has an `id="..."`. Comments mark each card. |
| `style.css` | The look. Colours are defined once at the top (`:root`) and reused. One rule per line, with section comments. Has a phone layout at the bottom. |
| `script.js` | The logic, in 5 numbered sections: the data (`home`), talking to the server, drawing the page, the buttons, and start-up. |

How a click works in `script.js`:

```
click "+ Brighter"  ->  changeLight(10)   (refuses if the light is OFF)
                    ->  render()          (redraw numbers and bars)
                    ->  saveToServer()    (POST the whole "home" as text to server.cpp)
```

The dashboard uses the same limits as the C++ classes (light 0-100, fan 0-5, AC 16-30, timer 0-720 in steps of 15, TV volume 0-100 in steps of 5, channel 1-999).
Even if the page is changed by mistake, **the server checks everything again**, so `state.txt` can never contain an invalid value.

---

## 8. Adding something new (e.g. a new device or setting)

Because the work is split into small parts, a new setting touches the same four places, in this order:

1. **`smarthome.cpp`** - add the field and its getter/setter to the device class (or a new class that inherits `Device`). Add one line to `saveState` and one `pick(...)` line to `loadState`.
2. **`server.cpp`** - add one row to the `RULES` table (key, label, low, high) and one line to `DEFAULT_STATE`.
3. **`panel/script.js`** - add one row to the `FIELDS` list, and a function plus `connect(...)` for the button.
4. **`panel/index.html`** - add the buttons / numbers with the `id`s the script uses.

The key name (for example `fan_speed`) must be spelled **identically** in all four places.
---

## 10. Troubleshooting

| Problem | Fix |
|---|---|
| `g++ is not recognized` | Install MinGW/MSYS2/WinLibs and add its `bin` folder to PATH. Reopen the terminal. |
| `Build failed` / `.exe not found` | Read the first error above it. Check that no old `smarthome.exe` / `server.exe` is still running (close it). |
| Page says *Cannot reach the C++ server* | The server window is closed. Run `run.bat` → option 2. |
| `Could not use port 8080` | Another program uses the port. Close it, or change `PORT` at the top of `server.cpp` and the address in `run.bat`. |
| Dashboard shows old values | Wait 3 seconds (it refreshes itself), or reload the page. |
| Everything reset to defaults | Delete `state.txt`; it is recreated on the next change. |
