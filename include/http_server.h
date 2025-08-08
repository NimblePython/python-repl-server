#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <functional>

namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace websocket = boost::beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class PythonExecutor;

using RequestHandler = std::function<std::string(const std::string&)>;

class HttpServer {
public:
    HttpServer(net::io_context& ioc, const std::string& address, unsigned short port);
    ~HttpServer() = default;

    // Start HTTP server
    void start();
    
    // Set request handler
    void setRequestHandler(RequestHandler handler);

    // Set python executor
    void setPythonExecutor(std::shared_ptr<PythonExecutor> executor);

private:
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::string address_;
    unsigned short port_;
    RequestHandler request_handler_;
    std::shared_ptr<PythonExecutor> python_executor_;

    void do_accept();
    void handle_http_session(tcp::socket socket, std::shared_ptr<http::request_parser<http::string_body>> parser, std::shared_ptr<beast::flat_buffer> buffer);
    void handle_websocket_session(tcp::socket socket);
    void do_websocket_read(std::shared_ptr<websocket::stream<tcp::socket>> ws);
    http::response<http::string_body> process_request(const http::request<http::string_body>& req);
    std::string handle_graphql_request(const std::string& body);
    void send_response(tcp::socket socket, const http::response<http::string_body>& response);
};