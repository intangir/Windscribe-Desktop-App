#pragma once

#include "basefailover.h"
#include "utils/hardcodedsettings.h"

#include <QTimer>

namespace server_api {

// Procedurally Generated Domain
class RandomDomainFailover : public BaseFailover
{
    Q_OBJECT
public:
    explicit RandomDomainFailover(QObject *parent) : BaseFailover(parent) {}
    void getHostnames(bool /*bIgnoreSslErrors*/) override {
        QTimer::singleShot(0, [this]() {
            emit finished(FailoverRetCode::kSuccess, QStringList() << HardcodedSettings::instance().generateDomain());
        });
    }

    QString name() const override
    {
        // the domain name has been reduced to 3 characters for log security
        return "rnd";
    }

};

} // namespace server_api

