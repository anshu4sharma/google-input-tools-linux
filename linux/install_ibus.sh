#!/bin/bash
# Install and register Google Input Tools as a system-wide IBus input method on Linux
# Pure headless C++ engine and CLI - ZERO GUI overhead.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=== Registering Google Input Tools Headless IBus Engine ==="

# Check if running as root
if [ "$(id -u)" -ne 0 ]; then
    echo "Administrative privileges required to install to /usr/share/ibus/component."
    echo "Running with sudo..."
    sudo bash "$0" "$@"
    exit 0
fi

# Ensure dictionary exists
if [ ! -f "${SCRIPT_DIR}/assets/dict/hi_t13n.bin" ]; then
    echo "Compiling complete Hindi dictionary..."
    python3 "${SCRIPT_DIR}/build_dictionary.py"
fi

# Ensure binaries are built
make -C "${SCRIPT_DIR}" all strip

# Target system locations
BIN_DIR="/usr/bin"
LIB_DIR="/usr/lib"
LIBEXEC_DIR="/usr/libexec"
SHARE_DIR="/usr/share/google-input-tools"
DICT_DIR="${SHARE_DIR}/dict"
COMPONENT_DIR="/usr/share/ibus/component"
ICON_DIR="/usr/share/icons/hicolor/scalable/apps"
FONT_DIR="/usr/share/fonts/truetype/noto"

mkdir -p "${BIN_DIR}" "${LIB_DIR}" "${LIBEXEC_DIR}" "${SHARE_DIR}" "${DICT_DIR}" "${COMPONENT_DIR}" "${ICON_DIR}" "${FONT_DIR}"

echo "Installing native C++ headless binaries & complete dictionary..."
cp "${SCRIPT_DIR}/bin/git-cli" "${BIN_DIR}/git-cli"
chmod 755 "${BIN_DIR}/git-cli"

cp "${SCRIPT_DIR}/bin/ibus-engine-google-input-tools" "${LIBEXEC_DIR}/ibus-engine-google-input-tools"
chmod 755 "${LIBEXEC_DIR}/ibus-engine-google-input-tools"

cp "${SCRIPT_DIR}/lib/libgoogleinputtools.so" "${LIB_DIR}/libgoogleinputtools.so"
chmod 755 "${LIB_DIR}/libgoogleinputtools.so"

# Install complete 1.5M+ dictionary
cp "${SCRIPT_DIR}/assets/dict/hi_t13n.bin" "${DICT_DIR}/hi_t13n.bin"
chmod 644 "${DICT_DIR}/hi_t13n.bin"

cp "${SCRIPT_DIR}/ibus/google-input-tools.xml" "${COMPONENT_DIR}/google-input-tools.xml"
cp "${SCRIPT_DIR}/assets/google-input-tools.svg" "${ICON_DIR}/google-input-tools.svg"


# Refresh IBus registry and activate engine in desktop session
echo "Updating IBus cache and desktop session..."
TARGET_USER="${SUDO_USER:-$USER}"
USER_ID=$(id -u "${TARGET_USER}")
RUNTIME_DIR="/run/user/${USER_ID}"
DBUS_ADDR="unix:path=${RUNTIME_DIR}/bus"

if [ -d "${RUNTIME_DIR}" ]; then
    sudo -u "${TARGET_USER}" XDG_RUNTIME_DIR="${RUNTIME_DIR}" DBUS_SESSION_BUS_ADDRESS="${DBUS_ADDR}" ibus write-cache 2>/dev/null || true
    sudo -u "${TARGET_USER}" XDG_RUNTIME_DIR="${RUNTIME_DIR}" DBUS_SESSION_BUS_ADDRESS="${DBUS_ADDR}" ibus restart 2>/dev/null || true

    # Automatically add to GNOME input sources if GNOME is used
    CURRENT_SOURCES=$(sudo -u "${TARGET_USER}" XDG_RUNTIME_DIR="${RUNTIME_DIR}" DBUS_SESSION_BUS_ADDRESS="${DBUS_ADDR}" gsettings get org.gnome.desktop.input-sources sources 2>/dev/null || echo "")
    if [ -n "${CURRENT_SOURCES}" ] && [[ "${CURRENT_SOURCES}" != *"google-input-tools-hindi"* ]]; then
        NEW_SOURCES=$(echo "${CURRENT_SOURCES}" | sed "s/]/, ('ibus', 'google-input-tools-hindi')]/")
        sudo -u "${TARGET_USER}" XDG_RUNTIME_DIR="${RUNTIME_DIR}" DBUS_SESSION_BUS_ADDRESS="${DBUS_ADDR}" gsettings set org.gnome.desktop.input-sources sources "${NEW_SOURCES}" 2>/dev/null || true
    fi
else
    ibus write-cache 2>/dev/null || true
    ibus restart 2>/dev/null || true
fi

echo ""
echo "=========================================================="
echo "✓ Google Input Tools (Hindi) is ready and active!"
echo "=========================================================="
echo "1. Press [Super + Space] (Windows key + Space) to switch"
echo "   between English and Hindi (Google Input Tools)."
echo "2. Type phonetically (e.g., 'namaste') and press Space"
echo "   -> 'नमस्ते' will appear in any application!"
echo "3. Or run 'git-cli' in your terminal for interactive typing."
echo "4. Google Input Tools API autocomplete acts automatically"
echo "   as fallback for unknown or rare words."
echo "=========================================================="
