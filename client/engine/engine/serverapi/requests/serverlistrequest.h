#pragma once

#include "baserequest.h"
#include "engine/apiinfo/location.h"
#include "engine/connectstatecontroller/iconnectstatecontroller.h"

namespace server_api {

class ServerListRequest : public BaseRequest
{
    Q_OBJECT
public:
    explicit ServerListRequest(QObject *parent, const QString &language, const QString &revision, bool isPro, PROTOCOL protocol,
                               const QStringList &alcList, IConnectStateController *connectStateController);

    QUrl url(const QString &domain) const override;
    QString name() const override;
    void handle(const QByteArray &arr) override;

    // output values
    QVector<apiinfo::Location> locations() const;
    QStringList forceDisconnectNodes() const;

private:
    QString language_;
    QString revision_;
    bool isPro_;
    PROTOCOL protocol_;
    QStringList alcList_;
    IConnectStateController *connectStateController_; //FIXME: should be here?
    bool isFromDisconnectedVPNState_;

    // output values
    QVector<apiinfo::Location> locations_;
    QStringList forceDisconnectNodes_;
};

} // namespace server_api {

