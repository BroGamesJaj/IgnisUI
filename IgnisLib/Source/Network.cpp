#include "IgnisLib.h"

#define ASIO_STANDALONE 
#define ASIO_HAS_STD_ADDRESSOF
#define ASIO_HAS_STD_ARRAY
#define ASIO_HAS_CSTDINT
#define ASIO_HAS_STD_SHARED_PTR
#define ASIO_HAS_STD_TYPE_TRAITS

#include "asio/asio.hpp"
#include "asio/asio/ssl.hpp"
#include "nlohmann/json.hpp"

namespace Ignis {
    void Network::Test() {
        asio::io_context io;
        asio::ip::tcp::socket socket(io);
        asio::ssl::context ctx(asio::ssl::context::tls_client);
        ctx.set_default_verify_paths();

        try {
            asio::ip::tcp::endpoint endpoint(
                asio::ip::make_address("25.32.203.59"),80);
            socket.connect(endpoint);

            std::string request =
                "GET / HTTP/1.0\r\n"
                "Host: 25.32.203.59\r\n"
                "Accept: */*\r\n"
                "Connection: close\r\n\r\n";
            asio::write(socket, asio::buffer(request));

            std::stringstream response;
            asio::streambuf buf;
            asio::error_code ec;
            while (asio::read(socket, buf, asio::transfer_at_least(1), ec)) {
            }
            if (ec != asio::error::eof) throw asio::system_error(ec);

            std::string data(
                asio::buffers_begin(buf.data()),
                asio::buffers_end(buf.data())
            );
            
            auto body_pos = data.find("\r\n\r\n");
            if (body_pos != std::string::npos) {
                std::string body = data.substr(body_pos + 4);
                try {
                    auto json = nlohmann::json::parse(body);
                    std::cout << json.dump(4) << "\n";
                    SetConsoleCP(CP_UTF8);
                    SetConsoleOutputCP(CP_UTF8);
                    std::cout << json["surfingman"].get<std::string>() << "\n";
                    
                }
                catch (nlohmann::json::parse_error& e) {
                    std::cerr << "JSON parse error: " << e.what() << "\n";
                }
            }
        }
        catch (std::system_error e){
            std::cout << e.what() << std::endl;
        }
    }
}

