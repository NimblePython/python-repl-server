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
#include <sys/wait.h>

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
    
    // Временно отключаем удаление для отладки
    // cleanupTempFile(temp_file);
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

struct ProcessResult {
    std::string output;  // весь вывод (stderr+stdout)
    std::string error;   // только ошибка, если есть
    int exit_code;       // код выхода процесса
};

void execCaptureStderr(const std::string& cmd, ProcessResult& pr) {
    std::string full_cmd = cmd + " 2>&1";
    std::array<char, 4096> buffer{};
    std::string result;

    FILE* pipe = popen(full_cmd.c_str(), "r");
    if (!pipe) throw std::runtime_error("popen() failed!");

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    int status = pclose(pipe);


    pr.output = result;
    if (WIFEXITED(status)) {
        pr.exit_code = WEXITSTATUS(status);
    } else {
        pr.exit_code = -1; // процесс завершился ненормально
    }
}

// Функция: считает количество Unicode codepoints в UTF-8 строке
size_t utf8_len(const std::string& str) {
    size_t len = 0;
    for (size_t i = 0; i < str.size(); ) {
        unsigned char c = str[i];
        size_t char_len = 1;
        if ((c & 0x80) == 0x00) char_len = 1;        // 0xxxxxxx
        else if ((c & 0xE0) == 0xC0) char_len = 2;   // 110xxxxx
        else if ((c & 0xF0) == 0xE0) char_len = 3;   // 1110xxxx
        else if ((c & 0xF8) == 0xF0) char_len = 4;   // 11110xxx
        else char_len = 1; // на всякий случай

        i += char_len;
        len++;
    }
    return len;
}

// Функция: пересчитывает строку с галочкой, чтобы она была под нужным символом
std::string fix_arrow_alignment(const std::string& code_line, const std::string& arrow_line) {
    // Считаем количество символов (codepoints) до галочки в arrow_line
    size_t arrow_pos_bytes = arrow_line.find('^');
    if (arrow_pos_bytes == std::string::npos) {
        return arrow_line; // нет галочки
    }

    // Считаем, сколько символов реально до галочки (в codepoints)
    size_t arrow_chars_count = utf8_len(code_line.substr(0, arrow_pos_bytes));

    // Строим новую строку: нужное кол-во пробелов + ^
    std::string new_arrow(arrow_chars_count, ' ');
    new_arrow += '^';
    return new_arrow;
}

PythonResult PythonExecutor::executeFile(const std::string& file_path) {

    // Выполняем команду один раз, но перенаправляем stderr в отдельный файл
    std::string cmd = python_path_ + " " + file_path;
    ProcessResult pr;
    execCaptureStderr(cmd, pr);

    //std::string error_file = file_path + ".error";
    //std::string cmd_with_error = cmd + " 2>" + error_file;
    
    // Выполняем команду через system() (идентично bash)
    // int exit_code = system(cmd_with_error.c_str());
    // exit_code = WEXITSTATUS(exit_code);
    std::cout << pr.output; // Вывод прямо в консоль

    // Сохраняем в файл (в UTF-8)
    std::ofstream error_file(file_path + ".error", std::ios::binary);
    error_file << pr.output;
    error_file.close();
    
    // Читаем stderr из файла как текст и исправляем позицию стрелки
    std::ifstream error_stream(file_path + ".error");
    if (error_stream.is_open()) {
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(error_stream, line)) {
            lines.push_back(line);
        }
        error_stream.close();
        
        // Исправляем позицию стрелки
        for (size_t i = 0; i < lines.size(); i++) {
            if (lines[i].find('^') != std::string::npos && i > 0) {
                // Находим строку с кодом (обычно перед строкой со стрелкой)
                std::string code_line = lines[i-1];
                // Исправляем позицию стрелки для любой строки кода
                lines[i] = fix_arrow_alignment(code_line, lines[i]);
            }
        }
        
        // Собираем результат
        for (const auto& l : lines) {
            pr.error += l + "\n";
        }
        
        // Выводим ошибку на экран сервера для отладки
        std::cout << "[DEBUG] Raw error from file:" << std::endl;
        std::cout << pr.error << std::endl;
        std::cout << "[DEBUG] End of error" << std::endl;
        
        // Удаляем временный файл с ошибками
        // Временно отключаем удаление для отладки
        // std::remove(error_file.c_str());
    }
    
    std::cout << "[DEBUG] executeFile: Exit code: " << pr.exit_code << ", Output: '" << pr.output << "', Error: '" << pr.error << "'" << std::endl;
    std::cout << "[DEBUG] Error length: " << pr.error.length() << std::endl;
    std::cout << "[DEBUG] Error bytes: ";
    for (char c : pr.error) {
        if (c == '\n') {
            std::cout << "\\n";
        } else if (c == '\t') {
            std::cout << "\\t";
        } else {
            std::cout << c;
        }
    }
    std::cout << std::endl;
    
    return {
        pr.output,
        pr.error,
        pr.exit_code,
        std::chrono::milliseconds(0),
        pr.exit_code == 0
    };
}

void PythonExecutor::cleanupTempFile(const std::string& file_path) {
    try {
        fs::remove(file_path);
    } catch (const std::exception& e) {
        std::cerr << "Error removing temporary file: " << e.what() << std::endl;
    }
}