#!/usr/bin/env bash
set -e

# ==============================================================================
# Google Input Tools for Linux - 1-Click Installer
# ==============================================================================

echo ""
echo "=============================================================="
echo "  🇮🇳 Google Input Tools for Linux (Hindi Phonetic IME)"
echo "=============================================================="
echo ""

RELEASE_URL="https://github.com/anshu4sharma/google-input-tools-linux/releases/download/v2.0.0/google-hindi.deb"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" 2>/dev/null && pwd || echo "")"
LOCAL_DEB="${SCRIPT_DIR}/linux/dist/google-hindi.deb"
TMP_DEB="/tmp/google-hindi.deb"

# 1. Acquire sudo privileges with friendly instructions
if [ "$(id -u)" -ne 0 ]; then
    echo "🔒 Administrator permission (sudo) is required to install."
    echo "   👉 Type your password and press Enter."
    echo "   💡 (Note: In Linux, letters do NOT show as you type password — this is normal!)"
    echo ""
    sudo -v || { echo "❌ Password authentication failed. Aborting."; exit 1; }
    SUDO_CMD="sudo"
else
    SUDO_CMD=""
fi

# 2. Locate or download the .deb package
TARGET_DEB=""
if [ -n "${SCRIPT_DIR}" ] && [ -f "${LOCAL_DEB}" ]; then
    echo "📦 Using local package: ${LOCAL_DEB}"
    TARGET_DEB="${LOCAL_DEB}"
else
    echo "⬇️  Downloading official release package..."
    if command -v curl &>/dev/null; then
        curl -fSL "${RELEASE_URL}" -o "${TMP_DEB}" --progress-bar
    elif command -v wget &>/dev/null; then
        wget -q --show-progress "${RELEASE_URL}" -O "${TMP_DEB}"
    else
        echo "❌ Neither curl nor wget found. Please install curl or wget."
        exit 1
    fi
    TARGET_DEB="${TMP_DEB}"
fi

# 3. Install the package
echo ""
echo "⚙️  Installing Google Input Tools engine..."
${SUDO_CMD} dpkg -i "${TARGET_DEB}" || {
    echo "⚠️  Resolving missing dependencies..."
    ${SUDO_CMD} apt-get install -f -y
}

# 4. Clean up temporary download if used
[ -f "${TMP_DEB}" ] && rm -f "${TMP_DEB}"

# 5. Refresh IBus cache and restart daemon
echo "🔄 Refreshing IBus cache & restarting daemon..."
ibus write-cache 2>/dev/null || true
ibus restart 2>/dev/null || true

# If running under sudo, also restart for original desktop user
if [ -n "${SUDO_USER}" ] && [ "${SUDO_USER}" != "root" ]; then
    su - "${SUDO_USER}" -c "ibus write-cache && ibus restart" 2>/dev/null || true
fi

# 6. Automatically register in GNOME / Ubuntu Input Sources if available
if command -v gsettings &>/dev/null; then
    CURRENT_SOURCES=$(gsettings get org.gnome.desktop.input-sources sources 2>/dev/null || echo "")
    if [[ "$CURRENT_SOURCES" != *"google-input-tools-hindi"* && "$CURRENT_SOURCES" == *"["* ]]; then
        NEW_SOURCES="${CURRENT_SOURCES%]*}, ('ibus', 'google-input-tools-hindi')]"
        gsettings set org.gnome.desktop.input-sources sources "$NEW_SOURCES" 2>/dev/null || true
        echo "✅ Automatically added 'Hindi (Google Input Tools)' to keyboard input sources!"
    fi
fi

echo ""
echo "=============================================================="
echo "  🎉 Installation Completed Successfully!"
echo "=============================================================="
echo ""
echo "  ✨ To toggle between English and Hindi:"
echo "     👉 Press [ Super (Windows Key) + Space ]"
echo ""
echo "  ⌨️  Try typing in any app (Chrome, VS Code, Notes, LibreOffice):"
echo "     namaste + Space   ➡️   नमस्ते"
echo "     bharat + Space    ➡️   भारत"
echo ""
echo "=============================================================="
echo ""
