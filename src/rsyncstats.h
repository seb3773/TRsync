#ifndef GRSYNC_TQT3_RSYNCSTATS_H
#define GRSYNC_TQT3_RSYNCSTATS_H

#include <ntqstring.h>
#include <ntqstringlist.h>
#include <ntqmap.h>

class RsyncStats
{
public:
    RsyncStats();
    
    void parseLine(const TQString &line, bool isError);
    void setExitCode(int code);
    
    TQString generateReport() const;
    TQString generateHTMLReport() const;
    
    int filesTransferred() const { return m_filesTransferred; }
    int filesSkipped() const { return m_filesSkipped; }
    int filesDeleted() const { return m_filesDeleted; }
    int errorCount() const { return m_errors.count(); }
    
    long long bytesTransferred() const { return m_bytesTransferred; }
    long long totalSize() const { return m_totalSize; }
    
    TQStringList errors() const { return m_errors; }
    TQStringList transferredFiles() const { return m_transferredFiles; }
    TQStringList deletedFiles() const { return m_deletedFiles; }
    
private:
    void parseRsyncSummary(const TQString &line);
    TQString formatBytes(long long bytes) const;
    TQString formatDuration(int seconds) const;
    
    int m_filesTransferred;
    int m_filesSkipped;
    int m_filesDeleted;
    int m_directoriesCreated;
    int m_exitCode;
    
    long long m_bytesTransferred;
    long long m_totalSize;
    long long m_speedupRatio;
    
    int m_durationSeconds;
    
    TQStringList m_errors;
    TQStringList m_transferredFiles;
    TQStringList m_deletedFiles;
    TQStringList m_warnings;
    
    TQMap<TQString, int> m_errorTypes;
};

#endif