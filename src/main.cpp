#include <http_server.h>
#include <python_executor.h>
#include <iostream>
#include <memory>
#include <csignal>

std::shared_ptr<PythonExecutor> executor;
std::shared_ptr<HttpServer> server;

void signal_handler([[maybe_unused]] int signal) {
    const char* msg = "\nReceived signal, <<NimbleCode>> Python REPL server is shutting down...\n";
    write(STDERR_FILENO, msg, strlen(msg));
    exit(0);
}

int main(int argc, char* argv[]) {
    try {
        std::string address = "0.0.0.0";
        unsigned short port = 8080;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--host" && i + 1 < argc) {
                address = argv[++i];
            } else if (arg == "--port" && i + 1 < argc) {
                port = static_cast<unsigned short>(std::stoi(argv[++i]));
            } else if (arg == "--help"|| arg == "-h") {
                std::cout << "<<NimbleCode>> Python REPL server\n"
                          << "Usage: " << argv[0] << " [options]\n"
                          << "Options:\n"
                          << "  --host <address>  Server address (default: 127.0.0.1)\n"
                          << "  --port <port>     Server port (default: 8080)\n"
                          << "  --help, -h        Show this help message\n";
                return 0;
            }
        }
        
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);

        std::cout << "Starting Python REPL server on " << address << ":" << port << std::endl;

        executor = std::make_shared<PythonExecutor>();

        if (!executor->isPythonAvailable()) {
            std::cerr << "Error: Python3 is not available on the host machine." << std::endl;
            std::cerr << "Please install Python3 and / or ensure it's in your PATH." << std::endl;            
            return 1;
        }

        std::cout << "Python version: " << executor->getPythonVersion() << std::endl;

        // Создадим I/O контекст для асинхронной работы
        net::io_context ioc;
        
        // Создадим HTTP server
        server = std::make_shared<HttpServer>(ioc, address, port);
        server->setPythonExecutor(executor);

        // Запустим сервер
        server->start();
        std::cout << "<<NimbleCode>> Python REPL\n";
        std::cout << "Server is running on http://" << address << ":" << port << std::endl;
        std::cout << "GraphQL endpoint: http://" << address << ":" << port << "/graphql" << std::endl;
        std::cout << "Health check: http://" << address << ":" << port << "/" << std::endl;
        std::cout << "Press Ctrl+C to stop the server\n" << std::endl;
        // Запустим I/O контекст
        ioc.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}