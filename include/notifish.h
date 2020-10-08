#pragma once

#include <string_view>
#include <filesystem>

#ifdef WIN32
#define NF_WINDOWS
#elif __APPLE__
#define NF_APPLE
#else
#define NF_LINUX
#endif



namespace nf
{

// Core
void notify_local(std::string_view title, std::string_view text );
int start_daemon(int port);

// Config

static constexpr const char* CONFIG_FILE_NAME = "notifish.conf";

std::filesystem::path get_config_path();
void ensure_config_file_exists();
void get_config_file();

}