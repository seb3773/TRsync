#include <ntqtooltip.h>
#include "mainwindow.h"
#include "rsyncdialog_new.h"

#include <ntqcombobox.h>
#include <ntqdir.h>
#include <ntqfile.h>
#include <ntqlabel.h>
#include <ntqlayout.h>
#include <ntqlineedit.h>
#include <ntqmessagebox.h>
#include <ntqinputdialog.h>
#include <ntqfiledialog.h>
#include <ntqpushbutton.h>
#include <ntqdialog.h>
#include <ntqbuttongroup.h>
#include <ntqstringlist.h>
#include <ntqtabwidget.h>
#include <ntqtextedit.h>
#include <ntqwidget.h>

#include <ntqtextstream.h>

#include <ntqcheckbox.h>
#include <ntqframe.h>
#include <ntqprogressbar.h>
#include <ntqtoolbutton.h>
#include <ntqlistview.h>
#include <ntqprocess.h>
#include <ntqfileinfo.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/stat.h>

#include <ntqapplication.h>
#include <ntqtimer.h>

#include <ntqmenubar.h>
#include <ntqpopupmenu.h>

#include "rsyncprocess.h"
#include "inifile.h"
#include <ntqpixmap.h>
#include <ntqiconset.h>
#include "embedded_icons.h"

static TQIconSet loadEmbeddedIcon(const unsigned char *data, unsigned int len)
{
    TQPixmap pix;
    if (pix.loadFromData(data, len, "PNG")) {
        return TQIconSet(pix);
    }
    return TQIconSet();
}


static int isReservedSessionName(const TQString &name);
static int runCommandSync(const TQString &cmdLine, TQString *out, TQString *err);
static TQString trsyncConfigDir();
static TQString trsyncIniPath();

static bool copyFile(const TQString &src, const TQString &dst)
{
    TQFile fSrc(src);
    TQFile fDst(dst);
    if (!fSrc.open(IO_ReadOnly)) return false;
    if (!fDst.open(IO_WriteOnly)) {
        fSrc.close();
        return false;
    }
    TQByteArray buf = fSrc.readAll();
    fDst.writeBlock(buf.data(), buf.size());
    fSrc.close();
    fDst.close();
    return true;
}

static TQString findBinary(const TQString &name)
{
    TQStringList paths = TQStringList::split(":", getenv("PATH"));
    for (TQStringList::ConstIterator it = paths.begin(); it != paths.end(); ++it) {
        TQString fullPath = *it + "/" + name;
        if (TQFileInfo(fullPath).isExecutable()) {
            return fullPath;
        }
    }
    return TQString();
}

static bool trsyncMakePath(const TQString &path)
{
    TQDir dir;
    if (dir.exists(path)) return true;

    TQStringList parts = TQStringList::split("/", path);
    TQString currentPath;
    if (path.startsWith("/")) {
        currentPath = "/";
    }

    for (TQStringList::Iterator it = parts.begin(); it != parts.end(); ++it) {
        if ((*it).isEmpty()) continue;
        if (currentPath.isEmpty()) {
            currentPath = *it;
        } else {
            if (currentPath == "/") {
                currentPath += *it;
            } else {
                currentPath += "/" + *it;
            }
        }
        if (!dir.exists(currentPath)) {
            if (!dir.mkdir(currentPath, true)) {
                return false;
            }
        }
    }
    return true;
}

static int runCommandWithStdin(const TQString &program, const TQStringList &args, const TQString &stdinData, TQString *out = 0, TQString *err = 0)
{
    TQProcess proc;
    proc.addArgument(program);
    for (TQStringList::ConstIterator it = args.begin(); it != args.end(); ++it) {
        proc.addArgument(*it);
    }
    if (!proc.start()) return -1;
    if (!stdinData.isEmpty()) {
        proc.writeToStdin(stdinData + "\n");
        // Let the event loop run to flush stdin buffer to the child process
        for (int i = 0; i < 10; ++i) {
            tqApp->processEvents();
            usleep(10000);
        }
        proc.closeStdin();
    } else {
        proc.closeStdin();
    }
    while (proc.isRunning()) {
        tqApp->processEvents();
        usleep(10000);
    }
    if (out) {
        *out = TQString::fromLocal8Bit(proc.readStdout());
    }
    if (err) {
        *err = TQString::fromLocal8Bit(proc.readStderr());
    }
    return proc.normalExit() ? proc.exitStatus() : -1;
}

static void unmountTempDir(const TQString &tempDir)
{
    if (tempDir.isEmpty() || !TQDir(tempDir).exists()) return;
    
    TQStringList args;
    args << "-u" << tempDir;
    TQString out, err;
    int ret = runCommandWithStdin("fusermount", args, "", &out, &err);
    if (ret != 0) {
        TQStringList lazyArgs;
        lazyArgs << "-u" << "-z" << tempDir;
        runCommandWithStdin("fusermount", lazyArgs, "", &out, &err);
    }
    
    TQDir().rmdir(tempDir);
}

static TQStringList parseShellLikeArgs(const TQString &text)
{
    TQStringList out;
    TQString cur;
    int inSingle = 0;
    int inDouble = 0;
    int esc = 0;

    for (int i = 0; i < (int)text.length(); ++i) {
        const TQChar ch = text[i];

        if (esc) {
            cur += ch;
            esc = 0;
            continue;
        }
        if (ch == '\\' && !inSingle) {
            esc = 1;
            continue;
        }

        if (ch == '\'' && !inDouble) {
            inSingle ^= 1;
            continue;
        }

        if (ch == '"' && !inSingle) {
            inDouble ^= 1;
            continue;
        }

        if (!inSingle && !inDouble && ch.isSpace()) {
            if (!cur.isEmpty()) {
                out << cur;
                cur = TQString();
            }
            continue;
        }

        cur += ch;
    }

    if (!cur.isEmpty()) out << cur;
    return out;
}

void MainWindow::loadPreferences()
{
    IniFile ini;
    ini.load(trsyncIniPath());
    IniGroup cfg = ini.group("__CONFIG");

    m_cfg_command = cfg.readString("command", "rsync");
    m_cfg_output = cfg.readBool("output", false);
    m_cfg_remember = cfg.readBool("remember", true);
    m_cfg_errorlist = cfg.readBool("errorlist", false);
    m_cfg_log = cfg.readBool("logging", false);
    m_cfg_log_overwrite = cfg.readBool("logging_overwrite", false);
    m_cfg_fastscroll = cfg.readBool("fastscroll", true);
    m_cfg_switchbutton = cfg.readBool("switchbutton", false);
    m_cfg_trayicon = cfg.readBool("trayicon", false);
    m_cfg_custom_log_dir_enabled = cfg.readBool("logging_custom_dir_enabled", false);
    m_cfg_custom_log_dir = cfg.readString("logging_custom_dir", "");
}

void MainWindow::savePreferences()
{
    IniFile ini;
    ini.load(trsyncIniPath());
    IniGroup cfg = ini.group("__CONFIG");

    cfg.writeString("command", m_cfg_command);
    cfg.writeBool("output", m_cfg_output);
    cfg.writeBool("remember", m_cfg_remember);
    cfg.writeBool("errorlist", m_cfg_errorlist);
    cfg.writeBool("logging", m_cfg_log);
    cfg.writeBool("logging_overwrite", m_cfg_log_overwrite);
    cfg.writeBool("fastscroll", m_cfg_fastscroll);
    cfg.writeBool("switchbutton", m_cfg_switchbutton);
    cfg.writeBool("trayicon", m_cfg_trayicon);
    cfg.writeBool("logging_custom_dir_enabled", m_cfg_custom_log_dir_enabled);
    cfg.writeString("logging_custom_dir", m_cfg_custom_log_dir);

    ini.save(trsyncIniPath());
}

void MainWindow::applyPreferencesToUi()
{
    if (m_output) {
        if (m_cfg_output) m_output->show();
        else m_output->hide();
    }

    if (m_btnSwap) {
        if (m_cfg_switchbutton) m_btnSwap->show();
        else m_btnSwap->hide();
    }
}

void MainWindow::applyRememberedSession()
{
    if (!m_sessions) return;
    if (!m_cfg_remember) return;

    IniFile ini;
    ini.load(trsyncIniPath());
    IniGroup cfg = ini.group("__CONFIG");
    const TQString last = cfg.readString("last_session", TQString());

    if (last.isEmpty()) return;

    for (int i = 0; i < m_sessions->count(); ++i) {
        if (m_sessions->text(i) == last) {
            m_sessions->setCurrentItem(i);
            break;
        }
    }
}

void MainWindow::slotBrowseSource()
{
    if (!m_source) return;

    const TQString start = m_source->text();
    if (m_check_browse_files && m_check_browse_files->isChecked()) {
        const TQString file = TQFileDialog::getOpenFileName(start, TQString(), this, 0, "Select source file");
        if (!file.isEmpty()) m_source->setText(file);
        return;
    }

    TQString dir = TQFileDialog::getExistingDirectory(start, this, 0, "Select source folder", true);
    if (!dir.isEmpty()) {
        if (dir.endsWith("/") && dir != "/") {
            dir.truncate(dir.length() - 1);
        }
        m_source->setText(dir);
    }
}

void MainWindow::slotBrowseDest()
{
    if (!m_dest) return;

    const TQString start = m_dest->text();
    if (m_check_browse_files && m_check_browse_files->isChecked()) {
        const TQString file = TQFileDialog::getOpenFileName(start, TQString(), this, 0, "Select destination file");
        if (!file.isEmpty()) m_dest->setText(file);
        return;
    }

    TQString dir = TQFileDialog::getExistingDirectory(start, this, 0, "Select destination folder", true);
    if (!dir.isEmpty()) {
        if (dir.endsWith("/") && dir != "/") {
            dir.truncate(dir.length() - 1);
        }
        m_dest->setText(dir);
    }
}

void MainWindow::slotBrowseLogDir()
{
    if (!m_prefCustomLogDirEdit) return;

    const TQString start = m_prefCustomLogDirEdit->text().isEmpty() ? TQString("/var/log") : m_prefCustomLogDirEdit->text();
    TQString dir = TQFileDialog::getExistingDirectory(start, this, 0, "Select custom log directory", true);
    if (!dir.isEmpty()) {
        if (dir.endsWith("/") && dir != "/") {
            dir.truncate(dir.length() - 1);
        }
        m_prefCustomLogDirEdit->setText(dir);
    }
}

void MainWindow::slotUpdatePrefLogState()
{
    if (!m_prefCbLog) return;
    bool logging = m_prefCbLog->isChecked();
    if (m_prefCbOverwrite) m_prefCbOverwrite->setEnabled(logging);
    if (m_prefCbCustomLogDir) m_prefCbCustomLogDir->setEnabled(logging);
    
    bool customDir = logging && m_prefCbCustomLogDir && m_prefCbCustomLogDir->isChecked();
    if (m_prefCustomLogDirEdit) m_prefCustomLogDirEdit->setEnabled(customDir);
    if (m_prefBtnBrowseLogDir) m_prefBtnBrowseLogDir->setEnabled(customDir);
}

void MainWindow::slotSwapSourceDest(bool checked)
{
    if (!m_source || !m_dest) return;
    TQString tmp = m_source->text();
    m_source->setText(m_dest->text());
    m_dest->setText(tmp);

    if (m_btnSwap) {
        if (checked) {
            m_btnSwap->setFlat(true);
            TQPalette swapPal = m_btnSwap->palette();
            swapPal.setColor(TQColorGroup::Button, TQColor("#FFE4B5")); // Moccasin
            m_btnSwap->setPalette(swapPal);
        } else {
            m_btnSwap->setFlat(false);
            m_btnSwap->unsetPalette();
        }
    }
}

void MainWindow::slotMenuSwapSourceDest()
{
    if (m_btnSwap) {
        m_btnSwap->setOn(!m_btnSwap->isOn());
    }
}

void MainWindow::slotShowTrailingSlashInfo()
{
    const TQString msg =
        "A trailing slash on the source directory avoids creating an additional directory level at the destination.\n"
        "You can think of a trailing / on a source as meaning \"copy the contents of this directory\" as opposed to \"copy the directory itself and its contents\".\n\n"
        "In other words, each of the following commands copies the files in the same way:\n\n"
        "\t rsync -a /src/foo /dest\n"
        "\t rsync -a /src/foo/ /dest/foo\n";

    TQMessageBox::information(this, "About trailing slashes", msg);
}

TQString MainWindow::buildRsyncCommandLineString() const
{
    const TQString src = m_source ? m_source->text() : TQString();
    const TQString dst = m_dest ? m_dest->text() : TQString();

    if (src.isEmpty() || dst.isEmpty()) return TQString();

    TQString actualSrc = src;
    TQString actualDst = dst;
    if (m_comboEncryption && m_comboEncryption->currentText() != "none") {
        actualSrc = "/tmp/TRsync_backup_" + currentSessionName() + "_<pid>/";
        bool isBackup = !(m_btnSwap && m_btnSwap->isOn());
        if (isBackup && !src.endsWith("/")) {
            TQString srcPath = src;
            while (srcPath.endsWith("/")) {
                srcPath.truncate(srcPath.length() - 1);
            }
            int lastSlash = srcPath.findRev('/');
            TQString dirName = (lastSlash >= 0) ? srcPath.mid(lastSlash + 1) : srcPath;
            if (!actualDst.endsWith("/")) {
                actualDst += "/";
            }
            actualDst += dirName;
        }
    }

    TQStringList args;

    if (m_check_superuser && m_check_superuser->isChecked()) {
        TQString sudoCmd = findBinary("tdesudo");
        if (sudoCmd.isEmpty()) sudoCmd = "sudo";
        args << sudoCmd;
    }
    args << (m_cfg_command.isEmpty() ? TQString("rsync") : m_cfg_command);

    args << ((m_check_norecur && m_check_norecur->isChecked()) ? "-d" : "-r");
    if (m_dryrunNext) args << "-n";

    if (m_check_time && m_check_time->isChecked()) args << "-t";
    if (m_check_perm && m_check_perm->isChecked()) args << "-p";
    if (m_check_owner && m_check_owner->isChecked()) args << "-o";
    if (m_check_group && m_check_group->isChecked()) args << "-g";
    if (m_check_onefs && m_check_onefs->isChecked()) args << "-x";
    if (m_check_verbose && m_check_verbose->isChecked()) args << "-v";
    if (m_check_progr && m_check_progr->isChecked()) args << "--progress";
    if (m_check_delete && m_check_delete->isChecked()) args << "--delete";
    if (m_check_exist && m_check_exist->isChecked()) args << "--ignore-existing";
    if (m_check_size && m_check_size->isChecked()) args << "--size-only";
    if (m_check_skipnew && m_check_skipnew->isChecked()) args << "-u";
    if (m_check_windows && m_check_windows->isChecked()) args << "--modify-window=1";

    if (m_check_sum && m_check_sum->isChecked()) args << "-c";
    if (m_check_symlink && m_check_symlink->isChecked()) args << "-l";
    if (m_check_hardlink && m_check_hardlink->isChecked()) args << "-H";
    if (m_check_dev && m_check_dev->isChecked()) args << "-D";
    if (m_check_update && m_check_update->isChecked()) args << "--existing";
    if (m_check_keepart && m_check_keepart->isChecked()) args << "--partial";
    if (m_check_mapuser && m_check_mapuser->isChecked()) args << "--numeric-ids";
    if (m_check_compr && m_check_compr->isChecked()) args << "-z";
    if (m_check_backup && m_check_backup->isChecked()) args << "-b";
    if (m_check_itemized && m_check_itemized->isChecked()) args << "-i";
    if (m_check_protectargs && m_check_protectargs->isChecked()) args << "-s";

    const TQString extraArgsText = m_additional ? m_additional->text() : TQString();
    if (!extraArgsText.isEmpty()) args += parseShellLikeArgs(extraArgsText);

    args << actualSrc;
    args << actualDst;

    TQString cmd;
    for (TQStringList::ConstIterator it = args.begin(); it != args.end(); ++it) {
        const TQString a = *it;
        if (cmd.isEmpty()) {
            cmd = a;
            continue;
        }
        if (a.find(' ') >= 0 || a.find('\t') >= 0) {
            cmd += " \"" + a + "\"";
        } else {
            cmd += " " + a;
        }
    }
    return cmd;
}

void MainWindow::slotShowRsyncCommandLine()
{
    const TQString cmd = buildRsyncCommandLineString();
    if (cmd.isEmpty()) {
        TQMessageBox::information(this, "trsync", "Source and Dest are required");
        return;
    }
    TQMessageBox::information(this, "trsync", cmd);
}

void MainWindow::slotPreferences()
{
    TQDialog dlg(this, 0, true);
    dlg.setCaption("TRsync preferences");

    TQVBoxLayout *v = new TQVBoxLayout(&dlg, 12, 12);

    TQGridLayout *g = new TQGridLayout(5, 2, 6);
    v->addLayout(g);

    TQLabel *lbl = new TQLabel("Rsync executable:", &dlg);
    TQLineEdit *edCommand = new TQLineEdit(&dlg);
    g->addWidget(lbl, 0, 0);
    g->addWidget(edCommand, 0, 1);

    TQCheckBox *cbOutput = new TQCheckBox("Show rsync output by default", &dlg);
    TQCheckBox *cbRemember = new TQCheckBox("Remember last used session", &dlg);
    TQCheckBox *cbErrorlist = new TQCheckBox("Show error list when finished", &dlg);
    TQCheckBox *cbLog = new TQCheckBox("Enable logging", &dlg);
    TQCheckBox *cbFastscroll = new TQCheckBox("Fast rsync output scrolling", &dlg);
    TQCheckBox *cbSwitchbutton = new TQCheckBox("Enable source/destination switch button", &dlg);
    TQCheckBox *cbTrayicon = new TQCheckBox("Use tray icon", &dlg);
    TQCheckBox *cbOverwrite = new TQCheckBox("Overwrite logs", &dlg);

    g->addWidget(cbOutput, 1, 0);
    g->addWidget(cbRemember, 1, 1);
    g->addWidget(cbErrorlist, 2, 0);
    g->addWidget(cbLog, 2, 1);
    g->addWidget(cbFastscroll, 3, 0);
    g->addWidget(cbSwitchbutton, 3, 1);
    g->addWidget(cbTrayicon, 4, 0);
    g->addWidget(cbOverwrite, 4, 1);

    TQHBoxLayout *logDirLayout = new TQHBoxLayout(0, 0, 6);
    v->addLayout(logDirLayout);

    TQCheckBox *cbCustomLogDir = new TQCheckBox("Save logs to custom directory:", &dlg);
    m_prefCustomLogDirEdit = new TQLineEdit(&dlg);
    TQPushButton *btnBrowseLogDir = new TQPushButton("Browse...", &dlg);

    logDirLayout->addWidget(cbCustomLogDir);
    logDirLayout->addWidget(m_prefCustomLogDirEdit, 1);
    logDirLayout->addWidget(btnBrowseLogDir);

    m_prefCbLog = cbLog;
    m_prefCbOverwrite = cbOverwrite;
    m_prefCbCustomLogDir = cbCustomLogDir;
    m_prefBtnBrowseLogDir = btnBrowseLogDir;

    connect(cbLog, SIGNAL(toggled(bool)), this, SLOT(slotUpdatePrefLogState()));
    connect(cbCustomLogDir, SIGNAL(toggled(bool)), this, SLOT(slotUpdatePrefLogState()));
    connect(btnBrowseLogDir, SIGNAL(clicked()), this, SLOT(slotBrowseLogDir()));

    edCommand->setText(m_cfg_command.isEmpty() ? TQString("rsync") : m_cfg_command);
    cbOutput->setChecked(m_cfg_output);
    cbRemember->setChecked(m_cfg_remember);
    cbErrorlist->setChecked(m_cfg_errorlist);
    cbLog->setChecked(m_cfg_log);
    cbOverwrite->setChecked(m_cfg_log_overwrite);
    cbFastscroll->setChecked(m_cfg_fastscroll);
    cbSwitchbutton->setChecked(m_cfg_switchbutton);
    cbTrayicon->setChecked(m_cfg_trayicon);

    cbCustomLogDir->setChecked(m_cfg_custom_log_dir_enabled);
    m_prefCustomLogDirEdit->setText(m_cfg_custom_log_dir.isEmpty() ? trsyncConfigDir() : m_cfg_custom_log_dir);
    slotUpdatePrefLogState();

    TQHBoxLayout *h = new TQHBoxLayout(0, 0, 6);
    v->addLayout(h);
    h->addStretch(1);
    TQPushButton *btnCancel = new TQPushButton("Cancel", &dlg);
    TQPushButton *btnOk = new TQPushButton("OK", &dlg);
    h->addWidget(btnCancel);
    h->addWidget(btnOk);
    btnOk->setDefault(true);

    connect(btnCancel, SIGNAL(clicked()), &dlg, SLOT(reject()));
    connect(btnOk, SIGNAL(clicked()), &dlg, SLOT(accept()));

    if (dlg.exec() != TQDialog::Accepted) {
        m_prefCustomLogDirEdit = 0;
        m_prefCbLog = 0;
        m_prefCbOverwrite = 0;
        m_prefCbCustomLogDir = 0;
        m_prefBtnBrowseLogDir = 0;
        return;
    }

    m_cfg_command = edCommand->text().stripWhiteSpace();
    if (m_cfg_command.isEmpty()) m_cfg_command = "rsync";
    m_cfg_output = cbOutput->isChecked();
    m_cfg_remember = cbRemember->isChecked();
    m_cfg_errorlist = cbErrorlist->isChecked();
    m_cfg_log = cbLog->isChecked();
    m_cfg_log_overwrite = cbOverwrite->isChecked();
    m_cfg_fastscroll = cbFastscroll->isChecked();
    m_cfg_switchbutton = cbSwitchbutton->isChecked();
    m_cfg_trayicon = cbTrayicon->isChecked();

    m_cfg_custom_log_dir_enabled = cbCustomLogDir->isChecked();
    m_cfg_custom_log_dir = m_prefCustomLogDirEdit->text().stripWhiteSpace();
    m_prefCustomLogDirEdit = 0;
    m_prefCbLog = 0;
    m_prefCbOverwrite = 0;
    m_prefCbCustomLogDir = 0;
    m_prefBtnBrowseLogDir = 0;

    savePreferences();
    applyPreferencesToUi();
}

void MainWindow::slotAbout()
{
    TQDialog dlg(this, 0, true);
    dlg.setCaption("About TRsync");
    TQColor bgColor("#1A1C1A");
    dlg.setBackgroundColor(bgColor);

    TQVBoxLayout *layout = new TQVBoxLayout(&dlg, 20, 15);

    TQLabel *lbl1 = new TQLabel("TRsync", &dlg);
    lbl1->setAlignment(TQt::AlignCenter);
    lbl1->setPaletteForegroundColor(TQColor(255, 255, 255));
    lbl1->setBackgroundColor(bgColor);
    TQFont f1 = lbl1->font();
    f1.setPointSize(f1.pointSize() + 4);
    f1.setBold(true);
    lbl1->setFont(f1);
    layout->addWidget(lbl1);

    TQLabel *lblSub = new TQLabel("A tqt3 GUI frontend for rsync", &dlg);
    lblSub->setAlignment(TQt::AlignCenter);
    lblSub->setPaletteForegroundColor(TQColor(255, 255, 255));
    lblSub->setBackgroundColor(bgColor);
    TQFont fSub = dlg.font();
    fSub.setItalic(true);
    lblSub->setFont(fSub);
    layout->addWidget(lblSub);

    TQFont f2 = dlg.font();
    f2.setPointSize(f2.pointSize() - 1);

    TQLabel *lbl2 = new TQLabel("based on GRsync (c) Piero Orsoni and others", &dlg);
    lbl2->setAlignment(TQt::AlignCenter);
    lbl2->setPaletteForegroundColor(TQColor(255, 255, 255));
    lbl2->setBackgroundColor(bgColor);
    lbl2->setFont(f2);
    layout->addWidget(lbl2);

    TQLabel *lblImg = new TQLabel(&dlg);
    lblImg->setAlignment(TQt::AlignCenter);
    lblImg->setBackgroundColor(bgColor);
    TQPixmap pix;
    if (pix.loadFromData(icon_about, icon_about_len, "PNG")) {
        lblImg->setPixmap(pix);
    }
    layout->addWidget(lblImg);

    TQLabel *lbl3 = new TQLabel("by seb3773 - https://github.com/seb3773", &dlg);
    lbl3->setAlignment(TQt::AlignCenter);
    lbl3->setPaletteForegroundColor(TQColor(255, 255, 255));
    lbl3->setBackgroundColor(bgColor);
    lbl3->setFont(f2);
    layout->addWidget(lbl3);

    layout->addSpacing(15);

    TQPushButton *btnClose = new TQPushButton("Close", &dlg);
    connect(btnClose, SIGNAL(clicked()), &dlg, SLOT(accept()));

    TQHBoxLayout *btnLayout = new TQHBoxLayout(layout);
    btnLayout->addStretch(1);
    btnLayout->addWidget(btnClose);
    btnLayout->addStretch(1);

    dlg.exec();
}

void MainWindow::slotRsyncInfo()
{
    TQString out, err;
    const int st = runCommandSync("rsync --version", &out, &err);
    if (st != 0 && !err.isEmpty()) {
        TQMessageBox::information(this, "trsync", err);
        return;
    }
    if (!out.isEmpty()) {
        TQMessageBox::information(this, "trsync", out);
        return;
    }
    TQMessageBox::information(this, "trsync", "Unable to query rsync version");
}

void MainWindow::slotSimulate()
{
    m_dryrunNext = 1;
    slotRun();
}

void MainWindow::slotSessionAdd()
{
    const TQString cur = currentSessionName();
    if (!cur.isEmpty()) saveUiToSession(cur);

    TQDialog dlg(this, "new_session", true);
    dlg.setCaption("New session");
    TQVBoxLayout *layout = new TQVBoxLayout(&dlg, 12, 6);
    
    layout->addWidget(new TQLabel("Session name:", &dlg));
    TQLineEdit *editName = new TQLineEdit(&dlg);
    layout->addWidget(editName);
    
    TQCheckBox *chkIsSet = new TQCheckBox("Add as session set", &dlg);
    layout->addWidget(chkIsSet);
    
    TQHBoxLayout *btnLayout = new TQHBoxLayout(layout, 6);
    btnLayout->addStretch(1);
    TQPushButton *btnCancel = new TQPushButton("Cancel", &dlg);
    TQPushButton *btnOk = new TQPushButton("OK", &dlg);
    btnOk->setDefault(true);
    btnLayout->addWidget(btnCancel);
    btnLayout->addWidget(btnOk);
    
    connect(btnCancel, SIGNAL(clicked()), &dlg, SLOT(reject()));
    connect(btnOk, SIGNAL(clicked()), &dlg, SLOT(accept()));
    
    if (dlg.exec() != TQDialog::Accepted) return;

    const TQString newses = editName->text().stripWhiteSpace();
    bool isSetSession = chkIsSet->isChecked();
    
    if (newses.isEmpty()) {
        TQMessageBox::warning(this, "trsync", "Session name cannot be empty");
        return;
    }
    
    if (isReservedSessionName(newses)) {
        TQMessageBox::critical(this, "trsync", "This session name is reserved");
        return;
    }

    IniFile ini;
    ini.load(trsyncIniPath());
    const TQStringList groups = ini.groups();
    for (TQStringList::ConstIterator it = groups.begin(); it != groups.end(); ++it) {
        if ((*it) == newses) {
            TQMessageBox::critical(this, "trsync", "A session named like this already exists");
            return;
        }
    }
    
    // Validate required fields
    if (!isSetSession) {
        const TQString src = m_source ? m_source->text().stripWhiteSpace() : TQString();
        const TQString dst = m_dest ? m_dest->text().stripWhiteSpace() : TQString();
        
        if (src.isEmpty() || dst.isEmpty()) {
            TQMessageBox::warning(this, "trsync",
                "Cannot create session: Source and Destination are required fields.\n\n"
                "Please fill in both fields before creating a new session.");
            return;
        }
        saveUiToSession(newses);
    } else {
        // For a set session, initialize with empty values and is_set = true
        IniFile iniNew;
        iniNew.load(trsyncIniPath());
        IniGroup grp = iniNew.group(newses);
        grp.writeBool("is_set", true);
        grp.writeString("text_source", "");
        grp.writeString("text_dest", "");
        iniNew.save(trsyncIniPath());
    }
    
    // Reload sessions list
    m_sessions->clear();
    IniFile ini2;
    ini2.load(trsyncIniPath());
    TQStringList allGroups = ini2.groups();
    if (allGroups.isEmpty()) allGroups.append("default");
    
    bool hasAny = false;
    int newSessionIndex = -1;
    int currentIndex = 0;
    
    for (TQStringList::ConstIterator it = allGroups.begin(); it != allGroups.end(); ++it) {
        if ((*it) == "__CONFIG") continue;
        if ((*it).isEmpty()) continue;
        m_sessions->insertItem(*it);
        if ((*it) == newses) {
            newSessionIndex = currentIndex;
        }
        currentIndex++;
        hasAny = true;
    }
    
    if (!hasAny) m_sessions->insertItem("default");
    
    // Select the newly created session
    if (newSessionIndex >= 0) {
        m_sessions->setCurrentItem(newSessionIndex);
        loadSessionToUi(newses);
    }
}

void MainWindow::slotSessionDelete()
{
    const TQString ses = currentSessionName();
    if (ses.isEmpty()) return;

    if (ses == "default") {
        TQMessageBox::critical(this, "trsync", "You cannot delete the default session");
        return;
    }

    const int res = TQMessageBox::question(this, "trsync", "Are you sure you want to delete the '" + ses + "' session?",
                                          TQMessageBox::Ok, TQMessageBox::Cancel);
    if (res != TQMessageBox::Ok) return;

    IniFile ini;
    ini.load(trsyncIniPath());
    ini.removeGroup(ses);
    ini.save(trsyncIniPath());
    loadSessions();
    loadSessionToUi(currentSessionName());
}

void MainWindow::slotSessionImport()
{
    const TQString filename = TQFileDialog::getOpenFileName(TQString(), "*.trsync", this, 0, "Import");
    if (filename.isEmpty()) return;

    loadSessionToUi(currentSessionName());

    // Import the session file by appending its content to our INI file
    TQFile in(filename);
    if (!in.open(IO_ReadOnly)) return;

    TQFile out(trsyncIniPath());
    if (!out.open(IO_WriteOnly | IO_Append)) {
        in.close();
        return;
    }

    TQTextStream tsi(&in);
    TQTextStream tso(&out);
    while (!tsi.eof()) tso << tsi.readLine() << "\n";
    in.close();
    out.close();

    TQMessageBox::information(this, "trsync", "Session imported");
    loadSessions();
}

void MainWindow::slotSessionExport()
{
    const TQString ses = currentSessionName();
    if (ses.isEmpty()) return;

    const TQString suggested = ses + ".trsync";
    const TQString filename = TQFileDialog::getSaveFileName(suggested, "*.trsync", this, 0, "Export");
    if (filename.isEmpty()) return;

    saveUiToSession(ses);

    TQFile in(trsyncIniPath());
    if (!in.open(IO_ReadOnly)) return;

    TQFile out(filename);
    if (!out.open(IO_WriteOnly | IO_Truncate)) {
        in.close();
        return;
    }

    TQTextStream tsi(&in);
    TQTextStream tso(&out);

    int inGroup = 0;
    const TQString hdr = "[" + ses + "]";
    while (!tsi.eof()) {
        const TQString line = tsi.readLine();
        const TQString s = line.stripWhiteSpace();
        if (!s.isEmpty() && s[0] == '[') {
            inGroup = (s == hdr);
        }
        if (inGroup) tso << line << "\n";
    }

    in.close();
    out.close();

    TQMessageBox::information(this, "trsync", "Session exported");
}

void MainWindow::slotSetModeToggled(bool on)
{
    // Enable/disable the set controls based on checkbox state
    if (m_setList) {
        m_setList->setEnabled(on);
    }
    if (m_setSessionSelector) {
        m_setSessionSelector->setEnabled(on);
    }
    if (m_btnAddToSet) {
        m_btnAddToSet->setEnabled(on);
    }
    if (m_btnRemoveFromSet) {
        m_btnRemoveFromSet->setEnabled(on);
    }
    
    const TQString ses = currentSessionName();
    if (ses.isEmpty()) return;
    saveUiToSession(ses);
    
    // Refresh the list and selector when enabled
    if (on) {
        updateSetListFromSettings(ses);
        updateSessionSelector();
    }
}

void MainWindow::slotSetListItemChanged(TQListViewItem *item)
{
    if (!item) return;
    
    // Save the current state when a checkbox is toggled
    const TQString ses = currentSessionName();
    if (ses.isEmpty()) return;
    
    TQCheckListItem *ci = dynamic_cast<TQCheckListItem *>(item);
    if (!ci) return;
    
    const TQString sessionName = item->text(1);
    if (sessionName.isEmpty()) return;
    
    // Save to settings
    IniFile ini;
    ini.load(trsyncIniPath());
    IniGroup grp = ini.group(ses);
    
    const TQString key = "set_session_enabled_" + sessionName;
    grp.writeBool(key, ci->isOn());
    
    ini.save(trsyncIniPath());
}

void MainWindow::slotAddSessionToSet()
{
    if (!m_setSessionSelector) return;
    if (!m_setList) return;
    
    const TQString sessionToAdd = m_setSessionSelector->currentText();
    if (sessionToAdd.isEmpty()) return;
    
    // Check if session is already in the list
    TQListViewItem *it = m_setList->firstChild();
    while (it) {
        if (it->text(1) == sessionToAdd) {
            TQMessageBox::information(this, "trsync", "This session is already in the set");
            return;
        }
        it = it->nextSibling();
    }
    
    // Add the session to the list
    TQCheckListItem *item = new TQCheckListItem(m_setList, "", TQCheckListItem::CheckBox);
    item->setText(1, sessionToAdd);
    item->setOn(true); // Enable by default
    
    // Save to settings
    const TQString ses = currentSessionName();
    if (!ses.isEmpty()) {
        IniFile ini;
        ini.load(trsyncIniPath());
        IniGroup grp = ini.group(ses);
        
        const TQString key = "set_session_enabled_" + sessionToAdd;
        grp.writeBool(key, true);
        
        ini.save(trsyncIniPath());
    }
    
    // Update the selector to remove the added session
    updateSessionSelector();
}

void MainWindow::slotRemoveSessionFromSet()
{
    if (!m_setList) return;
    
    TQListViewItem *item = m_setList->selectedItem();
    if (!item) {
        TQMessageBox::information(this, "trsync", "Please select a session to remove");
        return;
    }
    
    const TQString sessionName = item->text(1);
    if (sessionName.isEmpty()) return;
    
    // Remove from settings
    const TQString ses = currentSessionName();
    if (!ses.isEmpty()) {
        IniFile ini;
        ini.load(trsyncIniPath());
        IniGroup grp = ini.group(ses);
        
        const TQString key = "set_session_enabled_" + sessionName;
        grp.writeString(key, "");  // Write empty string to remove
        
        ini.save(trsyncIniPath());
    }
    
    // Remove from list
    delete item;
    
    // Update the selector to add back the removed session
    updateSessionSelector();
}

void MainWindow::updateSetListFromSettings(const TQString &session)
{
    if (!m_setList) return;
    m_setList->clear();

    IniFile ini;
    ini.load(trsyncIniPath());
    IniGroup grp = ini.group(session);

    // Only show sessions that have been explicitly added (have a setting key)
    const TQStringList groups = ini.groups();
    for (TQStringList::ConstIterator it = groups.begin(); it != groups.end(); ++it) {
        if ((*it) == "__CONFIG") continue;
        if ((*it).isEmpty()) continue;
        if ((*it) == session) continue;

        const TQString key = "set_session_enabled_" + (*it);
        
        // Check if key exists by reading it - if it's not empty, it was set
        const TQString val = grp.readString(key, "");
        if (!val.isEmpty()) {
            const bool enabled = grp.readBool(key, false);
            
            TQCheckListItem *item = new TQCheckListItem(m_setList, "", TQCheckListItem::CheckBox);
            item->setText(1, *it);
            item->setOn(enabled);
        }
    }

    // Update the session selector after updating the list
    updateSessionSelector();
}

void MainWindow::saveSetListToSettings(IniFile &ini, const TQString &session)
{
    if (!m_setList) return;

    IniGroup grp = ini.group(session);
    TQListViewItem *it = m_setList->firstChild();
    while (it) {
        TQCheckListItem *ci = dynamic_cast<TQCheckListItem *>(it);
        const TQString name = it->text(1);
        if (ci && !name.isEmpty()) {
            const TQString key = "set_session_enabled_" + name;
            if (ci->isOn()) grp.writeBool(key, true);
        }
        it = it->nextSibling();
    }
}

void MainWindow::updateSessionSelector()
{
    if (!m_setSessionSelector) return;
    
    const TQString currentSes = currentSessionName();
    if (currentSes.isEmpty()) return;
    
    // Get all available sessions
    IniFile ini;
    ini.load(trsyncIniPath());
    const TQStringList allSessions = ini.groups();
    
    // Get sessions already in the set list
    TQStringList sessionsInSet;
    if (m_setList) {
        TQListViewItem *it = m_setList->firstChild();
        while (it) {
            const TQString name = it->text(1);
            if (!name.isEmpty()) {
                sessionsInSet << name;
            }
            it = it->nextSibling();
        }
    }
    
    // Clear and repopulate the selector with available sessions
    m_setSessionSelector->clear();
    
    for (TQStringList::ConstIterator it = allSessions.begin(); it != allSessions.end(); ++it) {
        const TQString &ses = *it;
        
        // Skip reserved names, empty names, and current session
        if (ses == "__CONFIG") continue;
        if (ses.isEmpty()) continue;
        if (ses == currentSes) continue;
        
        // Only add if not already in the set list
        if (!sessionsInSet.contains(ses)) {
            m_setSessionSelector->insertItem(ses);
        }
    }
}

static int isReservedSessionName(const TQString &name)
{
    if (name.isEmpty()) return 1;
    if (name == "__CONFIG") return 1;
    if (name.find('/') >= 0) return 1;
    return 0;
}

static int runCommandSync(const TQString &cmdLine, TQString *out, TQString *err)
{
    if (out) *out = TQString();
    if (err) *err = TQString();

    const TQStringList argv = parseShellLikeArgs(cmdLine);
    if (argv.isEmpty()) return -1;

    TQProcess p(argv, 0, 0);
    p.setCommunication(TQProcess::Stdout | TQProcess::Stderr);

    if (!p.start()) return -1;

    while (p.isRunning()) {
        tqApp->processEvents();
    }

    if (out) *out = TQString::fromLocal8Bit(p.readStdout());
    if (err) *err = TQString::fromLocal8Bit(p.readStderr());
    return p.exitStatus();
}

static TQString trsyncConfigDir()
{
    TQString dir = TQDir::homeDirPath() + "/.config/trsync";
    if (TQDir(dir).exists()) return dir;

    dir = TQDir::homeDirPath() + "/.trsync";
    if (TQDir(dir).exists()) return dir;

    dir = TQDir::homeDirPath() + "/.config/trsync";
    TQDir().mkdir(dir);
    return dir;
}

static TQString trsyncIniPath()
{
    return trsyncConfigDir() + "/trsync.ini";
}

MainWindow::MainWindow()
    : TQMainWindow(0, "main_window")
    , m_sessions(0)
    , m_source(0)
    , m_dest(0)
    , m_additional(0)

    , m_setList(0)
    , m_is_set(0)
    , m_setSessionSelector(0)
    , m_btnAddToSet(0)
    , m_btnRemoveFromSet(0)

    , m_notes(0)
    , m_check_com_before(0)
    , m_com_before(0)
    , m_check_com_halt(0)
    , m_check_com_after(0)
    , m_com_after(0)
    , m_check_com_onerror(0)
    , m_check_browse_files(0)
    , m_check_superuser(0)

    , m_check_time(0)
    , m_check_perm(0)
    , m_check_owner(0)
    , m_check_group(0)
    , m_check_delete(0)
    , m_check_onefs(0)
    , m_check_verbose(0)
    , m_check_progr(0)
    , m_check_exist(0)
    , m_check_size(0)
    , m_check_skipnew(0)
    , m_check_windows(0)

    , m_check_sum(0)
    , m_check_compr(0)
    , m_check_dev(0)
    , m_check_update(0)
    , m_check_keepart(0)
    , m_check_mapuser(0)
    , m_check_symlink(0)
    , m_check_hardlink(0)
    , m_check_backup(0)
    , m_check_itemized(0)
    , m_check_norecur(0)
    , m_check_protectargs(0)

    , m_run(0)
    , m_stop(0)
    , m_output(0)

    , m_btnSwap(0)
    , m_rsync(0)
    , m_rsyncDialog(0)
    , m_logFile(0)
    , m_prefCustomLogDirEdit(0)
    , m_prefCbLog(0)
    , m_prefCbOverwrite(0)
    , m_prefCbCustomLogDir(0)
    , m_prefBtnBrowseLogDir(0)

    , m_dryrunNext(0)
    , m_runningDryrun(0)

    , m_cfg_command("rsync")
    , m_cfg_output(false)
    , m_cfg_remember(true)
    , m_cfg_errorlist(false)
    , m_cfg_log(false)
    , m_cfg_log_overwrite(false)
    , m_cfg_fastscroll(true)
    , m_cfg_switchbutton(false)
    , m_cfg_trayicon(false)
    , m_cfg_custom_log_dir_enabled(false)
    , m_cfg_custom_log_dir("")

    , m_isExecutingSet(false)
    , m_currentSetIndex(0)
    , m_setHadErrors(false)

    , m_cmdlineExecute(false)
    , m_cmdlineStayOpen(false)
    , m_cmdlineImport(false)
    , m_pollTimer(0)
    , m_lastReadOffset(0)
    , m_comboEncryption(0)
    , m_lblWarningNotInstalled(0)
    , m_lblPassword(0)
    , m_editPassword(0)
    , m_btnShowPassword(0)
    , m_checkEncryptNames(0)
    , m_mountedTempDir("")
    , m_isBackupRun(false)
    , m_currentConfigPath("")
    , m_destConfigDir("")
{
    TQPopupMenu *menuFile = new TQPopupMenu(this);
    menuFile->insertItem(loadEmbeddedIcon(icon_source, icon_source_len), "Browse Source", this, SLOT(slotBrowseSource()));
    menuFile->insertItem(loadEmbeddedIcon(icon_target, icon_target_len), "Browse Destination", this, SLOT(slotBrowseDest()));
    menuFile->insertItem(loadEmbeddedIcon(icon_swap, icon_swap_len), "Switch source with destination", this, SLOT(slotMenuSwapSourceDest()));
    menuFile->insertSeparator();
    menuFile->insertItem(loadEmbeddedIcon(icon_simulation, icon_simulation_len), "Simulation", this, SLOT(slotSimulate()));
    menuFile->insertItem(loadEmbeddedIcon(icon_synchronize, icon_synchronize_len), "Execute", this, SLOT(slotRun()));
    menuFile->insertItem(loadEmbeddedIcon(icon_command_line, icon_command_line_len), "Rsync command line", this, SLOT(slotShowRsyncCommandLine()));
    menuFile->insertItem(loadEmbeddedIcon(icon_settings, icon_settings_len), "Preferences", this, SLOT(slotPreferences()));
    menuFile->insertSeparator();
    menuFile->insertItem(loadEmbeddedIcon(icon_quit, icon_quit_len), "Quit", this, SLOT(close()));

    TQPopupMenu *menuSessions = new TQPopupMenu(this);
    menuSessions->insertItem(loadEmbeddedIcon(icon_add, icon_add_len), "Add", this, SLOT(slotSessionAdd()));
    menuSessions->insertItem(loadEmbeddedIcon(icon_delete, icon_delete_len), "Delete", this, SLOT(slotSessionDelete()));
    menuSessions->insertItem(loadEmbeddedIcon(icon_import, icon_import_len), "Import", this, SLOT(slotSessionImport()));
    menuSessions->insertItem(loadEmbeddedIcon(icon_export, icon_export_len), "Export", this, SLOT(slotSessionExport()));

    TQPopupMenu *menuHelp = new TQPopupMenu(this);
    menuHelp->insertItem(loadEmbeddedIcon(icon_info, icon_info_len), "About", this, SLOT(slotAbout()));
    menuHelp->insertItem(loadEmbeddedIcon(icon_help, icon_help_len), "Rsync info", this, SLOT(slotRsyncInfo()));

    menuBar()->insertItem("File", menuFile);
    menuBar()->insertItem("Sessions", menuSessions);
    menuBar()->insertItem("Help", menuHelp);

    TQWidget *cw = new TQWidget(this);
    setCentralWidget(cw);

    TQVBoxLayout *v = new TQVBoxLayout(cw);

    TQWidget *topbar = new TQWidget(cw);
    TQHBoxLayout *topl = new TQHBoxLayout(topbar, 0, 6);

    m_sessions = new TQComboBox(topbar);
    topl->addWidget(m_sessions, 1);

    TQPushButton *btnAdd = new TQPushButton(topbar);
    btnAdd->setIconSet(loadEmbeddedIcon(icon_add, icon_add_len));
    btnAdd->setMaximumWidth(35);
    TQToolTip::add(btnAdd, "Click to add a new session");
    topl->addWidget(btnAdd);

    TQPushButton *btnDel = new TQPushButton(topbar);
    btnDel->setIconSet(loadEmbeddedIcon(icon_delete, icon_delete_len));
    btnDel->setMaximumWidth(35);
    TQToolTip::add(btnDel, "Click to delete the current session");
    topl->addWidget(btnDel);

    TQPushButton *btnSim = new TQPushButton(topbar);
    btnSim->setIconSet(loadEmbeddedIcon(icon_simulation, icon_simulation_len));
    btnSim->setMaximumWidth(35);
    TQToolTip::add(btnSim, "Show what would have been done, but actually do nothing (\"dry-run\" in rsync language)");
    topl->addWidget(btnSim);

    TQPushButton *btnStart = new TQPushButton(topbar);
    btnStart->setIconSet(loadEmbeddedIcon(icon_synchronize, icon_synchronize_len));
    btnStart->setMaximumWidth(35);
    TQToolTip::add(btnStart, "Make a full run (go!)");
    topl->addWidget(btnStart);

    v->addWidget(topbar, 0);

    TQTabWidget *tabs = new TQTabWidget(cw);
    v->addWidget(tabs, 1);

    m_output = new TQTextEdit(cw);
    m_output->setReadOnly(true);
    m_output->hide();
    v->addWidget(m_output, 0);

    TQWidget *tabBasic = new TQWidget(tabs);
    tabs->addTab(tabBasic, "Basic options");

    TQWidget *tabAdvanced = new TQWidget(tabs);
    tabs->addTab(tabAdvanced, "Advanced options");

    TQWidget *tabExtra = new TQWidget(tabs);
    tabs->addTab(tabExtra, "Extra options");

    TQWidget *tabSet = new TQWidget(tabs);
    tabs->addTab(tabSet, "Set options");

    TQWidget *tabEncryption = new TQWidget(tabs);
    tabs->addTab(tabEncryption, "Encryption");

    TQGridLayout *basic = new TQGridLayout(tabBasic, 9, 2, 6, 6);
    basic->setColStretch(0, 1);
    basic->setColStretch(1, 1);

    TQWidget *srcHeader = new TQWidget(tabBasic);
    TQHBoxLayout *srcHl = new TQHBoxLayout(srcHeader, 0, 6);
    TQLabel *srcLbl = new TQLabel("Source and Destination:", srcHeader);
    srcHl->addWidget(srcLbl);
    TQPushButton *srcInfo = new TQPushButton(srcHeader);
    srcInfo->setIconSet(loadEmbeddedIcon(icon_help, icon_help_len));
    srcInfo->setMaximumWidth(32);
    connect(srcInfo, SIGNAL(clicked()), this, SLOT(slotShowTrailingSlashInfo()));
    srcHl->addWidget(srcInfo);
    srcHl->addStretch(1);
    m_btnSwap = new TQPushButton(srcHeader);
    m_btnSwap->setToggleButton(true);
    m_btnSwap->setIconSet(loadEmbeddedIcon(icon_swap, icon_swap_len));
    m_btnSwap->setMaximumWidth(35);
    TQToolTip::add(m_btnSwap, "Click here to switch the source with the destination directory");
    srcHl->addWidget(m_btnSwap);

    basic->addMultiCellWidget(srcHeader, 0, 0, 0, 1);

    TQWidget *srcBody = new TQWidget(tabBasic);
    TQGridLayout *sd = new TQGridLayout(srcBody, 2, 2, 0, 6);
    sd->setColStretch(0, 1);
    sd->setColStretch(1, 0);

    m_source = new TQLineEdit(srcBody);
    TQPushButton *browseSrc = new TQPushButton(srcBody);
    browseSrc->setIconSet(loadEmbeddedIcon(icon_source, icon_source_len));
    browseSrc->setMaximumWidth(35);
    TQToolTip::add(browseSrc, "Click to open the file browser");

    m_dest = new TQLineEdit(srcBody);
    TQPushButton *browseDst = new TQPushButton(srcBody);
    browseDst->setIconSet(loadEmbeddedIcon(icon_target, icon_target_len));
    browseDst->setMaximumWidth(35);
    TQToolTip::add(browseDst, "Click to open the file browser");

    sd->addWidget(m_source, 0, 0);
    sd->addWidget(browseSrc, 0, 1);

    sd->addWidget(m_dest, 1, 0);
    sd->addWidget(browseDst, 1, 1);

    basic->addMultiCellWidget(srcBody, 1, 1, 0, 1);

    connect(browseSrc, SIGNAL(clicked()), this, SLOT(slotBrowseSource()));
    connect(browseDst, SIGNAL(clicked()), this, SLOT(slotBrowseDest()));
    connect(m_btnSwap, SIGNAL(toggled(bool)), this, SLOT(slotSwapSourceDest(bool)));

    m_check_time = new TQCheckBox("Preserve time", tabBasic);
    m_check_time->setChecked(true);  // Active par défaut dans GTK
    TQToolTip::add(m_check_time, "Preserve time");
    m_check_perm = new TQCheckBox("Preserve permissions", tabBasic);
    TQToolTip::add(m_check_perm, "Preserve permissions");
    basic->addWidget(m_check_time, 2, 0);
    basic->addWidget(m_check_perm, 2, 1);

    m_check_owner = new TQCheckBox("Preserve owner", tabBasic);
    TQToolTip::add(m_check_owner, "Preserve owner");
    m_check_group = new TQCheckBox("Preserve group", tabBasic);
    TQToolTip::add(m_check_group, "Preserve group");
    basic->addWidget(m_check_owner, 3, 0);
    basic->addWidget(m_check_group, 3, 1);

    TQFrame *sepBasic = new TQFrame(tabBasic);
    sepBasic->setFrameShape(TQFrame::HLine);
    sepBasic->setFrameShadow(TQFrame::Sunken);
    basic->addMultiCellWidget(sepBasic, 4, 4, 0, 1);

    m_check_delete = new TQCheckBox("Delete on destination", tabBasic);
    TQToolTip::add(m_check_delete, "Delete files in destination which are not present in the source");
    m_check_onefs = new TQCheckBox("Do not leave filesystem", tabBasic);
    TQToolTip::add(m_check_onefs, "Do not cross filesystem boundaries");
    basic->addWidget(m_check_delete, 5, 0);
    basic->addWidget(m_check_onefs, 5, 1);

    m_check_verbose = new TQCheckBox("Verbose", tabBasic);
    m_check_verbose->setChecked(true);  // Active par défaut dans GTK
    TQToolTip::add(m_check_verbose, "Show more information");
    m_check_progr = new TQCheckBox("Show transfer progress", tabBasic);
    m_check_progr->setChecked(true);  // Active par défaut dans GTK
    TQToolTip::add(m_check_progr, "Show transfer progress");
    basic->addWidget(m_check_verbose, 6, 0);
    basic->addWidget(m_check_progr, 6, 1);

    m_check_exist = new TQCheckBox("Ignore existing", tabBasic);
    TQToolTip::add(m_check_exist, "Ignore files which already exist in the destination");
    m_check_size = new TQCheckBox("Size only", tabBasic);
    TQToolTip::add(m_check_size, "Check size only, ignore time and checksum");
    basic->addWidget(m_check_exist, 7, 0);
    basic->addWidget(m_check_size, 7, 1);

    m_check_skipnew = new TQCheckBox("Skip newer", tabBasic);
    TQToolTip::add(m_check_skipnew, "Do not update newer files");
    m_check_windows = new TQCheckBox("Windows compatibility", tabBasic);
    TQToolTip::add(m_check_windows, "Provides workaround for a windows FAT filesystem limitation");
    basic->addWidget(m_check_skipnew, 8, 0);
    basic->addWidget(m_check_windows, 8, 1);

    basic->setRowStretch(8, 1);

    TQGridLayout *adv = new TQGridLayout(tabAdvanced, 9, 2, 6, 6);
    adv->setColStretch(0, 1);
    adv->setColStretch(1, 1);

    m_check_sum = new TQCheckBox("Always checksum", tabAdvanced);
    TQToolTip::add(m_check_sum, "Always compare file contents (by checksum)");
    m_check_compr = new TQCheckBox("Compress file data", tabAdvanced);
    TQToolTip::add(m_check_compr, "Compress file data when transferring (useful only if at least one side is remote)");
    adv->addWidget(m_check_sum, 0, 0);
    adv->addWidget(m_check_compr, 0, 1);

    m_check_dev = new TQCheckBox("Preserve devices", tabAdvanced);
    TQToolTip::add(m_check_dev, "Preserve devices");
    m_check_update = new TQCheckBox("Only update existing files", tabAdvanced);
    TQToolTip::add(m_check_update, "Only update files which already exist on destination");
    adv->addWidget(m_check_dev, 1, 0);
    adv->addWidget(m_check_update, 1, 1);

    m_check_keepart = new TQCheckBox("Keep partially transferred files", tabAdvanced);
    TQToolTip::add(m_check_keepart, "Keep partially transferred files");
    m_check_mapuser = new TQCheckBox("Don't map uid/gid values", tabAdvanced);
    TQToolTip::add(m_check_mapuser, "Keep numeric uid/gid instead of mapping user names and group names");
    adv->addWidget(m_check_keepart, 2, 0);
    adv->addWidget(m_check_mapuser, 2, 1);

    m_check_symlink = new TQCheckBox("Copy symlinks as symlinks", tabAdvanced);
    TQToolTip::add(m_check_symlink, "Symbolic links are copied as such, do not copy link target file");
    m_check_hardlink = new TQCheckBox("Copy hardlinks as hardlinks", tabAdvanced);
    TQToolTip::add(m_check_hardlink, "Hard links are copied as such, do not copy link target file");
    adv->addWidget(m_check_symlink, 3, 0);
    adv->addWidget(m_check_hardlink, 3, 1);

    m_check_backup = new TQCheckBox("Make backups", tabAdvanced);
    TQToolTip::add(m_check_backup, "Make backups of existing files on the destination");
    m_check_itemized = new TQCheckBox("Show itemized changes list", tabAdvanced);
    TQToolTip::add(m_check_itemized, "Show additional information on every changed file");
    adv->addWidget(m_check_backup, 4, 0);
    adv->addWidget(m_check_itemized, 4, 1);

    m_check_norecur = new TQCheckBox("Disable recursion", tabAdvanced);
    TQToolTip::add(m_check_norecur, "If checked the subdirectories of the source folder will be ignored");
    m_check_protectargs = new TQCheckBox("Protect remote args", tabAdvanced);
    m_check_protectargs->setChecked(true);  // Active par défaut dans GTK
    TQToolTip::add(m_check_protectargs, "Protect remote arguments from shell expansion");
    adv->addWidget(m_check_norecur, 5, 0);
    adv->addWidget(m_check_protectargs, 5, 1);

    adv->addMultiCellWidget(new TQLabel("Additional options:", tabAdvanced), 6, 6, 0, 1);

    m_additional = new TQTextEdit(tabAdvanced);
    adv->addMultiCellWidget(m_additional, 7, 8, 0, 1);
    adv->setRowStretch(8, 1);

    TQGridLayout *ext = new TQGridLayout(tabExtra, 12, 2, 6, 6);
    ext->setColStretch(0, 0);
    ext->setColStretch(1, 1);

    m_check_com_before = new TQCheckBox("Execute this command before rsync:", tabExtra);
    TQToolTip::add(m_check_com_before, "Click on this item if you want to run a command before syncing. Can be useful, for instance, to mount a filesystem before starting or to do some cleanup.");
    m_com_before = new TQLineEdit(tabExtra);
    TQToolTip::add(m_com_before, "Command to run before rsync");
    ext->addWidget(m_check_com_before, 0, 0);
    ext->addWidget(m_com_before, 0, 1);

    m_check_com_halt = new TQCheckBox("Halt on failure", tabExtra);
    TQToolTip::add(m_check_com_halt, "When running the \"before\" command, if it fails for some reason, don't run rsync and halt");
    ext->addMultiCellWidget(m_check_com_halt, 1, 1, 0, 1);

    ext->addMultiCellWidget(new TQLabel("", tabExtra), 2, 2, 0, 1);

    m_check_com_after = new TQCheckBox("Execute this command after rsync:", tabExtra);
    TQToolTip::add(m_check_com_after, "Click on this item if you want to run a command after syncing. Can be useful, for instance, to unmount a filesystem at the end or to do some cleanup.");
    m_com_after = new TQLineEdit(tabExtra);
    TQToolTip::add(m_com_after, "Command to run after rsync");
    ext->addWidget(m_check_com_after, 3, 0);
    ext->addWidget(m_com_after, 3, 1);

    m_check_com_onerror = new TQCheckBox("On rsync error only", tabExtra);
    TQToolTip::add(m_check_com_onerror, "Execute the \"after\" command only if syncronization had errors.");
    ext->addMultiCellWidget(m_check_com_onerror, 4, 4, 0, 1);

    ext->addMultiCellWidget(new TQLabel("", tabExtra), 5, 5, 0, 1);

    m_check_browse_files = new TQCheckBox("Browse files instead of folders", tabExtra);
    TQToolTip::add(m_check_browse_files, "By setting this switch, the browse source and destination buttons will open a dialog for selecting files instead of folders");
    m_check_superuser = new TQCheckBox("Run as superuser", tabExtra);
    TQToolTip::add(m_check_superuser, "Run rsync with superuser (root, administrator) privileges. Use this if you need to access system files, for example when doing a full system backup.");
    ext->addWidget(m_check_browse_files, 6, 0);
    ext->addWidget(m_check_superuser, 6, 1);

    ext->addMultiCellWidget(new TQLabel("", tabExtra), 7, 7, 0, 1);

    ext->addMultiCellWidget(new TQLabel("Notes:", tabExtra), 8, 8, 0, 1);
    m_notes = new TQLineEdit(tabExtra);
    TQToolTip::add(m_notes, "Free text notes on the current session");
    ext->addMultiCellWidget(m_notes, 9, 9, 0, 1);

    ext->setRowStretch(10, 1);

    TQVBoxLayout *set = new TQVBoxLayout(tabSet, 6, 6);
    m_is_set = new TQCheckBox("This session is a set", tabSet);
    set->addWidget(m_is_set);
    
    // Add session selector and buttons
    TQWidget *setControls = new TQWidget(tabSet);
    TQHBoxLayout *setCtrlLayout = new TQHBoxLayout(setControls, 0, 6);
    
    setCtrlLayout->addWidget(new TQLabel("Available sessions:", setControls));
    m_setSessionSelector = new TQComboBox(setControls);
    setCtrlLayout->addWidget(m_setSessionSelector, 1);
    
    m_btnAddToSet = new TQPushButton("Add", setControls);
    m_btnAddToSet->setMaximumWidth(60);
    setCtrlLayout->addWidget(m_btnAddToSet);
    
    set->addWidget(setControls);
    
    // List of sessions in the set
    set->addWidget(new TQLabel("Sessions in this set:", tabSet));

    m_setList = new TQListView(tabSet);
    m_setList->setAllColumnsShowFocus(true);
    m_setList->setRootIsDecorated(false);
    m_setList->setSorting(-1);
    m_setList->addColumn("Enabled");
    m_setList->addColumn("Session");
    m_setList->setEnabled(false); // Disabled by default until "is_set" is checked
    set->addWidget(m_setList, 1);
    
    // Remove button below the list
    TQWidget *removeControls = new TQWidget(tabSet);
    TQHBoxLayout *removeLayout = new TQHBoxLayout(removeControls, 0, 6);
    removeLayout->addStretch(1);
    m_btnRemoveFromSet = new TQPushButton("Remove", removeControls);
    m_btnRemoveFromSet->setMaximumWidth(80);
    removeLayout->addWidget(m_btnRemoveFromSet);
    set->addWidget(removeControls);
    
    // Connect signals
    connect(m_setList, SIGNAL(clicked(TQListViewItem*)), this, SLOT(slotSetListItemChanged(TQListViewItem*)));
    connect(m_btnAddToSet, SIGNAL(clicked()), this, SLOT(slotAddSessionToSet()));
    connect(m_btnRemoveFromSet, SIGNAL(clicked()), this, SLOT(slotRemoveSessionFromSet()));

    // Encryption Tab layout
    TQVBoxLayout *encLayout = new TQVBoxLayout(tabEncryption, 12, 6);

    TQHBoxLayout *hlCombo = new TQHBoxLayout(encLayout, 6);
    hlCombo->addWidget(new TQLabel("Activate client-side encryption:", tabEncryption));
    m_comboEncryption = new TQComboBox(tabEncryption);
    m_comboEncryption->insertItem("none");
    m_comboEncryption->insertItem("encfs");
    m_comboEncryption->insertItem("gocryptfs");
    hlCombo->addWidget(m_comboEncryption, 1);
    hlCombo->addStretch(1);

    m_lblWarningNotInstalled = new TQLabel(tabEncryption);
    m_lblWarningNotInstalled->setPaletteForegroundColor(TQColor(255, 0, 0));
    m_lblWarningNotInstalled->hide();
    encLayout->addWidget(m_lblWarningNotInstalled);

    TQHBoxLayout *hlPassword = new TQHBoxLayout(encLayout, 6);
    m_lblPassword = new TQLabel("Key / Password:", tabEncryption);
    hlPassword->addWidget(m_lblPassword);
    
    m_editPassword = new TQLineEdit(tabEncryption);
    m_editPassword->setEchoMode(TQLineEdit::Password);
    m_editPassword->setEnabled(false);
    hlPassword->addWidget(m_editPassword, 1);

    m_btnShowPassword = new TQPushButton(tabEncryption);
    m_btnShowPassword->setFixedWidth(28);
    m_btnShowPassword->setIconSet(loadEmbeddedIcon(icon_show_password, icon_show_password_len));
    m_btnShowPassword->setToggleButton(true);
    m_btnShowPassword->setEnabled(false);
    hlPassword->addWidget(m_btnShowPassword);

    m_checkEncryptNames = new TQCheckBox("Encrypt file and directory names too", tabEncryption);
    m_checkEncryptNames->setChecked(true);
    m_checkEncryptNames->setEnabled(false);
    encLayout->addWidget(m_checkEncryptNames);

    encLayout->addStretch(1);

    // Connect encryption signals
    connect(m_comboEncryption, SIGNAL(activated(int)), this, SLOT(slotEncryptionTypeChanged(int)));
    connect(m_btnShowPassword, SIGNAL(toggled(bool)), this, SLOT(slotToggleShowPassword()));

    m_rsync = new RsyncProcess(this);
    m_rsyncDialog = new RsyncDialogNew(this);

    m_pollTimer = new TQTimer(this);
    connect(m_pollTimer, SIGNAL(timeout()), this, SLOT(slotPollLogFile()));

    connect(m_rsync, SIGNAL(sigFinished(int,int)), this, SLOT(slotRsyncFinished(int,int)));
    
    connect(m_rsyncDialog, SIGNAL(canceled()), this, SLOT(slotStop()));
    connect(m_rsyncDialog, SIGNAL(pauseRequested()), this, SLOT(slotPauseRsync()));
    connect(m_rsyncDialog, SIGNAL(resumeRequested()), this, SLOT(slotResumeRsync()));

    connect(btnStart, SIGNAL(clicked()), this, SLOT(slotRun()));
    connect(btnSim, SIGNAL(clicked()), this, SLOT(slotSimulate()));
    connect(btnAdd, SIGNAL(clicked()), this, SLOT(slotSessionAdd()));
    connect(btnDel, SIGNAL(clicked()), this, SLOT(slotSessionDelete()));
    connect(m_sessions, SIGNAL(activated(int)), this, SLOT(slotSessionChanged(int)));
    connect(m_sessions, SIGNAL(highlighted(int)), this, SLOT(slotSessionChanged(int)));
    connect(m_is_set, SIGNAL(toggled(bool)), this, SLOT(slotSetModeToggled(bool)));

    setCaption("TRsync - Trinity Desktop Rsync GUI");
    TQPixmap appPix;
    if (appPix.loadFromData(icon_app, icon_app_len, "PNG")) {
        setIcon(appPix);
    }

    setMinimumSize(800, 520);
    resize(800, 520);

    loadPreferences();
    applyPreferencesToUi();

    loadSessions();

    applyRememberedSession();
    loadSessionToUi(currentSessionName());
}

MainWindow::~MainWindow()
{
    unmountTempDir(m_mountedTempDir);
    if (m_rsync) m_rsync->stop();
    if (m_logFile) {
        if (m_logFile->isOpen()) m_logFile->close();
        delete m_logFile;
        m_logFile = 0;
    }

    const TQString s = currentSessionName();
    if (!s.isEmpty()) saveUiToSession(s);
}

TQString MainWindow::currentSessionName() const
{
    const int idx = m_sessions ? m_sessions->currentItem() : -1;
    if (idx < 0) return TQString();
    return m_sessions->text(idx);
}

void MainWindow::loadSessions()
{
    m_sessions->clear();

    IniFile ini;
    ini.load(trsyncIniPath());
    TQStringList groups = ini.groups();
    if (groups.isEmpty()) groups << "default";

    bool hasAny = false;
    for (TQStringList::ConstIterator it = groups.begin(); it != groups.end(); ++it) {
        if ((*it) == "__CONFIG") continue;
        if ((*it).isEmpty()) continue;
        m_sessions->insertItem(*it);
        hasAny = true;
    }

    if (!hasAny) m_sessions->insertItem("default");

    int idxDefault = -1;
    for (int i = 0; i < m_sessions->count(); ++i) {
        if (m_sessions->text(i) == "default") {
            idxDefault = i;
            break;
        }
    }

    if (!m_cfg_remember && idxDefault >= 0) m_sessions->setCurrentItem(idxDefault);
    else m_sessions->setCurrentItem(0);

    loadSessionToUi(currentSessionName());
}

void MainWindow::loadSessionToUi(const TQString &session)
{
    if (session.isEmpty()) return;

    if (m_btnSwap && m_btnSwap->isOn()) {
        m_btnSwap->setOn(false);
    }

    IniFile ini;
    ini.load(trsyncIniPath());
    IniGroup grp = ini.group(session);

    const bool is_set = grp.readBool("is_set", false);
    if (m_is_set) {
        m_is_set->setChecked(is_set);
        // Enable/disable set controls based on is_set value
        if (m_setList) {
            m_setList->setEnabled(is_set);
        }
        if (m_setSessionSelector) {
            m_setSessionSelector->setEnabled(is_set);
        }
        if (m_btnAddToSet) {
            m_btnAddToSet->setEnabled(is_set);
        }
        if (m_btnRemoveFromSet) {
            m_btnRemoveFromSet->setEnabled(is_set);
        }
    }
    m_source->setText(grp.readString("text_source", ""));
    m_dest->setText(grp.readString("text_dest", ""));

    if (m_notes) m_notes->setText(grp.readString("text_notes", ""));
    if (m_com_before) m_com_before->setText(grp.readString("text_com_before", ""));
    if (m_com_after) m_com_after->setText(grp.readString("text_com_after", ""));
    if (m_additional) m_additional->setText(grp.readString("text_addit", ""));

    if (m_check_time) m_check_time->setChecked(grp.readBool("check_time", false));
    if (m_check_perm) m_check_perm->setChecked(grp.readBool("check_perm", false));
    if (m_check_owner) m_check_owner->setChecked(grp.readBool("check_owner", false));
    if (m_check_group) m_check_group->setChecked(grp.readBool("check_group", false));
    if (m_check_onefs) m_check_onefs->setChecked(grp.readBool("check_onefs", false));
    if (m_check_verbose) m_check_verbose->setChecked(grp.readBool("check_verbose", false));
    if (m_check_progr) m_check_progr->setChecked(grp.readBool("check_progr", false));
    if (m_check_delete) m_check_delete->setChecked(grp.readBool("check_delete", false));
    if (m_check_exist) m_check_exist->setChecked(grp.readBool("check_exist", false));
    if (m_check_size) m_check_size->setChecked(grp.readBool("check_size", false));
    if (m_check_skipnew) m_check_skipnew->setChecked(grp.readBool("check_skipnew", false));
    if (m_check_windows) m_check_windows->setChecked(grp.readBool("check_windows", false));

    if (m_check_sum) m_check_sum->setChecked(grp.readBool("check_sum", false));
    if (m_check_symlink) m_check_symlink->setChecked(grp.readBool("check_symlink", false));
    if (m_check_hardlink) m_check_hardlink->setChecked(grp.readBool("check_hardlink", false));
    if (m_check_dev) m_check_dev->setChecked(grp.readBool("check_dev", false));
    if (m_check_update) m_check_update->setChecked(grp.readBool("check_update", false));
    if (m_check_keepart) m_check_keepart->setChecked(grp.readBool("check_keepart", false));
    if (m_check_mapuser) m_check_mapuser->setChecked(grp.readBool("check_mapuser", false));
    if (m_check_compr) m_check_compr->setChecked(grp.readBool("check_compr", false));
    if (m_check_backup) m_check_backup->setChecked(grp.readBool("check_backup", false));
    if (m_check_itemized) m_check_itemized->setChecked(grp.readBool("check_itemized", false));
    if (m_check_norecur) m_check_norecur->setChecked(grp.readBool("check_norecur", false));
    if (m_check_protectargs) m_check_protectargs->setChecked(grp.readBool("check_protectargs", false));

    if (m_check_com_before) m_check_com_before->setChecked(grp.readBool("check_com_before", false));
    if (m_check_com_halt) m_check_com_halt->setChecked(grp.readBool("check_com_halt", false));
    if (m_check_com_after) m_check_com_after->setChecked(grp.readBool("check_com_after", false));
    if (m_check_com_onerror) m_check_com_onerror->setChecked(grp.readBool("check_com_onerror", false));
    if (m_check_browse_files) m_check_browse_files->setChecked(grp.readBool("check_browse_files", false));
    if (m_check_superuser) m_check_superuser->setChecked(grp.readBool("check_superuser", false));

    if (m_btnShowPassword) {
        m_btnShowPassword->setOn(false);
        m_btnShowPassword->setIconSet(loadEmbeddedIcon(icon_show_password, icon_show_password_len));
    }
    if (m_editPassword) {
        m_editPassword->setEchoMode(TQLineEdit::Password);
    }

    if (m_comboEncryption) {
        TQString encType = grp.readString("encryption_type", "none");
        int idx = 0;
        if (encType == "encfs") idx = 1;
        else if (encType == "gocryptfs") idx = 2;
        m_comboEncryption->setCurrentItem(idx);
        slotEncryptionTypeChanged(idx);
    }
    if (m_editPassword) m_editPassword->setText(grp.readString("encryption_password", ""));
    if (m_checkEncryptNames) m_checkEncryptNames->setChecked(grp.readBool("encrypt_names", true));

    updateSetListFromSettings(session);
}

void MainWindow::saveUiToSession(const TQString &session)
{
    if (session.isEmpty()) return;

    // Ensure config directory exists
    const TQString configDir = trsyncConfigDir();
    TQDir().mkdir(configDir);
    
    IniFile ini;
    ini.load(trsyncIniPath());
    IniGroup grp = ini.group(session);

    grp.writeBool("is_set", m_is_set ? m_is_set->isChecked() : false);
    if (m_btnSwap && m_btnSwap->isOn()) {
        grp.writeString("text_source", m_dest->text());
        grp.writeString("text_dest", m_source->text());
    } else {
        grp.writeString("text_source", m_source->text());
        grp.writeString("text_dest", m_dest->text());
    }

    grp.writeString("text_notes", m_notes ? m_notes->text() : TQString(""));
    grp.writeString("text_com_before", m_com_before ? m_com_before->text() : TQString(""));
    grp.writeString("text_com_after", m_com_after ? m_com_after->text() : TQString(""));
    grp.writeString("text_addit", m_additional ? m_additional->text() : TQString(""));

    grp.writeBool("check_time", m_check_time ? m_check_time->isChecked() : false);
    grp.writeBool("check_perm", m_check_perm ? m_check_perm->isChecked() : false);
    grp.writeBool("check_owner", m_check_owner ? m_check_owner->isChecked() : false);
    grp.writeBool("check_group", m_check_group ? m_check_group->isChecked() : false);
    grp.writeBool("check_onefs", m_check_onefs ? m_check_onefs->isChecked() : false);
    grp.writeBool("check_verbose", m_check_verbose ? m_check_verbose->isChecked() : false);
    grp.writeBool("check_progr", m_check_progr ? m_check_progr->isChecked() : false);
    grp.writeBool("check_delete", m_check_delete ? m_check_delete->isChecked() : false);
    grp.writeBool("check_exist", m_check_exist ? m_check_exist->isChecked() : false);
    grp.writeBool("check_size", m_check_size ? m_check_size->isChecked() : false);
    grp.writeBool("check_skipnew", m_check_skipnew ? m_check_skipnew->isChecked() : false);
    grp.writeBool("check_windows", m_check_windows ? m_check_windows->isChecked() : false);

    grp.writeBool("check_sum", m_check_sum ? m_check_sum->isChecked() : false);
    grp.writeBool("check_symlink", m_check_symlink ? m_check_symlink->isChecked() : false);
    grp.writeBool("check_hardlink", m_check_hardlink ? m_check_hardlink->isChecked() : false);
    grp.writeBool("check_dev", m_check_dev ? m_check_dev->isChecked() : false);
    grp.writeBool("check_update", m_check_update ? m_check_update->isChecked() : false);
    grp.writeBool("check_keepart", m_check_keepart ? m_check_keepart->isChecked() : false);
    grp.writeBool("check_mapuser", m_check_mapuser ? m_check_mapuser->isChecked() : false);
    grp.writeBool("check_compr", m_check_compr ? m_check_compr->isChecked() : false);
    grp.writeBool("check_backup", m_check_backup ? m_check_backup->isChecked() : false);
    grp.writeBool("check_itemized", m_check_itemized ? m_check_itemized->isChecked() : false);
    grp.writeBool("check_norecur", m_check_norecur ? m_check_norecur->isChecked() : false);
    grp.writeBool("check_protectargs", m_check_protectargs ? m_check_protectargs->isChecked() : false);

    grp.writeBool("check_com_before", m_check_com_before ? m_check_com_before->isChecked() : false);
    grp.writeBool("check_com_halt", m_check_com_halt ? m_check_com_halt->isChecked() : false);
    grp.writeBool("check_com_after", m_check_com_after ? m_check_com_after->isChecked() : false);
    grp.writeBool("check_com_onerror", m_check_com_onerror ? m_check_com_onerror->isChecked() : false);
    grp.writeBool("check_browse_files", m_check_browse_files ? m_check_browse_files->isChecked() : false);
    grp.writeBool("check_superuser", m_check_superuser ? m_check_superuser->isChecked() : false);

    grp.writeString("encryption_type", m_comboEncryption ? m_comboEncryption->currentText() : TQString("none"));
    grp.writeString("encryption_password", m_editPassword ? m_editPassword->text() : TQString(""));
    grp.writeBool("encrypt_names", m_checkEncryptNames ? m_checkEncryptNames->isChecked() : true);

    saveSetListToSettings(ini, session);

    ini.save(trsyncIniPath());
}

void MainWindow::slotSessionChanged(int idx)
{
    (void)idx;
    loadSessionToUi(currentSessionName());

    if (m_cfg_remember) {
        const TQString ses = currentSessionName();
        if (!ses.isEmpty()) {
            IniFile ini;
            ini.load(trsyncIniPath());
            IniGroup cfg = ini.group("__CONFIG");
            cfg.writeString("last_session", ses);
            ini.save(trsyncIniPath());
        }
    }
}

void MainWindow::slotRun()
{
    const TQString session = currentSessionName();
    if (session.isEmpty()) return;

    saveUiToSession(session);

    if (m_is_set && m_is_set->isChecked() && !m_isExecutingSet) {
        m_setSessionsToRun.clear();
        if (m_setList) {
            TQListViewItem *it = m_setList->firstChild();
            while (it) {
                TQCheckListItem *ci = dynamic_cast<TQCheckListItem *>(it);
                if (ci && ci->isOn()) {
                    m_setSessionsToRun.append(it->text(1));
                }
                it = it->nextSibling();
            }
        }
        
        if (m_setSessionsToRun.isEmpty()) {
            TQMessageBox::information(this, "trsync", "No active sessions in this set");
            return;
        }
        
        m_isExecutingSet = true;
        m_originalSetSessionName = session;
        m_currentSetIndex = 0;
        m_setHadErrors = false;
        
        // We set m_runningDryrun from m_dryrunNext so it's saved for the whole set run
        m_runningDryrun = m_dryrunNext;
        m_dryrunNext = 0;
        
        executeNextSetSession();
        return;
    }

    const TQString src = m_source->text();
    const TQString dst = m_dest->text();

    if (src.isEmpty() || dst.isEmpty()) {
        TQMessageBox::warning(this, "trsync", "Source and Dest are required");
        return;
    }

    if (m_comboEncryption && m_comboEncryption->currentText() != "none" && m_check_superuser && m_check_superuser->isChecked()) {
        int ret = TQMessageBox::warning(this, "TRsync - Encryption & Superuser warning",
            "You have enabled both client-side encryption and 'Run as superuser'.\n\n"
            "By default, FUSE filesystems are not accessible to the root user.\n"
            "Unless you run TRsync itself as root (e.g. using tdesudo trsync) or "
            "have enabled 'user_allow_other' in /etc/fuse.conf, the backup will fail "
            "with a permission error.\n\n"
            "Do you want to continue anyway?",
            "Yes", "No", TQString(), 0, 1);
        if (ret == 1) {
            if (m_isExecutingSet) m_isExecutingSet = false;
            return;
        }
    }

    m_mountedTempDir = "";
    TQString actualSrc = src;
    TQString actualDst = dst;

    if (m_comboEncryption && m_comboEncryption->currentText() != "none") {
        TQString coreEncType = m_comboEncryption->currentText();
        bool isBackup = !(m_btnSwap && m_btnSwap->isOn());

        TQString binaryPath = findBinary(coreEncType);
        if (binaryPath.isEmpty()) {
            TQMessageBox::critical(this, "trsync", coreEncType + " is not installed on your system. Please install it to use encryption.");
            if (m_isExecutingSet) m_isExecutingSet = false;
            return;
        }

        TQString password = m_editPassword ? m_editPassword->text() : "";
        if (password.isEmpty()) {
            TQMessageBox::warning(this, "trsync", "Password/Key is required for client-side encryption.");
            if (m_isExecutingSet) m_isExecutingSet = false;
            return;
        }

        // Create temporary mount point directory
        TQString tempDir = "/tmp/TRsync_backup_" + session + "_" + TQString::number(getpid());
        if (!TQDir().mkdir(tempDir)) {
            TQMessageBox::critical(this, "trsync", "Failed to create temporary mount directory: " + tempDir);
            if (m_isExecutingSet) m_isExecutingSet = false;
            return;
        }

        TQString restoreSrc = src;
        if (!isBackup) {
            // In Restore mode, if the backup was created without a trailing slash, the actual 
            // encrypted root directory is the target folder (ex: /media/data1/SYNCTEST/sitestudio-test).
            // UI Source is /media/data1/SYNCTEST, UI Destination is /home/cdef/_PROJETS/sitestudio-test.
            if (!dst.endsWith("/")) {
                TQString dstPath = dst;
                while (dstPath.endsWith("/")) {
                    dstPath.truncate(dstPath.length() - 1);
                }
                int lastSlash = dstPath.findRev('/');
                TQString dirName = (lastSlash >= 0) ? dstPath.mid(lastSlash + 1) : dstPath;
                if (!restoreSrc.endsWith("/")) {
                    restoreSrc += "/";
                }
                restoreSrc += dirName;
            }
        }

        TQString configPath = trsyncConfigDir() + "/" + coreEncType + "_" + session + ".conf";
        if (coreEncType == "encfs") {
            configPath = trsyncConfigDir() + "/" + coreEncType + "_" + session + ".xml";
        }

        // If restoring and the backup folder contains the config file, use it to ensure portability
        if (!isBackup) {
            TQString backupConfig = restoreSrc + (coreEncType == "encfs" ? "/.encfs6.xml" : "/gocryptfs.conf");
            if (TQFile::exists(backupConfig)) {
                configPath = backupConfig;
            }
        }

        // Create secure password file in RAM (/dev/shm)
        TQString passFile = "/dev/shm/trsync_pass_" + session + "_" + TQString::number(getpid()) + ".tmp";
        TQFile file(passFile);
        if (!file.open(IO_WriteOnly)) {
            TQMessageBox::critical(this, "trsync", "Failed to create secure password file in RAM: " + passFile);
            TQDir().rmdir(tempDir);
            if (m_isExecutingSet) m_isExecutingSet = false;
            return;
        }
        chmod(passFile.local8Bit(), 0600);
        TQTextStream ts(&file);
        ts << password;
        file.close();

        bool success = false;
        TQString mountErr;

        if (coreEncType == "gocryptfs") {
            if (isBackup) {
                // First check if initialized, if not do it
                if (!TQFile::exists(configPath)) {
                    TQStringList initArgs;
                    initArgs << "-init" << "-reverse" << "-config" << configPath;
                    if (m_checkEncryptNames && !m_checkEncryptNames->isChecked()) {
                        initArgs << "-plainnames";
                    }
                    initArgs << "-passfile" << passFile << src;
                    TQString out, err;
                    int initRet = runCommandWithStdin("gocryptfs", initArgs, "", &out, &err);
                    if (initRet != 0) {
                        mountErr = "gocryptfs init failed: " + err + "\n" + out;
                        TQFile::remove(passFile);
                        TQDir().rmdir(tempDir);
                        TQMessageBox::critical(this, "trsync", mountErr);
                        if (m_isExecutingSet) m_isExecutingSet = false;
                        return;
                    }
                }

                // Mount reverse
                TQStringList mountArgs;
                mountArgs << "-reverse" << "-config" << configPath << "-passfile" << passFile << src << tempDir;
                TQString out, err;
                int mountRet = runCommandWithStdin("gocryptfs", mountArgs, "", &out, &err);
                if (mountRet == 0) success = true;
                else mountErr = "gocryptfs mount failed: " + err + "\n" + out;
            } else {
                // Mount normal (Restore)
                TQStringList mountArgs;
                mountArgs << "-config" << configPath << "-passfile" << passFile << restoreSrc << tempDir;
                TQString out, err;
                int mountRet = runCommandWithStdin("gocryptfs", mountArgs, "", &out, &err);
                if (mountRet == 0) success = true;
                else mountErr = "gocryptfs mount failed: " + err + "\n" + out;
            }
        } else if (coreEncType == "encfs") {
            if (isBackup) {
                TQString sourceConfig = src;
                if (!sourceConfig.endsWith("/")) {
                    sourceConfig += "/";
                }
                sourceConfig += ".encfs6.xml";

                if (!TQFile::exists(configPath) && TQFile::exists(sourceConfig)) {
                    copyFile(sourceConfig, configPath);
                    TQFile::remove(sourceConfig);
                }

                if (!TQFile::exists(configPath)) {
                    // Initialize encfs reverse mount (requires generating config in source folder first)
                    TQStringList mountArgs;
                    mountArgs << "--reverse" << "--standard" << "--stdinpass" << src << tempDir;
                    TQString out, err;
                    int mountRet = runCommandWithStdin("encfs", mountArgs, password, &out, &err);
                    if (mountRet == 0) {
                        TQString createdConfig = src;
                        if (!createdConfig.endsWith("/")) {
                            createdConfig += "/";
                        }
                        createdConfig += ".encfs6.xml";
                        
                        if (TQFile::exists(createdConfig)) {
                            copyFile(createdConfig, configPath);
                            TQFile::remove(createdConfig);
                            success = true;
                        } else {
                            mountErr = "encfs initialized but config file was not found at: " + createdConfig;
                            unmountTempDir(tempDir);
                        }
                    } else {
                        mountErr = "encfs initialization failed: " + err + "\n" + out;
                    }
                } else {
                    // Config exists, mount normally
                    TQStringList mountArgs;
                    mountArgs << "--reverse" << "--config" << configPath << "--extpass" << "cat " + passFile << src << tempDir;
                    TQString out, err;
                    int mountRet = runCommandWithStdin("encfs", mountArgs, "", &out, &err);
                    if (mountRet == 0) success = true;
                    else mountErr = "encfs mount failed: " + err + "\n" + out;
                }
            } else {
                TQStringList mountArgs;
                mountArgs << "--config" << configPath << "--extpass" << "cat " + passFile << restoreSrc << tempDir;
                TQString out, err;
                int mountRet = runCommandWithStdin("encfs", mountArgs, "", &out, &err);
                if (mountRet == 0) success = true;
                else mountErr = "encfs mount failed: " + err + "\n" + out;
            }
        }

        TQFile::remove(passFile);

        if (!success) {
            TQDir().rmdir(tempDir);
            TQMessageBox::critical(this, "trsync", mountErr);
            if (m_isExecutingSet) m_isExecutingSet = false;
            return;
        }

        m_mountedTempDir = tempDir;
        actualSrc = tempDir + "/";

        m_isBackupRun = isBackup;
        m_currentConfigPath = isBackup ? configPath : TQString("");
        m_destConfigDir = isBackup ? actualDst : TQString("");

        // Handle trailing slash logic for destination when client-side encryption backup is active
        if (isBackup && !src.endsWith("/")) {
            TQString srcPath = src;
            while (srcPath.endsWith("/")) {
                srcPath.truncate(srcPath.length() - 1);
            }
            int lastSlash = srcPath.findRev('/');
            TQString dirName = (lastSlash >= 0) ? srcPath.mid(lastSlash + 1) : srcPath;
            if (!actualDst.endsWith("/")) {
                actualDst += "/";
            }
            actualDst += dirName;
            m_destConfigDir = actualDst;
        }
    }

    TQStringList args;

    if (m_check_superuser && m_check_superuser->isChecked()) {
        TQString sudoCmd = findBinary("tdesudo");
        if (sudoCmd.isEmpty()) sudoCmd = "sudo";
        args << sudoCmd;
    }
    args << "rsync";

    args << ((m_check_norecur && m_check_norecur->isChecked()) ? "-d" : "-r");
    if (m_dryrunNext) args << "-n";

    if (m_check_time && m_check_time->isChecked()) args << "-t";
    if (m_check_perm && m_check_perm->isChecked()) args << "-p";
    if (m_check_owner && m_check_owner->isChecked()) args << "-o";
    if (m_check_group && m_check_group->isChecked()) args << "-g";
    if (m_check_onefs && m_check_onefs->isChecked()) args << "-x";
    if (m_check_verbose && m_check_verbose->isChecked()) args << "-v";
    if (m_check_progr && m_check_progr->isChecked()) args << "--progress";
    if (m_check_delete && m_check_delete->isChecked()) args << "--delete";
    if (m_check_exist && m_check_exist->isChecked()) args << "--ignore-existing";
    if (m_check_size && m_check_size->isChecked()) args << "--size-only";
    if (m_check_skipnew && m_check_skipnew->isChecked()) args << "-u";
    if (m_check_windows && m_check_windows->isChecked()) args << "--modify-window=1";

    if (m_check_sum && m_check_sum->isChecked()) args << "-c";
    if (m_check_symlink && m_check_symlink->isChecked()) args << "-l";
    if (m_check_hardlink && m_check_hardlink->isChecked()) args << "-H";
    if (m_check_dev && m_check_dev->isChecked()) args << "-D";
    if (m_check_update && m_check_update->isChecked()) args << "--existing";
    if (m_check_keepart && m_check_keepart->isChecked()) args << "--partial";
    if (m_check_mapuser && m_check_mapuser->isChecked()) args << "--numeric-ids";
    if (m_check_compr && m_check_compr->isChecked()) args << "-z";
    if (m_check_backup && m_check_backup->isChecked()) args << "-b";
    if (m_check_itemized && m_check_itemized->isChecked()) args << "-i";
    if (m_check_protectargs && m_check_protectargs->isChecked()) args << "-s";

    const TQString extraArgsText = m_additional ? m_additional->text() : TQString();
    if (!extraArgsText.isEmpty()) args += parseShellLikeArgs(extraArgsText);

    args << actualSrc;
    args << actualDst;

    TQString program;
    if (!args.isEmpty()) {
        program = args[0];
        args.remove(args.begin());
    }

    m_runningDryrun = m_dryrunNext;
    m_dryrunNext = 0;

    bool halt = false;
    if (m_check_com_before && m_check_com_before->isChecked() && m_com_before) {
        TQString out, err;
        const int st = runCommandSync(m_com_before->text(), &out, &err);
        if (st != 0) {
            if (m_check_com_halt && m_check_com_halt->isChecked()) {
                halt = true;
                if (m_isExecutingSet) m_isExecutingSet = false;
            }
        }
    }

    if (!halt) {
        const TQString ses = currentSessionName();
        if (m_cfg_log) {
            if (m_logFile) {
                if (m_logFile->isOpen()) m_logFile->close();
                delete m_logFile;
                m_logFile = 0;
            }
            TQString logDir = (m_cfg_custom_log_dir_enabled && !m_cfg_custom_log_dir.isEmpty()) ? m_cfg_custom_log_dir : trsyncConfigDir();
            trsyncMakePath(logDir);
            TQString logPath = logDir + "/" + ses + ".log";
            m_logFile = new TQFile(logPath);
            int flags = IO_WriteOnly;
            if (m_cfg_log_overwrite) {
                flags |= IO_Truncate;
            } else {
                flags |= IO_Append;
            }
            if (m_logFile->open(flags)) {
                TQTextStream stream(m_logFile);
                stream << "**** " << ses << " - " << TQDateTime::currentDateTime().toString() << "\n";
                if (m_check_com_before && m_check_com_before->isChecked() && m_com_before) {
                    stream << "** Launching BEFORE command:\n" << m_com_before->text() << "\n\n";
                }
                stream << (m_runningDryrun ? "** Launching RSYNC command (simulation mode):\n" : "** Launching RSYNC command:\n");
                stream << program;
                for (TQStringList::ConstIterator it = args.begin(); it != args.end(); ++it) {
                    stream << " " << *it;
                }
                stream << "\n\n";
            } else {
                delete m_logFile;
                m_logFile = 0;
            }
        }

        TQString cmdStr = "exec " + program;
        for (TQStringList::ConstIterator it = args.begin(); it != args.end(); ++it) {
            TQString arg = *it;
            arg.replace("'", "'\\''");
            cmdStr += " '" + arg + "'";
        }
        TQString tempLogPath = "/dev/shm/trsync_" + ses + ".tmp";
        TQFile::remove(tempLogPath);
        cmdStr += " > " + tempLogPath + " 2>&1";

        TQStringList shellArgs;
        shellArgs << "-c" << cmdStr;

        m_lastReadOffset = 0;

        if (!m_rsync->start("sh", shellArgs)) {
            TQMessageBox::critical(this, "trsync", "Failed to start rsync");
            if (m_isExecutingSet) m_isExecutingSet = false;
            if (m_logFile) {
                m_logFile->close();
                delete m_logFile;
                m_logFile = 0;
            }
            return;
        }

        m_pollTimer->start(100); // Poll every 100ms
        
        // Show rsync dialog
        m_rsyncDialog->startOperation(ses, m_runningDryrun != 0, m_isExecutingSet && m_currentSetIndex > 0);
        m_rsyncDialog->show();
    }
}

void MainWindow::slotStop()
{
    m_isExecutingSet = false;
    if (m_rsync) m_rsync->stop();
}

void MainWindow::slotRsyncStdout(const TQString &text)
{
    if (!text.isEmpty()) {
        if (m_cfg_output) m_output->append(text);
        if (m_rsyncDialog) {
            m_rsyncDialog->appendOutput(text);
        }
        if (m_logFile && m_logFile->isOpen()) {
            TQTextStream stream(m_logFile);
            stream << text;
        }
    }
}

void MainWindow::slotRsyncStderr(const TQString &text)
{
    if (!text.isEmpty()) {
        if (m_cfg_output) m_output->append(text);
        if (m_rsyncDialog) {
            m_rsyncDialog->appendOutput(text);
        }
        if (m_logFile && m_logFile->isOpen()) {
            TQTextStream stream(m_logFile);
            stream << text;
        }
    }
}

void MainWindow::slotPollLogFile()
{
    const TQString ses = currentSessionName();
    TQString tempLogPath = "/dev/shm/trsync_" + ses + ".tmp";
    TQFile file(tempLogPath);
    if (file.open(IO_ReadOnly)) {
        if (file.size() > m_lastReadOffset) {
            file.at(m_lastReadOffset);
            TQByteArray ba = file.readAll();
            m_lastReadOffset = file.size();
            if (!ba.isEmpty()) {
                TQString text = TQString::fromLocal8Bit(ba.data(), ba.size());
                if (m_cfg_output) m_output->append(text);
                if (m_rsyncDialog) {
                    m_rsyncDialog->appendOutput(text);
                }
                if (m_logFile && m_logFile->isOpen()) {
                    TQTextStream stream(m_logFile);
                    stream << text;
                }
            }
        }
        file.close();
    }
}

void MainWindow::slotRsyncFinished(int exitCode, int exitStatus)
{
    if (!m_mountedTempDir.isEmpty()) {
        unmountTempDir(m_mountedTempDir);
        m_mountedTempDir = "";
    }

    m_pollTimer->stop();
    slotPollLogFile();

    TQString tempLogPath = "/dev/shm/trsync_" + currentSessionName() + ".tmp";
    TQFile::remove(tempLogPath);

    const bool hadErr = !(exitCode == 0 && exitStatus == 0);

    if (!hadErr && m_isBackupRun && !m_currentConfigPath.isEmpty() && !m_destConfigDir.isEmpty()) {
        TQString configFileName = "gocryptfs.conf";
        if (m_currentConfigPath.endsWith(".xml")) {
            configFileName = ".encfs6.xml";
        }
        TQString destConfigPath = m_destConfigDir + "/" + configFileName;
        
        if (TQFile::exists(destConfigPath)) {
            TQFile::remove(destConfigPath);
        }
        TQDir().mkdir(m_destConfigDir);
        copyFile(m_currentConfigPath, destConfigPath);
    }
    m_isBackupRun = false;
    m_currentConfigPath = "";
    m_destConfigDir = "";

    if (m_isExecutingSet && hadErr) {
        m_setHadErrors = true;
    }

    bool hasMoreInSet = false;
    if (m_isExecutingSet) {
        if (m_currentSetIndex + 1 < (int)m_setSessionsToRun.count()) {
            hasMoreInSet = true;
        }
    }

    // Update dialog only if not in the middle of a set
    if (m_rsyncDialog && !hasMoreInSet) {
        m_rsyncDialog->setFinished(exitCode, m_isExecutingSet ? m_setHadErrors : hadErr);
    }
    
    // Execute after command if configured
    if (m_check_com_after && m_check_com_after->isChecked() && m_com_after) {
        if (!m_check_com_onerror || !m_check_com_onerror->isChecked() || hadErr) {
            TQString out, err;
            (void)runCommandSync(m_com_after->text(), &out, &err);
            
            if (m_rsyncDialog && (!out.isEmpty() || !err.isEmpty())) {
                m_rsyncDialog->appendOutput("\n** After command output:\n");
                if (!out.isEmpty()) m_rsyncDialog->appendOutput(out);
                if (!err.isEmpty()) m_rsyncDialog->appendOutput(err);
            }

            if (m_logFile && m_logFile->isOpen()) {
                TQTextStream stream(m_logFile);
                stream << "\n** After command output:\n";
                if (!out.isEmpty()) stream << out;
                if (!err.isEmpty()) stream << err;
            }
        }
    }

    if (m_logFile) {
        if (m_logFile->isOpen()) m_logFile->close();
        delete m_logFile;
        m_logFile = 0;
    }

    if (m_isExecutingSet) {
        m_currentSetIndex++;
        executeNextSetSession();
        return;
    }
    
    // Show error list if configured
    if (m_cfg_errorlist && hadErr && m_rsyncDialog) {
        TQString errors = m_rsyncDialog->getErrorList();
        if (!errors.isEmpty()) {
            TQMessageBox::warning(this, "Rsync Errors",
                "The operation completed with errors. Check the dialog for details.");
        }
    }

    if (m_cmdlineExecute && !m_cmdlineStayOpen && (!m_cfg_errorlist || !hadErr)) {
        close();
    }
}

void MainWindow::slotPauseRsync()
{
    if (m_rsync) {
        m_rsync->pause();
    }
}

void MainWindow::slotResumeRsync()
{
    if (m_rsync) {
        m_rsync->resume();
    }
}

void MainWindow::executeNextSetSession()
{
    if (m_currentSetIndex >= (int)m_setSessionsToRun.count()) {
        m_isExecutingSet = false;
        
        // Restore original set session
        for (int i = 0; i < m_sessions->count(); ++i) {
            if (m_sessions->text(i) == m_originalSetSessionName) {
                m_sessions->setCurrentItem(i);
                loadSessionToUi(m_originalSetSessionName);
                break;
            }
        }
        
        if (m_cmdlineExecute && !m_cmdlineStayOpen && !m_setHadErrors) {
            close();
        }
        return;
    }

    const TQString nextSession = m_setSessionsToRun[m_currentSetIndex];
    
    // Select and load the session in UI
    for (int i = 0; i < m_sessions->count(); ++i) {
        if (m_sessions->text(i) == nextSession) {
            m_sessions->setCurrentItem(i);
            loadSessionToUi(nextSession);
            break;
        }
    }

    // Run the sub-session
    m_dryrunNext = m_runningDryrun;
    slotRun();
}

void MainWindow::handleCommandLineArgs(bool execute, bool stayOpen, bool importMode, const TQString &session, const TQString &importFile)
{
    m_cmdlineExecute = execute;
    m_cmdlineStayOpen = stayOpen;
    m_cmdlineImport = importMode;
    m_cmdlineSessionName = session;
    m_cmdlineImportFilename = importFile;

    if (importMode && !importFile.isEmpty()) {
        TQFile in(importFile);
        if (in.open(IO_ReadOnly)) {
            TQFile out(trsyncIniPath());
            if (out.open(IO_WriteOnly | IO_Append)) {
                TQTextStream tsi(&in);
                TQTextStream tso(&out);
                while (!tsi.eof()) tso << tsi.readLine() << "\n";
                out.close();
            }
            in.close();
            TQMessageBox::information(this, "trsync", "Session imported");
            loadSessions();
        }
    }

    if (!session.isEmpty()) {
        bool found = false;
        for (int i = 0; i < m_sessions->count(); ++i) {
            if (m_sessions->text(i) == session) {
                m_sessions->setCurrentItem(i);
                loadSessionToUi(session);
                found = true;
                break;
            }
        }
        
        if (!found && !importMode) {
            TQMessageBox::critical(this, "trsync", "The session you specified on the command line doesn't exist");
            for (int i = 0; i < m_sessions->count(); ++i) {
                if (m_sessions->text(i) == "default") {
                    m_sessions->setCurrentItem(i);
                    loadSessionToUi("default");
                    break;
                }
            }
            m_cmdlineExecute = false;
        }
    }

    if (m_cmdlineExecute) {
        slotRun();
    }
}

void MainWindow::slotEncryptionTypeChanged(int idx)
{
    if (!m_comboEncryption) return;
    TQString type = m_comboEncryption->text(idx);
    if (type == "none") {
        if (m_lblWarningNotInstalled) m_lblWarningNotInstalled->hide();
        if (m_editPassword) m_editPassword->setEnabled(false);
        if (m_btnShowPassword) m_btnShowPassword->setEnabled(false);
        if (m_checkEncryptNames) m_checkEncryptNames->setEnabled(false);
    } else {
        TQString binaryPath = findBinary(type);
        if (binaryPath.isEmpty()) {
            if (m_lblWarningNotInstalled) {
                m_lblWarningNotInstalled->setText(type + " not installed, please install to use " + type + " encryption");
                m_lblWarningNotInstalled->show();
            }
            if (m_editPassword) m_editPassword->setEnabled(false);
            if (m_btnShowPassword) m_btnShowPassword->setEnabled(false);
            if (m_checkEncryptNames) m_checkEncryptNames->setEnabled(false);
        } else {
            if (m_lblWarningNotInstalled) m_lblWarningNotInstalled->hide();
            if (m_editPassword) m_editPassword->setEnabled(true);
            if (m_btnShowPassword) m_btnShowPassword->setEnabled(true);
            if (m_checkEncryptNames) m_checkEncryptNames->setEnabled(true);
        }
    }
}

void MainWindow::slotToggleShowPassword()
{
    if (!m_btnShowPassword || !m_editPassword) return;
    if (m_btnShowPassword->isOn()) {
        m_editPassword->setEchoMode(TQLineEdit::Normal);
        m_btnShowPassword->setIconSet(loadEmbeddedIcon(icon_hide_password, icon_hide_password_len));
    } else {
        m_editPassword->setEchoMode(TQLineEdit::Password);
        m_btnShowPassword->setIconSet(loadEmbeddedIcon(icon_show_password, icon_show_password_len));
    }
}
