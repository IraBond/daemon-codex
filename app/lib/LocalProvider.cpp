/*
 * Local LLM provider implementation
 * Part of Daemon Codex - a safety-first, plan-before-move file organizer
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "LocalProvider.hpp"
#include "LocalLLMClient.hpp"
#include "Types.hpp"
#include "Logger.hpp"

#include <chrono>
#include <filesystem>

LocalProvider::LocalProvider(std::string model_path, ClientFactory client_factory)
    : model_path_(std::move(model_path))
    , client_factory_(std::move(client_factory))
{}

ProviderCapability LocalProvider::capabilities() const
{
    return ProviderCapability::LocalInference;
}

HealthStatus LocalProvider::health_check() const
{
    if (model_path_.empty()) {
        return HealthStatus::NotConfigured;
    }
    
    if (!std::filesystem::exists(model_path_)) {
        return HealthStatus::Unavailable;
    }
    
    return HealthStatus::Healthy;
}

std::vector<ModelInfo> LocalProvider::list_models() const
{
    std::vector<ModelInfo> models;
    
    if (!model_path_.empty() && std::filesystem::exists(model_path_)) {
        ModelInfo info;
        info.id = model_path_;
        info.name = std::filesystem::path(model_path_).filename().string();
        info.description = "Local GGUF model";
        info.is_local = true;
        models.push_back(std::move(info));
    }
    
    return models;
}

bool LocalProvider::is_configured() const
{
    return !model_path_.empty() && std::filesystem::exists(model_path_);
}

void LocalProvider::set_model_path(const std::string& path)
{
    model_path_ = path;
    cached_client_.reset();
}

std::unique_ptr<ILLMClient> LocalProvider::create_client() const
{
    if (client_factory_) {
        return client_factory_(model_path_);
    }
    return std::make_unique<LocalLLMClient>(model_path_);
}

std::string LocalProvider::build_prompt(const std::vector<ChatMessage>& messages) const
{
    std::string prompt;
    for (const auto& msg : messages) {
        switch (msg.role) {
            case MessageRole::System:
                prompt += "System: " + msg.content + "\n\n";
                break;
            case MessageRole::User:
                prompt += "User: " + msg.content + "\n\n";
                break;
            case MessageRole::Assistant:
                prompt += "Assistant: " + msg.content + "\n\n";
                break;
        }
    }
    return prompt;
}

LlmResponse LocalProvider::chat(const LlmRequest& request) const
{
    LlmResponse response;
    response.provider_id = id();
    response.model_used = model_path_;
    response.used_remote_inference = false;
    response.actual_privacy_level = PrivacyLevel::LocalOnly;
    
    const auto start_time = std::chrono::steady_clock::now();
    
    if (!is_configured()) {
        response.success = false;
        response.error_code = 1;
        response.error_message = "Local provider not configured: model path missing or invalid";
        return response;
    }
    
    try {
        auto client = create_client();
        std::string prompt = build_prompt(request.messages);
        
        std::string result = client->complete_prompt(prompt, request.max_tokens);
        
        const auto end_time = std::chrono::steady_clock::now();
        response.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        response.text = std::move(result);
        response.success = true;
        
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->debug("LocalProvider completed request in {}ms", response.latency.count());
        }
    } catch (const std::exception& ex) {
        response.success = false;
        response.error_code = 2;
        response.error_message = std::string("Local inference failed: ") + ex.what();
        
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->error("LocalProvider error: {}", ex.what());
        }
    }
    
    return response;
}

LlmResponse LocalProvider::categorize(const std::string& filename,
                                      const std::string& filepath,
                                      bool is_directory,
                                      const std::string& consistency_context,
                                      const LlmRequest& base_request) const
{
    LlmResponse response;
    response.provider_id = id();
    response.model_used = model_path_;
    response.used_remote_inference = false;
    response.actual_privacy_level = PrivacyLevel::LocalOnly;
    
    const auto start_time = std::chrono::steady_clock::now();
    
    if (!is_configured()) {
        response.success = false;
        response.error_code = 1;
        response.error_message = "Local provider not configured: model path missing or invalid";
        return response;
    }
    
    try {
        auto client = create_client();
        
        FileType file_type = is_directory ? FileType::Directory : FileType::File;
        std::string result = client->categorize_file(filename, filepath, file_type, consistency_context);
        
        const auto end_time = std::chrono::steady_clock::now();
        response.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        response.text = std::move(result);
        response.success = true;
        
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->debug("LocalProvider categorized '{}' in {}ms", filename, response.latency.count());
        }
    } catch (const std::exception& ex) {
        response.success = false;
        response.error_code = 2;
        response.error_message = std::string("Local categorization failed: ") + ex.what();
        
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->error("LocalProvider categorization error: {}", ex.what());
        }
    }
    
    return response;
}
