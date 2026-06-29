#include "tqtmultiprogressdialog.h"

#include <ntqapplication.h>
#include <ntqcheckbox.h>
#include <ntqlabel.h>
#include <ntqlayout.h>
#include <ntqprogressbar.h>
#include <ntqpushbutton.h>
#include <ntqtextedit.h>

// Complexity: O(1)
TQtMultiProgressDialog::TQtMultiProgressDialog(TQWidget* parent, const char* name)
    : TQWidget(parent, name),
      m_totalLabel(0),
      m_totalBar(0),
      m_partialLabel(0),
      m_partialBar(0),
      m_detailsButton(0),
      m_messageBox(0),
      m_autoCloseChecker(0),
      m_cancelButton(0),
      m_wasCanceled(0)
{
    m_totalLabel = new TQLabel(this);
    m_totalLabel->setMinimumWidth(400);
    
    m_totalBar = new TQProgressBar(this);
    m_totalBar->setMinimumWidth(400);
    m_totalBar->setMinimumHeight(20);

    m_partialLabel = new TQLabel(this);
    m_partialLabel->setMinimumWidth(400);
    
    m_partialBar = new TQProgressBar(this);
    m_partialBar->setMinimumWidth(400);
    m_partialBar->setMinimumHeight(20);

    m_autoCloseChecker = new TQCheckBox(tr("Close dialog after operation complete"), this);
    m_autoCloseChecker->setChecked(false);

    m_cancelButton = new TQPushButton(tr("Cancel"), this);
    connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(cancel()));

    m_messageBox = new TQTextEdit(this);
    m_messageBox->setReadOnly(true);
    m_messageBox->setMinimumHeight(200);
    m_messageBox->setMinimumWidth(400);
    m_messageBox->hide();

    m_detailsButton = new TQPushButton(tr("Show Details..."), this);
    m_detailsButton->setToggleButton(true);
    m_detailsButton->setOn(false);
    m_detailsButton->hide();
    connect(m_detailsButton, SIGNAL(toggled(bool)), this, SLOT(slotDetailsToggled_(bool)));

    TQHBoxLayout* detailsLayout = new TQHBoxLayout();
    detailsLayout->setSpacing(6);
    detailsLayout->addWidget(m_detailsButton);
    detailsLayout->addStretch();

    TQHBoxLayout* buttonLayout = new TQHBoxLayout();
    buttonLayout->setSpacing(6);
    buttonLayout->addWidget(m_autoCloseChecker);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_cancelButton);

    TQVBoxLayout* mainLayout = new TQVBoxLayout(this, 8, 6);
    mainLayout->addWidget(m_totalLabel);
    mainLayout->addWidget(m_totalBar);
    mainLayout->addWidget(m_partialLabel);
    mainLayout->addWidget(m_partialBar);
    mainLayout->addLayout(detailsLayout);
    mainLayout->addWidget(m_messageBox, 1);
    mainLayout->addLayout(buttonLayout);

    setSizePolicy(TQSizePolicy::Fixed, TQSizePolicy::Fixed);
}

// Complexity: O(1)
TQtMultiProgressDialog::~TQtMultiProgressDialog()
{
}

// Complexity: O(1)
TQProgressBar* TQtMultiProgressDialog::progressBar_(ProgressHint hint) const
{
    if (hint == TotalProgress)
        return m_totalBar;
    return m_partialBar;
}

// Complexity: O(1)
TQLabel* TQtMultiProgressDialog::label_(ProgressHint hint) const
{
    if (hint == TotalProgress)
        return m_totalLabel;
    return m_partialLabel;
}

// Complexity: O(1)
int TQtMultiProgressDialog::progress(ProgressHint hint) const
{
    return progressBar_(hint)->progress();
}

// Complexity: O(1)
int TQtMultiProgressDialog::minimum(ProgressHint hint) const
{
    (void)hint;
    return 0;
}

// Complexity: O(1)
int TQtMultiProgressDialog::maximum(ProgressHint hint) const
{
    return progressBar_(hint)->totalSteps();
}

// Complexity: O(1)
TQString TQtMultiProgressDialog::labelText(ProgressHint hint) const
{
    return label_(hint)->text();
}

// Complexity: O(1)
bool TQtMultiProgressDialog::isAutoClose() const
{
    return m_autoCloseChecker->isChecked();
}

// Complexity: O(1)
bool TQtMultiProgressDialog::wasCanceled() const
{
    return m_wasCanceled ? true : false;
}

// Complexity: O(1)
void TQtMultiProgressDialog::setProgress(int value, ProgressHint hint)
{
    TQProgressBar* b = progressBar_(hint);
    const int maxv = b->totalSteps();
    if (value < 0)
        value = 0;
    if (maxv > 0 && value > maxv)
        value = maxv;
    b->setProgress(value);
    
    // Force UI update
    tqApp->processEvents();

    if (hint == PartialProgress) {
        if (value == m_partialBar->totalSteps()) {
            const int tmax = m_totalBar->totalSteps();
            int tp = m_totalBar->progress();
            if (tmax > 0 && tp < tmax) {
                m_totalBar->setProgress(tp + 1);
            }
        }
        return;
    }

    if (hint == TotalProgress) {
        if (value == m_totalBar->totalSteps()) {
            disconnect(m_cancelButton, 0, 0, 0);
            m_cancelButton->setText(tr("Close"));
            connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(close()));

            if (m_autoCloseChecker->isChecked())
                close();
            else
                m_autoCloseChecker->hide();
        }
    }
}

// Complexity: O(1)
void TQtMultiProgressDialog::setRange(int minimum, int maximum, ProgressHint hint)
{
    (void)minimum;
    TQProgressBar* b = progressBar_(hint);
    b->setTotalSteps(maximum);
}

// Complexity: O(1)
void TQtMultiProgressDialog::setMinimum(int minimum, ProgressHint hint)
{
    (void)minimum;
    (void)hint;
}

// Complexity: O(1)
void TQtMultiProgressDialog::setMaximum(int maximum, ProgressHint hint)
{
    TQProgressBar* b = progressBar_(hint);
    b->setTotalSteps(maximum);
}

// Complexity: O(1)
void TQtMultiProgressDialog::setLabelText(const TQString& text, ProgressHint hint)
{
    label_(hint)->setText(text);
    // Force UI update
    tqApp->processEvents();
}

// Complexity: O(1)
void TQtMultiProgressDialog::setAutoClose(bool on)
{
    m_autoCloseChecker->setChecked(on ? true : false);
}

// Complexity: O(1)
void TQtMultiProgressDialog::cancel()
{
    m_wasCanceled = 1;
    emit canceled();
    clearMessages();
}

// Complexity: O(1)
void TQtMultiProgressDialog::reset()
{
    m_wasCanceled = 0;
    m_cancelButton->setEnabled(true);
    m_cancelButton->setText(tr("Cancel"));
    disconnect(m_cancelButton, SIGNAL(clicked()), this, SLOT(close()));
    disconnect(m_cancelButton, SIGNAL(clicked()), this, SLOT(cancel()));
    connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(cancel()));

    m_autoCloseChecker->show();
    clearMessages();

    setProgress(0, TotalProgress);
    setProgress(0, PartialProgress);
}

// Complexity: O(n)
void TQtMultiProgressDialog::message(const TQString& text)
{
    if (!m_detailsButton->isVisible())
        m_detailsButton->show();

    if (!m_messageBox->isVisible() && m_detailsButton->isOn())
        m_messageBox->show();

    const TQString prev = m_messageBox->text();
    if (prev.isEmpty())
        m_messageBox->setText(text);
    else
        m_messageBox->setText(prev + "\n" + text);
    
    // Scroll to bottom to show latest message
    m_messageBox->scrollToBottom();
    
    // Force UI update
    tqApp->processEvents();
}

// Complexity: O(1)
void TQtMultiProgressDialog::clearMessages()
{
    m_messageBox->setText(TQString());
}

// Complexity: O(1)
void TQtMultiProgressDialog::showDetails(bool show)
{
    m_detailsButton->setOn(show);
    slotDetailsToggled_(show);
}

// Complexity: O(1)
void TQtMultiProgressDialog::slotDetailsToggled_(bool on)
{
    if (on) {
        m_detailsButton->setText(tr("Hide Details"));
        m_messageBox->show();
    } else {
        m_detailsButton->setText(tr("Show Details..."));
        m_messageBox->hide();
    }
    adjustSize();
}

