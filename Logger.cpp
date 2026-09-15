#define _CRT_SECURE_NO_WARNINGS
#include<thread>
#include<mutex>
#include<condition_variable>
#include<queue>
#include<fstream>
#include<vector>
#include<sstream>
#include<stdexcept>
#include<string>
#include<iostream>
#include<chrono>
#include<ctime>
#include<iomanip>

enum class Log_level {INFO,DEBUG,ERROR};

template<typename T>
std::string to_string_helper(T&& args) {
    std::ostringstream oss;
    oss<<std::forward<T>(args);
    return oss.str();
}
   
std::string get_time() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << "[" << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << "]";
    return oss.str();
}
class LogQueue {
public:
    void push(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mtx);
        q_.push(msg);
        cond.notify_one();
    }
    bool pop(std::string& msg)
    {   
        std::unique_lock<std::mutex> lock(mtx);
        while (q_.empty() && !is_shutdown) {
            cond.wait(lock);
        }
        if (is_shutdown && q_.empty()) {
            return false;
        }
        msg = q_.front();
        q_.pop();
        return true;
    }
    void shutdown(){
        std::lock_guard<std::mutex> lock(mtx);
        is_shutdown = true;
        cond.notify_all();
    }
private:
    std::mutex  mtx;
    std::condition_variable cond;
    std::queue<std::string> q_;
    bool is_shutdown = false;
};

class Logger {
public:
    Logger(const std::string& filename) :log_file(filename, std::ios::out | std::ios::app) {
        if (!log_file.is_open()) {
            throw std::runtime_error("文件打开错误");
        }
        log_thread = std::thread(&Logger::processqueue, this);
    }

    ~Logger() {
        Lq.shutdown();
        if (log_thread.joinable()) {
            log_thread.join();
        }
        if(log_file.is_open()) {
            log_file.close();
        }
    }

    template<typename... Args>
    void log(Log_level level, const std::string& format, Args&& ...args) {
        std::string level_str;
        switch(level){
            case Log_level::INFO: level_str="[INFO]";break;
            case Log_level::DEBUG: level_str="[DEBUG]";break;
            case Log_level::ERROR: level_str="[ERROR]";break;
        }
        Lq.push(level_str+formatMessage(format,std::forward<Args>(args)...));
    }


    template<typename... Args>
    void log(const std::string& format, Args&& ...args) {
        Lq.push(formatMessage(format, std::forward<Args>(args)...));
    }

    
private:
    LogQueue Lq;
    std::thread log_thread;
    std::ofstream  log_file;

    void processqueue() {
        std::string msg;
        while (Lq.pop(msg)) {
            log_file << msg << std::endl;
        }
    }
    template<typename...Args>
    std::string formatMessage(const std::string& format, Args...args) {
        std::vector<std::string> arg_strings = { to_string_helper(std::forward<Args>(args))... };
        std::ostringstream oss;
        oss << get_time();
        size_t arg_index = 0;
        size_t pos = 0;
        size_t placeholder = format.find("{}", pos);
        while (placeholder != std::string::npos) {
            oss << format.substr(pos, placeholder - pos);
            if (arg_index < arg_strings.size()) {
                oss << arg_strings[arg_index++];
            }
            else {
                oss << "{}";
            }
            pos = placeholder + 2;
            placeholder = format.find("{}", pos);
        }
        oss << format.substr(pos);
        while (arg_index < arg_strings.size()) {
            oss << arg_strings[arg_index++];
        }
        return oss.str();
    }

};
int main() {
    try {
        Logger logger("log.txt");

        logger.log("Starting application.");

        int user_id = 42;
        std::string action = "login";
        double duration = 3.5;
        std::string world = "World";

        logger.log(Log_level::INFO,"User {} performed {} in {} seconds.", user_id, action, duration);
        logger.log(Log_level::DEBUG,"Hello {}", world);
        logger.log(Log_level::ERROR,"This is a message without placeholders.");
        logger.log(Log_level::INFO,"Multiple placeholders: {}, {}, {}.", 1, 2, 3);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    catch (const std::exception& ex) {
        std::cerr << "日志系统初始化失败: " << ex.what() << std::endl;
    }

    return 0;
}

