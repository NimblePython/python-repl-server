 #include "graphql_handler.h"
 #include <iostream>
 #include <regex>
 #include <nlohmann/json.hpp>

 using json = nlohmann::json;

 GraphQLHandler::GraphQLHandler(std::shared_ptr<PythonExecutor> executor)
    : python_executor_(executor) {}

 GraphQLRequest GraphQLHandler::parseRequest(const std::string& body) {
    GraphQLRequest request;
    try {
        json j = json::parse(body);

        if (j.contains("query")) {
            request.query = j["query"];
        }

        if (j.contains("variables")) {
            request.variables = j["variables"].dump();
        }

        if (j.contains("operationName")) {
            request.operation_name = j["operationName"];
        }
    } catch (const json::exception& e) {
        std::cerr << "Error parsing GraphQL request: " << e.what() << std::endl;
    }

    return request;
}

GraphQLResponse GraphQLHandler::executeQuery(const GraphQLRequest& request) {
    std::cout << "[DEBUG] executeQuery: Starting..." << std::endl;
    GraphQLResponse response;

    try {
        // Check for introspection query        
        if (request.query.find("__schema") != std::string::npos ||
            request.query.find("__type") != std::string::npos) {
            std::cout << "[DEBUG] executeQuery: Handling introspection query" << std::endl;
            response.data = handleIntrospection(request.query);
            response.success = true;
            return response;
        }

        // Extract code from query
        std::cout << "[DEBUG] executeQuery: Extracting code from query..." << std::endl;
        std::string code = extractCodeFromQuery(request.query);

        if (code.empty()) {
            std::cout << "[DEBUG] executeQuery: No code found in query" << std::endl;
            response.errors = "No Python code found in query. Expected: executePython(code: \"...\")";
            response.success = false;
            return response;
        }

        std::cout << "[DEBUG] executeQuery: Code extracted: " << code << std::endl;

        // Execute Python code
        if (!python_executor_) {
            std::cout << "[DEBUG] executeQuery: Python executor is null" << std::endl;
            response.errors = "Python executor not available";
            response.success = false;
        } else {
            try {
                std::cout << "[DEBUG] executeQuery: About to execute Python code..." << std::endl;
                PythonResult py_result = python_executor_->execute(code);
                std::cout << "[DEBUG] executeQuery: Python execution completed, success: " << py_result.success << std::endl;
                std::cout << "[DEBUG] executeQuery: Output: " << py_result.output << std::endl;
                std::cout << "[DEBUG] executeQuery: Error: " << py_result.error << std::endl;
                std::cout << "[DEBUG] executeQuery: Exit code: " << py_result.exit_code << std::endl;
                
                std::cout << "[DEBUG] executeQuery: Creating JSON objects..." << std::endl;
                json data_json;
                json execute_python_json;
                
                try {
                    execute_python_json["output"] = py_result.output;
                    std::cout << "[DEBUG] executeQuery: output added to JSON" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "[ERROR] executeQuery: Exception adding output: " << e.what() << std::endl;
                }
                
                try {
                    std::cout << "[DEBUG] executeQuery: About to add error field, error.empty(): " << py_result.error.empty() << std::endl;
                    if (py_result.error.empty()) {
                        execute_python_json["error"] = nullptr;
                    } else {
                        execute_python_json["error"] = py_result.error;
                    }
                    std::cout << "[DEBUG] executeQuery: error added to JSON" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "[ERROR] executeQuery: Exception adding error: " << e.what() << std::endl;
                }
                
                // Safe conversion of execution time
                double execution_time_seconds = 0.0;
                try {
                    execution_time_seconds = py_result.execution_time.count() / 1000.0;
                } catch (const std::exception& e) {
                    std::cerr << "[ERROR] executeQuery: Exception in execution time conversion: " << e.what() << std::endl;
                    execution_time_seconds = 0.0;
                }
                std::cout << "[DEBUG] executeQuery: Execution time: " << execution_time_seconds << std::endl;
                
                try {
                    execute_python_json["executionTime"] = execution_time_seconds;
                    std::cout << "[DEBUG] executeQuery: executionTime added to JSON" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "[ERROR] executeQuery: Exception adding executionTime: " << e.what() << std::endl;
                }
                
                try {
                    data_json["executePython"] = execute_python_json;
                    std::cout << "[DEBUG] executeQuery: executePython added to data_json" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "[ERROR] executeQuery: Exception adding executePython: " << e.what() << std::endl;
                }
                
                if (!py_result.success) {
                    json error_entry = {
                        {"message", py_result.error.empty() ? "Python execution failed" : py_result.error},
                        {"extensions", {{"code", "PYTHON_EXECUTION_ERROR"}, {"exitCode", py_result.exit_code}}}
                    };
                    data_json["errors"] = {error_entry};
                    response.success = false;
                    response.errors = py_result.error.empty() ? "Python execution failed" : py_result.error;
                } else {
                    response.success = true;
                }
                response.data = data_json.dump(2);
                std::cout << "[DEBUG] executeQuery: Response prepared successfully" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] executeQuery: Python execution exception: " << e.what() << std::endl;
                response.errors = std::string("Python execution error: ") + e.what();
                response.success = false;
            }
        }
            
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] executeQuery: General exception: " << e.what() << std::endl;
        response.errors = std::string("Error executing query: ") + e.what();
        response.success = false;
    }

    return response;
}

std::string GraphQLHandler::handleExecutePython(const std::string& code) {
    if (!python_executor_) {
        return createResponse({
            "",
            "Python executor not initialized",
            -1,
            std::chrono::milliseconds(0),
            false
        });
    }

    PythonResult result = python_executor_->execute(code);
    return createResponse(result);
}

std::string GraphQLHandler::getSchema() {
    json schema = {
        {"__schema", {
            {"queryType", {
                {"name", "Query"}
            }},
            {"mutationType", nullptr},
            {"subscriptionType", nullptr},
            {"types", {
                {
                    {"name", "Query"},
                    {"kind", "OBJECT"},
                    {"fields", {
                        {
                            {"name", "executePython"},
                            {"type", {
                                {"name", "PythonResult"},
                                {"kind", "OBJECT"}
                            }},
                            {"args", {
                                {
                                    {"name", "code"},
                                    {"type", {
                                        {"name", "String"},
                                        {"kind", "SCALAR"}
                                    }},
                                    {"defaultValue", nullptr}
                                }
                            }}
                        }
                    }}
                },
                {
                    {"name", "PythonResult"},
                    {"kind", "OBJECT"},
                    {"fields", {
                        {
                            {"name", "output"},
                            {"type", {
                                {"name", "String"},
                                {"kind", "SCALAR"}
                            }}
                        },
                        {
                            {"name", "error"},
                            {"type", {
                                {"name", "String"},
                                {"kind", "SCALAR"}
                            }}
                        },
                        {
                            {"name", "executionTime"},
                            {"type", {
                                {"name", "Float"},
                                {"kind", "SCALAR"}
                            }}
                        }
                    }}
                },
                {
                    {"name", "String"},
                    {"kind", "SCALAR"}
                },
                {
                    {"name", "Float"},
                    {"kind", "SCALAR"}
                }
            }}
        }}
    };
    
    return json{{"data", schema}}.dump(2);
}

bool GraphQLHandler::validateQuery(const std::string& query) {
    // Check for basic required structure
    if (query.find("executePython") == std::string::npos) { 
        return false;
    }

    if (query.find("code:") == std::string::npos) {
        return false;
    }

    return true;
}

std::string GraphQLHandler::extractCodeFromQuery(const std::string& query) {
    // Try multiple patterns for extracting code
    
    // Pattern 1: executePython(code: "code here")
    std::regex code_regex1("executePython\\(code:\\s*\"([^\"]*)\"\\)");
    std::smatch match1;
    
    if (std::regex_search(query, match1, code_regex1) && match1.size() > 1) {
        return unescapeString(match1[1].str());
    }
    
    // Pattern 2: executePython(code: """code here""")
    std::regex code_regex2("executePython\\(code:\\s*\"\"\"([^\"]*)\"\"\"\\)");
    std::smatch match2;
    
    if (std::regex_search(query, match2, code_regex2) && match2.size() > 1) {
        return unescapeString(match2[1].str());
    }
    
    // Pattern 3: executePython(code: '''code here''')
    std::regex code_regex3("executePython\\(code:\\s*'''([^']*)'''\\)");
    std::smatch match3;
    
    if (std::regex_search(query, match3, code_regex3) && match3.size() > 1) {
        return unescapeString(match3[1].str());
    }
    
    return "";    
}

std::string GraphQLHandler::unescapeString(const std::string& str) {
    std::string unescaped;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '\\' && i + 1 < str.length()) {
            switch (str[i + 1]) {
                case 'n': unescaped += '\n'; break;
                case 't': unescaped += '\t'; break;
                case 'r': unescaped += '\r'; break;
                case '"': unescaped += '"'; break;
                case '\\': unescaped += '\\'; break;
                default: unescaped += str[i + 1]; break;
            }
            ++i;
        } else {
            unescaped += str[i];
        }
    }
    return unescaped;
}

std::string GraphQLHandler::createResponse(const PythonResult& result) {
    json response;
    
    if (result.success) {
        response["data"] = {
            {"executePython", {
                {"output", result.output},
                {"error", result.error},
                {"executionTime", result.execution_time.count()}
            }}
        };
    } else {
        response["errors"] = {
            {
                {"message", result.error.empty() ? "Unknown error" : result.error},
                {"extensions", {
                    {"code", "PYTHON_EXECUTION_ERROR"},
                    {"exitCode", result.exit_code}
                }}
            }
        };
    }
    
    return response.dump(2);
}

std::string GraphQLHandler::handleIntrospection(const std::string& query) {
    // Return our schema for introspection queries
    return getSchema();
}

std::string GraphQLHandler::extractArgument(const std::string& query, const std::string& field_name, const std::string& arg_name) {
    std::string pattern = field_name + "\\([^)]*?" + arg_name + ":\\s*\"([^\"]*)\"[^)]*\\)";
    std::regex arg_regex(pattern);
    std::smatch match;
    
    if (std::regex_search(query, match, arg_regex) && match.size() > 1) {
        return unescapeString(match[1].str());
    }
    
    return "";
}