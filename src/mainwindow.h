
#ifndef GRSYNC_TQT3_MAINWINDOW_H
#define GRSYNC_TQT3_MAINWINDOW_H

#include <ntqmainwindow.h>

class TQComboBox;
class TQCheckBox;
class TQLineEdit;
class TQListView;
class TQListViewItem;
class TQPushButton;
class TQTextEdit;

class RsyncProcess;
class RsyncDialogNew;

class MainWindow : public TQMainWindow
{
    TQ_OBJECT

public:
    MainWindow();
    ~MainWindow();

    void handleCommandLineArgs(bool execute, bool stayOpen, bool importMode, const TQString &session, const TQString &importFile);

private slots:
    void slotRun();
    void slotSimulate();
    void slotStop();
    void slotSessionChanged(int idx);

    void slotBrowseSource();
    void slotBrowseDest();
    void slotSwapSourceDest(bool checked);
    void slotMenuSwapSourceDest();
    void slotShowTrailingSlashInfo();

    void slotSessionAdd();
    void slotSessionDelete();
    void slotSessionImport();
    void slotSessionExport();

    void slotSetModeToggled(bool on);
    void slotSetListItemChanged(TQListViewItem *item);
    void slotAddSessionToSet();
    void slotRemoveSessionFromSet();

    void slotShowRsyncCommandLine();
    void slotPreferences();
    void slotAbout();
    void slotRsyncInfo();
    void slotBrowseLogDir();
    void slotUpdatePrefLogState();

    void slotRsyncStdout(const TQString &text);
    void slotRsyncStderr(const TQString &text);
    void slotRsyncFinished(int exitCode, int exitStatus);
    void slotPollLogFile();
    
    void slotPauseRsync();
    void slotResumeRsync();

    void slotEncryptionTypeChanged(int idx);
    void slotToggleShowPassword();

private:
    void loadSessions();
    void loadSessionToUi(const TQString &session);
    void saveUiToSession(const TQString &session);

    void updateSetListFromSettings(const TQString &session);
    void saveSetListToSettings(class IniFile &ini, const TQString &session);
    void updateSessionSelector();

    TQString buildRsyncCommandLineString() const;

    void loadPreferences();
    void savePreferences();
    void applyPreferencesToUi();
    void applyRememberedSession();

    TQString currentSessionName() const;
    void executeNextSetSession();

    TQComboBox *m_sessions;
    TQLineEdit *m_source;
    TQLineEdit *m_dest;
    TQTextEdit *m_additional;

    TQListView *m_setList;
    TQCheckBox *m_is_set;
    TQComboBox *m_setSessionSelector;
    TQPushButton *m_btnAddToSet;
    TQPushButton *m_btnRemoveFromSet;

    TQLineEdit *m_notes;
    TQCheckBox *m_check_com_before;
    TQLineEdit *m_com_before;
    TQCheckBox *m_check_com_halt;
    TQCheckBox *m_check_com_after;
    TQLineEdit *m_com_after;
    TQCheckBox *m_check_com_onerror;
    TQCheckBox *m_check_browse_files;
    TQCheckBox *m_check_superuser;

    TQCheckBox *m_check_time;
    TQCheckBox *m_check_perm;
    TQCheckBox *m_check_owner;
    TQCheckBox *m_check_group;
    TQCheckBox *m_check_delete;
    TQCheckBox *m_check_onefs;
    TQCheckBox *m_check_verbose;
    TQCheckBox *m_check_progr;
    TQCheckBox *m_check_exist;
    TQCheckBox *m_check_size;
    TQCheckBox *m_check_skipnew;
    TQCheckBox *m_check_windows;

    TQCheckBox *m_check_sum;
    TQCheckBox *m_check_compr;
    TQCheckBox *m_check_dev;
    TQCheckBox *m_check_update;
    TQCheckBox *m_check_keepart;
    TQCheckBox *m_check_mapuser;
    TQCheckBox *m_check_symlink;
    TQCheckBox *m_check_hardlink;
    TQCheckBox *m_check_backup;
    TQCheckBox *m_check_itemized;
    TQCheckBox *m_check_norecur;
    TQCheckBox *m_check_protectargs;

    TQPushButton *m_run;
    TQPushButton *m_stop;
    TQTextEdit *m_output;

    TQPushButton *m_btnSwap;

    RsyncProcess *m_rsync;
    RsyncDialogNew *m_rsyncDialog;
    class TQFile *m_logFile;
    TQLineEdit *m_prefCustomLogDirEdit;
    class TQCheckBox *m_prefCbLog;
    class TQCheckBox *m_prefCbOverwrite;
    class TQCheckBox *m_prefCbCustomLogDir;
    class TQPushButton *m_prefBtnBrowseLogDir;

    int m_dryrunNext;
    int m_runningDryrun;

    TQString m_cfg_command;
    bool m_cfg_output;
    bool m_cfg_remember;
    bool m_cfg_errorlist;
    bool m_cfg_log;
    bool m_cfg_log_overwrite;
    bool m_cfg_fastscroll;
    bool m_cfg_switchbutton;
    bool m_cfg_trayicon;
    bool m_cfg_custom_log_dir_enabled;
    TQString m_cfg_custom_log_dir;

    // Session sets execution state
    bool m_isExecutingSet;
    TQStringList m_setSessionsToRun;
    int m_currentSetIndex;
    TQString m_originalSetSessionName;
    bool m_setHadErrors;

    // Command line args state
    bool m_cmdlineExecute;
    bool m_cmdlineStayOpen;
    bool m_cmdlineImport;
    TQString m_cmdlineSessionName;
    TQString m_cmdlineImportFilename;

    class TQTimer *m_pollTimer;
    unsigned long m_lastReadOffset;

    // Encryption UI components
    class TQComboBox *m_comboEncryption;
    class TQLabel *m_lblWarningNotInstalled;
    class TQLabel *m_lblPassword;
    class TQLineEdit *m_editPassword;
    class TQPushButton *m_btnShowPassword;
    class TQCheckBox *m_checkEncryptNames;

    TQString m_mountedTempDir;
    bool m_isBackupRun;
    TQString m_currentConfigPath;
    TQString m_destConfigDir;
};

#endif

