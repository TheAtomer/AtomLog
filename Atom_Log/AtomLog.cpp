#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <atomic>
#include <unordered_map>

// 日志级别
enum class LogLevel {
    DEBUG = 0, //调试信息
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

class Logger {
public:
    // 获取单例实例
    static Logger& instance() {
        static Logger logger;  // 静态局部变量确保线程安全初始化
        return logger;
    }

    // 设置日志输出级别
    void set_log_level(LogLevel level) {
        log_level_.store(level, std::memory_order_relaxed);
    }

    /**
     * 记录日志的核心函数
     * @param level 日志级别
     * @param message 日志内容
     * @param file 源代码文件名（可选）
     * @param line 源代码行号（可选）
     */
    void log(LogLevel level, const std::string& message,
        const char* file = nullptr, int line = 0) {

        // 检查日志级别是否达到输出要求
        if (level < log_level_.load(std::memory_order_relaxed))
            return;

        // 获取当前时间点
        auto now = std::chrono::system_clock::now();
        // 计算毫秒部分
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        // 转换为time_t类型（秒级精度）
        auto timer = std::chrono::system_clock::to_time_t(now);

        // 跨平台时间转换
        std::tm bt;
#if defined(_WIN32) || defined(_WIN64)
        // Windows平台使用线程安全的localtime_s
        localtime_s(&bt, &timer);
#else
        // POSIX平台使用localtime_r
        localtime_r(&timer, &bt);
#endif

        // 构建日志消息流
        std::ostringstream stream;

        // 添加时间戳：格式为YYYY-MM-DD HH:MM:SS.mmm
        stream << std::put_time(&bt, "%Y-%m-%d %H:%M:%S");
        stream << '.' << std::setfill('0') << std::setw(3) << ms.count();

        // 添加日志级别标签
        stream << " [" << level_to_string(level) << "] ";

        // 添加日志内容
        stream << message;

        // 对于警告及以上级别，添加源代码位置信息
        if (level >= LogLevel::WARN && file && line > 0) {
            stream << " [at " << file << ":" << line << "]";
        }

        // 根据级别选择输出流和颜色
        if (level >= LogLevel::WARN) {
            // 错误流输出（stderr），带颜色
            std::cerr << level_to_color(level) << stream.str()
                << "\033[0m" << std::endl;
        }
        else {
            // 标准输出（stdout），无颜色
            std::cout << stream.str() << std::endl;
        }
    }

    // 删除拷贝构造函数和赋值运算符（确保单例）
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    // 私有构造函数（单例模式）
    Logger() {
        // 初始化日志级别映射
        level_strings_[LogLevel::INFO] = "INFO";
        level_strings_[LogLevel::WARN] = "WARN";
        level_strings_[LogLevel::ERROR] = "ERROR";
        level_strings_[LogLevel::DEBUG] = "DEBUG";

        // 初始化颜色映射（ANSI转义序列）
        colors_[LogLevel::INFO] = "\033[0m";    // 默认白色
        colors_[LogLevel::DEBUG] = "\033[36m";  // 青色（调试信息）
        colors_[LogLevel::WARN] = "\033[33m";   // 黄色（警告）
        colors_[LogLevel::ERROR] = "\033[31m";  // 红色（错误）
    }

    // 将日志级别转换为字符串
    const std::string& level_to_string(LogLevel level) {
        static std::string unknown = "UNKNOWN";
        auto it = level_strings_.find(level);
        return (it != level_strings_.end()) ? it->second : unknown;
    }

    // 获取日志级别对应的颜色代码
    const std::string& level_to_color(LogLevel level) {
        static std::string default_color = "\033[0m";
        auto it = colors_.find(level);
        return (it != colors_.end()) ? it->second : default_color;
    }

    // 原子变量存储当前日志级别（默认为INFO）
    std::atomic<LogLevel> log_level_{ LogLevel::INFO };

    // 日志级别到字符串的映射
    std::unordered_map<LogLevel, std::string> level_strings_;

    // 日志级别到颜色代码的映射
    std::unordered_map<LogLevel, std::string> colors_;
};

// ============= 日志宏定义（用户接口） =============

// 信息日志（无源代码位置）
#define LOG_INFO(msg) Logger::instance().log(LogLevel::INFO, (msg))

// 调试日志（无源代码位置）
#define LOG_DEBUG(msg) Logger::instance().log(LogLevel::DEBUG, (msg))

// 警告日志（自动捕获文件名和行号）
#define LOG_WARN(msg) Logger::instance().log(LogLevel::WARN, (msg), __FILE__, __LINE__)

// 错误日志（自动捕获文件名和行号）
#define LOG_ERROR(msg) Logger::instance().log(LogLevel::ERROR, (msg), __FILE__, __LINE__)

// 自定义日志（可指定任意级别）
#define LOG_CUSTOM(level, msg) Logger::instance().log((level), (msg))

// 关键错误日志（红色加粗显示）
#define LOG_CRITICAL(msg) \
    Logger::instance().log(LogLevel::ERROR, \
        std::string("\033[1;31mCRITICAL:\033[0m ") + (msg), __FILE__, __LINE__)

// 带检查的条件日志
#define LOG_IF(condition, level, msg) \
    if (condition) Logger::instance().log((level), (msg))



/*-----------------------DEMO--------------------------------*/

int main() {
    // 设置只记录WARN及以上级别的日志
    // Logger::instance().set_log_level(LogLevel::WARN);

    LOG_DEBUG("This debug message won't be shown");
    LOG_INFO("This info message won't be shown");


    LOG_WARN("Disk space below 20%");


    LOG_ERROR("Failed to open config file");

    // 记录自定义级别日志
    LOG_CUSTOM(LogLevel::WARN, "Custom warning: Network latency high");

    // 记录关键错误（特殊格式）
    LOG_CRITICAL("System overheating!");

    // 条件日志
    int retry = 3;
    LOG_IF(retry > 2, LogLevel::ERROR,
        "Exceeded max retry attempts: " + std::to_string(retry));

    return 0;
}