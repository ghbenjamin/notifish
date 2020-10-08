#include <notifish.h>

#include <vector>
#include <sstream>
#include <unordered_map>
#include <iostream>
#include <fstream>

#include <rapidjson/rapidjson.h>

#ifdef NF_WINDOWS
#include <Windows.h>
#include <shellapi.h>
#include <ShlObj.h>
#endif

static constexpr int MAX_TITLE_SIZE = 64;
static constexpr int MAX_MESSAGE_SIZE = 255;

#ifdef NF_WINDOWS
void nf::notify_local(std::string_view title, std::string_view text)
{
    if ( title.size() >= MAX_TITLE_SIZE )
    {
        title = title.substr(0, MAX_TITLE_SIZE);
    }

    if ( text.size() >= MAX_MESSAGE_SIZE )
    {
        text = text.substr(0, MAX_MESSAGE_SIZE);
    }

    NOTIFYICONDATA data = { sizeof(data) };

    data.uFlags = NIF_GUID | NIF_INFO;
    data.dwInfoFlags = NIIF_INFO;

    strncpy_s( data.szInfoTitle, title.data(), title.size() );
    strncpy_s( data.szInfo, text.data(), text.size() );

    BOOL result = Shell_NotifyIcon(NIM_ADD, &data);
    if (!result)
    {
        Shell_NotifyIcon(NIM_MODIFY, &data);
    }
}
#else
void nf::notify( std::string_view title, std::string_view text)
{
    // TODO Linux notify_local
}
#endif

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

void nf::get_config_file()
{
    ensure_config_file_exists();


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
                out[std::string{current_arg}] = "";
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
    auto args = parse_command_line(argc, argv);

    if ( args.contains("daemon") )
    {
        // TODO
    }
    else
    {
        auto path = nf::get_config_path();
        int r = 5;
    }
}
