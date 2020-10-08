#pragma once

#include <string_view>
#include <filesystem>
#include <unordered_map>

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
    std::string command;
    std::string title;
    std::string message;

    bool is_silent = false;
    bool is_local = true;
    int notify_min_timeout_ms = -1;

    std::string remote_server;

    int return_code = -1;
};



// Core
void do_notify( NotifyInfo& ninfo );
void notify_local( NotifyInfo& ninfo );
int start_daemon(int port);

// Config

static constexpr const char* CONFIG_FILE_NAME = "notifish.conf";

std::filesystem::path get_config_path();
void ensure_config_file_exists();
std::unordered_map<std::string, std::string> get_config_data();

}