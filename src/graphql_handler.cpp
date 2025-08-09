#include "graphql_handler.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <memory>
#include <regex>

using json = nlohmann::json;

GraphQLHandler::GraphQLHandler(std::shared_ptr<PythonExecutor> executor)
    : python_executor_(executor) {}

GraphQLRequest GraphQLHandler::parseRequest(const std::string& body) {
    GraphQLRequest request;

    try {
        std::cout << "[DEBUG] parseRequest: Starting to parse body: '" << body << "'" << std::endl;
        json req_json = json::parse(body);
        std::cout << "[DEBUG] parseRequest: JSON parsed successfully" << std::endl;

        if (req_json.contains("query") && req_json["query"].is_string()) {
            request.query = req_json["query"].get<std::string>();
            std::cout << "[DEBUG] parseRequest: Query extracted from JSON: '" << request.query << "'" << std::endl;
        } else {
            request.errors = "Invalid GraphQL request: 'query' field missing or not a string.";
            return request;
        }

        // Parse GraphQL query using libgraphqlparser
        const char* error = nullptr;
        std::cout << "[DEBUG] parseRequest: About to parse GraphQL query: '" << request.query << "'" << std::endl;
        GraphQLAstNode* ast = graphql_parse_string(request.query.c_str(), &error);

        if (error) {
            std::cout << "[DEBUG] parseRequest: GraphQL parsing error: " << error << std::endl;
            request.errors = std::string("GraphQL parsing error: ") + error;
            graphql_error_free(const_cast<char*>(error));
            return request;
        }
        if (!ast) {
            std::cout << "[DEBUG] parseRequest: GraphQL AST is null" << std::endl;
            request.errors = "Failed to parse GraphQL query.";
            return request;
        }
        
        std::cout << "[DEBUG] parseRequest: GraphQL parsing successful, extracting code..." << std::endl;
        // Extract code from AST
        request.code = extractCodeFromQuery(request.query);

        if (request.code.empty()) {
            std::cout << "[DEBUG] parseRequest: No code extracted from query" << std::endl;
            request.errors = "GraphQL query must contain 'executePython(code: \"...\")'.";
        } else {
            std::cout << "[DEBUG] parseRequest: Code extracted successfully: '" << request.code << "'" << std::endl;
        }

        // Cleanup AST
        graphql_node_free(ast);
    } catch(const json::exception& e) {
        request.errors = "Invalid JSON in GraphQL request: " + std::string(e.what());
    } catch(const std::exception& e) {
        request.errors = "Error parsing GraphQL request: " + std::string(e.what());
    }

    return request;
}
std::string GraphQLHandler::extractCodeFromQuery(const std::string& query) {
    std::cout << "[DEBUG] extractCodeFromQuery: Starting with query: '" << query << "'" << std::endl;
    
    // Parse the query to get AST
    const char* error = nullptr;
    GraphQLAstNode* ast = graphql_parse_string(query.c_str(), &error);
    
    if (error || !ast) {
        if (error) {
            std::cout << "[DEBUG] extractCodeFromQuery: GraphQL parsing error: " << error << std::endl;
            graphql_error_free(const_cast<char*>(error));
        } else {
            std::cout << "[DEBUG] extractCodeFromQuery: GraphQL AST is null" << std::endl;
        }
        return "";
    }

    // Traverse AST to find executePython field and its code argument
    // This is a simplified version - you'll need to implement proper AST traversal
    std::string code;
    
    // For now, fallback to regex for code extraction
    // TODO: Implement proper AST traversal
    std::cout << "[DEBUG] extractCodeFromQuery: Using regex to extract code" << std::endl;
    std::regex code_regex("executePython\\(code:\\s*\"((?:\\\\\"|[^\"])*)\"\\)");
    std::smatch matches;
    
    std::cout << "[DEBUG] extractCodeFromQuery: Running regex search..." << std::endl;
    if (std::regex_search(query, matches, code_regex) && matches.size() > 1) {
        code = matches[1].str();
        std::cout << "[DEBUG] extractCodeFromQuery: Raw extracted code: '" << code << "'" << std::endl;
        
        // Unescape all escaped characters
        std::cout << "[DEBUG] extractCodeFromQuery: Starting unescaping..." << std::endl;
        std::string unescaped_code;
        for (size_t i = 0; i < code.length(); ++i) {
            if (code[i] == '\\' && i + 1 < code.length()) {
                std::cout << "[DEBUG] extractCodeFromQuery: Found escape sequence at position " << i << ": \\" << code[i + 1] << std::endl;
                switch (code[i + 1]) {
                    case '"':
                        unescaped_code += '"';
                        ++i; // Skip the next character
                        break;
                    case 'n':
                        unescaped_code += '\n';
                        ++i; // Skip the next character
                        break;
                    case 't':
                        unescaped_code += '\t';
                        ++i; // Skip the next character
                        break;
                    case 'r':
                        unescaped_code += '\r';
                        ++i; // Skip the next character
                        break;
                    case '\\':
                        unescaped_code += '\\';
                        ++i; // Skip the next character
                        break;
                    default:
                        unescaped_code += code[i]; // Keep the backslash
                        break;
                }
            } else {
                unescaped_code += code[i];
            }
        }
        code = unescaped_code;
        std::cout << "[DEBUG] extractCodeFromQuery: Final unescaped code: '" << code << "'" << std::endl;
    } else {
        std::cout << "[DEBUG] extractCodeFromQuery: Regex search failed - no matches found" << std::endl;
        std::cout << "[DEBUG] extractCodeFromQuery: Query length: " << query.length() << std::endl;
        std::cout << "[DEBUG] extractCodeFromQuery: Looking for pattern: executePython(code: \"...\")" << std::endl;
    }

    graphql_node_free(ast);
    return code;
}

GraphQLResponse GraphQLHandler::executeQuery(const GraphQLRequest& request) {
    GraphQLResponse response;
    
    try {
        // Handle introspection queries
        if (request.query.find("__schema") != std::string::npos ||
            request.query.find("__type") != std::string::npos) {
            response.data = handleIntrospection(request.query);
            response.success = true;
            return response;
        }

        // Execute Python code
        if (!python_executor_) {
            response.errors = "Python executor not available";
            response.success = false;
            return response;
        }

        if (request.code.empty()) {
            response.errors = "No Python code found in query.";
            response.success = false;
            return response;
        }

        PythonResult py_result = python_executor_->execute(request.code);
        
        // Create GraphQL response
        json data_json;
        json execute_python_json;
        
        execute_python_json["output"] = py_result.output;
        if (py_result.error.empty()) {
            execute_python_json["error"] = "";
        } else {
            execute_python_json["error"] = py_result.error;
        }
        execute_python_json["executionTime"] = py_result.execution_time.count() / 1000.0;
        
        data_json["executePython"] = execute_python_json;
        
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
        
        json final_response;
        final_response["data"] = data_json;
        response.data = final_response.dump(2);
        
    } catch (const std::exception& e) {
        response.errors = std::string("Error executing query: ") + e.what();
        response.success = false;
    }
    
    return response;
}

std::string GraphQLHandler::handleExecutePython(const std::string& code) {
    if (!python_executor_) {
        return "{\"error\": \"Python executor not available\"}";
    }
    
    PythonResult result = python_executor_->execute(code);
    return createResponse(result);
}

std::string GraphQLHandler::createResponse(const PythonResult& result) {
    json response;
    response["output"] = result.output;
    response["error"] = result.error.empty() ? nullptr : result.error;
    response["executionTime"] = result.execution_time.count() / 1000.0;
    response["success"] = result.success;
    
    return response.dump(2);
}

std::string GraphQLHandler::getSchema() {
    json schema = {
        {"__schema", {
            {"queryType", {{"name", "Query"}}},
            {"types", {
                {
                    {"kind", "OBJECT"},
                    {"name", "Query"},
                    {"fields", {
                        {
                            {"name", "executePython"},
                            {"type", {{"name", "PythonExecutionResult"}}},
                            {"args", {
                                {{"name", "code"}, {"type", {{"name", "String"}}}}
                            }}
                        }
                    }}
                },
                {
                    {"kind", "OBJECT"},
                    {"name", "PythonExecutionResult"},
                    {"fields", {
                        {{"name", "output"}, {"type", {{"name", "String"}}}},
                        {{"name", "error"}, {"type", {{"name", "String"}}}},
                        {{"name", "executionTime"}, {"type", {{"name", "Float"}}}}
                    }}
                },
                {{"kind", "SCALAR"}, {"name", "String"}},
                {{"kind", "SCALAR"}, {"name", "Float"}}
            }}
        }}
    };
    return json{{"data", schema}}.dump(2);
}

bool GraphQLHandler::validateQuery(const std::string& query) {
    const char* error = nullptr;
    GraphQLAstNode* ast = graphql_parse_string(query.c_str(), &error);
    
    if (error) {
        graphql_error_free(const_cast<char*>(error));
        return false;
    }
    
    if (ast) {
        graphql_node_free(ast);
        return true;
    }
    
    return false;
}

std::string GraphQLHandler::handleIntrospection(const std::string& query) {
    return getSchema();
}

// TODO: Implement AST traversal methods when needed