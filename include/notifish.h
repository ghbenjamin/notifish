#pragma once

#include <string_view>
#include <filesystem>
#include <unordered_map>
#include <optional>

#include <fmt/format.h>

#ifdef WIN32
#define NF_WINDOWS
#elif __APPLE__
#define NF_APPLE
#else
#define NF_LINUX
#endif


namespace nf
{

struct NotifyInfo
{
    bool is_silent = false;
    bool is_local = true;
    std::string remote_server;
    std::string host_name;
    std::string command;

    int exit_code = -1;


    std::string to_json() const;
    static std::optional<NotifyInfo> from_json( std::string_view json_str );
};



// Core
void do_command(NotifyInfo& ninfo );
void notify_local( NotifyInfo& ninfo );
void notify_remote( NotifyInfo& ninfo );

[[noreturn]] int start_daemon( int port );

// Config
std::filesystem::path get_config_path();
void ensure_config_file_exists();
std::unordered_map<std::string, std::string> get_config_data();


// Util

std::string get_local_hostname();

// Logging

template <typename T, typename... Args>
void log_to_console( const char* verb, T const& message, Args&&...args )
{
    std::cout << verb << fmt::format( message, std::forward<Args>(args)... ) << std::endl;
}

template <typename T, typename... Args>
void log_warn( T const& message, Args&&...args )
{
    log_to_console( "Warning: ", message, std::forward<Args>(args)... );
}

template <typename T, typename... Args>
void log_error( T const& message, Args&&...args )
{
    log_to_console( "Error: ", message, std::forward<Args>(args)... );
}

template <typename T, typename... Args>
void log_info( T const& message, Args&&...args )
{
    log_to_console( "", message, std::forward<Args>(args)... );
}


}