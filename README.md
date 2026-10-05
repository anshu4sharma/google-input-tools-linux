# Google Input Tools for Linux (Hindi Phonetic IME)

Fast, native phonetic Hindi typing for Ubuntu and Linux. Type in Roman English letters and get Devanagari Hindi text instantly everywhere (Chrome, VS Code, LibreOffice, Terminal, WhatsApp, etc.).

> **Example**: Type `namaste` and press <kbd>Space</kbd> &rarr; **नमस्ते**  
> **Example**: Type `mera bharat mahan` &rarr; **मेरा भारत महान**

---

## ⚡ Quick 1-Command Installation

### Option 1: 1-Line Automatic Installer (Recommended)

Open your terminal and paste this single command:

```bash
curl -fsSL https://raw.githubusercontent.com/anshu4sharma/google-input-tools-linux/master/install.sh | bash
```

*Or with `wget`:*
```bash
wget -qO- https://raw.githubusercontent.com/anshu4sharma/google-input-tools-linux/master/install.sh | bash
```

> [!TIP]
> **💡 When prompted for your password (`[sudo] password`):**  
> Type your computer login password and press <kbd>Enter</kbd>.  
> **Note**: In Linux terminals, **no letters or asterisks (`***`) appear on screen** while typing passwords for security — just type your password and hit <kbd>Enter</kbd>!

---

### Option 2: Direct Package Download & Install

If you prefer installing the `.deb` package manually, run this single chained command:

```bash
wget -O google-hindi.deb https://github.com/anshu4sharma/google-input-tools-linux/releases/download/v2.0.0/google-hindi.deb && sudo dpkg -i google-hindi.deb && ibus write-cache && ibus restart
```

*(Using `&&` ensures each command waits for your password to be entered properly before proceeding).*

---

### Option 3: Graphical (.deb) Installation

1. Download [**`google-hindi.deb`** (16.5 MB)](https://github.com/anshu4sharma/google-input-tools-linux/releases/download/v2.0.0/google-hindi.deb).
2. Double-click the downloaded `.deb` file to open and install via **Ubuntu Software / App Center**.
3. Open a terminal and run `ibus write-cache && ibus restart`.

---

## ⚙️ How to Add the Keyboard Layout in Linux

### Step 1: Open Keyboard Settings
1. Open Ubuntu / Linux **Settings**.
2. Click **Keyboard** (or **Region & Language** on older Ubuntu versions).
3. Under **Input Sources**, click the **+ (Add)** button.

### Step 2: Add Hindi (Google Input Tools)
1. In the search box, type `Hindi`.
2. Select **Hindi (Google Input Tools)**.
3. Click **Add**.

### Step 3: Switch Between English & Hindi
* Press <kbd>Super</kbd> + <kbd>Space</kbd> (Windows Key + Space) to switch between English and Hindi anytime.
* You will see the language indicator in your top panel switch to **hi (Google Input Tools)**.

---

## 🔄 IBus Cache Clear & Restart (If Not Showing in Settings)

If **Hindi (Google Input Tools)** does not show up in the settings list immediately after installation, clear the IBus cache and restart the daemon:

```bash
ibus write-cache
ibus restart
```

Then reopen **Settings &rarr; Keyboard &rarr; Input Sources &rarr; +** and it will be visible.

---

## ⌨️ How to Type & Phonetic Shortcuts

Type phonetically as the word sounds in English:

| You Type | Output | Note |
| :--- | :--- | :--- |
| `namaste` + <kbd>Space</kbd> | **नमस्ते** | Automatic top suggestion commit |
| `bharat` + <kbd>Space</kbd> | **भारत** | Proper noun |
| `aap kaise ho` | **आप कैसे हो** | Full sentences |
| `kya` / `kyon` | **क्या** / **क्यों** | Half-letters & conjuncts |
| `dost` / `shanti` | **दोस्त** / **शांति** | Common words |
| `dhanyawad` / `shukriya` | **धन्यवाद** / **शुक्रिया** | Greetings & expressions |
| <kbd>Backspace</kbd> (after Space) | Reverts to `namaste` | **Smart Undo** to English spelling |
| <kbd>Ctrl</kbd> + <kbd>Backspace</kbd> | Deletes whole word | **Quick Word Delete** |
| <kbd>Shift</kbd> + <kbd>Backspace</kbd> | Deletes letter | **Clean Character Delete** |
| <kbd>1</kbd> – <kbd>5</kbd> | Selects candidate | Choose from popup menu |

---

## 💻 Terminal CLI Typing Assistant

You can also type and transliterate directly in your terminal with zero GUI:

```bash
# Launch interactive typing terminal:
git-cli

# Direct command-line transliteration:
git-cli namaste dosto aap sabhi ka swagat hai
# Output: नमस्ते दोस्तो आप सभी का स्वागत है

# Inspect all candidate suggestions:
git-cli --cands hindi
```

---

## ❓ Frequently Asked Questions & Troubleshooting

### Q: Does it require active internet to type?
**No.** It bundles an offline dictionary with **1.5+ million words** compiled from Google Research Dakshina and AI4Bharat corpora. Typing is instant (< 0.1 µs) without internet. The online Google Input Tools API is called strictly as an automatic fallback for rare words or novel terms.

### Q: Why isn't the layout switching with Super + Space?
1. Check that IBus is running: `ibus status` or `ibus restart`.
2. Check your active input sources: `gsettings get org.gnome.desktop.input-sources sources`.
3. To add it directly via terminal:
   ```bash
   gsettings set org.gnome.desktop.input-sources sources "[('xkb', 'us'), ('ibus', 'google-input-tools-hindi')]"
   ```

### Q: How do I uninstall?
```bash
sudo dpkg -r google-input-tools-hindi
ibus restart
```

---

## 📄 License
Licensed under the Apache License 2.0.
