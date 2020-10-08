#include <notifish.h>

#include <sstream>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <algorithm>

#include <rapidjson/document.h>

#ifdef NF_WINDOWS
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#include <ShlObj.h>
#undef GetObject
#endif

static constexpr int MAX_TITLE_SIZE = 63;
static constexpr int MAX_MESSAGE_SIZE = 254;

void nf::notify_local(NotifyInfo& ninfo)
{
#ifdef NF_WINDOWS

    int title_len = std::min( MAX_TITLE_SIZE, (int)ninfo.title.size() );
    int msg_len = std::min( MAX_MESSAGE_SIZE, (int)ninfo.message.size() );

    NOTIFYICONDATA data = { sizeof(data) };

    data.uFlags = NIF_GUID | NIF_INFO;

    if (ninfo.return_code == 0)
    {
        data.dwInfoFlags = NIIF_INFO;
    }
    else
    {
        data.dwInfoFlags = NIIF_ERROR;
    }

    if (ninfo.is_silent)
    {
        data.dwInfoFlags = data.dwInfoFlags | NIIF_NOSOUND;
    }

    strncpy_s( data.szInfoTitle, ninfo.title.data(), title_len );
    strncpy_s( data.szInfo, ninfo.message.data(), msg_len );

    BOOL result = Shell_NotifyIcon(NIM_ADD, &data);
    if (!result)
    {
        Shell_NotifyIcon(NIM_MODIFY, &data);
    }

    Shell_NotifyIcon(NIM_DELETE, &data);

#endif
}

int nf::start_daemon( int port )
{
    return 0;
}

std::filesystem::path nf::get_config_path()
{
#ifdef NF_WINDOWS

    PWSTR path;
    HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path );

    auto fs_path = std::filesystem::path(path).append( "notifish" ).append( nf::CONFIG_FILE_NAME );

    CoTaskMemFree(path);

    return fs_path;
#endif
}

void nf::ensure_config_file_exists()
{
    auto path = get_config_path();
    if ( std::filesystem::exists(path) )
    {
        return;
    }

    auto parent_path = path.parent_path();
    if ( !std::filesystem::exists(parent_path) )
    {
        std::filesystem::create_directory( parent_path );
    }

    std::ofstream ofs(path);
    ofs << "{}\n";
    ofs.close();

    return;
}

std::unordered_map<std::string, std::string> nf::get_config_data()
{
    ensure_config_file_exists();

    std::unordered_map<std::string, std::string> data;
    std::ifstream fstream( nf::get_config_path() );

    std::stringstream buffer;
    buffer << fstream.rdbuf();

    rapidjson::Document doc;
    doc.Parse( buffer.str().c_str() );

    if (doc.HasParseError())
    {

    }
    else if (!doc.HasMember("config"))
    {

    }

    return std::move(data);
}

void nf::do_notify(nf::NotifyInfo &ninfo)
{
    if ( ninfo.is_local )
    {
        ninfo.return_code = system( ninfo.command.c_str() );

        if (ninfo.return_code == 0)
        {
            ninfo.title = "Program completed";
        }
        else
        {
            ninfo.title = "Program failed";
        }

        ninfo.message = ninfo.command;
        nf::notify_local( ninfo );
    }
    else
    {

    }
}

std::unordered_map<std::string, std::string> parse_command_line(int argc, char** argv)
{
    std::unordered_map<std::string, std::string> out;

    for (int i = 1; i < argc; i++)
    {
        std::string s = argv[i];
        if (!s.empty())
        {
            std::string_view current_arg;

            if (s.starts_with("--"))
            {
                current_arg = s;
                current_arg.remove_prefix(2);
            }
            else if (s.starts_with('/'))
            {
                current_arg = s;
                current_arg.remove_prefix(1);
            }
            else
            {
                std::stringstream ss;

                for (int j = i; j < argc; j++)
                {
                    ss << argv[j] << " ";
                }

                out["target_command"] = ss.str();
                break;
            }

            auto pivot_idx = current_arg.find( "=" );
            if ( pivot_idx != std::string_view::npos )
            {
                auto key = std::string{current_arg.substr(0, pivot_idx)};
                auto value = std::string{current_arg.substr(pivot_idx + 1, current_arg.size())};

                out[key] = value;
            }
            else
            {
                out[std::string{current_arg}] = "true";
            }
        }
    }

    return std::move(out);
}


/**
 *  notifish foo bar -baz
 *  notifish --config=my/path foo -bar
 *  notifish --other-easy-settings foo-bar
 *  notifish --daemon
 *  notifish --server=foo:1234 foo bar
 */
int main( int argc, char** argv )
{
    auto cmd_args = parse_command_line(argc, argv);
    auto config_args = nf::get_config_data();

    config_args.merge( cmd_args );
    auto args = std::move(config_args);

    if ( args.contains("help")
            || args.contains("h")
            || args.empty()
            || !args.contains("target_command")
            || args["target_command"].empty() )
    {
        std::cout << "Help text here" << std::endl;
        return 0;
    }
    else if ( args.contains("daemon") )
    {

    }
    else
    {
        nf::NotifyInfo ninfo;

        ninfo.command = args["target_command"];

        if ( args.contains("server") )
        {
            ninfo.is_local = false;
            ninfo.remote_server = args["server"];
        }
        else
        {
            ninfo.is_local = true;
        }

        if (args.contains("min-timeout"))
        {
            ninfo.notify_min_timeout_ms = std::stoi(args["min-timeout"]);
        }

        if (args.contains("silent"))
        {
            ninfo.is_silent = true;
        }


        nf::do_notify( ninfo );
    }
}
