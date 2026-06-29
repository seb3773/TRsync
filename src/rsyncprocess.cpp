#include "rsyncprocess.h"

#include <ntqprocess.h>

#include <ntqcstring.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

RsyncProcess::RsyncProcess(TQObject *parent)
    : TQObject(parent)
    , m_proc(0)
{
    m_proc = new TQProcess(this);

    m_proc->setCommunication(TQProcess::Stdout | TQProcess::Stderr);

    connect(m_proc, SIGNAL(readyReadStdout()), this, SLOT(slotReadyReadStdout()));
    connect(m_proc, SIGNAL(readyReadStderr()), this, SLOT(slotReadyReadStderr()));
    connect(m_proc, SIGNAL(processExited()), this, SLOT(slotProcessExited()));
}

RsyncProcess::~RsyncProcess()
{
    stop();
}

bool RsyncProcess::start(const TQString &program, const TQStringList &args)
{
    stop();

    if (!m_proc) return false;

    TQStringList full;
    full << program;
    for (TQStringList::ConstIterator it = args.begin(); it != args.end(); ++it) full << *it;

    m_proc->setArguments(full);
    return m_proc->start();
}

void RsyncProcess::stop()
{
    if (m_proc) {
        if (m_proc->isRunning()) m_proc->kill();
        m_proc->clearArguments();
    }
}

void RsyncProcess::pause()
{
    if (!m_proc || !m_proc->isRunning()) return;
    
    int pid = m_proc->processIdentifier();
    if (pid > 0) {
        ::kill(pid, SIGSTOP);
    }
}

void RsyncProcess::resume()
{
    if (!m_proc || !m_proc->isRunning()) return;
    
    int pid = m_proc->processIdentifier();
    if (pid > 0) {
        ::kill(pid, SIGCONT);
    }
}

int RsyncProcess::processIdentifier() const
{
    if (!m_proc) return -1;
    return m_proc->processIdentifier();
}

void RsyncProcess::slotReadyReadStdout()
{
    if (!m_proc) return;
    const TQByteArray ba = m_proc->readStdout();
    if (ba.isEmpty()) return;
    emit sigStdout(TQString::fromLocal8Bit(ba.data()));
}

void RsyncProcess::slotReadyReadStderr()
{
    if (!m_proc) return;
    const TQByteArray ba = m_proc->readStderr();
    if (ba.isEmpty()) return;
    emit sigStderr(TQString::fromLocal8Bit(ba.data()));
}

void RsyncProcess::slotProcessExited()
{
    const int code = m_proc ? m_proc->exitStatus() : -1;
    const int status = 0;

    emit sigFinished(code, status);

    stop();
}
