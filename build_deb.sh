#!/usr/bin/env bash
set -euo pipefail

PKG_NAME="trsync"
PKG_VERSION="1.3.1"
PKG_MAINTAINER="seb3773"
PKG_SECTION="utils"
PKG_PRIORITY="optional"

SRC_ROOT="$(cd "$(dirname "$0")" && pwd)"
ARCH="$(dpkg --print-architecture)"
BUILD_DIR="$SRC_ROOT/build"
PKGROOT="$BUILD_DIR/pkgroot"
PKGTMP="$BUILD_DIR/pkgtmp"

need_cmd() {
	command -v "$1" >/dev/null 2>&1 || {
		echo "error: missing required command: $1" >&2
		exit 1
	}
}

need_cmd cmake
need_cmd pkg-config
need_cmd dpkg-deb
need_cmd strip
need_cmd sed
need_cmd awk
need_cmd du
need_cmd ln
need_cmd install

mkdir -p -- "$BUILD_DIR"
rm -rf -- "$PKGTMP"
mkdir -p -- "$PKGTMP"

# Build (out-of-tree).
"$SRC_ROOT/build.sh"

BIN_PATH="$BUILD_DIR/trsync"
if test ! -x "$BIN_PATH"; then
	echo "error: missing built binary: $BIN_PATH" >&2
	exit 1
fi

# Stage filesystem layout.
rm -rf -- "$PKGROOT"
mkdir -p -- \
	"$PKGROOT/DEBIAN" \
	"$PKGROOT/usr/bin" \
	"$PKGROOT/usr/share/applications" \
	"$PKGROOT/usr/share/icons/hicolor"

# Install binary.
install -m 0755 "$BIN_PATH" "$PKGROOT/usr/bin/trsync"

# Install desktop entry.
cat > "$PKGROOT/usr/share/applications/trsync.desktop" <<EOF
[Desktop Entry]
Version=1.0
Name=TRsync
GenericName=Synchronize Files
Comment=Synchronize files and folders with rsync and Trinity
Exec=trsync
Icon=trsync
Terminal=false
Type=Application
Categories=System;Utility;
EOF
chmod 0644 "$PKGROOT/usr/share/applications/trsync.desktop"

# Install application icon tree.
ICON_SRC="$SRC_ROOT/icons/trsync_icon2.png"
if test -f "$ICON_SRC"; then
	real_sz="64x64"
	real_dir="$PKGROOT/usr/share/icons/hicolor/$real_sz/apps"
	mkdir -p -- "$real_dir"
	install -m 0644 "$ICON_SRC" "$real_dir/trsync.png"
	for sz in 16x16 22x22 24x24 32x32 48x48; do
		dstdir="$PKGROOT/usr/share/icons/hicolor/$sz/apps"
		mkdir -p -- "$dstdir"
		ln -sf "../../$real_sz/apps/trsync.png" "$dstdir/trsync.png"
	done
else
	echo "warning: missing $ICON_SRC (application icon will not be installed)" >&2
fi

# Specify explicit optimal runtime dependencies (not too restrictive for tqt3/TDE)
DEPENDS="libtqt3-mt-trinity (>= 4:14.0.0) | libtqt3-mt, rsync"
RECOMMENDS="encfs, gocryptfs"

# Strip the staged binary
STAGED_BIN="$PKGROOT/usr/bin/trsync"
if command -v sstrip >/dev/null 2>&1; then
	echo "info: stripping with sstrip"
	sstrip "$STAGED_BIN" >/dev/null 2>&1 || true
else
	echo "info: sstrip not found, using strip --strip-all"
	strip --strip-all "$STAGED_BIN" >/dev/null 2>&1 || true
fi

# Debian control file.
INSTALLED_SIZE_KB="$(du -sk "$PKGROOT/usr" | awk '{print $1}')"
cat > "$PKGROOT/DEBIAN/control" <<EOF
Package: $PKG_NAME
Version: $PKG_VERSION
Section: $PKG_SECTION
Priority: $PKG_PRIORITY
Architecture: $ARCH
Maintainer: $PKG_MAINTAINER
Installed-Size: $INSTALLED_SIZE_KB
Depends: $DEPENDS
Recommends: $RECOMMENDS
Description: Synchronize files and folders with FUSE client-side encryption
 A native TQt3 frontend for rsync providing advanced profile management,
 Trinity integration, dry-run simulation, detailed log auditing,
 and seamless client-side encryption via EncFS or gocryptfs.
EOF

# postinst: refresh icon cache if available.
cat > "$PKGROOT/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
	gtk-update-icon-cache -f -t /usr/share/icons/hicolor >/dev/null 2>&1 || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
	update-desktop-database -q /usr/share/applications >/dev/null 2>&1 || true
fi
exit 0
EOF
chmod 0755 "$PKGROOT/DEBIAN/postinst"

# prerm: refresh icon cache on removal (best effort).
cat > "$PKGROOT/DEBIAN/prerm" <<'EOF'
#!/bin/sh
set -e
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
	gtk-update-icon-cache -f -t /usr/share/icons/hicolor >/dev/null 2>&1 || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
	update-desktop-database -q /usr/share/applications >/dev/null 2>&1 || true
fi
exit 0
EOF
chmod 0755 "$PKGROOT/DEBIAN/prerm"

OUT_DEB="$SRC_ROOT/${PKG_NAME}_${PKG_VERSION}_${ARCH}.deb"
rm -f -- "$OUT_DEB"

dpkg-deb --build "$PKGROOT" "$OUT_DEB" >/dev/null

echo "Debian package successfully built: $OUT_DEB"
exit 0
