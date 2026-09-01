#!/usr/bin/env bash
set -euo pipefail

# ==============================================================================
# Script de mise à jour du Dépôt APT TRsync & GitHub Pages
# ==============================================================================

REPO_DIR="$(cd "$(dirname "$0")" && pwd)"
PAGES_BRANCH="gh-pages"

need_cmd() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "Error: missing required command: $1" >&2
        exit 1
    }
}

need_cmd dpkg-scanpackages
need_cmd apt-ftparchive
need_cmd git
need_cmd gzip

echo "=================================================="
echo " TRsync APT Repository & GitHub Pages Sync"
echo "=================================================="

# Ensure we have deb and qsi packages
if [ -x "$REPO_DIR/build_deb.sh" ]; then
    echo "[Info] Rebuilding Debian package..."
    "$REPO_DIR/build_deb.sh"
fi

if [ -x "$REPO_DIR/build_qsi.sh" ]; then
    echo "[Info] Rebuilding QSI package..."
    "$REPO_DIR/build_qsi.sh"
fi

DEB_FILES=($(find "$REPO_DIR" -maxdepth 1 -name "trsync_*_amd64.deb" | sort -V -r))
if [ ${#DEB_FILES[@]} -eq 0 ]; then
    echo "Error: No trsync_*_amd64.deb package found in $REPO_DIR." >&2
    exit 1
fi

QSI_FILES=($(find "$REPO_DIR" -maxdepth 1 -name "setup_trsync_*.qsi" | sort -V -r))

PAGES_DIR=$(mktemp -d -t trsync-gh-pages-XXXXXX)
echo "Staging in temporary directory: $PAGES_DIR"

REMOTE_URL="$(git -C "$REPO_DIR" remote get-url origin)"

# Clone or checkout gh-pages into temp directory
git clone --branch "$PAGES_BRANCH" --single-branch "$REMOTE_URL" "$PAGES_DIR" 2>/dev/null || {
    echo "Creating new orphan gh-pages branch in temp directory..."
    git init "$PAGES_DIR"
    (
        cd "$PAGES_DIR"
        git checkout --orphan "$PAGES_BRANCH"
        git remote add origin "$REMOTE_URL"
    )
}

# Structure pool and dists directories for standard APT repo
POOL_DIR="$PAGES_DIR/pool/main/t/trsync"
DISTS_DIR="$PAGES_DIR/dists/stable/main/binary-amd64"
mkdir -p "$POOL_DIR"
mkdir -p "$DISTS_DIR"

# Copy all deb packages into pool
for deb in "${DEB_FILES[@]}"; do
    echo "  -> Added DEB: $(basename "$deb")"
    cp -a "$deb" "$POOL_DIR/"
done

# Copy latest QSI to root of pages
for qsi in "${QSI_FILES[@]}"; do
    echo "  -> Added QSI: $(basename "$qsi")"
    cp -a "$qsi" "$PAGES_DIR/"
done

# Generate Packages & Packages.gz index files
echo "Generating Packages index..."
(
    cd "$PAGES_DIR"
    dpkg-scanpackages --multiversion --arch amd64 pool/main > "$DISTS_DIR/Packages"
    gzip -9 -c "$DISTS_DIR/Packages" > "$DISTS_DIR/Packages.gz"
)

# Generate Release file
echo "Generating Release manifest..."
apt-ftparchive \
  -o APT::FTPArchive::Release::Origin="TRsync" \
  -o APT::FTPArchive::Release::Label="TRsync APT Repository" \
  -o APT::FTPArchive::Release::Suite="stable" \
  -o APT::FTPArchive::Release::Codename="stable" \
  -o APT::FTPArchive::Release::Architectures="amd64" \
  -o APT::FTPArchive::Release::Components="main" \
  -o APT::FTPArchive::Release::Description="APT Repository for TRsync (Trinity Desktop & Linux)" \
  release "$PAGES_DIR/dists/stable" > "$PAGES_DIR/dists/stable/Release"

# Copy assets (header logo, favicon, screenshots, etc.)
if [ -f "$REPO_DIR/konqi_sync.png" ]; then
    cp -a "$REPO_DIR/konqi_sync.png" "$PAGES_DIR/"
fi
if [ -f "$REPO_DIR/favicon.png" ]; then
    cp -a "$REPO_DIR/favicon.png" "$PAGES_DIR/"
elif [ -f "$REPO_DIR/icons/trsync_icon2.png" ]; then
    cp -a "$REPO_DIR/icons/trsync_icon2.png" "$PAGES_DIR/favicon.png"
fi

# Create .nojekyll to prevent GitHub Pages Jekyll processing
touch "$PAGES_DIR/.nojekyll"

# Find latest file names and version for HTML
LATEST_VERSION="1.3.1"
LATEST_DEB_NAME=$(basename "${DEB_FILES[0]}")
LATEST_QSI_NAME=""
if [ ${#QSI_FILES[@]} -gt 0 ]; then
    LATEST_QSI_NAME=$(basename "${QSI_FILES[0]}")
fi

cat << 'EOF' > "$PAGES_DIR/index.html"
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>TRsync v1.3.1 - Native TQt3 GUI for Rsync &amp; FUSE Encryption</title>
  <link rel="icon" type="image/png" href="favicon.png">
  <meta name="description" content="Official APT Repository and download portal for TRsync - High-performance graphical rsync frontend with FUSE client-side encryption for Trinity Desktop Environment (TDE) & Linux.">
  <style>
    :root {
      --bg: #12141a;
      --card-bg: #1c1f2b;
      --card-hover: #222738;
      --accent: #38bdf8;
      --accent-grad: linear-gradient(135deg, #0284c7, #38bdf8);
      --text: #e2e8f0;
      --text-muted: #94a3b8;
      --code-bg: #0f1117;
      --border: #2e364f;
      --radius: 12px;
      --radius-sm: 8px;
    }

    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
    }

    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      background-color: var(--bg);
      color: var(--text);
      line-height: 1.6;
      padding: 40px 20px;
    }

    .container {
      max-width: 840px;
      margin: 0 auto;
    }

    header {
      text-align: center;
      margin-bottom: 40px;
    }

    .logo {
      max-width: 240px;
      height: auto;
      margin-bottom: 16px;
      filter: drop-shadow(0 8px 24px rgba(56, 189, 248, 0.35));
      transition: transform 0.3s cubic-bezier(0.34, 1.56, 0.64, 1);
    }

    .logo:hover {
      transform: scale(1.04);
    }

    .badge-group {
      display: flex;
      justify-content: center;
      gap: 10px;
      margin-bottom: 12px;
      flex-wrap: wrap;
    }

    .badge {
      display: inline-block;
      padding: 4px 14px;
      font-size: 0.85rem;
      font-weight: 600;
      color: #fff;
      background: var(--accent-grad);
      border-radius: 20px;
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }

    .badge-green {
      background: linear-gradient(135deg, #15803d, #22c55e);
    }

    .badge-purple {
      background: linear-gradient(135deg, #6366f1, #a855f7);
    }

    .version-pill {
      display: inline-block;
      font-size: 1.1rem;
      font-weight: 600;
      color: #38bdf8;
      background: rgba(56, 189, 248, 0.12);
      border: 1px solid rgba(56, 189, 248, 0.35);
      padding: 2px 12px;
      border-radius: 20px;
      vertical-align: middle;
      margin-left: 8px;
    }

    h1 {
      font-size: 2.4rem;
      font-weight: 700;
      margin-bottom: 8px;
      display: flex;
      align-items: center;
      justify-content: center;
    }

    p.lead {
      font-size: 1.1rem;
      color: var(--text-muted);
      max-width: 680px;
      margin: 0 auto;
    }

    .card {
      background: var(--card-bg);
      border: 1px solid var(--border);
      border-radius: var(--radius);
      padding: 24px;
      margin-bottom: 24px;
      box-shadow: 0 10px 30px rgba(0, 0, 0, 0.3);
    }

    h2 {
      font-size: 1.3rem;
      font-weight: 600;
      margin-bottom: 16px;
      color: #fff;
      display: flex;
      align-items: center;
      gap: 10px;
    }

    .code-block {
      position: relative;
      background: var(--code-bg);
      border: 1px solid var(--border);
      border-radius: var(--radius-sm);
      padding: 16px;
      margin-bottom: 12px;
      font-family: "SFMono-Regular", Consolas, "Liberation Mono", Menlo, Courier, monospace;
      font-size: 0.95rem;
      color: #38bdf8;
      overflow-x: auto;
    }

    .code-block code {
      white-space: pre;
    }

    .copy-btn {
      position: absolute;
      top: 10px;
      right: 10px;
      background: #242b3d;
      border: 1px solid var(--border);
      color: var(--text);
      padding: 4px 10px;
      border-radius: 6px;
      font-size: 0.75rem;
      cursor: pointer;
      transition: all 0.2s;
    }

    .copy-btn:hover {
      background: var(--accent);
      color: #000;
      font-weight: 600;
    }

    .features-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
      gap: 16px;
      margin-top: 12px;
    }

    .feature-item {
      background: var(--code-bg);
      border: 1px solid var(--border);
      border-radius: var(--radius-sm);
      padding: 16px;
      display: flex;
      flex-direction: column;
      gap: 6px;
    }

    .feature-icon {
      font-size: 1.4rem;
      margin-bottom: 2px;
    }

    .feature-title {
      font-weight: 600;
      color: #fff;
      font-size: 1rem;
    }

    .feature-text {
      font-size: 0.85rem;
      color: var(--text-muted);
      line-height: 1.4;
    }

    .downloads-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
      gap: 16px;
      margin-top: 16px;
    }

    .download-card {
      background: var(--code-bg);
      border: 1px solid var(--border);
      border-radius: var(--radius-sm);
      padding: 20px;
      display: flex;
      flex-direction: column;
      justify-content: space-between;
      transition: border-color 0.2s, transform 0.2s;
    }

    .download-card:hover {
      border-color: var(--accent);
      transform: translateY(-2px);
    }

    .download-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 8px;
    }

    .download-title {
      font-weight: 600;
      font-size: 1.1rem;
      color: #fff;
    }

    .download-tag {
      font-size: 0.75rem;
      background: rgba(56, 189, 248, 0.15);
      color: var(--accent);
      padding: 2px 8px;
      border-radius: 12px;
      font-weight: 600;
    }

    .download-desc {
      font-size: 0.85rem;
      color: var(--text-muted);
      margin-bottom: 16px;
    }

    .btn-download {
      display: inline-block;
      text-align: center;
      background: var(--accent-grad);
      color: #0f172a;
      font-weight: 600;
      text-decoration: none;
      padding: 10px 16px;
      border-radius: var(--radius-sm);
      transition: opacity 0.2s, transform 0.1s;
    }

    .btn-download:hover {
      opacity: 0.92;
      transform: scale(1.02);
    }

    footer {
      text-align: center;
      margin-top: 40px;
      color: var(--text-muted);
      font-size: 0.9rem;
    }

    footer a {
      color: var(--accent);
      text-decoration: none;
    }

    footer a:hover {
      text-decoration: underline;
    }

    .footer-links {
      margin-top: 12px;
      font-size: 0.85rem;
    }
  </style>
</head>
<body>
  <div class="container">
    
    <!-- Header -->
    <header>
      <img src="konqi_sync.png" alt="TRsync Logo" class="logo">
      <div class="badge-group">
        <span class="badge">Trinity Desktop (TDE)</span>
        <span class="badge badge-green">Debian / Q4OS</span>
        <span class="badge badge-purple">Client-Side Encryption</span>
      </div>
      <h1>TRsync <span class="version-pill">v1.3.1</span></h1>
      <p class="lead">Native TQt3 GUI for Rsync &amp; FUSE Encryption (gocryptfs / EncFS) with decoupled high-performance logging.</p>
    </header>

    <!-- APT Installation Method -->
    <div class="card">
      <h2>
        <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="#38bdf8" stroke-width="2"><path d="M4 17l6-6-6-6M12 19h8"/></svg>
        Automated APT Repository Setup (Recommended)
      </h2>
      <p style="color: var(--text-muted); font-size: 0.9rem; margin-bottom: 12px;">
        Add the official repository to your Debian / Ubuntu / Q4OS system to receive automatic updates:
      </p>

      <div class="code-block">
        <button class="copy-btn" onclick="copyCode('apt-cmd', this)">Copy</button>
        <code id="apt-cmd">echo "deb [trusted=yes] https://seb3773.github.io/TRsync/ stable main" | sudo tee /etc/apt/sources.list.d/trsync.list
sudo apt update
sudo apt install trsync</code>
      </div>
    </div>

    <!-- Direct Downloads -->
    <div class="card">
      <h2>
        <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="#38bdf8" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4M7 10l5 5 5-5M12 15V3"/></svg>
        Direct Package Downloads
      </h2>
      <p style="color: var(--text-muted); font-size: 0.9rem;">
        Prefer standalone packages? Download the appropriate format for your environment:
      </p>

      <div class="downloads-grid">
        <div class="download-card">
          <div class="download-header">
            <span class="download-title">Debian Package (.deb)</span>
            <span class="download-tag">Recommended</span>
          </div>
          <p class="download-desc">Standard dynamically linked build for Trinity Desktop / Debian-based systems.</p>
          <a href="pool/main/t/trsync/trsync_1.3.1_amd64.deb" class="btn-download">
            Download .deb
          </a>
        </div>

        <div class="download-card">
          <div class="download-header">
            <span class="download-title">Q4OS Installer (.qsi)</span>
            <span class="download-tag">Q4OS 1-Click</span>
          </div>
          <p class="download-desc">Graphical one-click installer wizard designed specifically for Q4OS Trinity desktop.</p>
          <a href="setup_trsync_1.3.1.qsi" class="btn-download">
            Download .qsi
          </a>
        </div>
      </div>
      <p style="color: var(--text-muted); font-size: 0.85rem; margin-top: 16px;">
        * Note: The Q4OS installer (.qsi) automatically configures the APT repository during installation for future updates.
      </p>
    </div>

    <!-- Key Features -->
    <div class="card">
      <h2>
        <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="#38bdf8" stroke-width="2"><path d="M13 2L3 14h9l-1 8 10-12h-9l1-8z"/></svg>
        Key Capabilities &amp; Architecture
      </h2>

      <div class="features-grid">
        <div class="feature-item">
          <span class="feature-icon">🔒</span>
          <div class="feature-title">FUSE Client-Side Encryption</div>
          <div class="feature-text">Seamless on-the-fly zero-knowledge encryption for backups using <b>gocryptfs</b> or <b>EncFS</b> with isolated configuration keys.</div>
        </div>

        <div class="feature-item">
          <span class="feature-icon">🔄</span>
          <div class="feature-title">One-Click Restore &amp; Swap</div>
          <div class="feature-text">Smart swap toggle with dynamic orange visual indicator, automatic FUSE decryption mounting, and destination path reconstitution.</div>
        </div>

        <div class="feature-item">
          <span class="feature-icon">🚀</span>
          <div class="feature-title">RAM-Decoupled Logging</div>
          <div class="feature-text">Rsync output is buffered in RAM (<code>/dev/shm</code>) and rendered in batches at 20Hz, preventing GUI lockups on heavy transfers.</div>
        </div>

        <div class="feature-item">
          <span class="feature-icon">⚡</span>
          <div class="feature-title">Simulation &amp; Profile Sets</div>
          <div class="feature-text">Full dry-run simulation mode, unlimited named session configurations, and batch execution sets.</div>
        </div>

        <div class="feature-item">
          <span class="feature-icon">🛡️</span>
          <div class="feature-title">Native Trinity Integration</div>
          <div class="feature-text">Native TQt3 design with <code>tdesudo</code> superuser elevation and size-optimized standalone binary (~163 KB).</div>
        </div>

        <div class="feature-item">
          <span class="feature-icon">📁</span>
          <div class="feature-title">Custom Log Storage</div>
          <div class="feature-text">Globally customizable log directory with automatic recursive path creation and coordinated preference controls.</div>
        </div>
      </div>
    </div>

    <!-- Footer -->
    <footer>
      <p>Source Code &amp; Releases: <a href="https://github.com/seb3773/TRsync" target="_blank" rel="noopener">github.com/seb3773/TRsync</a></p>
      <p style="margin-top: 6px;">Based on GRsync &copy; Piero Orsoni &bull; Ported &amp; enhanced by seb3773 for Trinity Desktop Environment.</p>
      <p class="footer-links">
        <a href="http://trinitydesktop.org/" target="_blank" rel="noopener">http://trinitydesktop.org/</a> &bull; 
        <a href="https://www.q4os.org/" target="_blank" rel="noopener">https://www.q4os.org/</a> &bull; 
        <a href="https://www.q4os.org/forum/index.php" target="_blank" rel="noopener">https://www.q4os.org/forum/index.php</a>
      </p>
    </footer>

  </div>

  <script>
    function copyCode(id, btn) {
      const text = document.getElementById(id).innerText;
      navigator.clipboard.writeText(text).then(() => {
        const orig = btn.innerText;
        btn.innerText = "Copied!";
        setTimeout(() => btn.innerText = orig, 2000);
      });
    }
  </script>
</body>
</html>
EOF

# Git commit and push to gh-pages
echo "Committing and pushing to gh-pages branch..."
(
    cd "$PAGES_DIR"
    git add -A
    git commit -m "Update TRsync APT repository and GitHub Pages: $(date +'%Y-%m-%d %H:%M:%S')" || echo "No changes to commit."
    git push origin "$PAGES_BRANCH"
)

echo "Cleaning up temporary directory..."
rm -rf "$PAGES_DIR"

echo "=================================================="
echo " SUCCESS: APT repository updated on gh-pages!"
echo " URL: https://seb3773.github.io/TRsync/"
echo "=================================================="
