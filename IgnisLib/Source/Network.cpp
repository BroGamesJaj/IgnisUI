#include "IgnisLib.h"

#define ASIO_STANDALONE 

#include "asio/asio.hpp"
#include "asio/asio/ssl.hpp"
#include "nlohmann/json.hpp"

using asio::ip::tcp;

namespace Ignis {

    struct Network::Context {
        Context() : ctx(asio::ssl::context::tls_client), io(), guard(asio::make_work_guard(io)) {
            ctx.set_default_verify_paths();
        }

        asio::io_context io;
        asio::ssl::context ctx;
        asio::executor_work_guard<asio::io_context::executor_type> guard;
    };

    struct Network::SecureSocket {
        SecureSocket() : socket(io->io, io->ctx) {}

        asio::ssl::stream<asio::ip::tcp::socket> socket;
    };

    struct Network::Response {

        unsigned int status_code;
        std::string status_message;
        nlohmann::json body;
        RedirectInfo redirect;
    };

    Network::Socket::Socket() {
        socket = new tcp::socket(io->io);
    }
    Network::Socket::~Socket() {
        delete static_cast<tcp::socket*>(socket);
    }

    void Network::Socket::Read() {
        ((tcp::socket*)socket)->async_read_some(
            asio::buffer(data),
            [this](std::error_code ec, std::size_t len) {
                if (!ec) {
                    func(data, len);
                    Read();
                } else{
                    if(ec.value() == 10054) return;
                    std::cout << ec.category().name() << ": " << ec.value() << " - " << ec.message() << std::endl;
                } 
            }
        );
    }

    void Network::Socket::Write(const char* data, std::size_t len) {
        asio::async_write(
            *((tcp::socket*)socket),
            asio::buffer(data, len),
            [this](std::error_code ec, std::size_t bytes_sent) {
                if(ec) {
                    std::cout << ec.category().name() << ": " << ec.value() << " - " << ec.message() << std::endl;
                }
            }
        );
    }

    void Network::Socket::Host(uint16_t port, std::function<void(const char*, std::size_t)> onRead){
        tcp::acceptor acceptor = tcp::acceptor(io->io, tcp::endpoint(tcp::v4(), port));
        acceptor.accept(*(tcp::socket*)socket);
        OnRead(onRead);
    }

    void Network::Socket::Join(std::string host, std::string port, std::function<void(const char*, std::size_t)> onRead){
        tcp::resolver resolver(io->io);
        auto endpoints = resolver.resolve(host, port);
        asio::connect(*(tcp::socket*)socket, endpoints);
        OnRead(onRead);
    }

    void Print(const char* text, size_t len){
        std::cout.write(text, len);
        std::cout << std::endl;
    }

    void Network::Init() {
        io = new Network::Context();

        std::thread ioThread([&] {
            std::cout << "IO THREAD START\n";
            io->io.run();
            std::cout << "IO THREAD END\n";
        });

        Network::Socket socky;

        std::string input;

        std::cin >> input;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if(input == "a"){
            socky.Host(12345, Print);
            while(true){
                std::getline(std::cin, input);
                if(input == "exit") break;
                socky.Write(input.c_str(), input.size());
            }
        }
        else if(input == "b"){
            socky.Join("127.0.0.1", "12345", Print);
            while(true){
                std::getline(std::cin, input);
                if(input == "exit") break;
                socky.Write(input.c_str(), input.size());
            }
        }

        io->io.stop();
        ioThread.join();
    }

    std::string ParseHost(const std::string& address) {
        std::string host = address;

        auto schemeEnd = host.find("://");
        if (schemeEnd != std::string::npos)
            host = host.substr(schemeEnd + 3);

        auto pathStart = host.find('/');
        if (pathStart != std::string::npos)
            host = host.substr(0, pathStart);

        auto portStart = host.find(':');
        if (portStart != std::string::npos)
            host = host.substr(0, portStart);

        return host;
    }

    Network::Response  Network::RedirectInfo::Redirect(Network::HTTPMethod method) {
        struct Network::Request rqs;
        rqs.method = method;

        return Network::Request(redirectString, rqs);
    }
    
    //keep-alive isnt working, always creating new connections
    Network::Response Network::Request(std::string address, struct Request request) {
        try {
            std::string method;
            switch (request.method) {
            case GET:
                method = "GET";
                break;
            case POST:
                method = "POST";
                break;
            case PUT:
                method = "PUT";
                break;
            case PATCH:
                method = "PATCH";
                break;
            case DEL:
                method = "DELETE";
                break;
            default:
                throw std::runtime_error("Can't set method for request");
                break;
            }

            asio::ip::tcp::endpoint endpoint;

            try {
                asio::ip::address addressData = asio::ip::make_address(address);
                endpoint = asio::ip::tcp::endpoint(addressData, 443);
            }
            catch (std::system_error e) {
                asio::ip::tcp::resolver resolver(io->io);
                address = ParseHost(address);
                asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(address, "https");
                endpoint = *endpoints.begin();
            }


            std::string content;
            switch (request.contentType)
            {
            case ANY:
                content = "*/*";
                break;
            case JSON:
                content = "application/json";
                break;
            case PLAIN:
                content = "text/plain";
                break;
            default:
                throw std::runtime_error("Can't set type for request content");
                break;
            }

            std::string state = request.connectionState == CLOSE ? "close" : "keep-alive";

            SecureSocket socket = Network::SecureSocket();
            socket.socket.lowest_layer().connect(endpoint);

            SSL_set_tlsext_host_name(socket.socket.native_handle(), address.c_str());
            socket.socket.handshake(asio::ssl::stream_base::client);

            std::string requestString =
                method + " " + request.location + " HTTP/1.1\r\n"
                "Host: " + address + "\r\n"
                "Accept: " + content + "\r\n"
                "Connection: " + state + "\r\n"
                "User-Agent: Mozilla/5.0\r\n\r\n";

            std::cout << requestString << std::endl;

            asio::write(socket.socket, asio::buffer(requestString));

            Response responseData;

            asio::streambuf response;
            asio::read_until(socket.socket, response, "\r\n\r\n");

            std::istream resp_stream(&response);
            std::string http_version;

            resp_stream >> http_version >> responseData.status_code;
            std::getline(resp_stream, responseData.status_message);

            std::string header;
            std::size_t content_length = 0;
            bool chunked = false;

            while (std::getline(resp_stream, header) && header != "\r") {
                if (header.rfind("Content-Length:", 0) == 0) {
                    content_length = std::stoul(header.substr(15));
                }
                else if (header.rfind("Transfer-Encoding:", 0) == 0 &&
                    header.find("chunked") != std::string::npos) {
                    chunked = true;
                }
                else if (responseData.status_code >= 300 && responseData.status_code < 400 &&
                    header.rfind("Location:", 0) == 0) {
                    responseData.redirect.redirectString = header.substr(10);
                    responseData.redirect.redirectString.erase(
                        responseData.redirect.redirectString.find_last_not_of("\r\n") + 1);
                }
            }

            std::ostringstream bodyStream;

            if (response.size() > 0) {
                bodyStream << &response;
            }

            asio::error_code ec;

            if (chunked) {
                while (true) {
                    std::string line;
                    asio::read_until(socket.socket, response, "\r\n", ec);
                    if (ec) {
                        if (ec != asio::ssl::error::stream_truncated && ec != asio::error::eof)
                            std::cout << "Read error: " << ec.message() << std::endl;
                        break;
                    }

                    std::getline(resp_stream, line);
                    std::size_t chunk_size = std::stoul(line, nullptr, 16);
                    if (chunk_size == 0) break;

                    asio::read(socket.socket, response, asio::transfer_exactly(chunk_size + 2), ec);
                    if (ec) {
                        if (ec != asio::ssl::error::stream_truncated && ec != asio::error::eof)
                            std::cout << "Read error: " << ec.message() << std::endl;
                        break;
                    }
                    
                    bodyStream << std::string(
                        asio::buffers_begin(response.data()),
                        asio::buffers_begin(response.data()) + chunk_size);
                    response.consume(chunk_size + 2);
                }
            }
            else if (content_length > bodyStream.str().size()) {
                asio::read(socket.socket, response,
                    asio::transfer_exactly(content_length - bodyStream.str().size()), ec);
                if (ec && ec != asio::ssl::error::stream_truncated && ec != asio::error::eof)
                    std::cout << "Read error: " << ec.message() << std::endl;
                bodyStream << &response;
            }
            else {
                asio::read(socket.socket, response, asio::transfer_all(), ec);
                if (ec && ec != asio::ssl::error::stream_truncated && ec != asio::error::eof)
                    std::cout << "Read error: " << ec.message() << std::endl;
                bodyStream << &response;
            }

            try {
                responseData.body = nlohmann::json::parse(bodyStream.str());
            }
            catch (const nlohmann::json::parse_error& e) {
                responseData.body["message"] = bodyStream.str();
            }

            return responseData;
        }
        catch (std::system_error e) {
            std::cout << "Error:" << e.what() << std::endl;
        }
    }

    Network::Context* Network::io = nullptr;
}

