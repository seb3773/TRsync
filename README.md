# TRsync

![TRsync](./konqi_sync.png)

**TRsync** is a native TQt3 port of [Grsync](http://www.opbyte.it/grsync/), designed for the **Trinity Desktop Environment (TDE)**. It provides a lightweight, premium, and feature-rich graphical user interface for `rsync`, the powerful command-line directory and file synchronization tool.

This project is based on the original **GRsync (c) Piero Orsoni and others**, ported to tqt3 and enhanced.

---

## Features

### Classic Grsync Features
* **Session Profile Management**: Save multiple settings and configurations under customized names ("sessions") with no limits.
* **Session Sets**: Create batch session sets to run multiple synchronization tasks sequentially in one go.
* **Simulation (Dry-Run)**: Test synchronization rules safely before performing real file writes.
* **Rsync Output Parsing**: Captures rsync output to display progress bars, transfer speeds, and remaining file counts in real time.
* **Error Isolation**: Separates and highlights errors in a dedicated scrollable window for quick troubleshooting.
* **Pre/Post-execution Hooks**: Execute custom shell commands before or after rsync (with option to halt on failure).
* **Import/Export Profiles**: Easily export and import session settings to share configurations.

### Advanced Enhancements in TRsync
* **FUSE Client-Side Encryption**:
  * Seamless support for **gocryptfs** and **EncFS** to perform secure, encrypted backups.
  * Isolation of encryption configuration files in `~/.config/trsync/` to keep source folders clean.
  * Portable backup configurations: configuration keys are automatically mirrored to the backup destination to allow decryption on any system.
* **One-Click Restore & Decryption**:
  * The **Swap** button acts as a toggle to instantly invert source and destination.
  * When swap is active, the button turns **light orange** (indicating restoration mode) and TRsync automatically configures FUSE to mount, decrypt, and restore files to their original location.
  * Reconstitutes source paths automatically when restoring backups created without trailing slashes.
* **High Performance Output Rendering**:
  * **RAM Redirection**: Rsync stdout/stderr is buffered in RAM (`/dev/shm`) to completely decouple the GUI from heavy console printing and prevent application freezes.
  * **Batch Appending & Rate Limiting**: Batches console text updates at 20Hz and limits widget repaints to 50ms intervals, saving CPU cycles and ensuring smooth scrolling.
* **Custom Log Directory**:
  * Configure custom log paths globally in Preferences.
  * Automatically creates missing log folder trees recursively on demand.
  * Dynamic GUI logic: logging sub-options (overwrite, custom paths) are automatically enabled/disabled based on the master "Enable logging" switch.
* **Trinity Integration**:
  * Uses TDE's native **`tdesudo`** graphical administrative utility for "Run as superuser" options, with `sudo` fallback.
  * Displays smart warn-on-launch dialogs when using superuser privileges alongside FUSE encryption (reminding users about `user_allow_other` in `/etc/fuse.conf`).
* **Optimized Executable**:
  * Built as a size-optimized freested binary (~163 KB) with embedded PNG icons loaded directly from memory.
  * Dynamic Show/Hide password toggle button with status-reactive eye icons.

---

## Build Instructions

### Prerequisites
Ensure the following packages are installed on your TDE/Debian/Ubuntu system:
* `cmake`
* `pkg-config`
* `libtqt3-mt-trinity` (or `libtqt3-mt`)
* `tqt3-dev-tools`
* `rsync`
* `gocryptfs` and/or `encfs` (optional, for client-side encryption)

### Compiling from Source
To compile TRsync, run the provided build script:
```bash
./build.sh
```
This script will:
1. Verify system dependencies.
2. Configure the project out-of-tree inside the `build/` directory.
3. Build the binary using CMake.

The compiled executable `trsync` will be created in `build/`.

---

## Packaging

To generate a standard Debian package (`.deb`), run the packaging script:
```bash
./build_deb.sh
```
This script automatically:
* Calls `build.sh` to compile the binary.
* Creates the Debian staging filesystem.
* Strips debugging symbols (using `sstrip` or `strip --strip-all`).
* Generates a modern desktop launcher (`trsync.desktop`) and registers the system launcher category.
* Configures application icons and creates standard system resolution links (from 16x16 up to 64x64).
* Produces a packaged archive named `trsync_1.3.1_<architecture>.deb` in the current directory, ready for installation.

---

## License
TRsync is open source and released under the **GNU GPL License**. See the `COPYING` file at the root of the project for license details.
