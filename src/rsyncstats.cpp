#include "rsyncstats.h"
#include <ntqregexp.h>
#include <ntqdatetime.h>

RsyncStats::RsyncStats()
    : m_filesTransferred(0)
    , m_filesSkipped(0)
    , m_filesDeleted(0)
    , m_directoriesCreated(0)
    , m_exitCode(0)
    , m_bytesTransferred(0)
    , m_totalSize(0)
    , m_speedupRatio(0)
    , m_durationSeconds(0)
{
}

void RsyncStats::parseLine(const TQString &line, bool isError)
{
    if (line.isEmpty()) return;
    
    if (isError) {
        m_errors.append(line);
        
        // Categorize error types
        if (line.contains("Permission denied", false)) {
            m_errorTypes["Permission denied"]++;
        } else if (line.contains("No such file", false)) {
            m_errorTypes["File not found"]++;
        } else if (line.contains("failed", false)) {
            m_errorTypes["Operation failed"]++;
        } else {
            m_errorTypes["Other errors"]++;
        }
        return;
    }
    
    // Parse rsync summary lines
    if (line.startsWith("Number of files:") || 
        line.startsWith("Number of created files:") ||
        line.startsWith("Number of deleted files:") ||
        line.startsWith("Number of regular files transferred:") ||
        line.startsWith("Total file size:") ||
        line.startsWith("Total transferred file size:") ||
        line.startsWith("Literal data:") ||
        line.startsWith("Matched data:") ||
        line.startsWith("File list size:") ||
        line.startsWith("Total bytes sent:") ||
        line.startsWith("Total bytes received:")) {
        parseRsyncSummary(line);
        return;
    }
    
    // Track transferred files (lines starting with file operations)
    if (line.startsWith(">f") || line.startsWith("cd") || line.startsWith("*deleting")) {
        if (line.startsWith("*deleting")) {
            m_filesDeleted++;
            TQString filename = line.mid(10).stripWhiteSpace();
            m_deletedFiles.append(filename);
        } else {
            m_filesTransferred++;
            // Extract filename after operation code
            TQString filename = line.mid(11).stripWhiteSpace();
            if (!filename.isEmpty()) {
                m_transferredFiles.append(filename);
            }
        }
    }
    
    // Detect warnings
    if (line.contains("warning:", false) || line.contains("skipping", false)) {
        m_warnings.append(line);
        if (line.contains("skipping", false)) {
            m_filesSkipped++;
        }
    }
}

void RsyncStats::parseRsyncSummary(const TQString &line)
{
    TQRegExp numRx("([0-9,]+)");
    
    if (line.startsWith("Number of regular files transferred:")) {
        if (numRx.search(line) >= 0) {
            TQString num = numRx.cap(1);
            num.replace(",", "");
            m_filesTransferred = num.toInt();
        }
    }
    else if (line.startsWith("Number of deleted files:")) {
        if (numRx.search(line) >= 0) {
            TQString num = numRx.cap(1);
            num.replace(",", "");
            m_filesDeleted = num.toInt();
        }
    }
    else if (line.startsWith("Number of created files:")) {
        if (numRx.search(line) >= 0) {
            TQString num = numRx.cap(1);
            num.replace(",", "");
            m_directoriesCreated = num.toInt();
        }
    }
    else if (line.startsWith("Total file size:")) {
        if (numRx.search(line) >= 0) {
            TQString num = numRx.cap(1);
            num.replace(",", "");
            m_totalSize = num.toLongLong();
        }
    }
    else if (line.startsWith("Total transferred file size:") || 
             line.startsWith("Literal data:")) {
        if (numRx.search(line) >= 0) {
            TQString num = numRx.cap(1);
            num.replace(",", "");
            m_bytesTransferred = num.toLongLong();
        }
    }
}

void RsyncStats::setExitCode(int code)
{
    m_exitCode = code;
}

TQString RsyncStats::formatBytes(long long bytes) const
{
    if (bytes < 1024) {
        return TQString::number(bytes) + " B";
    } else if (bytes < 1024 * 1024) {
        return TQString::number(bytes / 1024.0, 'f', 2) + " KB";
    } else if (bytes < 1024 * 1024 * 1024) {
        return TQString::number(bytes / (1024.0 * 1024.0), 'f', 2) + " MB";
    } else {
        return TQString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
    }
}

TQString RsyncStats::formatDuration(int seconds) const
{
    if (seconds < 60) {
        return TQString::number(seconds) + " seconds";
    } else if (seconds < 3600) {
        int mins = seconds / 60;
        int secs = seconds % 60;
        return TQString::number(mins) + "m " + TQString::number(secs) + "s";
    } else {
        int hours = seconds / 3600;
        int mins = (seconds % 3600) / 60;
        return TQString::number(hours) + "h " + TQString::number(mins) + "m";
    }
}

TQString RsyncStats::generateReport() const
{
    TQString report;
    
    report += "=== RSYNC OPERATION REPORT ===\n\n";
    
    // Status
    report += "Status: ";
    if (m_exitCode == 0 && m_errors.isEmpty()) {
        report += "SUCCESS\n";
    } else if (m_exitCode == 0 && !m_errors.isEmpty()) {
        report += "COMPLETED WITH WARNINGS\n";
    } else {
        report += "FAILED (Exit code: " + TQString::number(m_exitCode) + ")\n";
    }
    report += "\n";
    
    // File Statistics
    report += "--- File Statistics ---\n";
    report += "Files transferred: " + TQString::number(m_filesTransferred) + "\n";
    if (m_filesDeleted > 0) {
        report += "Files deleted: " + TQString::number(m_filesDeleted) + "\n";
    }
    if (m_filesSkipped > 0) {
        report += "Files skipped: " + TQString::number(m_filesSkipped) + "\n";
    }
    if (m_directoriesCreated > 0) {
        report += "Directories created: " + TQString::number(m_directoriesCreated) + "\n";
    }
    report += "\n";
    
    // Data Transfer
    if (m_bytesTransferred > 0 || m_totalSize > 0) {
        report += "--- Data Transfer ---\n";
        if (m_bytesTransferred > 0) {
            report += "Bytes transferred: " + formatBytes(m_bytesTransferred) + "\n";
        }
        if (m_totalSize > 0) {
            report += "Total size: " + formatBytes(m_totalSize) + "\n";
        }
        report += "\n";
    }
    
    // Errors
    if (!m_errors.isEmpty()) {
        report += "--- Errors (" + TQString::number(m_errors.count()) + ") ---\n";
        
        // Group by error type
        if (!m_errorTypes.isEmpty()) {
            report += "Error breakdown:\n";
            for (TQMap<TQString, int>::ConstIterator it = m_errorTypes.begin(); 
                 it != m_errorTypes.end(); ++it) {
                report += "  " + it.key() + ": " + TQString::number(it.data()) + "\n";
            }
            report += "\n";
        }
        
        report += "Error details:\n";
        int maxErrors = 20;
        for (int i = 0; i < (int)m_errors.count() && i < maxErrors; ++i) {
            report += "  " + m_errors[i] + "\n";
        }
        if ((int)m_errors.count() > maxErrors) {
            report += "  ... and " + TQString::number(m_errors.count() - maxErrors) + " more errors\n";
        }
        report += "\n";
    }
    
    // Warnings
    if (!m_warnings.isEmpty()) {
        report += "--- Warnings (" + TQString::number(m_warnings.count()) + ") ---\n";
        int maxWarnings = 10;
        for (int i = 0; i < (int)m_warnings.count() && i < maxWarnings; ++i) {
            report += "  " + m_warnings[i] + "\n";
        }
        if ((int)m_warnings.count() > maxWarnings) {
            report += "  ... and " + TQString::number(m_warnings.count() - maxWarnings) + " more warnings\n";
        }
        report += "\n";
    }
    
    // Deleted files list
    if (!m_deletedFiles.isEmpty() && m_deletedFiles.count() <= 50) {
        report += "--- Deleted Files ---\n";
        for (TQStringList::ConstIterator it = m_deletedFiles.begin(); 
             it != m_deletedFiles.end(); ++it) {
            report += "  " + (*it) + "\n";
        }
        report += "\n";
    }
    
    report += "=== END OF REPORT ===\n";
    
    return report;
}

TQString RsyncStats::generateHTMLReport() const
{
    TQString html;
    
    html += "<html><head><style>";
    html += "body { font-family: monospace; margin: 20px; }";
    html += "h2 { color: #333; border-bottom: 2px solid #666; }";
    html += "h3 { color: #555; }";
    html += ".success { color: green; font-weight: bold; }";
    html += ".warning { color: orange; font-weight: bold; }";
    html += ".error { color: red; font-weight: bold; }";
    html += ".stat { margin: 5px 0; }";
    html += ".stat-label { font-weight: bold; display: inline-block; width: 200px; }";
    html += ".error-list { background: #fee; padding: 10px; margin: 10px 0; }";
    html += ".warning-list { background: #ffc; padding: 10px; margin: 10px 0; }";
    html += "</style></head><body>";
    
    html += "<h2>Rsync Operation Report</h2>";
    
    // Status
    html += "<h3>Status</h3>";
    if (m_exitCode == 0 && m_errors.isEmpty()) {
        html += "<p class=\"success\">SUCCESS</p>";
    } else if (m_exitCode == 0 && !m_errors.isEmpty()) {
        html += "<p class=\"warning\">COMPLETED WITH WARNINGS</p>";
    } else {
        html += "<p class=\"error\">FAILED (Exit code: " + TQString::number(m_exitCode) + ")</p>";
    }
    
    // Statistics
    html += "<h3>File Statistics</h3>";
    html += "<div class=\"stat\"><span class=\"stat-label\">Files transferred:</span>" + 
            TQString::number(m_filesTransferred) + "</div>";
    if (m_filesDeleted > 0) {
        html += "<div class=\"stat\"><span class=\"stat-label\">Files deleted:</span>" + 
                TQString::number(m_filesDeleted) + "</div>";
    }
    if (m_filesSkipped > 0) {
        html += "<div class=\"stat\"><span class=\"stat-label\">Files skipped:</span>" + 
                TQString::number(m_filesSkipped) + "</div>";
    }
    
    // Data transfer
    if (m_bytesTransferred > 0 || m_totalSize > 0) {
        html += "<h3>Data Transfer</h3>";
        if (m_bytesTransferred > 0) {
            html += "<div class=\"stat\"><span class=\"stat-label\">Bytes transferred:</span>" + 
                    formatBytes(m_bytesTransferred) + "</div>";
        }
        if (m_totalSize > 0) {
            html += "<div class=\"stat\"><span class=\"stat-label\">Total size:</span>" + 
                    formatBytes(m_totalSize) + "</div>";
        }
    }
    
    // Errors
    if (!m_errors.isEmpty()) {
        html += "<h3 class=\"error\">Errors (" + TQString::number(m_errors.count()) + ")</h3>";
        html += "<div class=\"error-list\">";
        int maxErrors = 20;
        for (int i = 0; i < (int)m_errors.count() && i < maxErrors; ++i) {
            html += m_errors[i] + "<br/>";
        }
        if ((int)m_errors.count() > maxErrors) {
            html += "<i>... and " + TQString::number(m_errors.count() - maxErrors) + " more errors</i>";
        }
        html += "</div>";
    }
    
    html += "</body></html>";
    
    return html;
}