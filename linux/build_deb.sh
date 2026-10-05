#!/bin/bash
# Builds a pure native C++ Debian (.deb) package for Google Input Tools (Hindi) for Linux.
# ZERO GUI overhead - pure headless IBus input method engine daemon & CLI.
# Bundles complete Hindi & Devanagari dictionary (1.5M+ entries), full Unicode character set,
# and Google Input Tools API autocomplete fallback.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIST_DIR="${SCRIPT_DIR}/dist"
PKG_NAME="google-input-tools-hindi"
VERSION="2.0.0"
ARCH="amd64"
BUILD_DIR="${SCRIPT_DIR}/build_deb_tmp"

# Ensure complete dictionary is compiled
if [ ! -f "${SCRIPT_DIR}/assets/dict/hi_t13n.bin" ]; then
    echo "Compiling complete Hindi dictionary..."
    python3 "${SCRIPT_DIR}/build_dictionary.py"
fi

# Ensure native C++ binaries are compiled with -O3 and stripped
make -C "${SCRIPT_DIR}" strip

echo "=== Building Pure Headless Debian Package: ${PKG_NAME}_${VERSION}_${ARCH}.deb ==="

rm -rf "${BUILD_DIR}"
mkdir -p "${DIST_DIR}"
mkdir -p "${BUILD_DIR}/DEBIAN"
mkdir -p "${BUILD_DIR}/usr/bin"
mkdir -p "${BUILD_DIR}/usr/lib"
mkdir -p "${BUILD_DIR}/usr/libexec"
mkdir -p "${BUILD_DIR}/usr/share/google-input-tools/dict"
mkdir -p "${BUILD_DIR}/usr/share/ibus/component"
mkdir -p "${BUILD_DIR}/usr/share/icons/hicolor/scalable/apps"

# 1. Create DEBIAN/control - Standard system dependencies only (NO GTK, NO font conflict)
cat << 'EOF' > "${BUILD_DIR}/DEBIAN/control"
Package: google-input-tools-hindi
Version: 2.0.0
Section: utils
Priority: optional
Architecture: amd64
Depends: libibus-1.0-5, libc6, curl
Recommends: fonts-noto-core | fonts-deva
Maintainer: Anshu Sharma <anshusharma6327@gmail.com>
Description: Google Input Tools for Hindi (Headless Native Linux IME Engine)
 Complete Devanagari Hindi typing system with 1.5+ million vocabulary dictionary,
 full Unicode character set, IBus integration, CLI typing assistant, and
 Google Input Tools API autocomplete fallback.
 100% headless with zero GUI overhead and ultra-fast sub-microsecond response time.
EOF

# 2. Create DEBIAN/postinst - Auto registers D-Bus / IBus
cat << 'EOF' > "${BUILD_DIR}/DEBIAN/postinst"
#!/bin/sh
set -e

# Auto register IBus component and restart daemon
if command -v ibus >/dev/null 2>&1; then
    ibus write-cache --system 2>/dev/null || true
    ibus write-cache 2>/dev/null || true
    TARGET_USER="${SUDO_USER:-$USER}"
    if [ -n "${TARGET_USER}" ] && [ "${TARGET_USER}" != "root" ]; then
        USER_ID=$(id -u "${TARGET_USER}" 2>/dev/null || true)
        RUNTIME_DIR="/run/user/${USER_ID}"
        DBUS_ADDR="unix:path=${RUNTIME_DIR}/bus"
        if [ -d "${RUNTIME_DIR}" ]; then
            sudo -u "${TARGET_USER}" XDG_RUNTIME_DIR="${RUNTIME_DIR}" DBUS_SESSION_BUS_ADDRESS="${DBUS_ADDR}" ibus write-cache 2>/dev/null || true
            sudo -u "${TARGET_USER}" XDG_RUNTIME_DIR="${RUNTIME_DIR}" DBUS_SESSION_BUS_ADDRESS="${DBUS_ADDR}" ibus restart 2>/dev/null || true
        fi
    fi
fi
exit 0
EOF
chmod 755 "${BUILD_DIR}/DEBIAN/postinst"

# 3. Create DEBIAN/prerm
cat << 'EOF' > "${BUILD_DIR}/DEBIAN/prerm"
#!/bin/sh
set -e
if command -v ibus >/dev/null 2>&1; then
    ibus write-cache --system 2>/dev/null || true
    ibus write-cache 2>/dev/null || true
fi
exit 0
EOF
chmod 755 "${BUILD_DIR}/DEBIAN/prerm"

# 4. Copy Pure Native Headless Binaries, Libraries & Dictionary (NO GUI, NO font collision)
echo "Copying pure native IBus engine daemon, CLI, shared library, and dictionary..."
cp "${SCRIPT_DIR}/bin/ibus-engine-google-input-tools" "${BUILD_DIR}/usr/libexec/ibus-engine-google-input-tools"
chmod 755 "${BUILD_DIR}/usr/libexec/ibus-engine-google-input-tools"

cp "${SCRIPT_DIR}/bin/git-cli" "${BUILD_DIR}/usr/bin/git-cli"
chmod 755 "${BUILD_DIR}/usr/bin/git-cli"

cp "${SCRIPT_DIR}/lib/libgoogleinputtools.so" "${BUILD_DIR}/usr/lib/libgoogleinputtools.so"
chmod 755 "${BUILD_DIR}/usr/lib/libgoogleinputtools.so"

# Copy Complete 70MB Hindi Dictionary
cp "${SCRIPT_DIR}/assets/dict/hi_t13n.bin" "${BUILD_DIR}/usr/share/google-input-tools/dict/hi_t13n.bin"
chmod 644 "${BUILD_DIR}/usr/share/google-input-tools/dict/hi_t13n.bin"

cp "${SCRIPT_DIR}/ibus/google-input-tools.xml" "${BUILD_DIR}/usr/share/ibus/component/google-input-tools.xml"
cp "${SCRIPT_DIR}/assets/google-input-tools.svg" "${BUILD_DIR}/usr/share/icons/hicolor/scalable/apps/google-input-tools.svg"

# 5. Build .deb package
OUTPUT_DEB="${DIST_DIR}/${PKG_NAME}_${VERSION}_${ARCH}.deb"
echo "Packaging into ${OUTPUT_DEB}..."
dpkg-deb --build --root-owner-group "${BUILD_DIR}" "${OUTPUT_DEB}"

# Create convenient short symlink
ln -sf "${OUTPUT_DEB}" "${DIST_DIR}/google-hindi.deb"

# Cleanup temporary build directory
rm -rf "${BUILD_DIR}"

echo "=========================================================="
echo "Successfully built Pure Headless Linux Debian Package:"
echo "-> ${OUTPUT_DEB}"
echo "-> ${DIST_DIR}/google-hindi.deb"
echo "Size: $(du -h "${OUTPUT_DEB}" | cut -f1)"
echo ""
echo "To install on Ubuntu/Debian:"
echo "  sudo dpkg -i ${DIST_DIR}/google-hindi.deb && ibus restart"
echo "=========================================================="
