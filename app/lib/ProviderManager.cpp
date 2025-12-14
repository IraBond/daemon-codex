/*
 * Provider manager implementation
 * Part of Daemon Codex - a safety-first, plan-before-move file organizer
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "ProviderManager.hpp"
#include "Logger.hpp"

ProviderManager::ProviderManager()
    : privacy_mode_(PrivacyMode::LocalOnly)
{}

void ProviderManager::register_provider(ProviderPtr provider)
{
    if (!provider) {
        return;
    }
    
    const std::string provider_id = provider->id();
    providers_[provider_id] = std::move(provider);
    
    if (auto logger = Logger::get_logger("core_logger")) {
        logger->info("Registered provider: {}", provider_id);
    }
}

void ProviderManager::unregister_provider(const std::string& provider_id)
{
    auto it = providers_.find(provider_id);
    if (it != providers_.end()) {
        providers_.erase(it);
        
        if (active_provider_id_ == provider_id) {
            active_provider_id_.clear();
        }
        
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->info("Unregistered provider: {}", provider_id);
        }
    }
}

ProviderPtr ProviderManager::get_provider(const std::string& provider_id) const
{
    auto it = providers_.find(provider_id);
    if (it != providers_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<ProviderPtr> ProviderManager::all_providers() const
{
    std::vector<ProviderPtr> result;
    result.reserve(providers_.size());
    
    for (const auto& [id, provider] : providers_) {
        result.push_back(provider);
    }
    
    return result;
}

std::vector<ProviderPtr> ProviderManager::allowed_providers() const
{
    std::vector<ProviderPtr> result;
    
    for (const auto& [id, provider] : providers_) {
        if (is_provider_allowed(*provider)) {
            result.push_back(provider);
        }
    }
    
    return result;
}

bool ProviderManager::is_provider_allowed(const IProvider& provider) const
{
    // Local providers are always allowed
    if (!provider.requires_network()) {
        return true;
    }
    
    // Remote providers only allowed if privacy mode permits
    return privacy_mode_ == PrivacyMode::RemoteAllowed;
}

bool ProviderManager::set_active_provider(const std::string& provider_id)
{
    auto provider = get_provider(provider_id);
    if (!provider) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->warn("Cannot set active provider: {} not found", provider_id);
        }
        return false;
    }
    
    // Check if provider is allowed under current privacy settings
    if (!is_provider_allowed(*provider)) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->warn("Cannot set active provider: {} is remote and privacy mode is LocalOnly", 
                        provider_id);
        }
        return false;
    }
    
    active_provider_id_ = provider_id;
    
    if (auto logger = Logger::get_logger("core_logger")) {
        logger->info("Set active provider: {} (remote: {})", 
                    provider_id, 
                    provider->requires_network() ? "yes" : "no");
    }
    
    return true;
}

ProviderPtr ProviderManager::active_provider() const
{
    return get_provider(active_provider_id_);
}

bool ProviderManager::set_privacy_mode(PrivacyMode mode, bool user_confirmed)
{
    // Switching to RemoteAllowed requires explicit user confirmation
    if (mode == PrivacyMode::RemoteAllowed && !user_confirmed) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->warn("Cannot enable RemoteAllowed mode without user confirmation");
        }
        return false;
    }
    
    PrivacyMode old_mode = privacy_mode_;
    privacy_mode_ = mode;
    
    // If switching to LocalOnly, deactivate any remote provider
    if (mode == PrivacyMode::LocalOnly && !active_provider_id_.empty()) {
        auto provider = get_provider(active_provider_id_);
        if (provider && provider->requires_network()) {
            if (auto logger = Logger::get_logger("core_logger")) {
                logger->info("Privacy mode changed to LocalOnly, deactivating remote provider: {}", 
                            active_provider_id_);
            }
            active_provider_id_.clear();
        }
    }
    
    if (auto logger = Logger::get_logger("core_logger")) {
        const char* old_mode_str = old_mode == PrivacyMode::LocalOnly ? "LocalOnly" : "RemoteAllowed";
        const char* new_mode_str = mode == PrivacyMode::LocalOnly ? "LocalOnly" : "RemoteAllowed";
        logger->info("Privacy mode changed: {} -> {}", old_mode_str, new_mode_str);
    }
    
    return true;
}

LlmResponse ProviderManager::create_no_provider_error() const
{
    LlmResponse response;
    response.success = false;
    response.error_code = 1;
    response.error_message = "No active provider configured";
    return response;
}

LlmResponse ProviderManager::create_privacy_blocked_error(const std::string& provider_id) const
{
    LlmResponse response;
    response.provider_id = provider_id;
    response.success = false;
    response.error_code = 403;
    response.error_message = "Request blocked: provider '" + provider_id + 
                            "' requires network but privacy mode is LocalOnly. " +
                            "Enable remote providers in settings if you want to use this provider.";
    return response;
}

std::optional<std::string> ProviderManager::validate_request(const LlmRequest& request) const
{
    auto provider = active_provider();
    if (!provider) {
        return "No active provider configured";
    }
    
    if (provider->requires_network()) {
        if (privacy_mode_ == PrivacyMode::LocalOnly) {
            return "Active provider requires network but privacy mode is LocalOnly";
        }
        
        if (request.privacy_level == PrivacyLevel::LocalOnly) {
            return "Request is marked LocalOnly but active provider requires network";
        }
    }
    
    return std::nullopt;
}

LlmResponse ProviderManager::chat(const LlmRequest& request) const
{
    auto provider = active_provider();
    if (!provider) {
        return create_no_provider_error();
    }
    
    // SAFETY: Check privacy controls at the choke point
    if (!is_provider_allowed(*provider)) {
        return create_privacy_blocked_error(provider->id());
    }
    
    // Validate request privacy level
    if (provider->requires_network() && request.privacy_level == PrivacyLevel::LocalOnly) {
        LlmResponse response;
        response.provider_id = provider->id();
        response.success = false;
        response.error_code = 403;
        response.error_message = "Request marked as LocalOnly cannot be sent to remote provider";
        return response;
    }
    
    if (auto logger = Logger::get_logger("core_logger")) {
        logger->debug("ProviderManager dispatching chat request to provider: {}", provider->id());
    }
    
    return provider->chat(request);
}

LlmResponse ProviderManager::categorize(
    const std::string& filename,
    const std::string& filepath,
    bool is_directory,
    const std::string& consistency_context,
    const LlmRequest& base_request) const
{
    auto provider = active_provider();
    if (!provider) {
        return create_no_provider_error();
    }
    
    // SAFETY: Check privacy controls at the choke point
    if (!is_provider_allowed(*provider)) {
        return create_privacy_blocked_error(provider->id());
    }
    
    // Validate request privacy level
    if (provider->requires_network() && base_request.privacy_level == PrivacyLevel::LocalOnly) {
        LlmResponse response;
        response.provider_id = provider->id();
        response.success = false;
        response.error_code = 403;
        response.error_message = "Categorization request marked as LocalOnly cannot be sent to remote provider";
        return response;
    }
    
    if (auto logger = Logger::get_logger("core_logger")) {
        logger->debug("ProviderManager dispatching categorize request to provider: {} (file: {})", 
                     provider->id(), filename);
    }
    
    return provider->categorize(filename, filepath, is_directory, consistency_context, base_request);
}
