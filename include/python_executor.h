#pragma once

#include <string>
#include <memory>
#include <chrono>

struct PythonResult {
    std::string output;
    std::string error;
    int exit_code;
    std::chrono::milliseconds execution_time;
    bool success;
};

class PythonExecutor {
public:
    PythonExecutor();
    ~PythonExecutor() = default;

    PythonResult execute(const std::string& code);

    bool isPythonAvailable() const;
    std::string getPythonVersion() const;

private:
    std::string python_path_;
    std::string python_version_;
    bool python_available_;

    std::string createTempFile(const std::string& code);
    PythonResult executeFile(const std::string& file_path);
    void cleanupTempFile(const std::string& file_path);

};