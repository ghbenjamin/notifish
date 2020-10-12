#include <notifish.h>

#include <sstream>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <algorithm>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <zmq.hpp>

#ifdef NF_WINDOWS
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#include <ShlObj.h>
#undef GetObject
#endif


namespace nf
{
static constexpr int MAX_TITLE_SIZE = 63;
static constexpr int MAX_MESSAGE_SIZE = 254;
static constexpr const char* CONFIG_FILE_NAME = "notifish.conf";
static int DEFAULT_PORT = 1316;

}

std::string nf::NotifyInfo::to_json() const
{
    // Construct a rapidjson object to contain our NotifyInfo data
    rapidjson::Document doc;
    auto& alloc = doc.GetAllocator();

    doc.SetObject();

    auto sr_command = rapidjson::StringRef( this->command.c_str() );
    doc.AddMember( "command", sr_command, alloc );

    doc.AddMember( "is_silent", this->is_silent, alloc );
    doc.AddMember( "is_local", this->is_local, alloc );

    auto sr_remote_server = rapidjson::StringRef( this->remote_server.c_str() );
    doc.AddMember( "remote_server", sr_remote_server, alloc );

    auto sr_hostname = rapidjson::StringRef( this->host_name.c_str() );
    doc.AddMember( "host_name", sr_hostname, alloc );

    doc.AddMember( "exit_code", this->exit_code, alloc );


    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    doc.Accept(writer);

    return sb.GetString();
}

std::optional<nf::NotifyInfo> nf::NotifyInfo::from_json( std::string_view json_str )
{
    nf::NotifyInfo ninfo;
    rapidjson::Document doc;
    doc.Parse( json_str.data(), json_str.size() );

    if ( doc.HasParseError() )
    {
        return {};
    }

    ninfo.command = doc.FindMember("command")->value.GetString();
    ninfo.is_silent = doc.FindMember("is_silent")->value.GetBool();
    ninfo.is_local = doc.FindMember("is_local")->value.GetBool();
    ninfo.remote_server = doc.FindMember("remote_server")->value.GetString();
    ninfo.host_name = doc.FindMember("host_name")->value.GetString();
    ninfo.exit_code = doc.FindMember("exit_code")->value.GetInt();

    return ninfo;
}

void nf::notify_local(NotifyInfo& ninfo)
{
    std::string title;
    std::string message;

    if ( ninfo.exit_code == 0 )
    {
        title = "Command completed";
    }
    else
    {
        title = "Command failed";
    }


    if ( ninfo.is_local )
    {
        message = ninfo.command;
    }
    else
    {
        message = fmt::format( "{} - {}", ninfo.host_name, ninfo.command );
    }

#ifdef NF_WINDOWS

    int title_len = std::min( nf::MAX_TITLE_SIZE, (int)title.size() );
    int msg_len = std::min( nf::MAX_MESSAGE_SIZE, (int)message.size() );

    NOTIFYICONDATA data = { sizeof(data) };

    data.uFlags = NIF_GUID | NIF_INFO;

    if (ninfo.exit_code == 0)
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

    strncpy_s( data.szInfoTitle, title.data(), title_len );
    strncpy_s( data.szInfo, message.data(), msg_len );

    BOOL result = Shell_NotifyIcon(NIM_ADD, &data);
    if (!result)
    {
        Shell_NotifyIcon(NIM_MODIFY, &data);
    }

    Shell_NotifyIcon(NIM_DELETE, &data);

#endif
}

void nf::notify_remote(nf::NotifyInfo &ninfo)
{
    assert( !ninfo.is_local );

    auto json = ninfo.to_json();

    // Connect to the remote socket and push the json string
    std::string zmq_binding = fmt::format( "tcp://{}", ninfo.remote_server );
    zmq::context_t zmq_context{1};
    zmq::socket_t socket{zmq_context, zmq::socket_type::push};

    socket.connect( zmq_binding );
    socket.send( zmq::buffer( json ), zmq::send_flags::none );

    socket.close();
    zmq_context.close();
}

[[noreturn]] int nf::start_daemon( int port )
{
    std::string zmq_binding = fmt::format( "tcp://*:{}", port );

    zmq::context_t zmq_context{1};
    zmq::socket_t socket{zmq_context, zmq::socket_type::pull};

    socket.bind( zmq_binding );
    log_info( "nf daemon started on port {}", port );

    for (;;)
    {
        zmq::message_t request;
        socket.recv (request);

        auto data = request.to_string_view();

        auto ninfo = nf::NotifyInfo::from_json( data );

        if ( !ninfo )
        {
            log_error( "Malformed server request:" );
            log_info("{}\n", data);
            continue;
        }
        else
        {
            log_info( "{}\n", data );
            nf::notify_local( ninfo.value() );
        }
    }

    socket.close();
    zmq_context.close();

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
    ofs << "{\n  \"config\": {\n  }\n}\n";
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
        log_warn("could not read configuration file");
        return data;
    }
    else if (!doc.HasMember("config"))
    {
        log_warn("config file is readable but malformed");
        return data;
    }

    return std::move(data);
}

void nf::do_command(nf::NotifyInfo &ninfo)
{
    ninfo.exit_code = system( ninfo.command.c_str() );

    if ( ninfo.is_local )
    {
        nf::notify_local( ninfo );
    }
    else
    {
        notify_remote( ninfo );
    }
}

std::string nf::get_local_hostname()
{
#ifdef NF_WINDOWS
    TCHAR buf[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD buf_len;

    auto ret = GetComputerNameEx( (COMPUTER_NAME_FORMAT)0, buf, &buf_len );

    if (ret)
    {
        return std::string(buf);
    }
    else
    {
        return "<unknown machine>";
    }
#else
#endif
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

    if ( args.contains("help") || args.contains("h") )
    {
        std::cout << "Help text here" << std::endl;
        return 0;
    }
    else if ( args.contains("daemon") )
    {
        if ( args.contains("target_command") && !args["target_command"].empty() )
        {
            nf::log_error( "Positional arguments and daemon are mutually exclusive" );
            return 1;
        }

        int port = nf::DEFAULT_PORT;
        if ( args.contains("port") )
        {
            try
            {
                port = std::stoi( args["port"] );
            }
            catch ( std::exception ex )
            {
                nf::log_error( "Port needs to be a number (got '{}')", args["port"] );
                return 1;
            }
        }

        nf::start_daemon(port);
    }
    else
    {
        if ( !args.contains("target_command") || args["target_command"].empty() )
        {
            nf::log_error( "No command specified" );
            return 1;
        }

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

        if (args.contains("silent"))
        {
            ninfo.is_silent = true;
        }

        if (args.contains("port"))
        {
            nf::log_warn( "Port argument only applies to daemon mode - use the server argument for notify mode" );
        }

        ninfo.host_name = nf::get_local_hostname();

        nf::do_command(ninfo);
    }
}
