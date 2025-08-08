#include "python_executor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <chrono>
#include <vector>
#include <array>
#include <memory>

namespace fs = std::filesystem;

PythonExecutor::PythonExecutor() {
    const std::vector<std::string> python_paths = {
        "/usr/bin/python3",
        "/usr/local/bin/python3",
        "/opt/homebrew/bin/python3",
        "python3",
    };

    for (const auto& path : python_paths) {
        if (fs::exists(path) || system(("which " + path + " > /dev/null 2>&1").c_str()) == 0) {
            python_path_ = path;
            break;
        }
    }

    if (python_path_.empty()) {
        python_available_ = false;
        return;
    }
}

PythonResult PythonExecutor::execute(const std::string& code) {
    if (!isPythonAvailable()) {
        return {
            "",
            "Python3 is not available on the host machine",
            -1,
            std::chrono::milliseconds(0),
            false
        };
    }

    std::string temp_file = createTempFile(code);
    if (temp_file.empty()) {
        return {
            "",
            "Failed to create temporary file",
            -1,
            std::chrono::milliseconds(0),
            false
        };
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    PythonResult result = executeFile(temp_file);

    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    cleanupTempFile(temp_file);
    return result;
}

bool PythonExecutor::isPythonAvailable() const {
    return !python_path_.empty();
}

std::string PythonExecutor::getPythonVersion() const {
    if (!isPythonAvailable()) {
        return "Python3 is not available on the host machine";
    }

    std::array<char, 128> buffer;
    std::string result;

    std::string cmd = python_path_ + " --version 2>&1";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);

    if (!pipe) {
        return "Failed to get Python version";
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

std::string PythonExecutor::createTempFile(const std::string& code) {
    try {
        fs::path temp_dir = fs::temp_directory_path();
        fs::path temp_file = temp_dir / ("python_repl_" + std::to_string(std::time(nullptr)) + ".py");

        std::ofstream file(temp_file);
        if (!file.is_open()) {
            return "";
        }

        file << code;
        file.close();

        return temp_file.string();
    } catch (const std::exception& e) {
        std::cerr << "Error creating temporary file: " << e.what() << std::endl;
        return "";
    }
}

PythonResult PythonExecutor::executeFile(const std::string& file_path) {
    std::array<char, 128> buffer;
    std::string output, error;

    std::string cmd = python_path_ + " " + file_path + " 2>&1";
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);

    if (!pipe) {
        return {
            "",
            "Failed to execute Python process",
            -1,
            std::chrono::milliseconds(0),
            false
        };
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        output += buffer.data();
    }

    int exit_code = pclose(pipe.release());
    
    return {
        output,
        error,
        exit_code,
        std::chrono::milliseconds(0),
        exit_code == 0
    };    
}

void PythonExecutor::cleanupTempFile(const std::string& file_path) {
    try {
        fs::remove(file_path);
    } catch (const std::exception& e) {
        std::cerr << "Error removing temporary file: " << e.what() << std::endl;
    }
}