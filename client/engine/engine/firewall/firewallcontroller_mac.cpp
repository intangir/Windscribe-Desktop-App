#include "firewallcontroller_mac.h"
#include <QStandardPaths>
#include "utils/logger.h"
#include <QDir>
#include <QCoreApplication>

FirewallController_mac::FirewallController_mac(QObject *parent, IHelper *helper) :
    FirewallController(parent), forceUpdateInterfaceToSkip_(false)
{
    helper_ = dynamic_cast<Helper_mac *>(helper);
}

FirewallController_mac::~FirewallController_mac()
{

}

bool FirewallController_mac::firewallOn(const QString &ip, bool bAllowLanTraffic)
{
    QMutexLocker locker(&mutex_);
    FirewallController::firewallOn(ip, bAllowLanTraffic);
    if (isStateChanged())
    {
        qCDebug(LOG_FIREWALL_CONTROLLER) << "firewall changed with ips count:" << countIps(ip);
        return firewallOnImpl(ip, bAllowLanTraffic, latestStaticIpPorts_);
    }
    else if (forceUpdateInterfaceToSkip_)
    {
        qCDebug(LOG_FIREWALL_CONTROLLER) << "firewall changed due to interface-to-skip update";
        return firewallOnImpl(ip, bAllowLanTraffic, latestStaticIpPorts_);
    }
    else
    {
        return true;
    }
}

bool FirewallController_mac::firewallOff()
{
    QMutexLocker locker(&mutex_);
    FirewallController::firewallOff();
    if (isStateChanged())
    {
        //restore settings
        QString str = helper_->executeRootCommand("pfctl -v -f /etc/pf.conf");
        qCDebug(LOG_FIREWALL_CONTROLLER) << "Output from pfctl -v -f /etc/pf.conf command: " << str;
        str = helper_->executeRootCommand("pfctl -d");
        qCDebug(LOG_FIREWALL_CONTROLLER) << "Output from disable firewall command: " << str;

        // force delete the table "windscribe_ips"
        str = helper_->executeRootCommand("pfctl -t windscribe_ips -T kill");

        str = helper_->executeRootCommand("pfctl -si");
        qCDebug(LOG_FIREWALL_CONTROLLER) << "Output from status firewall command: " << str;

        qCDebug(LOG_FIREWALL_CONTROLLER) << "firewallOff disabled";

        return true;
    }
    else
    {
        return true;
    }
}

bool FirewallController_mac::firewallActualState()
{
    QMutexLocker locker(&mutex_);
    if (helper_->currentState() != IHelper::STATE_CONNECTED)
    {
        return false;
    }

    // Additionally check the table "windscribe_ips", which will indicate that the firewall is enabled by our program.
    QString tableReport = helper_->executeRootCommand("pfctl -t windscribe_ips -T show");
    if (tableReport.isEmpty())
    {
        return false;
    }

    QString report = helper_->executeRootCommand("pfctl -si");
    if (report.indexOf("Status: Enabled") != -1)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool FirewallController_mac::whitelistPorts(const types::StaticIpPortsVector &ports)
{
    QMutexLocker locker(&mutex_);
    FirewallController::whitelistPorts(ports);
    if (isStateChanged() && latestEnabledState_)
    {
        return firewallOnImpl(latestIp_, latestAllowLanTraffic_, ports);
    }
    else
    {
        return true;
    }
}

bool FirewallController_mac::deleteWhitelistPorts()
{
    return whitelistPorts(types::StaticIpPortsVector());
}

bool FirewallController_mac::firewallOnImpl(const QString &ip, bool bAllowLanTraffic, const types::StaticIpPortsVector &ports )
{
    QString pfConfigFilePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir dir(pfConfigFilePath);
    dir.mkpath(pfConfigFilePath);
    pfConfigFilePath += "/pf.conf";
    qCDebug(LOG_BASIC) << pfConfigFilePath;

    forceUpdateInterfaceToSkip_ = false;

    QString pf = "";
    pf += "# Automatically generated by Windscribe. Any manual change will be overridden.\n";
    pf += "# Block policy, RST for quickly notice\n";
    pf += "set block-policy return\n";

    pf += "# Skip interfaces: lo0 and utun (only when connected)\n";
    if (!interfaceToSkip_.isEmpty())
    {
        pf += "set skip on { lo0 " + interfaceToSkip_ + " }\n";
    }
    else
    {
        pf += "set skip on { lo0}\n";
    }

    pf += "# Scrub\n";
    pf += "scrub in all\n"; // 2.9

    pf += "# Drop everything that doesn't match a rule\n";
    pf += "block in all\n";
    pf += "block out all\n";

    QString ips =ip;
    ips = ips.replace(";", " ");
    pf += "table <windscribe_ips> const { " + ips + " }\n";

    pf += "pass out quick inet proto udp from 0.0.0.0 to 255.255.255.255 port = 67\n";
    pf += "pass in quick proto udp from any to any port = 68\n";

    pf += "pass out quick inet from any to <windscribe_ips> flags S/SA keep state\n";
    pf += "pass in quick inet from any to <windscribe_ips> flags S/SA keep state\n";

    if (!ports.isEmpty())
    {
        //pass in proto tcp from any to any port 1234
        for (unsigned int port : ports)
        {
            pf += "pass in quick proto tcp from any to any port = " + QString::number(port) + "\n";
        }
    }

    if (bAllowLanTraffic)
    {
        // Local Network;
        pf += "pass out quick inet from any to 192.168.0.0/16 flags S/SA keep state\n";
        pf += "pass in quick inet from 192.168.0.0/16 to any flags S/SA keep state\n";
        pf += "pass out quick inet from any to 172.16.0.0/12 flags S/SA keep state\n";
        pf += "pass in quick inet from 172.16.0.0/12 to any flags S/SA keep state\n";
        pf += "pass out quick inet from any to 169.254.0.0/16 flags S/SA keep state\n";
        pf += "pass in quick inet from 169.254.0.0/16 to any flags S/SA keep state\n";
        pf += "block out quick inet from any to 10.255.255.0/24\n";
        pf += "block in quick inet from 10.255.255.0/24 to any\n";
        pf += "pass out quick inet from any to 10.0.0.0/8 flags S/SA keep state\n";
        pf += "pass in quick inet from 10.0.0.0/8 to any flags S/SA keep state\n";

        // Loopback addresses to the local host
        pf += "pass in quick inet from 127.0.0.0/8 to any flags S/SA keep state\n";

        // Multicast addresses
        pf += "pass in quick inet from 224.0.0.0/4 to any flags S/SA keep state\n";

        // Allow AirDrop
        pf += "pass in quick on awdl0 inet6 proto udp from any to any port = 5353 keep state\n";
        pf += "pass out quick on awdl0 proto tcp all flags any keep state\n";

        // UPnP
        //pf += "pass out quick inet proto udp from 239.255.255.250 to 239.255.255.250 port = 1900\n";
        //pf += "pass in quick inet proto udp from 239.255.255.250 to 239.255.255.250 port = 1900\n";

        //pf += "pass out quick inet proto udp from 239.255.255.250 to 239.255.255.250 port = 1901\n";
        //pf += "pass in quick inet proto udp from 239.255.255.250 to 239.255.255.250 port = 1901\n";

        pf += "pass out quick inet proto udp from any to any port = 1900\n";
        pf += "pass in quick proto udp from any to any port = 1900\n";
        pf += "pass out quick inet proto udp from any to any port = 1901\n";
        pf += "pass in quick proto udp from any to any port = 1901\n";

        pf += "pass out quick inet proto udp from any to any port = 5350\n";
        pf += "pass in quick proto udp from any to any port = 5350\n";
        pf += "pass out quick inet proto udp from any to any port = 5351\n";
        pf += "pass in quick proto udp from any to any port = 5351\n";
        pf += "pass out quick inet proto udp from any to any port = 5353\n";
        pf += "pass in quick proto udp from any to any port = 5353\n";
    }

    QFile f(pfConfigFilePath);
    if (f.open(QIODevice::WriteOnly))
    {
        QTextStream ts(&f);
        ts << pf;
        f.close();

        // Note:
        // Be careful adding '-F all' to this command to fix an issue.  Adding it will prevent the
        // OpenVPN over TCP and Stealth protocols from completing their connection setup.

        QString reply = helper_->executeRootCommand("pfctl -v -f \"" + pfConfigFilePath + "\"");
        //qCDebug(LOG_FIREWALL_CONTROLLER) << "Firewall on pfctl result:" << reply;
        Q_UNUSED(reply);

        helper_->executeRootCommand("pfctl -e");

        return true;
    }
    else
    {
        return false;
    }
}

void FirewallController_mac::setInterfaceToSkip_posix(const QString &interfaceToSkip)
{
    QMutexLocker locker(&mutex_);
    qCDebug(LOG_BASIC) << "FirewallController_mac::setInterfaceToSkip_posix ->" << interfaceToSkip;
    if (interfaceToSkip_ != interfaceToSkip) {
        interfaceToSkip_ = interfaceToSkip;
        forceUpdateInterfaceToSkip_ = true;
    }
}

void FirewallController_mac::enableFirewallOnBoot(bool bEnable)
{
    qCDebug(LOG_BASIC) << "Enable firewall on boot, bEnable =" << bEnable;
    QString strTempFilePath = QString::fromLocal8Bit(getenv("TMPDIR")) + "windscribetemp.plist";
    QString filePath = "/Library/LaunchDaemons/com.aaa.windscribe.firewall_on.plist";

    QString pfConfFilePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString pfBashScriptFile = pfConfFilePath + "/windscribe_pf.sh";
    pfConfFilePath = pfConfFilePath + "/pf.conf";

    if (bEnable)
    {
        //create bash script
        {
            QString exePath = QCoreApplication::applicationFilePath();
            QFile file(pfBashScriptFile);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                file.resize(0);
                QTextStream in(&file);
                in << "#!/bin/bash\n";
                in << "FILE=\"" << exePath << "\"\n";
                in << "if [ ! -f \"$FILE\" ]\n";
                in << "then\n";
                in << "echo \"File $FILE does not exists\"\n";
                in << "launchctl stop com.aaa.windscribe.firewall_on\n";
                in << "launchctl unload " << filePath << "\n";
                in << "launchctl remove com.aaa.windscribe.firewall_on\n";
                in << "srm \"$0\"\n";
                //in << "rm " << filePath << "\n";
                in << "else\n";
                in << "echo \"File $FILE exists\"\n";
                in << "ipconfig waitall\n";
                in << "/sbin/pfctl -e -f \"" << pfConfFilePath << "\"\n";
                in << "fi\n";
                file.close();

                // set executable flag
                helper_->executeRootCommand("chmod +x \"" + pfBashScriptFile + "\"");
            }
        }

        // create plist
        QFile file(strTempFilePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            file.resize(0);
            QTextStream in(&file);
            in << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
            in << "<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
            in << "<plist version=\"1.0\">\n";
            in << "<dict>\n";
            in << "<key>Label</key>\n";
            in << "<string>com.aaa.windscribe.firewall_on</string>\n";

            in << "<key>ProgramArguments</key>\n";
            in << "<array>\n";
            in << "<string>/bin/bash</string>\n";
            in << "<string>" << pfBashScriptFile << "</string>\n";
            in << "</array>\n";

            in << "<key>StandardErrorPath</key>\n";
            in << "<string>/var/log/windscribe_pf.log</string>\n";
            in << "<key>StandardOutPath</key>\n";
            in << "<string>/var/log/windscribe_pf.log</string>\n";

            in << "<key>RunAtLoad</key>\n";
            in << "<true/>\n";
            in << "</dict>\n";
            in << "</plist>\n";

            file.close();

            helper_->executeRootCommand("cp " + strTempFilePath + " " + filePath);
            helper_->executeRootCommand("launchctl load -w " + filePath);
        }
        else
        {
            qCDebug(LOG_BASIC) << "Can't create plist file for startup firewall: " << filePath;
        }
    }
    else
    {
        qCDebug(LOG_BASIC) << "Execute command: "
                           << "launchctl unload " + Utils::cleanSensitiveInfo(filePath);
        helper_->executeRootCommand("launchctl unload " + filePath);
        qCDebug(LOG_BASIC) << "Execute command: " << "rm " + Utils::cleanSensitiveInfo(filePath);
        helper_->executeRootCommand("rm " + filePath);
        qCDebug(LOG_BASIC) << "Execute command: "
                           << "rm " + Utils::cleanSensitiveInfo(pfBashScriptFile);
        helper_->executeRootCommand("rm \"" + pfBashScriptFile + "\"");
    }
}
