#pragma once

#include <QObject>
#include <QString>
#include "generalmessage.h"
#include "mainwindowcontroller.h"

class GeneralMessageController: public QObject
{
    Q_OBJECT

public:
    static GeneralMessageController &instance()
    {
        static GeneralMessageController gmc;
        return gmc;
    }

    // must be called to complete init
    void setMainWindowController(MainWindowController *mwc);

    void showMessage(const QString &icon, const QString &title, const QString &desc,
                     const QString &acceptText,
                     const QString &rejectText = "",
                     const QString &tertiaryText = "",
                     std::function<void(bool)> acceptFunc = std::function<void(bool)>(nullptr),
                     std::function<void(bool)> rejectFunc = std::function<void(bool)>(nullptr),
                     std::function<void(bool)> tertiaryFunc = std::function<void(bool)>(nullptr),
                     GeneralMessage::Flags flags = GeneralMessage::Flags::kNone,
                     const QString &learnMoreUrl = "");

    void showMessage(GeneralMessage *message);

private slots:
    void onAcceptClick();
    void onRejectClick();
    void onTertiaryClick();

private:
    enum Result { NONE, ACCEPT, REJECT, TERTIARY };

    MainWindowController *controller_;
    QList<GeneralMessage *> messages_;
    bool isShowing_;

    explicit GeneralMessageController();
    void showNext();
    void handleResult(Result res);
};