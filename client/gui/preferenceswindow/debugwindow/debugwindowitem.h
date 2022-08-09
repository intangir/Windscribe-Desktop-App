#ifndef DEBUGWINDOWITEM_H
#define DEBUGWINDOWITEM_H

#include <QTimer>
#include "../basepage.h"
#include "../connectionwindow/subpageitem.h"
#include "viewlogitem.h"
#include "../comboboxitem.h"
#include "../checkboxitem.h"
#include "backend/preferences/preferences.h"
#include "backend/preferences/preferenceshelper.h"
#include "apiresolutionitem.h"
#include "tooltips/tooltiptypes.h"
#include "../openurlitem.h"


enum DEBUG_SCREEN { DEBUG_SCREEN_HOME, DEBUG_SCREEN_ADVANCED_PARAMETERS };

namespace PreferencesWindow {

class DebugWindowItem : public BasePage
{
    Q_OBJECT
public:
    explicit DebugWindowItem(ScalableGraphicsObject *parent, Preferences *preferences, PreferencesHelper *preferencesHelper);

    QString caption();

    DEBUG_SCREEN getScreen();
    void setScreen(DEBUG_SCREEN screen);
    void setDebugLogResult(bool success);

    void updateScaling() override;

signals:
    void viewLogClick();
    void sendLogClick();
    void advParametersClick();
#ifdef Q_OS_WIN
    void setIpv6StateInOS(bool bEnabled, bool bRestartNow);
#endif
private slots:
    void onApiResolutionChanged(const types::DnsResolutionSettings &dns);
    void onIgnoreSslErrorsStateChanged(bool isChecked);
    void onKeepAliveStateChanged(bool isChecked);
    void onAppInternalDnsItemChanged(QVariant dns);
#ifdef Q_OS_LINUX
    void onDnsManagerItemChanged(QVariant dns);
#endif

    void onViewLogItemSendLogClicked();
    void onViewButtonHoverEnter();
    void onSendButtonHoverEnter();
    void onViewOrSendButtonHoverLeave();

    void onIgnoreSslErrorsPreferencesChanged(bool b);
    void onKeepAlivePreferencesChanged(bool b);
    void onDnsPolicyPreferencesChanged(DNS_POLICY_TYPE d);
#ifdef Q_OS_LINUX
    void onDnsManagerPreferencesChanged(DNS_MANAGER_TYPE d);
#endif
    void onApiResolutionPreferencesChanged(const types::DnsResolutionSettings &dns);

    void onLanguageChanged();

#ifdef Q_OS_WIN
    void onIPv6StateChanged(bool isChecked);
    void onPreferencesIpv6InOSStateChanged(bool bEnabled);
    void onTapAdapterChanged(QVariant v);
    void onTapAdapterPreferencesChanged(TAP_ADAPTER_TYPE tapAdapter);
#endif

protected:
    void hideOpenPopups() override;

private:
    SubPageItem *advParamtersItem_;
    ViewLogItem *viewLogItem_;

    ApiResolutionItem *apiResolutionItem_;
    CheckBoxItem *cbIgnoreSslErrors_;
    CheckBoxItem *cbKeepAlive_;
    ComboBoxItem *comboBoxAppInternalDns_;
#ifdef Q_OS_LINUX
    ComboBoxItem *comboBoxDnsManager_;
#endif
    OpenUrlItem *viewLicensesItem_;

    Preferences *preferences_;
    PreferencesHelper *preferencesHelper_;

    DEBUG_SCREEN currentScreen_;

    bool pendingDebugSendResposeFromEngine_;

#ifdef Q_OS_WIN
    CheckBoxItem *checkBoxIPv6_;
    ComboBoxItem *comboBoxTapAdapter_;
    void updateTapAdaptersList();
#endif

    void showSendButtonToolTip(const QString &text);

};

} // namespace PreferencesWindow

#endif // DEBUGWINDOWITEM_H
