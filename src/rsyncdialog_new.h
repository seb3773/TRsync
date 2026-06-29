#ifndef RSYNCDIALOG_NEW_H
#define RSYNCDIALOG_NEW_H

#include <ntqdialog.h>
#include <ntqstring.h>

class TQLabel;
class TQProgressBar;
class TQPushButton;
class TQTextEdit;
class TQFrame;
class TQVBoxLayout;
class RsyncStats;

/**
 * Dialogue de synchronisation rsync - Layout EXACT de l'original GTK
 * 
 * Structure (grsync.glade lignes 1624-1830):
 * - Label fichier en cours
 * - ProgressBar fichier
 * - Label progression globale
 * - ProgressBar globale
 * - Séparateur horizontal
 * - Bouton toggle "Show Details" (équivalent GtkExpander)
 * - TextEdit logs (caché par défaut)
 * - Boutons: Warning, Pause, Stop
 */
class RsyncDialogNew : public TQDialog
{
    TQ_OBJECT

public:
    RsyncDialogNew(TQWidget *parent = 0, const char *name = 0);
    ~RsyncDialogNew();

    void startOperation(const TQString &sessionName, bool isDryRun, bool appendLogs = false);
    void appendOutput(const TQString &line);
    void setFinished(int exitCode, bool hadError);
    TQString getErrorList() const { return m_errorList; }
    
    bool wasCanceled() const { return m_wasCanceled; }
    bool isPaused() const { return m_isPaused; }

signals:
    void canceled();
    void pauseRequested();
    void resumeRequested();

protected:
    void closeEvent(TQCloseEvent *e);

private slots:
    void slotStop();
    void slotPause();
    void slotShowErrors();
    void slotToggleDetails();

private:
    void setupUI();
    void parseRsyncOutput(const TQString &line);
    void setFileProgress(int percent);
    void setGlobalProgress(int percent);
    void setFileLabel(const TQString &text);
    void setGlobalLabel(const TQString &text);
    
    // Widgets - ORDRE EXACT de l'original GTK
    TQLabel *m_labelFile;           // "Idle" ou nom du fichier
    TQProgressBar *m_progressFile;  // Barre fichier en cours
    TQLabel *m_labelGlobal;         // "Global progress"
    TQProgressBar *m_progressGlobal;// Barre progression globale
    TQFrame *m_separator;           // Séparateur horizontal
    TQPushButton *m_btnDetails;     // "Show Details..." (toggle)
    TQTextEdit *m_textOutput;       // Logs rsync
    TQPushButton *m_btnWarning;     // Bouton Warning (désactivé par défaut)
    TQPushButton *m_btnPause;       // Bouton Pause
    TQPushButton *m_btnStop;        // Bouton Stop
    
    TQVBoxLayout *m_mainLayout;
    
    // État
    TQString m_sessionName;
    bool m_isDryRun;
    bool m_wasCanceled;
    bool m_isPaused;
    bool m_isFinished;
    bool m_hadErrors;
    bool m_detailsVisible;
    TQString m_logBuffer;
    bool m_lastLineWasCarriage;
    
    // Statistiques
    int m_filesTransferred;
    int m_totalFiles;
    double m_currentFileProgress;
    double m_globalProgress;
    TQString m_currentFile;
    long long m_bytesTransferred;
    long long m_totalBytes;
    
    RsyncStats *m_stats;
    TQString m_errorList;
};

#endif // RSYNCDIALOG_NEW_H

// Made with Bob
