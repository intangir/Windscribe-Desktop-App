#include "logger.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QString>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include "spdlog_utils.h"
#include "paths.h"

namespace log_utils {

bool Logger::install(const QString &logFilePath, bool consoleOutput)
{
    QLoggingCategory::setFilterRules("qt.tlsbackend.ossl=false\nqt.network.ssl=false");
    log_utils::paths::deleteOldUnusedLogs();

#ifdef Q_OS_WIN
    std::wstring path = logFilePath.toStdWString();
#else
    std::string path = logFilePath.toStdString();
#endif

    // Initialize spdlog logger
    try
    {
        if (log_utils::isOldLogFormat(path)) {
            std::filesystem::remove(path);
        }
        // Create rotation logger with 2 file with unlimited size
        // rotate it on open, the first file is the current log, the 2nd is the previous log
        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(path, SIZE_MAX, 1, true);
        auto defaultLogger = std::make_shared<spdlog::logger>("default", fileSink);
        spdlog::set_default_logger(defaultLogger);

        // Create the logger without formatting for logging output from libraries such as wsnet, which format logs themselves
        auto rawLogger = std::make_shared<spdlog::logger>("raw", fileSink);
        spdlog::register_logger(rawLogger);

        // this will trigger flush on every log message
        defaultLogger->flush_on(spdlog::level::trace);
        defaultLogger->set_level(spdlog::level::trace);
        rawLogger->flush_on(spdlog::level::trace);
        rawLogger->set_level(spdlog::level::trace);

        auto formatter = std::make_unique<log_utils::CustomFormatter>(spdlog::details::make_unique<spdlog::pattern_formatter>("{\"tm\": \"%Y-%m-%d %H:%M:%S.%e\", \"lvl\": \"%^%l%$\", %v}"));
        defaultLogger->set_formatter(std::move(formatter));
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        printf("spdlog init failed: %s\n", ex.what());
        return false;
    }

    prevMessageHandler_ = qInstallMessageHandler(myMessageHandler);
    return true;
}


Logger::Logger()
{
    connectionCategoryDefault_ = std::make_unique<QLoggingCategory>("connection");
}

void Logger::myMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &s)
{
    // Skip some of the non-value warnings of the Qt library.
    if (type == QtMsgType::QtWarningMsg) {
        static std::vector<std::string> pointlessStrings = {
            "parseIconEntryInfo(): Failed, OSType doesn't match:",
            "OpenType support missing for"};

        for (const auto &it : pointlessStrings) {
            if (s.contains(it.c_str()))
                return;
        }
    }

    std::string escapedMsg = log_utils::escape_string(s.toStdString());
    spdlog::debug("\"mod\": \"{}\", \"msg\": \"{}\"", context.category, escapedMsg);
}

void Logger::startConnectionMode(const std::string &id)
{
    QMutexLocker lock(&mutex_);
    static std::string connectionModeId;
    connectionModeId = "conn_" + id;
    connectionModeLoggingCategory_.reset(new QLoggingCategory(connectionModeId.c_str()));
    connectionMode_ = true;
}

void Logger::endConnectionMode()
{
    QMutexLocker lock(&mutex_);
    connectionMode_ = false;
}

const QLoggingCategory &Logger::connectionModeLoggingCategory()
{
    QMutexLocker lock(&mutex_);
    if (connectionMode_)
        return *connectionModeLoggingCategory_;
    else
        return *connectionCategoryDefault_;
}

}  // namespace log_utils