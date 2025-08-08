#pragma once

#include <string>
#include <memory>
#include <python_executor.h>

struct GraphQLRequest {
    std::string query;
    std::string variables;
    std::string operation_name;
};

struct GraphQLResponse {
    std::string data;
    std::string errors;
    bool success;
};

class GraphQLHandler {
public:
    GraphQLHandler(std::shared_ptr<PythonExecutor> executor);
    ~GraphQLHandler() = default;

    GraphQLRequest parseRequest(const std::string& body);
    GraphQLResponse executeQuery(const GraphQLRequest& request);
    std::string handleExecutePython(const std::string& code);

private:
    std::shared_ptr<PythonExecutor> python_executor_;
    std::string extractCodeFromQuery(const std::string& query);
    std::string createResponse(const PythonResult& result);
    std::string getSchema();
    bool validateQuery(const std::string& query);
    std::string unescapeString(const std::string& str);
    std::string handleIntrospection(const std::string& query);
    std::string extractArgument(const std::string& query, const std::string& field_name, const std::string& arg_name);
};