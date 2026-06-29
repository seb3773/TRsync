#ifndef TQTMULTIPROGRESSDIALOG_H
#define TQTMULTIPROGRESSDIALOG_H

#include <ntqwidget.h>
#include <ntqstring.h>

class TQCheckBox;
class TQLabel;
class TQProgressBar;
class TQPushButton;
class TQTextEdit;

class TQtMultiProgressDialog : public TQWidget {
    TQ_OBJECT
public:
    enum ProgressHint {
        PartialProgress = 0,
        TotalProgress = 1
    };

    TQtMultiProgressDialog(TQWidget* parent = 0, const char* name = 0);
    ~TQtMultiProgressDialog();

    int progress(ProgressHint hint = PartialProgress) const;
    int minimum(ProgressHint hint = PartialProgress) const;
    int maximum(ProgressHint hint = PartialProgress) const;

    TQString labelText(ProgressHint hint = PartialProgress) const;

    bool isAutoClose() const;
    bool wasCanceled() const;

public slots:
    void setProgress(int value, ProgressHint hint = PartialProgress);
    void setRange(int minimum, int maximum, ProgressHint hint = PartialProgress);
    void setMinimum(int minimum, ProgressHint hint = PartialProgress);
    void setMaximum(int maximum, ProgressHint hint = PartialProgress);

    void setLabelText(const TQString& text, ProgressHint hint = PartialProgress);
    void setAutoClose(bool on = true);

    void cancel();
    void reset();

    void message(const TQString& text);
    void clearMessages();
    void showDetails(bool show = true);

signals:
    void canceled();

private slots:
    void slotDetailsToggled_(bool on);

private:
    TQProgressBar* progressBar_(ProgressHint hint) const;
    TQLabel* label_(ProgressHint hint) const;

private:
    TQLabel* m_totalLabel;
    TQProgressBar* m_totalBar;

    TQLabel* m_partialLabel;
    TQProgressBar* m_partialBar;

    TQPushButton* m_detailsButton;
    TQTextEdit* m_messageBox;

    TQCheckBox* m_autoCloseChecker;
    TQPushButton* m_cancelButton;

    int m_wasCanceled;
};

#endif
