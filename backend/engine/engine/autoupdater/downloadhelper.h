#ifndef DOWNLOADHELPER_H
#define DOWNLOADHELPER_H

#include <QString>
#include <QObject>
#include <QFile>
#include <QSharedPointer>
#include <QMap>

class NetworkAccessManager;
class NetworkReply;

struct OptionalSharedNetworkReply{
    explicit OptionalSharedNetworkReply(const QSharedPointer<NetworkReply> &reply) : reply(reply), valid(true) {}
    OptionalSharedNetworkReply() : valid(false) {}
    QSharedPointer<NetworkReply> reply;
    bool valid;
};

class DownloadHelper : public QObject
{
    Q_OBJECT
public:
    explicit DownloadHelper(QObject *parent, NetworkAccessManager *networkAccessManager);

    enum DownloadState {
        DOWNLOAD_STATE_INIT,
        DOWNLOAD_STATE_RUNNING,
        DOWNLOAD_STATE_SUCCESS,
        DOWNLOAD_STATE_FAIL
    };

    const QString downloadInstallerPath();
    const QString downloadInstallerPathWithoutExtension();
    const QString publicKeyInstallPath();
    const QString signatureInstallPath();

    void get(QMap<QString, QString> downloads);
    void stop();

    DownloadState state();

signals:
    void finished(DownloadHelper::DownloadState state);
    void progressChanged(uint progressPercent);

private slots:
    void onReplyFinished();
    void onReplyDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onReplyReadyRead();

private:
    NetworkAccessManager *networkAccessManager_;

    struct FileAndProgress {
        QSharedPointer<QFile> file;
        qint64 bytesReceived = 0;
        qint64 bytesTotal = 0;
    };

    QMap<QSharedPointer<NetworkReply>, FileAndProgress> replies_;
    bool busy_;

    QString downloadDirectory_;
    uint progressPercent_;
    DownloadState state_;

    void getInner(const QString url, const QString targetFilenamePath);
    void removeAutoUpdateInstallerFiles();
    bool allRepliesDone();
    void abortAllReplies();

    OptionalSharedNetworkReply replyFromSender(QObject *sender);

};

#endif // DOWNLOADHELPER_H
