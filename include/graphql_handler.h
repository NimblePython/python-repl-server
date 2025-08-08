#pragma once

#include <string>
#include <memory>
#include <python_executor.h>

#include <GraphQLParser.h>
#include <c/GraphQLAstNode.h>
#include <c/GraphQLAstToJSON.h>
#include <c/GraphQLParser.h>

struct GraphQLRequest {
    std::string query;
    std::string variables;
    std::string operation_name;
    std::string code;
    std::string errors;
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
    std::string handleIntrospection(const std::string& query);

    // TODO: Implement AST traversal methods when needed
};