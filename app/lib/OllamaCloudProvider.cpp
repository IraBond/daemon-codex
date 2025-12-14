/*
 * Ollama Cloud provider implementation
 * Part of Daemon Codex - a safety-first, plan-before-move file organizer
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "OllamaCloudProvider.hpp"
#include "Logger.hpp"

#include <curl/curl.h>
#include <sstream>
#include <chrono>
#include <thread>

#ifdef _WIN32
    #include <json/json.h>
#elif __APPLE__
    #include <json/json.h>
#else
    #include <jsoncpp/json/json.h>
#endif

namespace {

// Helper function for curl write callback
size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* response)
{
    const size_t total_size = size * nmemb;
    response->append(static_cast<const char*>(contents), total_size);
    return total_size;
}

std::string escape_json_string(const std::string& input)
{
    std::string out;
    out.reserve(input.size() * 2);
    for (char c : input) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

} // namespace

OllamaCloudProvider::OllamaCloudProvider(OllamaCloudConfig config, HttpClient http_client)
    : config_(std::move(config))
    , http_client_(std::move(http_client))
{}

ProviderCapability OllamaCloudProvider::capabilities() const
{
    return ProviderCapability::RemoteInference;
}

HealthStatus OllamaCloudProvider::health_check() const
{
    if (config_.base_url.empty()) {
        return HealthStatus::NotConfigured;
    }
    
    // Try to reach the health endpoint
    auto response = make_request(kHealthEndpoint, "GET", "", 5000);
    
    if (!response.success()) {
        return HealthStatus::Unavailable;
    }
    
    return HealthStatus::Healthy;
}

std::vector<ModelInfo> OllamaCloudProvider::list_models() const
{
    std::vector<ModelInfo> models;
    
    if (!is_configured()) {
        return models;
    }
    
    // SCAFFOLDING(PR#1): Model listing is intentionally incomplete.
    // After reviewing Ollama Cloud API docs, implement:
    // - GET /api/tags endpoint to fetch available models
    // - Parse response and populate ModelInfo structs
    // For now, return only the configured model so the provider remains functional.
    
    if (!config_.model.empty()) {
        ModelInfo info;
        info.id = config_.model;
        info.name = config_.model;
        info.description = "Configured Ollama model";
        info.is_local = false;
        models.push_back(std::move(info));
    }
    
    return models;
}

bool OllamaCloudProvider::is_configured() const
{
    return !config_.base_url.empty() && !config_.model.empty();
}

void OllamaCloudProvider::set_config(const OllamaCloudConfig& config)
{
    config_ = config;
}

OllamaCloudProvider::HttpResponse OllamaCloudProvider::default_http_client(
    const std::string& url,
    const std::string& method,
    const std::string& body,
    const std::vector<std::pair<std::string, std::string>>& headers,
    int timeout_ms) const
{
    HttpResponse result;
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        result.error = "Failed to initialize cURL";
        return result;
    }
    
    std::string response_body;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(timeout_ms));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    
    curl_slist* curl_headers = nullptr;
    for (const auto& [key, value] : headers) {
        std::string header = key + ": " + value;
        curl_headers = curl_slist_append(curl_headers, header.c_str());
    }
    
    if (curl_headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
    }
    
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    }
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        result.error = curl_easy_strerror(res);
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status_code);
        result.body = std::move(response_body);
    }
    
    if (curl_headers) {
        curl_slist_free_all(curl_headers);
    }
    curl_easy_cleanup(curl);
    
    return result;
}

OllamaCloudProvider::HttpResponse OllamaCloudProvider::make_request(
    const std::string& endpoint,
    const std::string& method,
    const std::string& body,
    int timeout_ms) const
{
    std::string url = config_.base_url;
    // Remove trailing slash if present
    if (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    url += endpoint;
    
    std::vector<std::pair<std::string, std::string>> headers;
    headers.emplace_back("Content-Type", "application/json");
    
    if (!config_.api_key.empty()) {
        headers.emplace_back("Authorization", "Bearer " + config_.api_key);
    }
    
    if (http_client_) {
        return http_client_(url, method, body, headers, timeout_ms);
    }
    
    return default_http_client(url, method, body, headers, timeout_ms);
}

std::string OllamaCloudProvider::build_chat_payload(const LlmRequest& request) const
{
    // SCAFFOLDING(PR#1): Payload format is provisional.
    // Current implementation follows Ollama's local API format which may differ from
    // Ollama Cloud. After reviewing Ollama Cloud API docs, verify:
    // - Endpoint: /api/chat vs /v1/chat/completions
    // - Message format: {"role": "...", "content": "..."} structure
    // - Options/parameters naming: num_predict vs max_tokens, etc.
    
    std::ostringstream payload;
    payload << "{";
    payload << "\"model\": \"" << escape_json_string(config_.model) << "\",";
    payload << "\"stream\": false,";
    
    // Build messages array
    payload << "\"messages\": [";
    bool first = true;
    for (const auto& msg : request.messages) {
        if (!first) payload << ",";
        first = false;
        
        std::string role;
        switch (msg.role) {
            case MessageRole::System: role = "system"; break;
            case MessageRole::User: role = "user"; break;
            case MessageRole::Assistant: role = "assistant"; break;
        }
        
        payload << "{\"role\": \"" << role << "\", ";
        payload << "\"content\": \"" << escape_json_string(msg.content) << "\"}";
    }
    payload << "]";
    
    // Add optional parameters
    if (request.max_tokens > 0) {
        payload << ",\"options\": {";
        payload << "\"num_predict\": " << request.max_tokens;
        payload << ",\"temperature\": " << request.temperature;
        payload << "}";
    }
    
    payload << "}";
    return payload.str();
}

LlmResponse OllamaCloudProvider::parse_chat_response(
    const HttpResponse& http_response,
    std::chrono::milliseconds latency) const
{
    LlmResponse response;
    response.provider_id = id();
    response.model_used = config_.model;
    response.latency = latency;
    response.used_remote_inference = true;
    
    if (!http_response.success()) {
        response.success = false;
        response.error_code = http_response.status_code;
        response.error_message = "HTTP request failed: " + http_response.error;
        if (http_response.status_code > 0) {
            response.error_message += " (status: " + std::to_string(http_response.status_code) + ")";
        }
        return response;
    }
    
    // Parse JSON response
    Json::CharReaderBuilder reader_builder;
    Json::Value root;
    std::istringstream response_stream(http_response.body);
    std::string errors;
    
    if (!Json::parseFromStream(reader_builder, response_stream, &root, &errors)) {
        response.success = false;
        response.error_code = 3;
        response.error_message = "Failed to parse JSON response: " + errors;
        return response;
    }
    
    // SCAFFOLDING(PR#1): Response parsing assumes Ollama's local API format.
    // After reviewing Ollama Cloud API docs, validate response structure:
    // - Chat response: {"message": {"content": "..."}} vs {"choices": [...]}
    // - Error format: {"error": "..."} vs {"error": {"message": "..."}}
    // - Token usage fields: prompt_eval_count vs prompt_tokens
    
    if (root.isMember("message") && root["message"].isMember("content")) {
        response.text = root["message"]["content"].asString();
        response.success = true;
    } else if (root.isMember("response")) {
        response.text = root["response"].asString();
        response.success = true;
    } else if (root.isMember("error")) {
        response.success = false;
        response.error_message = root["error"].asString();
    } else {
        response.success = false;
        response.error_message = "Unexpected response format";
    }
    
    // Extract usage if available
    if (root.isMember("prompt_eval_count")) {
        response.usage.prompt_tokens = root["prompt_eval_count"].asInt();
    }
    if (root.isMember("eval_count")) {
        response.usage.completion_tokens = root["eval_count"].asInt();
    }
    response.usage.total_tokens = response.usage.prompt_tokens + response.usage.completion_tokens;
    
    return response;
}

LlmResponse OllamaCloudProvider::create_privacy_error(const std::string& reason) const
{
    LlmResponse response;
    response.provider_id = id();
    response.model_used = config_.model;
    response.success = false;
    response.error_code = 403;
    response.error_message = "Privacy control blocked request: " + reason;
    response.used_remote_inference = false;
    return response;
}

LlmResponse OllamaCloudProvider::create_config_error(const std::string& reason) const
{
    LlmResponse response;
    response.provider_id = id();
    response.model_used = config_.model;
    response.success = false;
    response.error_code = 1;
    response.error_message = "Configuration error: " + reason;
    response.used_remote_inference = false;
    return response;
}

LlmResponse OllamaCloudProvider::chat(const LlmRequest& request) const
{
    // SAFETY: Check privacy controls
    if (request.privacy_level == PrivacyLevel::LocalOnly) {
        return create_privacy_error("Request is marked LocalOnly but sent to remote provider");
    }
    
    if (!request.allow_content_upload && request.privacy_level == PrivacyLevel::FullContent) {
        return create_privacy_error("Full content upload requested but not explicitly allowed");
    }
    
    if (!is_configured()) {
        return create_config_error("Ollama Cloud provider not configured: base_url or model missing");
    }
    
    const auto start_time = std::chrono::steady_clock::now();
    
    std::string payload = build_chat_payload(request);
    
    // Retry loop with exponential backoff
    int retry_count = 0;
    HttpResponse http_response;
    
    while (retry_count <= request.max_retries) {
        if (retry_count > 0) {
            int backoff_ms = config_.retry_backoff_base_ms * (1 << (retry_count - 1));
            std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
            
            if (auto logger = Logger::get_logger("core_logger")) {
                logger->debug("OllamaCloudProvider retry {} after {}ms backoff", retry_count, backoff_ms);
            }
        }
        
        http_response = make_request(kChatEndpoint, "POST", payload, request.timeout_ms);
        
        // Success or non-retryable error
        if (http_response.success() || 
            (http_response.status_code >= 400 && http_response.status_code < 500)) {
            break;
        }
        
        retry_count++;
    }
    
    const auto end_time = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    LlmResponse response = parse_chat_response(http_response, latency);
    response.actual_privacy_level = request.privacy_level;
    
    if (auto logger = Logger::get_logger("core_logger")) {
        if (response.success) {
            logger->info("OllamaCloudProvider completed request in {}ms (REMOTE)", latency.count());
        } else {
            logger->error("OllamaCloudProvider error: {}", response.error_message);
        }
    }
    
    return response;
}

LlmResponse OllamaCloudProvider::categorize(
    const std::string& filename,
    const std::string& filepath,
    bool is_directory,
    const std::string& consistency_context,
    const LlmRequest& base_request) const
{
    // SAFETY: Check privacy controls
    if (base_request.privacy_level == PrivacyLevel::LocalOnly) {
        return create_privacy_error("Categorization marked LocalOnly but sent to remote provider");
    }
    
    if (!is_configured()) {
        return create_config_error("Ollama Cloud provider not configured: base_url or model missing");
    }
    
    // Build categorization prompt
    std::string item_type = is_directory ? "directory" : "file";
    std::string prompt;
    
    // SAFETY: Only include path if explicitly allowed
    if (base_request.allow_content_upload || 
        base_request.privacy_level == PrivacyLevel::FullContent) {
        prompt = "Categorize the " + item_type + " with full path: " + filepath + 
                "\nName: " + filename;
    } else {
        // MetadataOnly mode - only send filename
        prompt = "Categorize " + item_type + ": " + filename;
    }
    
    if (!consistency_context.empty()) {
        prompt += "\n\n" + consistency_context;
    }
    
    const std::string system_prompt =
        "You are a file categorization assistant. If it's an installer, describe the type of "
        "software it installs. Consider the filename, extension, and any directory context provided. "
        "Always reply with one line in the format <Main category> : <Subcategory>. Main category "
        "must be broad (one or two words, plural). Subcategory must be specific, relevant, and must "
        "not repeat the main category.";
    
    LlmRequest request = base_request;
    request.messages = {
        {MessageRole::System, system_prompt},
        {MessageRole::User, prompt}
    };
    
    LlmResponse response = chat(request);
    
    if (auto logger = Logger::get_logger("core_logger")) {
        if (response.success) {
            logger->info("OllamaCloudProvider categorized '{}' in {}ms (REMOTE)", 
                        filename, response.latency.count());
        }
    }
    
    return response;
}
