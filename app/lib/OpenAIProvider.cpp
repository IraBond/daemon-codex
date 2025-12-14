/*
 * OpenAI API provider implementation
 * Part of Daemon Codex - a safety-first, plan-before-move file organizer
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "OpenAIProvider.hpp"
#include "LLMClient.hpp"
#include "Types.hpp"
#include "Logger.hpp"

#include <chrono>

OpenAIProvider::OpenAIProvider(std::string api_key,
                               std::string model,
                               ClientFactory client_factory)
    : api_key_(std::move(api_key))
    , model_(std::move(model))
    , client_factory_(std::move(client_factory))
{
    if (model_.empty()) {
        model_ = "gpt-4o-mini";
    }
}

ProviderCapability OpenAIProvider::capabilities() const
{
    return ProviderCapability::RemoteInference | ProviderCapability::Streaming;
}

HealthStatus OpenAIProvider::health_check() const
{
    if (api_key_.empty()) {
        return HealthStatus::NotConfigured;
    }
    // Note: We don't make a network request here to check health
    // as that would incur costs and latency. We just check configuration.
    return HealthStatus::Healthy;
}

std::vector<ModelInfo> OpenAIProvider::list_models() const
{
    // Return a static list of well-known models
    // Dynamic fetching would require an API call
    std::vector<ModelInfo> models;
    
    auto add_model = [&](const std::string& id, const std::string& name) {
        ModelInfo info;
        info.id = id;
        info.name = name;
        info.description = "OpenAI model";
        info.is_local = false;
        models.push_back(std::move(info));
    };
    
    add_model("gpt-4o-mini", "GPT-4o Mini");
    add_model("gpt-4o", "GPT-4o");
    add_model("gpt-4-turbo", "GPT-4 Turbo");
    add_model("gpt-3.5-turbo", "GPT-3.5 Turbo");
    
    return models;
}

bool OpenAIProvider::is_configured() const
{
    return !api_key_.empty();
}

void OpenAIProvider::set_api_key(const std::string& key)
{
    api_key_ = key;
}

void OpenAIProvider::set_model(const std::string& model)
{
    model_ = model.empty() ? "gpt-4o-mini" : model;
}

std::unique_ptr<ILLMClient> OpenAIProvider::create_client() const
{
    if (client_factory_) {
        return client_factory_(api_key_, model_);
    }
    return std::make_unique<LLMClient>(api_key_, model_);
}

LlmResponse OpenAIProvider::create_privacy_error(const std::string& reason) const
{
    LlmResponse response;
    response.provider_id = id();
    response.model_used = model_;
    response.success = false;
    response.error_code = 403;
    response.error_message = "Privacy control blocked request: " + reason;
    response.used_remote_inference = false;
    return response;
}

LlmResponse OpenAIProvider::chat(const LlmRequest& request) const
{
    LlmResponse response;
    response.provider_id = id();
    response.model_used = model_;
    response.used_remote_inference = true;
    response.actual_privacy_level = request.privacy_level;
    
    // SAFETY: Check privacy controls
    if (request.privacy_level == PrivacyLevel::LocalOnly) {
        return create_privacy_error("Request is marked LocalOnly but sent to remote provider");
    }
    
    if (!request.allow_content_upload && request.privacy_level == PrivacyLevel::FullContent) {
        return create_privacy_error("Full content upload requested but not explicitly allowed");
    }
    
    if (!is_configured()) {
        response.success = false;
        response.error_code = 1;
        response.error_message = "OpenAI provider not configured: API key missing";
        response.used_remote_inference = false;
        return response;
    }
    
    const auto start_time = std::chrono::steady_clock::now();
    
    try {
        auto client = create_client();
        
        // Build prompt from messages
        std::string combined_prompt;
        for (const auto& msg : request.messages) {
            if (msg.role == MessageRole::User) {
                combined_prompt += msg.content;
            }
        }
        
        std::string result = client->complete_prompt(combined_prompt, request.max_tokens);
        
        const auto end_time = std::chrono::steady_clock::now();
        response.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        response.text = std::move(result);
        response.success = true;
        
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->info("OpenAIProvider completed request in {}ms (REMOTE)", response.latency.count());
        }
    } catch (const std::exception& ex) {
        response.success = false;
        response.error_code = 2;
        response.error_message = std::string("OpenAI request failed: ") + ex.what();
        
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->error("OpenAIProvider error: {}", ex.what());
        }
    }
    
    return response;
}

LlmResponse OpenAIProvider::categorize(const std::string& filename,
                                       const std::string& filepath,
                                       bool is_directory,
                                       const std::string& consistency_context,
                                       const LlmRequest& base_request) const
{
    LlmResponse response;
    response.provider_id = id();
    response.model_used = model_;
    response.used_remote_inference = true;
    response.actual_privacy_level = base_request.privacy_level;
    
    // SAFETY: Check privacy controls
    if (base_request.privacy_level == PrivacyLevel::LocalOnly) {
        return create_privacy_error("Categorization marked LocalOnly but sent to remote provider");
    }
    
    if (!is_configured()) {
        response.success = false;
        response.error_code = 1;
        response.error_message = "OpenAI provider not configured: API key missing";
        response.used_remote_inference = false;
        return response;
    }
    
    const auto start_time = std::chrono::steady_clock::now();
    
    try {
        auto client = create_client();
        
        // SAFETY: Only send metadata unless content upload is allowed
        std::string safe_path;
        if (base_request.allow_content_upload || 
            base_request.privacy_level == PrivacyLevel::FullContent) {
            safe_path = filepath;
        }
        // For MetadataOnly mode, only filename/extension is sent (no path)
        
        FileType file_type = is_directory ? FileType::Directory : FileType::File;
        std::string result = client->categorize_file(filename, safe_path, file_type, consistency_context);
        
        const auto end_time = std::chrono::steady_clock::now();
        response.latency = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        response.text = std::move(result);
        response.success = true;
        
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->info("OpenAIProvider categorized '{}' in {}ms (REMOTE)", 
                        filename, response.latency.count());
        }
    } catch (const std::exception& ex) {
        response.success = false;
        response.error_code = 2;
        response.error_message = std::string("OpenAI categorization failed: ") + ex.what();
        
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->error("OpenAIProvider categorization error: {}", ex.what());
        }
    }
    
    return response;
}
