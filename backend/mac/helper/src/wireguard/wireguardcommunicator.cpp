#include "wireguardcommunicator.h"
#include "../ipc/helper_commands.h"
#include "utils.h"
#include "logger.h"
#include <regex>
#include <type_traits>
#include <boost/algorithm/string/trim.hpp>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

namespace
{
template<typename T>
T stringToValueImpl(const std::string &str)
{
    return static_cast<T>(strtol(str.c_str(), nullptr, 10));
}
template<>
unsigned long long stringToValueImpl(const std::string &str)
{
    return strtoull(str.c_str(), nullptr, 10);
}
template<typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type
stringToValue(const std::string &str)
{
    return str.empty() ? T(0) : stringToValueImpl<T>(str);
}
}  // namespace

WireGuardCommunicator::Connection::Connection(const std::string deviceName)
    : status_(Status::NO_ACCESS), socketHandle_(-1), fileHandle_(nullptr)
{
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "/var/run/wireguard/%s.sock",
        deviceName.c_str());

    struct stat sbuf;
    auto ret = stat(addr.sun_path, &sbuf);
    if (ret < 0) {
        status_ = Status::NO_SOCKET;
        return;
    }
    if (!S_ISSOCK(sbuf.st_mode)) {
        errno = EBADF;
        LOG("File is not a socket: %s", addr.sun_path);
        return;
    }
    socketHandle_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socketHandle_ < 0) {
        LOG("Failed to open the socket: %s", addr.sun_path);
        return;
    }
    ret = ::connect(socketHandle_, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    if (ret < 0) {
        LOG("Failed to connect to the socket: %s", addr.sun_path);
        if (errno == ECONNREFUSED)
            unlink(addr.sun_path);
        if (socketHandle_ >= 0) {
            close(socketHandle_);
            socketHandle_ = -1;
        }
        return;
    }
    fileHandle_ = fdopen(socketHandle_, "r+");
    if (!fileHandle_) {
        if (socketHandle_ >= 0) {
            close(socketHandle_);
            socketHandle_ = -1;
        }
        return;
    }
    status_ = Status::OK;
}

WireGuardCommunicator::Connection::~Connection()
{
    if (fileHandle_)
        fclose(fileHandle_);
    if (socketHandle_ >= 0)
        close(socketHandle_);
}

bool WireGuardCommunicator::Connection::getOutput(ResultMap *results_map) const
{
    if (!fileHandle_)
        return false;
    std::string output;
    output.reserve(1024);
    for (;;) {
        auto c = static_cast<char>(fgetc(fileHandle_));
        if (c < 0)
            break;
        output.push_back(c);
    }
    boost::trim(output);
    if (output.empty())
        return false;
    if (results_map && !results_map->empty()) {
        std::regex output_rx("(^|\n)(\\w+)=(\\w+)(?=\n|$)");
        std::sregex_iterator it(output.begin(), output.end(), output_rx), end;
        for (; it != end; ++it) {
            if (it->size() != 4)
                continue;
            const auto &match = *it;
            auto mapitem = results_map->find(match[2].str());
            if (mapitem != results_map->end())
                mapitem->second = match[3].str();
        }
    }
    return true;
}

void WireGuardCommunicator::setDeviceName(const std::string &deviceName)
{
    assert(!deviceName.empty());
    deviceName_ = deviceName;
}

bool WireGuardCommunicator::configure(const std::string &clientPrivateKey,
    const std::string &peerPublicKey, const std::string &peerPresharedKey,
    const std::string &peerEndpoint, const std::vector<std::string> &allowedIps)
{
    Connection connection(deviceName_);
    if (connection.getStatus() != Connection::Status::OK) {
        LOG("WireGuardCommunicator::configure(): no connection to daemon");
        return false;
    }

    // Send set command.
    fputs("set=1\n", connection);
    fprintf(connection,
        "private_key=%s\n"
        "replace_peers=true\n"
        "public_key=%s\n"
        "preshared_key=%s\n"
        "endpoint=%s\n"
        "persistent_keepalive_interval=0\n"
        "replace_allowed_ips=true\n",
        clientPrivateKey.c_str(), peerPublicKey.c_str(), peerPresharedKey.c_str(),
        peerEndpoint.c_str());
    for (const auto &ip : allowedIps)
        fprintf(connection, "allowed_ip=%s\n", ip.c_str());
    fputs("\n", connection);
    fflush(connection);

    // Check results.
    Connection::ResultMap results{ std::make_pair("errno", "") };
    bool success = connection.getOutput(&results);
    if (success)
        success = stringToValue<int>(results["errno"]) == 0;
    return success;
}

unsigned long WireGuardCommunicator::getStatus(unsigned int *errorCode,
    unsigned long long *bytesReceived, unsigned long long *bytesTransmitted)
{
    Connection connection(deviceName_);
    const auto connection_status = connection.getStatus();
    if (connection_status != Connection::Status::OK) {
        if (connection.getStatus() == Connection::Status::NO_SOCKET)
            return WIREGUARD_STATE_STARTING;
        if (errorCode)
            *errorCode = static_cast<unsigned int>(errno);
        return WIREGUARD_STATE_ERROR;
    }

    // Send get command.
    fputs("get=1\n\n", connection);
    fflush(connection);

    Connection::ResultMap results{
        std::make_pair("errno", ""),
        std::make_pair("listen_port", ""),
        std::make_pair("public_key", ""),
        std::make_pair("rx_bytes", ""),
        std::make_pair("tx_bytes", ""),
        std::make_pair("last_handshake_time_sec", "")
    };
    bool success = connection.getOutput(&results);
    if (!success)
        return WIREGUARD_STATE_STARTING;

    // Check for errors.
    const auto errno_value = stringToValue<unsigned int>(results["errno"]);
    if (errno_value != 0) {
        if (errorCode)
            *errorCode = errno_value;
        return WIREGUARD_STATE_ERROR;
    }

    // Check if not yet listening.
    if (results["listen_port"].empty())
        return WIREGUARD_STATE_STARTING;

    // Check for handshake.
    if (stringToValue<unsigned long long>(results["last_handshake_time_sec"]) > 0) {
        if (bytesReceived)
            *bytesReceived = stringToValue<unsigned long long>(results["rx_bytes"]);
        if (bytesTransmitted)
            *bytesTransmitted = stringToValue<unsigned long long>(results["tx_bytes"]);
        return WIREGUARD_STATE_ACTIVE;
    }

    // If endpoint is set, we are connecting, otherwise simply listening.
    if (!results["public_key"].empty())
        return WIREGUARD_STATE_CONNECTING;
    return WIREGUARD_STATE_LISTENING;
}
