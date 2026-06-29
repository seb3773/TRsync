#include "rsyncdialog.h"
#include "tqtmultiprogressdialog.h"
#include "rsyncstats.h"

#include <ntqpushbutton.h>
#include <ntqtimer.h>
#include <ntqmessagebox.h>
#include <ntqdialog.h>
#include <ntqlayout.h>
#include <ntqlabel.h>
#include <ntqtextedit.h>
#include <ntqregexp.h>

RsyncDialog::RsyncDialog(TQWidget *parent, const char *name)
    : TQDialog(parent, name, false)
    , m_progressDlg(0)
    , m_pauseBtn(0)
    , m_errorBtn(0)
    , m_scrollTimer(0)
    , m_stats(0)
    , m_isDryRun(false)
    , m_isPaused(false)
    , m_isFinished(false)
    , m_wasCanceled(false)
    , m_hadErrors(false)
    , m_filesTransferred(0)
    , m_totalFiles(0)
    , m_currentFileProgress(0.0)
    , m_globalProgress(0.0)
    , m_bytesTransferred(0)
    , m_totalBytes(0)
{
    setCaption("Rsync Operation");
    
    m_progressDlg = new TQtMultiProgressDialog(this, "progress");
    m_progressDlg->setLabelText("Initializing...", TQtMultiProgressDialog::TotalProgress);
    m_progressDlg->setLabelText("Idle", TQtMultiProgressDialog::PartialProgress);
    m_progressDlg->setRange(0, 100, TQtMultiProgressDialog::TotalProgress);
    m_progressDlg->setRange(0, 100, TQtMultiProgressDialog::PartialProgress);
    m_progressDlg->setAutoClose(false);
    
    connect(m_progressDlg, SIGNAL(canceled()), this, SLOT(slotCancel()));
    
    m_scrollTimer = new TQTimer(this);
    connect(m_scrollTimer, SIGNAL(timeout()), this, SLOT(slotUpdateScrollTimer()));
    
    TQVBoxLayout *layout = new TQVBoxLayout(this, 8, 6);
    layout->addWidget(m_progressDlg);
    
    m_stats = new RsyncStats();
    
    // Set fixed size to prevent resizing issues
    setMinimumSize(500, 300);
    resize(500, 300);
    setSizePolicy(TQSizePolicy::Fixed, TQSizePolicy::Preferred);
}

RsyncDialog::~RsyncDialog()
{
    delete m_stats;
}

void RsyncDialog::startOperation(const TQString &sessionName, bool isDryRun)
{
    m_sessionName = sessionName;
    m_isDryRun = isDryRun;
    m_isFinished = false;
    m_wasCanceled = false;
    m_hadErrors = false;
    m_filesTransferred = 0;
    m_totalFiles = 0;
    m_currentFileProgress = 0.0;
    m_globalProgress = 0.0;
    m_bytesTransferred = 0;
    m_totalBytes = 0;
    m_errorList = TQString();
    m_outputLines.clear();
    
    if (m_stats) {
        delete m_stats;
        m_stats = new RsyncStats();
    }
    
    TQString title = isDryRun ? "Simulation: " : "Synchronization: ";
    title += sessionName;
    setCaption(title);
    
    TQString label = isDryRun ? "Simulation in progress..." : "Global progress";
    m_progressDlg->setLabelText(label, TQtMultiProgressDialog::TotalProgress);
    m_progressDlg->setLabelText("Preparing...", TQtMultiProgressDialog::PartialProgress);
    m_progressDlg->setProgress(0, TQtMultiProgressDialog::TotalProgress);
    m_progressDlg->setProgress(0, TQtMultiProgressDialog::PartialProgress);
    
    // Show details by default so logs are visible
    m_progressDlg->showDetails(true);
    
    m_scrollTimer->start(100);
}

void RsyncDialog::appendOutput(const TQString &text, bool isError)
{
    if (text.isEmpty()) return;
    
    m_outputLines.append(text);
    
    // Feed to stats analyzer
    if (m_stats) {
        m_stats->parseLine(text, isError);
    }
    
    if (isError) {
        m_hadErrors = true;
        m_errorList += text;
        m_progressDlg->message("<font color=\"red\">" + text + "</font>");
    } else {
        m_progressDlg->message(text);
        parseRsyncOutput(text);
    }
}

void RsyncDialog::parseRsyncOutput(const TQString &line)
{
    // Don't update progress bars in dry-run mode (simulation)
    if (m_isDryRun) return;
    
    // Parse rsync progress lines like:
    // "  12,345,678  45%  123.45MB/s    0:00:12"
    // Progress lines start with space and contain %
    
    if (!line.isEmpty() && line[0] == ' ' && line.contains("%")) {
        // This is a progress line
        // Extract file progress (first percentage)
        TQRegExp rx("(\\d+)%");
        if (rx.search(line) >= 0) {
            bool ok = false;
            int percent = rx.cap(1).toInt(&ok);
            if (ok) {
                m_currentFileProgress = percent / 100.0;
                m_progressDlg->setProgress(percent, TQtMultiProgressDialog::PartialProgress);
            }
        }
        
        // Extract global progress (second percentage if present)
        int firstPercent = line.find('%');
        if (firstPercent >= 0) {
            int secondPercent = line.find('%', firstPercent + 1);
            if (secondPercent >= 0) {
                // There's a second percentage
                TQString remaining = line.mid(firstPercent + 1);
                TQRegExp rx2("(\\d+)%");
                if (rx2.search(remaining) >= 0) {
                    bool ok = false;
                    int gPercent = rx2.cap(1).toInt(&ok);
                    if (ok) {
                        m_globalProgress = gPercent / 100.0;
                        m_progressDlg->setProgress(gPercent, TQtMultiProgressDialog::TotalProgress);
                    }
                }
            }
        }
        
        // Update current file label if we have one stored
        if (!m_currentFile.isEmpty()) {
            TQString label = m_currentFile;
            if (label.length() > 60) {
                label = "..." + label.right(57);
            }
            m_progressDlg->setLabelText(label, TQtMultiProgressDialog::PartialProgress);
        }
    } else {
        // Not a progress line - likely a filename
        TQString trimmed = line.stripWhiteSpace();
        if (!trimmed.isEmpty() &&
            !trimmed.startsWith("sending") &&
            !trimmed.startsWith("sent") &&
            !trimmed.startsWith("total") &&
            !trimmed.startsWith("deleting")) {
            m_currentFile = trimmed;
            m_filesTransferred++;
        }
    }
}

void RsyncDialog::setFinished(int exitCode, bool hadErrors)
{
    m_isFinished = true;
    m_hadErrors = hadErrors || (exitCode != 0);
    
    if (m_stats) {
        m_stats->setExitCode(exitCode);
    }
    
    m_scrollTimer->stop();
    
    m_progressDlg->setProgress(100, TQtMultiProgressDialog::TotalProgress);
    m_progressDlg->setProgress(100, TQtMultiProgressDialog::PartialProgress);
    
    TQString statusMsg;
    if (m_wasCanceled) {
        statusMsg = "<font color=\"orange\"><b>Operation canceled by user</b></font>";
        m_progressDlg->setLabelText("Canceled", TQtMultiProgressDialog::TotalProgress);
    } else if (m_hadErrors) {
        statusMsg = "<font color=\"red\"><b>Completed with errors!</b></font>";
        m_progressDlg->setLabelText("Completed with errors", TQtMultiProgressDialog::TotalProgress);
    } else {
        statusMsg = "<font color=\"darkgreen\"><b>Completed successfully!</b></font>";
        m_progressDlg->setLabelText("Completed successfully", TQtMultiProgressDialog::TotalProgress);
    }
    
    m_progressDlg->message("\n" + statusMsg);
    
    // Add detailed summary from stats
    if (m_stats) {
        TQString summary = "\n<b>Summary:</b>\n";
        summary += TQString("Files transferred: %1\n").arg(m_stats->filesTransferred());
        
        if (m_stats->filesDeleted() > 0) {
            summary += TQString("Files deleted: %1\n").arg(m_stats->filesDeleted());
        }
        if (m_stats->filesSkipped() > 0) {
            summary += TQString("Files skipped: %1\n").arg(m_stats->filesSkipped());
        }
        if (m_stats->bytesTransferred() > 0) {
            long long bytes = m_stats->bytesTransferred();
            TQString sizeStr;
            if (bytes < 1024 * 1024) {
                sizeStr = TQString::number(bytes / 1024.0, 'f', 2) + " KB";
            } else if (bytes < 1024 * 1024 * 1024) {
                sizeStr = TQString::number(bytes / (1024.0 * 1024.0), 'f', 2) + " MB";
            } else {
                sizeStr = TQString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
            }
            summary += TQString("Data transferred: %1\n").arg(sizeStr);
        }
        
        summary += TQString("Exit code: %1\n").arg(exitCode);
        
        if (m_stats->errorCount() > 0) {
            summary += TQString("<font color=\"red\">Errors: %1</font>\n").arg(m_stats->errorCount());
        }
        
        m_progressDlg->message(summary);
    }
    
    m_progressDlg->setLabelText("Idle", TQtMultiProgressDialog::PartialProgress);
}

bool RsyncDialog::wasCanceled() const
{
    return m_wasCanceled;
}

TQString RsyncDialog::getErrorList() const
{
    return m_errorList;
}

TQString RsyncDialog::getDetailedReport() const
{
    if (m_stats) {
        return m_stats->generateReport();
    }
    return TQString();
}

void RsyncDialog::showDetailedReport()
{
    if (!m_stats) return;
    
    TQString report = m_stats->generateReport();
    
    TQDialog *dlg = new TQDialog(this, "report", true);
    dlg->setCaption("Detailed Operation Report");
    
    TQVBoxLayout *layout = new TQVBoxLayout(dlg, 12, 6);
    
    TQTextEdit *textEdit = new TQTextEdit(dlg);
    textEdit->setReadOnly(true);
    textEdit->setText(report);
    textEdit->setTextFormat(TQt::PlainText);
    textEdit->setFont(TQFont("Monospace", 10));
    layout->addWidget(textEdit, 1);
    
    TQHBoxLayout *btnLayout = new TQHBoxLayout(0, 0, 6);
    
    TQPushButton *saveBtn = new TQPushButton("Save Report...", dlg);
    btnLayout->addWidget(saveBtn);
    btnLayout->addStretch();
    
    TQPushButton *closeBtn = new TQPushButton("Close", dlg);
    connect(closeBtn, SIGNAL(clicked()), dlg, SLOT(accept()));
    btnLayout->addWidget(closeBtn);
    
    layout->addLayout(btnLayout);
    
    dlg->resize(700, 500);
    dlg->exec();
    delete dlg;
}

void RsyncDialog::slotCancel()
{
    if (m_isFinished) {
        accept();
        return;
    }
    
    int result = TQMessageBox::question(
        this,
        "Cancel Operation",
        "Are you sure you want to cancel the rsync operation?",
        TQMessageBox::Yes,
        TQMessageBox::No
    );
    
    if (result == TQMessageBox::Yes) {
        m_wasCanceled = true;
        emit canceled();
    }
}

void RsyncDialog::slotPause()
{
    if (m_isPaused) {
        emit resumeRequested();
        m_isPaused = false;
        if (m_pauseBtn) m_pauseBtn->setText("Pause");
    } else {
        emit pauseRequested();
        m_isPaused = true;
        if (m_pauseBtn) m_pauseBtn->setText("Resume");
    }
}

void RsyncDialog::slotShowErrors()
{
    showErrorDialog();
}

void RsyncDialog::slotUpdateScrollTimer()
{
    // Timer for smooth scrolling updates
    // The TQtMultiProgressDialog handles auto-scrolling internally
}

void RsyncDialog::showErrorDialog()
{
    if (m_errorList.isEmpty()) {
        TQMessageBox::information(this, "No Errors", "No errors were recorded during the operation.");
        return;
    }
    
    TQDialog *dlg = new TQDialog(this, "errors", true);
    dlg->setCaption("Error List");
    
    TQVBoxLayout *layout = new TQVBoxLayout(dlg, 12, 6);
    
    TQLabel *label = new TQLabel("The following errors occurred:", dlg);
    layout->addWidget(label);
    
    TQTextEdit *textEdit = new TQTextEdit(dlg);
    textEdit->setReadOnly(true);
    textEdit->setText(m_errorList);
    textEdit->setTextFormat(TQt::PlainText);
    layout->addWidget(textEdit, 1);
    
    TQPushButton *closeBtn = new TQPushButton("Close", dlg);
    connect(closeBtn, SIGNAL(clicked()), dlg, SLOT(accept()));
    
    TQHBoxLayout *btnLayout = new TQHBoxLayout(0, 0, 6);
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);
    
    dlg->resize(600, 400);
    dlg->exec();
    delete dlg;
}