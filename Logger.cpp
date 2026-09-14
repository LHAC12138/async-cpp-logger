#include<queue>
#include<string>
#include<mutex> 
#include<condition_variable>
#include<thread>
#include<fstream>
#include<atomic>
#include<sstream>
#include<vector>
#include<stdexcept>
#include<chrono>
#include<ctime>
#include<iostream>

template<typename T> 
std::string to_string_helper(T&& args)
{
    std::ostringstream oss;
    oss<<std::forward<T>(args);
    return oss.str();
} 

class LogQueue{
public:
    void push(const std::string &msg){
        std::lock_guard<std::mutex> lock(mtx);
        q_.push(msg);
        cond.notify_one();
    }
    bool pop(std::string& msg){
        std::unique_lock<std::mutex> lock(mtx);
        while(q_.empty()&&!is_shutdown){
            cond.wait(lock);
        }
        if(q_.empty()&&is_shutdown){
            return false;
        }
        msg = q_.front();
        q_.pop();
        return true;
    }
    void shutdown(){
        std::lock_guard<std::mutex>lock(mtx);
        is_shutdown =true;
        cond.notify_all();
    }

private:
    std::queue<std::string> q_;
    std::mutex mtx;
    std::condition_variable cond;
    bool is_shutdown =false;
};

class Logger{
public:
    Logger(const std::string& filename): log_file(filename,std::ios::out|std::ios::app) ,exit_file(false){
        if(!log_file.is_open()){
            throw std::runtime_error("无法打开文件");
        }
       t1 =std::thread(&Logger::processQueue,this);
    } 
    ~Logger(){
        Lq.shutdown();
        if(t1.joinable()){
            t1.join();
        }
        if(log_file.is_open()){
            log_file.close();
        }
}

    template<typename... Args>
    void log(const std::string& format,Args... args){
        Lq.push(FormatMessage(format,std::forward<Args>(args)...));
    }

private:
    LogQueue Lq;
    std::thread t1;
    std::ofstream log_file;
    std::atomic<bool> exit_file;

    void processQueue()
    {
        std::string msg;
        while(Lq.pop(msg)){
            log_file<<msg<<std::endl;
        }
    }
    template<typename... Args>
    std::string FormatMessage(const std::string &format,Args... args){
        std::vector<std::string> arg_strings={to_string_helper(std::forward<Args>(args))...};
        std::ostringstream oss;
        size_t arg_index = 0;
        size_t pos =0;
        size_t placeholder =format.find("{}",pos);

        while(placeholder != std::string::npos){
            oss<<format.substr(pos,placeholder-pos);
            if(arg_index<arg_strings.size()){
                oss<<arg_strings[arg_index++];
            }else {
                oss<<"{}";
            }
            pos=placeholder+2;
            placeholder=format.find("{}",pos);
        }
        oss<<format.substr(pos);
        while(arg_index<arg_strings.size()){
            oss<<arg_strings[arg_index++];
        }
        return oss.str();
    }
};

int main(){
    try{
        Logger logger("log.txt");
        logger.log("Starting application.");

        int user_id =42;
        std::string action = "login";
        double duration =3.5;
        std::string world ="world";
        logger.log("User {} performed {} in {} seconds.", user_id, action, duration);
        logger.log("Hello {}", world);
        logger.log("This is a message without placeholders.");
        logger.log("Multiple placeholders: {}, {}, {}.", 1, 2, 3);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    catch(const std::exception& ex ){
        std::cerr<<"日志系统初始化失败"<<ex.what()<<std::endl;
    }
    return 0;
}