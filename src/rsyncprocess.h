
#ifndef GRSYNC_TQT3_RSYNC_PROCESS_H
#define GRSYNC_TQT3_RSYNC_PROCESS_H

#include <ntqobject.h>
#include <ntqstring.h>
#include <ntqstringlist.h>

class TQProcess;

class RsyncProcess : public TQObject
{
    TQ_OBJECT

public:
    RsyncProcess(TQObject *parent);
    ~RsyncProcess();

    bool start(const TQString &program, const TQStringList &args);
    void stop();
    void pause();
    void resume();
    int processIdentifier() const;

signals:
    void sigStdout(const TQString &text);
    void sigStderr(const TQString &text);
    void sigFinished(int exitCode, int exitStatus);

private slots:
    void slotReadyReadStdout();
    void slotReadyReadStderr();
    void slotProcessExited();

    TQProcess *m_proc;
};

#endif

