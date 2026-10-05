# Google Input Tools for Linux (Headless Native C++ & Full Devanagari Dictionary)

A blazing-fast, 100% native Linux implementation of **Google Input Tools** for Hindi (हिन्दी) and Devanagari script, built in **C++17** (`-O3 -march=native`) with **IBus** integration and a versatile **CLI** typing assistant.

> **Zero GUI Overhead**: Designed strictly as a headless input method engine daemon, command-line tool, and C++ shared library for Linux. No GUI windows, no GTK notepad apps.

---

## 🌟 Key Features

1. **Complete Hindi Language & Devanagari Dictionary (1.5M+ Entries)**:
   - **Google Research Dakshina Dataset**: 53,000+ verified human-annotated Hindi transliterations.
   - **AI4Bharat Aksharantar Dataset**: 1.31+ million real-world Hindi phonetic transliteration pairs.
   - **Shreeshrii Hindi Hunspell**: 228,000+ comprehensive Hindi vocabulary words and inflections.
   - **Pre-compiled High-Performance Binary Trie (`hi_t13n.bin`)**: 73 MB packed dictionary for instantaneous in-memory lookup.

2. **Smart Online Fallback for Autocomplete**:
   - **Offline First**: All known words and phonetic rules are resolved locally in < 0.1 microseconds (< 100 nanoseconds) without network latency.
   - **API Autocomplete Fallback**: When an unknown or rare word is not found in the local dictionary, the engine automatically queries the Google Input Tools API (`https://inputtools.google.com/request?itc=hi-t-i0-und...`) as a fallback in a detached background thread.
   - **Self-Learning User History**: Any candidates resolved via the API fallback are immediately cached in `~/.cache/google-input-tools/user_history.txt` and dynamically inserted into the runtime trie, making subsequent lookups 100% offline.

3. **Pure Headless Linux Design**:
   - **IBus Daemon (`ibus-engine-google-input-tools`)**: Seamless system-wide typing in any application (Chrome, terminal, VS Code, LibreOffice, Slack, etc.) using native desktop candidate popup menus.
   - **CLI Typing Tool (`git-cli`)**: Interactive or batch terminal transliteration and candidate inspection.
   - **C++ Shared Library (`libgoogleinputtools.so`)**: Embeddable C/C++ API for any application.
   - **Lightweight Debian Package (`.deb`)**: Installs daemon, CLI, library, fonts, and full dictionary in one command.

---

## 🚀 Quick Start Options

### Option 1: System-Wide Typing (IBus Engine)

To type Hindi in any application across your Linux desktop:

1. Install and register with IBus:
   ```bash
   sudo ./linux/install_ibus.sh
   ```

2. Open **Settings → Keyboard → Input Sources** (or **Region & Language**).
3. Click **+ (Add)** → Search for **Hindi** → Choose **Hindi (Google Input Tools)**.
4. Press <kbd>Super</kbd> + <kbd>Space</kbd> anytime to toggle between English and Hindi!

### Option 2: Interactive CLI / Terminal Typing

Use `git-cli` for instant terminal transliteration without any GUI:

```bash
# Interactive mode
./linux/bin/git-cli

# Direct text transliteration
./linux/bin/git-cli namaste bharat aap kaise ho

# Inspect autocomplete candidates for a word
./linux/bin/git-cli --cands ankur

# Query fallback Google Input Tools API for rare words
./linux/bin/git-cli --fallback abhyanukulit
```

### Option 3: Build & Install Debian Package (`.deb`)

```bash
# Build the .deb package
./linux/build_deb.sh

# Install system-wide
sudo dpkg -i linux/dist/google-hindi.deb
ibus restart
```

---

## ⚡ Performance Benchmarks

Run the built-in benchmark tool:

```bash
./linux/bin/test_engine
```

**Results:**
- **Lookup Latency**: **~0.06 µs (60 nanoseconds)** per transliteration lookup.
- **Throughput**: **> 15,000,000 queries / second**.
- **Dictionary Capacity**: **1,537,831 unique phonetic keys**, **1,890,957 candidates**.

---

## 🛠️ Building & Packaging

```bash
# Compile all Linux targets (-O3 C++17)
make -C linux

# Re-compile dictionary from raw datasets
python3 linux/build_dictionary.py

# Build stripped Debian package (.deb)
bash linux/build_deb.sh
```

---

## 📄 License

Licensed under the Apache License 2.0.
