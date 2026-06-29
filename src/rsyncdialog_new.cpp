#include "rsyncdialog_new.h"
#include "rsyncstats.h"

#include <ntqapplication.h>
#include <ntqframe.h>
#include <ntqhbox.h>
#include <ntqlabel.h>
#include <ntqlayout.h>
#include <ntqprogressbar.h>
#include <ntqpushbutton.h>
#include <ntqtextedit.h>
#include <ntqregexp.h>
#include <ntqmessagebox.h>
#include <ntqdatetime.h>

RsyncDialogNew::RsyncDialogNew(TQWidget *parent, const char *name)
    : TQDialog(parent, name, true)  // MODAL comme l'original GTK
    , m_labelFile(0)
    , m_progressFile(0)
    , m_labelGlobal(0)
    , m_progressGlobal(0)
    , m_separator(0)
    , m_btnDetails(0)
    , m_textOutput(0)
    , m_btnWarning(0)
    , m_btnPause(0)
    , m_btnStop(0)
    , m_mainLayout(0)
    , m_isDryRun(false)
    , m_wasCanceled(false)
    , m_isPaused(false)
    , m_isFinished(false)
    , m_hadErrors(false)
    , m_detailsVisible(false)
    , m_lastLineWasCarriage(false)
    , m_filesTransferred(0)
    , m_totalFiles(0)
    , m_currentFileProgress(0.0)
    , m_globalProgress(0.0)
    , m_bytesTransferred(0)
    , m_totalBytes(0)
    , m_stats(0)
{
    setCaption("rsync");
    m_stats = new RsyncStats();
    setupUI();
}

RsyncDialogNew::~RsyncDialogNew()
{
    delete m_stats;
}

void RsyncDialogNew::setupUI()
{
    // Layout principal - VBox comme l'original GTK
    m_mainLayout = new TQVBoxLayout(this, 12, 6);  // border=12, spacing=6
    
    // 1. Label fichier en cours - "Idle" par défaut
    m_labelFile = new TQLabel("Idle", this);
    m_labelFile->setAlignment(TQt::AlignLeft | TQt::AlignTop);
    m_mainLayout->addWidget(m_labelFile);
    
    // 2. ProgressBar fichier - avec padding de 3 comme l'original
    m_progressFile = new TQProgressBar(this);
    m_progressFile->setTotalSteps(100);
    m_progressFile->setProgress(0);
    m_progressFile->setMinimumHeight(20);
    m_progressFile->setPercentageVisible(true);
    m_mainLayout->addWidget(m_progressFile);
    m_mainLayout->addSpacing(3);  // padding comme GTK
    
    // 3. Label progression globale
    m_labelGlobal = new TQLabel("Global progress", this);
    m_labelGlobal->setAlignment(TQt::AlignLeft | TQt::AlignTop);
    m_mainLayout->addWidget(m_labelGlobal);
    
    // 4. ProgressBar globale
    m_progressGlobal = new TQProgressBar(this);
    m_progressGlobal->setTotalSteps(100);
    m_progressGlobal->setProgress(0);
    m_progressGlobal->setMinimumHeight(20);
    m_progressGlobal->setPercentageVisible(true);
    m_mainLayout->addWidget(m_progressGlobal);
    
    // 5. Séparateur horizontal (width=410 dans GTK)
    m_separator = new TQFrame(this);
    m_separator->setFrameShape(TQFrame::HLine);
    m_separator->setFrameShadow(TQFrame::Sunken);
    m_separator->setMinimumWidth(410);
    m_mainLayout->addWidget(m_separator);
    
    // 6. Bouton toggle "Show Details..." (équivalent GtkExpander)
    m_btnDetails = new TQPushButton("Show Details...", this);
    m_btnDetails->setToggleButton(true);
    m_btnDetails->setOn(false);
    connect(m_btnDetails, SIGNAL(toggled(bool)), this, SLOT(slotToggleDetails()));
    m_mainLayout->addWidget(m_btnDetails);
    
    // 7. TextEdit pour les logs (caché par défaut, height=250 dans GTK)
    m_textOutput = new TQTextEdit(this);
    m_textOutput->setReadOnly(true);
    m_textOutput->setMinimumHeight(250);
    m_textOutput->setMinimumWidth(410);
    m_textOutput->hide();  // Caché par défaut comme GtkExpander replié
    m_mainLayout->addWidget(m_textOutput);
    
    // 8. Boutons d'action (ButtonBox avec layout=end dans GTK)
    TQHBoxLayout *buttonLayout = new TQHBoxLayout(m_mainLayout, 6);
    
    // Bouton Warning (gtk-dialog-warning, désactivé par défaut)
    m_btnWarning = new TQPushButton("Warnings", this);
    m_btnWarning->setEnabled(false);  // Désactivé par défaut
    connect(m_btnWarning, SIGNAL(clicked()), this, SLOT(slotShowErrors()));
    buttonLayout->addWidget(m_btnWarning);
    
    // Bouton Pause (gtk-media-pause)
    m_btnPause = new TQPushButton("Pause", this);
    connect(m_btnPause, SIGNAL(clicked()), this, SLOT(slotPause()));
    buttonLayout->addWidget(m_btnPause);
    
    // Bouton Stop (gtk-stop)
    m_btnStop = new TQPushButton("Stop", this);
    connect(m_btnStop, SIGNAL(clicked()), this, SLOT(slotStop()));
    buttonLayout->addWidget(m_btnStop);
    
    // Taille fixe du dialogue
    setMinimumSize(450, 200);
    resize(450, 200);
}

void RsyncDialogNew::startOperation(const TQString &sessionName, bool isDryRun, bool appendLogs)
{
    m_sessionName = sessionName;
    m_isDryRun = isDryRun;
    m_isFinished = false;
    m_wasCanceled = false;
    m_isPaused = false;
    m_hadErrors = false;
    m_filesTransferred = 0;
    m_totalFiles = 0;
    m_currentFileProgress = 0.0;
    m_globalProgress = 0.0;
    m_bytesTransferred = 0;
    m_totalBytes = 0;
    
    // Réinitialiser l'interface
    m_labelFile->setText(isDryRun ? "Simulation in progress" : "Idle");
    m_progressFile->setProgress(0);
    m_progressGlobal->setProgress(0);
    m_logBuffer = TQString();
    m_lastLineWasCarriage = false;
    if (!appendLogs) {
        m_textOutput->clear();
        m_errorList = TQString();
        m_btnWarning->setEnabled(false);
    }
    m_btnPause->setText("Pause");
    m_btnPause->setEnabled(true);
    m_btnStop->setText("Stop");
    
    // Reconnect Stop button back to slotStop
    disconnect(m_btnStop, SIGNAL(clicked()), this, SLOT(accept()));
    disconnect(m_btnStop, SIGNAL(clicked()), this, SLOT(slotStop()));
    connect(m_btnStop, SIGNAL(clicked()), this, SLOT(slotStop()));
    
    // Afficher les détails en mode simulation ou si configuré
    // (config_output || dryrunning dans l'original ligne 661)
    if (isDryRun) {
        m_btnDetails->setOn(true);
        slotToggleDetails();
    }
    
    setCaption(TQString("rsync - %1 - running").arg(sessionName));
}

void RsyncDialogNew::appendOutput(const TQString &line)
{
    if (line.isEmpty()) return;
    
    m_logBuffer += line;
    
    TQStringList linesToAppend;
    bool updated = false;
    
    while (true) {
        int idxNewline = m_logBuffer.find('\n');
        int idxCarriage = m_logBuffer.find('\r');
        
        int idx = -1;
        bool isCarriage = false;
        
        if (idxNewline >= 0 && idxCarriage >= 0) {
            if (idxNewline < idxCarriage) {
                idx = idxNewline;
                isCarriage = false;
            } else {
                idx = idxCarriage;
                isCarriage = true;
            }
        } else if (idxNewline >= 0) {
            idx = idxNewline;
            isCarriage = false;
        } else if (idxCarriage >= 0) {
            idx = idxCarriage;
            isCarriage = true;
        }
        
        if (idx < 0) {
            break;
        }
        
        TQString segment = m_logBuffer.left(idx);
        m_logBuffer = m_logBuffer.mid(idx + 1);
        
        while (!segment.isEmpty() && (segment[segment.length() - 1] == '\r' || segment[segment.length() - 1] == '\n')) {
            segment.truncate(segment.length() - 1);
        }
        
        parseRsyncOutput(segment);
        
        if (segment.contains("error", false) || segment.contains("failed", false)) {
            m_hadErrors = true;
            m_errorList += segment + "\n";
        }
        
        if (isCarriage) {
            if (!linesToAppend.isEmpty()) {
                linesToAppend[linesToAppend.count() - 1] = segment;
            } else if (m_lastLineWasCarriage) {
                if (m_textOutput->paragraphs() > 0) {
                    m_textOutput->removeParagraph(m_textOutput->paragraphs() - 1);
                }
                m_lastLineWasCarriage = false;
                linesToAppend.append(segment);
            } else {
                linesToAppend.append(segment);
            }
        } else {
            if (m_lastLineWasCarriage && linesToAppend.isEmpty()) {
                if (m_textOutput->paragraphs() > 0) {
                    m_textOutput->removeParagraph(m_textOutput->paragraphs() - 1);
                }
                m_lastLineWasCarriage = false;
            }
            linesToAppend.append(segment);
        }
        
        m_lastLineWasCarriage = isCarriage;
        updated = true;
    }
    
    if (updated && !linesToAppend.isEmpty()) {
        TQString fullText = linesToAppend.join("\n");
        m_textOutput->append(fullText);
        m_textOutput->scrollToBottom();
        m_textOutput->repaint();
    }
    
    // Process events to maintain GUI responsiveness
    static bool inEventProcess = false;
    static TQTime lastEventProcessTime;
    if (!lastEventProcessTime.isValid()) {
        lastEventProcessTime.start();
    }
    if (!inEventProcess && lastEventProcessTime.elapsed() > 50) {
        inEventProcess = true;
        lastEventProcessTime.restart();
        tqApp->processEvents();
        inEventProcess = false;
    }
}

void RsyncDialogNew::parseRsyncOutput(const TQString &line)
{
    // Don't update progress bars in dry-run mode (comme l'original ligne 952)
    if (m_isDryRun) return;
    
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
                setFileProgress(percent);
            }
        }
        
        // Extract global progress (second percentage or check progress like ir-chk=1022/9550)
        double gFraction = 0.0;
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
                        gFraction = gPercent / 100.0;
                    }
                }
            } else if (line.contains("=")) {
                // Parse check progress (e.g., ir-chk=1022/9550 or to-chk=1022/9550)
                int idxEqual = line.findRev('=');
                int idxSlash = line.findRev('/');
                if (idxEqual >= 0 && idxSlash > idxEqual) {
                    TQString remainingStr = line.mid(idxEqual + 1, idxSlash - idxEqual - 1);
                    TQString totalStr = line.mid(idxSlash + 1);
                    
                    int idxParen = totalStr.find(')');
                    if (idxParen >= 0) {
                        totalStr = totalStr.left(idxParen);
                    }
                    totalStr = totalStr.stripWhiteSpace();
                    remainingStr = remainingStr.stripWhiteSpace();
                    
                    bool okTotal = false;
                    bool okRemaining = false;
                    double total = totalStr.toDouble(&okTotal);
                    double remaining = remainingStr.toDouble(&okRemaining);
                    if (okTotal && okRemaining && total > 0) {
                        double done = total - remaining;
                        gFraction = done / total;
                    }
                }
            }
        }
        
        if (gFraction > 0.0) {
            m_globalProgress = gFraction;
            setGlobalProgress((int)(gFraction * 100.0));
        }
        
        // Update current file label if we have one stored
        if (!m_currentFile.isEmpty()) {
            TQString label = m_currentFile;
            if (label.length() > 60) {
                label = "..." + label.right(57);
            }
            setFileLabel(label);
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

void RsyncDialogNew::setFileProgress(int percent)
{
    m_progressFile->setProgress(percent);
    
    static TQTime lastRepaintTime;
    if (!lastRepaintTime.isValid()) {
        lastRepaintTime.start();
    }
    if (percent == 100 || lastRepaintTime.elapsed() > 50) {
        lastRepaintTime.restart();
        m_progressFile->repaint();
    }
}

void RsyncDialogNew::setGlobalProgress(int percent)
{
    m_progressGlobal->setProgress(percent);
    
    static TQTime lastRepaintTime;
    if (!lastRepaintTime.isValid()) {
        lastRepaintTime.start();
    }
    if (percent == 100 || lastRepaintTime.elapsed() > 50) {
        lastRepaintTime.restart();
        m_progressGlobal->repaint();
    }
}

void RsyncDialogNew::setFileLabel(const TQString &text)
{
    m_labelFile->setText(text);
    
    static TQTime lastRepaintTime;
    if (!lastRepaintTime.isValid()) {
        lastRepaintTime.start();
    }
    if (lastRepaintTime.elapsed() > 50) {
        lastRepaintTime.restart();
        m_labelFile->repaint();
    }
}

void RsyncDialogNew::setGlobalLabel(const TQString &text)
{
    m_labelGlobal->setText(text);
    
    static TQTime lastRepaintTime;
    if (!lastRepaintTime.isValid()) {
        lastRepaintTime.start();
    }
    if (lastRepaintTime.elapsed() > 50) {
        lastRepaintTime.restart();
        m_labelGlobal->repaint();
    }
}

void RsyncDialogNew::setFinished(int exitCode, bool hadError)
{
    m_isFinished = true;
    m_hadErrors = hadError;
    
    // Mettre les barres à 100% si succès
    if (exitCode == 0 && !m_wasCanceled) {
        setFileProgress(100);
        setGlobalProgress(100);
    }
    
    // Changer le bouton Stop en Close
    m_btnStop->setText("Close");
    disconnect(m_btnStop, SIGNAL(clicked()), this, SLOT(slotStop()));
    connect(m_btnStop, SIGNAL(clicked()), this, SLOT(accept()));
    
    // Désactiver Pause
    m_btnPause->setEnabled(false);
    
    // Mettre à jour le titre
    TQString status = (exitCode == 0) ? "finished" : (m_wasCanceled ? "canceled" : "error");
    setCaption(TQString("rsync - %1 - %2").arg(m_sessionName).arg(status));
    
    // Afficher un résumé
    TQString summary = TQString("\n=== Operation %1 (exit code: %2) ===\n")
        .arg(status).arg(exitCode);
    m_textOutput->append(summary);

    if (m_hadErrors || !m_errorList.isEmpty()) {
        m_btnWarning->setEnabled(true);
    }
}

void RsyncDialogNew::slotStop()
{
    m_wasCanceled = true;
    emit canceled();
}

void RsyncDialogNew::slotPause()
{
    m_isPaused = !m_isPaused;
    m_btnPause->setText(m_isPaused ? "Resume" : "Pause");
    
    if (m_isPaused) {
        emit pauseRequested();
    } else {
        emit resumeRequested();
    }
}

void RsyncDialogNew::slotShowErrors()
{
    if (m_errorList.isEmpty()) {
        TQMessageBox::information(this, "Errors", "No errors to display.");
        return;
    }
    
    TQDialog dlg(this, "errors_dialog", true);
    dlg.setCaption("Errors list");
    dlg.resize(500, 350);
    
    TQVBoxLayout *layout = new TQVBoxLayout(&dlg, 12, 6);
    layout->addWidget(new TQLabel("The following warnings/errors occurred during synchronization:", &dlg));
    
    TQTextEdit *txt = new TQTextEdit(&dlg);
    txt->setReadOnly(true);
    txt->setText(m_errorList);
    layout->addWidget(txt);
    
    TQHBoxLayout *btnLayout = new TQHBoxLayout(layout, 6);
    btnLayout->addStretch(1);
    TQPushButton *btnClose = new TQPushButton("Close", &dlg);
    btnLayout->addWidget(btnClose);
    connect(btnClose, SIGNAL(clicked()), &dlg, SLOT(accept()));
    
    dlg.exec();
}

void RsyncDialogNew::slotToggleDetails()
{
    m_detailsVisible = m_btnDetails->isOn();
    
    if (m_detailsVisible) {
        m_btnDetails->setText("Hide Details...");
        m_textOutput->setMinimumHeight(250);
        m_textOutput->show();
        setMinimumSize(850, 590);
        resize(850, 590);  // Agrandir pour montrer les logs
    } else {
        m_btnDetails->setText("Show Details...");
        m_textOutput->hide();
        m_textOutput->setMinimumHeight(0);
        setMinimumSize(450, 200);
        resize(450, 200);  // Réduire
    }
    
    if (m_mainLayout) {
        m_mainLayout->activate();
    }
    tqApp->processEvents();
}

void RsyncDialogNew::closeEvent(TQCloseEvent *e)
{
    if (!m_isFinished) {
        // Demander confirmation si l'opération est en cours
        int ret = TQMessageBox::question(this, "Confirm", 
            "Rsync operation is still running. Do you want to stop it?",
            TQMessageBox::Yes, TQMessageBox::No);
        
        if (ret == TQMessageBox::Yes) {
            slotStop();
            e->accept();
        } else {
            e->ignore();
        }
    } else {
        e->accept();
    }
}

// Made with Bob
