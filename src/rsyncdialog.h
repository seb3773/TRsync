#ifndef GRSYNC_TQT3_RSYNCDIALOG_H
#define GRSYNC_TQT3_RSYNCDIALOG_H

#include <ntqdialog.h>
#include <ntqstring.h>
#include <ntqstringlist.h>

class TQtMultiProgressDialog;
class TQPushButton;
class TQTimer;
class RsyncStats;

class RsyncDialog : public TQDialog
{
    TQ_OBJECT

public:
    RsyncDialog(TQWidget *parent = 0, const char *name = 0);
    ~RsyncDialog();

    void startOperation(const TQString &sessionName, bool isDryRun);
    void appendOutput(const TQString &text, bool isError = false);
    void setFinished(int exitCode, bool hadErrors);
    
    bool wasCanceled() const;
    TQString getErrorList() const;
    TQString getDetailedReport() const;
    void showDetailedReport();
    
signals:
    void canceled();
    void pauseRequested();
    void resumeRequested();

private slots:
    void slotCancel();
    void slotPause();
    void slotShowErrors();
    void slotUpdateScrollTimer();

private:
    void parseRsyncOutput(const TQString &line);
    void updateProgress();
    void showErrorDialog();
    
    TQtMultiProgressDialog *m_progressDlg;
    TQPushButton *m_pauseBtn;
    TQPushButton *m_errorBtn;
    TQTimer *m_scrollTimer;
    
    TQString m_sessionName;
    TQString m_currentFile;
    TQString m_errorList;
    TQStringList m_outputLines;
    
    RsyncStats *m_stats;
    
    bool m_isDryRun;
    bool m_isPaused;
    bool m_isFinished;
    bool m_wasCanceled;
    bool m_hadErrors;
    
    int m_filesTransferred;
    int m_totalFiles;
    double m_currentFileProgress;
    double m_globalProgress;
    
    long long m_bytesTransferred;
    long long m_totalBytes;
};

#endif