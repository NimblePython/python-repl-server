 #include <http_server.h>
 #include <graphql_handler.h>
 #include <iostream>
 #include <nlohmann/json.hpp>

 using json = nlohmann::json;

 HttpServer::HttpServer(net::io_context& ioc, const std::string& address, unsigned short port) 
    : ioc_(ioc)
    , acceptor_(ioc)
    , address_(address)
    , port_(port) {
        
    beast::error_code ec;

    acceptor_.open(tcp::v4(), ec);
    if (ec) {
        throw std::runtime_error("Failed to open socket: " + ec.message());
    }

    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) {
        throw std::runtime_error("Failed to set socket option: " + ec.message());
    }

    acceptor_.bind({net::ip::make_address(address), port}, ec);
    if (ec) {
        throw std::runtime_error("Failed to bind socket: " + ec.message());
    }

    acceptor_.listen(net::socket_base::max_listen_connections, ec);
    if (ec) {
        throw std::runtime_error("Failed to listen: " + ec.message());
    }
}

void HttpServer::start() {
    std::cout << "Starting <<NimbleCode>> HTTP server on " << address_ << ":" << port_ << std::endl;
    do_accept();
}

void HttpServer::setRequestHandler(RequestHandler handler) {
    request_handler_ = handler;
}

void HttpServer::setPythonExecutor(std::shared_ptr<PythonExecutor> executor) {
    python_executor_ = executor;
}

void HttpServer::do_accept() {
    std::cout << "[DEBUG] Waiting for new connection..." << std::endl;
    acceptor_.async_accept(
        [this](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::cout << "[DEBUG] New connection accepted!" << std::endl;
                
                // Check if this is a WebSocket upgrade request
                auto parser = std::make_shared<http::request_parser<http::string_body>>();
                auto buffer = std::make_shared<beast::flat_buffer>();
                
                std::cout << "[DEBUG] Starting to read HTTP header..." << std::endl;
                // Use shared_ptr to keep socket alive
                auto sp_socket = std::make_shared<tcp::socket>(std::move(socket));
                http::async_read_header(*sp_socket, *buffer, *parser,
                    [this, sp_socket, parser, buffer]
                    (beast::error_code ec, std::size_t bytes_transferred) mutable {
                        if (!ec) {
                            std::cout << "[DEBUG] HTTP header read successfully, bytes: " << bytes_transferred << std::endl;
                            auto req = parser->get();
                            std::cout << "[DEBUG] Request method: " << req.method() << ", target: " << req.target() << std::endl;
                            
                            if (websocket::is_upgrade(req)) {
                                std::cout << "[DEBUG] WebSocket upgrade detected" << std::endl;
                                handle_websocket_session(std::move(*sp_socket));
                            } else {
                                std::cout << "[DEBUG] Regular HTTP request, handling..." << std::endl;
                                handle_http_session(std::move(*sp_socket), parser, buffer);
                            }
                        } else {
                            std::cerr << "[ERROR] Failed to read HTTP header: " << ec.message() << std::endl;
                        }
                    });
            } else {
                std::cerr << "[ERROR] Failed to accept connection: " << ec.message() << std::endl;
            }
            
            // Accept the next connection
            do_accept();
        });
}

void HttpServer::handle_http_session(tcp::socket socket, std::shared_ptr<http::request_parser<http::string_body>> parser, std::shared_ptr<beast::flat_buffer> buffer) {
    std::cout << "[DEBUG] handle_http_session: Starting to read full request..." << std::endl;
    
    // Create a shared_ptr to keep the socket alive
    auto sp_socket = std::make_shared<tcp::socket>(std::move(socket));
    
    http::async_read(*sp_socket, *buffer, *parser,
        [this, sp_socket, parser, buffer]
        (beast::error_code ec, std::size_t bytes_transferred) mutable {
            if (!ec) {
                std::cout << "[DEBUG] handle_http_session: Full request read successfully, bytes: " << bytes_transferred << std::endl;
                auto req = parser->get();
                std::cout << "[DEBUG] handle_http_session: Processing request..." << std::endl;
                auto response = process_request(req);
                std::cout << "[DEBUG] handle_http_session: Request processed, sending response..." << std::endl;
                send_response(std::move(*sp_socket), response);
            } else {
                std::cerr << "[ERROR] handle_http_session: Failed to read full request: " << ec.message() << std::endl;
                // Try to close the socket gracefully
                beast::error_code close_ec;
                sp_socket->close(close_ec);
            }
        });
}

void HttpServer::handle_websocket_session(tcp::socket socket) {
    auto ws = std::make_shared<websocket::stream<tcp::socket>>(std::move(socket));
    
    // Set a timeout for the websocket stream
    ws->set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
    
    // Accept the WebSocket handshake
    ws->async_accept([this, ws](beast::error_code ec) {
        if (!ec) {
            // Handshake successful, start reading WebSocket messages
            do_websocket_read(ws);
        }
    });
}

void HttpServer::do_websocket_read(std::shared_ptr<websocket::stream<tcp::socket>> ws) {
    auto buffer = std::make_shared<beast::flat_buffer>();
    
    ws->async_read(*buffer,
        [this, ws, buffer](beast::error_code ec, std::size_t bytes_transferred) {
            if (!ec) {
                // Process WebSocket message
                std::string message = beast::buffers_to_string(buffer->data());
                buffer->consume(buffer->size()); // Consume the buffer

                std::string response = handle_graphql_request(message);
                
                ws->async_write(
                    net::buffer(response),
                    [this, ws](beast::error_code ec, std::size_t bytes_transferred) {
                        if (!ec) {
                            do_websocket_read(ws); // Read next message
                        }
                    });
            } else if (ec == websocket::error::closed) {
                // Connection closed gracefully
                std::cout << "WebSocket connection closed." << std::endl;
            }
        });
}

http::response<http::string_body> HttpServer::process_request(const http::request<http::string_body>& req) {
    std::cout << "[DEBUG] process_request: Processing " << req.method() << " " << req.target() << std::endl;
    
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::server, "<<NimbleCode>> Python REPL server");
    res.set(http::field::content_type, "application/json");
    res.keep_alive(req.keep_alive());
    
    // Handle CORS
    res.set(http::field::access_control_allow_origin, "*");
    res.set(http::field::access_control_allow_methods, "GET, POST, OPTIONS");
    res.set(http::field::access_control_allow_headers, "Content-Type");
    
    if (req.method() == http::verb::options) {
        std::cout << "[DEBUG] process_request: Handling OPTIONS request" << std::endl;
        res.result(http::status::ok);
        res.prepare_payload();
        return res;
    }
    
    if (req.method() == http::verb::post && req.target() == "/graphql") {
        std::cout << "[DEBUG] process_request: Handling GraphQL POST request" << std::endl;
        std::string body = req.body();
        std::cout << "[DEBUG] process_request: Raw request body: '" << body << "'" << std::endl;
        std::string response = handle_graphql_request(body);
        res.body() = response;
        res.result(http::status::ok);
    } else if (req.method() == http::verb::get && req.target() == "/") {
        std::cout << "[DEBUG] process_request: Handling health check GET request" << std::endl;
        // Health check endpoint
        json health = {
            {"status", "ok"},
            {"service", "<<NimbleCode>> Python REPL server"},
            {"python_available", false}  // Default to false
        };
        
        // Safe check for python_executor_
        if (python_executor_) {
            std::cout << "[DEBUG] process_request: python_executor_ is not null" << std::endl;
            try {
                health["python_available"] = python_executor_->isPythonAvailable();
                if (python_executor_->isPythonAvailable()) {
                    health["python_version"] = python_executor_->getPythonVersion();
                }
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] process_request: Exception in python_executor_: " << e.what() << std::endl;
                health["python_available"] = false;
                health["error"] = e.what();
            }
        } else {
            std::cout << "[DEBUG] process_request: python_executor_ is null" << std::endl;
        }
        
        res.body() = health.dump(2);
        res.result(http::status::ok);
        std::cout << "[DEBUG] process_request: Health check response: " << res.body() << std::endl;
    } else {
        std::cout << "[DEBUG] process_request: 404 Not Found for " << req.method() << " " << req.target() << std::endl;
        res.result(http::status::not_found);
        res.body() = "{\"error\": \"Not found\"}";
    }
    
    res.prepare_payload();
    std::cout << "[DEBUG] process_request: Response prepared, body size: " << res.body().size() << std::endl;
    return res;
}

std::string HttpServer::handle_graphql_request(const std::string& body) {
            if (!python_executor_) {
            json error = {
                {"errors", {
                    {
                        {"message", "Python executor not available"},
                        {"extensions", {{"code", "INTERNAL_ERROR"}}}
                    }
                }}
            };
            return error.dump(2);
        }
        
        try {
            GraphQLHandler handler(python_executor_);
            GraphQLRequest request = handler.parseRequest(body);
            GraphQLResponse response = handler.executeQuery(request);
    
        if (response.success) {
            return response.data;
        } else {
            json error = {
                {"errors", {
                    {
                        {"message", response.errors},
                        {"extensions", {{"code", "EXECUTION_ERROR"}}}
                    }
                }}
            };
            return error.dump(2);
        }
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] handle_graphql_request: Exception: " << e.what() << std::endl;
        json error = {
            {"errors", {
                {
                    {"message", std::string("Internal server error: ") + e.what()},
                    {"extensions", {{"code", "INTERNAL_ERROR"}}}
                }
            }}
        };
        return error.dump(2);
    }
}

void HttpServer::send_response(tcp::socket socket, const http::response<http::string_body>& response) {
    // Use a shared_ptr to keep the socket alive until the write completes
    auto sp_socket = std::make_shared<tcp::socket>(std::move(socket));
    // Create a copy of response to keep it alive in the lambda
    auto response_copy = std::make_shared<http::response<http::string_body>>(response);
    
    http::async_write(*sp_socket, *response_copy,
        [sp_socket, response_copy](beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                std::cerr << "[ERROR] send_response: Error sending response: " << ec.message() << std::endl;
            } else {
                std::cout << "[DEBUG] send_response: Response sent successfully, bytes: " << bytes_transferred << std::endl;
            }
            
            // Close the socket after sending the response
            beast::error_code close_ec;
            sp_socket->shutdown(tcp::socket::shutdown_both, close_ec);
            if (close_ec) {
                std::cerr << "[ERROR] send_response: Error closing socket: " << close_ec.message() << std::endl;
            }
        });
}