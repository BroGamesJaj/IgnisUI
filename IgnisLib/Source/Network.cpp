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
    struct Network::Context {
        Context() : ctx(asio::ssl::context::tls_client), io() {
            ctx.set_default_verify_paths();
        }

        asio::io_context io;
        asio::ssl::context ctx;
    };

    struct Network::Socket {
        Socket(Network::Context* context) : socket(context->io, context->ctx) {}

        asio::ssl::stream<asio::ip::tcp::socket> socket;
    };
    void Network::Test() {
        io = new Network::Context();
        socket = new Network::Socket(io);

        struct Request rqs;
        rqs.location = "/redirect";
        Response rsp = Request("25.32.203.59", rqs);

        struct Request rqs2;
        rqs2.location = rsp.redirect;
        std::cout << rsp.redirect << std::endl;
        rsp = Request("25.32.203.59", rqs2);

        std::cout << rsp.body << std::endl;
    }

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

            asio::ip::address addressData = asio::ip::make_address(address);

            std::string content;
            switch (request.contentType)
            {
            case ANY:
                content = "*/*";
                break;
            case JSON:
                content = "application-json";
                break;
            case PLAIN:
                content = "text/plain";
                break;
            default:
                throw std::runtime_error("Can't set type for request content");
                break;
            }

            std::string state = request.connectionState == CLOSE ? "close" : "keep-alive";

            socket->socket.lowest_layer().close();
            socket = new Network::Socket(io);
            asio::ip::tcp::endpoint endpoint(addressData, 443);
            socket->socket.lowest_layer().connect(endpoint);
            socket->socket.handshake(asio::ssl::stream_base::client);

            std::string requestString =
                method + " " + request.location + " HTTP/1.0\r\n"
                "Host: " + address + "\r\n"
                "Accept: " + content + "\r\n"
                "Connection: " + state + "\r\n\r\n";

            asio::write(socket->socket, asio::buffer(requestString));

            Response responseData;

            asio::streambuf response;
            asio::read_until(socket->socket, response, "\r\n\r\n");

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
                    responseData.redirect = header.substr(9);
                    responseData.redirect.erase(
                        responseData.redirect.find_last_not_of("\r\n") + 1);
                }
            }

            std::ostringstream bodyStream;

            if (response.size() > 0) {
                bodyStream << &response;
            }

            if (chunked) {
                while (true) {
                    std::string line;
                    asio::read_until(socket->socket, response, "\r\n");
                    std::getline(resp_stream, line);
                    std::size_t chunk_size = std::stoul(line, nullptr, 16);
                    if (chunk_size == 0) break;

                    asio::read(socket->socket, response, asio::transfer_exactly(chunk_size + 2));
                    bodyStream << std::string(
                        asio::buffers_begin(response.data()),
                        asio::buffers_begin(response.data()) + chunk_size);
                    response.consume(chunk_size + 2);
                }
            }
            else if (content_length > bodyStream.str().size()) {
                asio::read(socket->socket, response,
                    asio::transfer_exactly(content_length - bodyStream.str().size()));
                bodyStream << &response;
            }

            responseData.body = bodyStream.str();
            return responseData;
        }
        catch (std::system_error e) {
            std::cout << "Error:" << e.what() << std::endl;
        }
    }

    Network::Context* Network::io = nullptr;
    Network::Socket* Network::socket = nullptr;
}

