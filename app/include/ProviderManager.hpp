/*
 * Provider manager for LLM provider selection and privacy controls
 * Part of Daemon Codex - a safety-first, plan-before-move file organizer
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef PROVIDER_MANAGER_HPP
#define PROVIDER_MANAGER_HPP

#include "IProvider.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

/**
 * Privacy mode settings
 */
enum class PrivacyMode {
    LocalOnly,        // Only local providers allowed (default)
    RemoteAllowed,    // Remote providers enabled with user consent
};

/**
 * Manages LLM providers with privacy controls
 * 
 * This is the single choke point for all provider operations.
 * Remote providers MUST be explicitly enabled via set_privacy_mode().
 * 
 * Safety principles:
 * 1. Default to LocalOnly mode
 * 2. Reject remote provider requests if not explicitly allowed
 * 3. Log all provider switches and remote operations
 * 4. Never cache sensitive data for remote providers
 */
class ProviderManager {
public:
    ProviderManager();
    ~ProviderManager() = default;
    
    /**
     * Register a provider
     * @param provider The provider to register
     */
    void register_provider(ProviderPtr provider);
    
    /**
     * Remove a provider
     * @param provider_id ID of provider to remove
     */
    void unregister_provider(const std::string& provider_id);
    
    /**
     * Get a provider by ID
     * @param provider_id Provider ID
     * @return Provider or nullptr if not found
     */
    ProviderPtr get_provider(const std::string& provider_id) const;
    
    /**
     * Get all registered providers
     */
    std::vector<ProviderPtr> all_providers() const;
    
    /**
     * Get providers that are currently allowed under privacy settings
     */
    std::vector<ProviderPtr> allowed_providers() const;
    
    /**
     * Set the active provider
     * @param provider_id Provider ID to activate
     * @return true if provider was set, false if blocked by privacy settings
     */
    bool set_active_provider(const std::string& provider_id);
    
    /**
     * Get the active provider
     * @return Active provider or nullptr if none set
     */
    ProviderPtr active_provider() const;
    
    /**
     * Set privacy mode
     * 
     * WARNING: Setting RemoteAllowed enables data exfiltration.
     * Callers should display appropriate warnings to users.
     * 
     * @param mode Privacy mode
     * @param user_confirmed Whether user explicitly confirmed (required for RemoteAllowed)
     * @return true if mode was set, false if user confirmation missing
     */
    bool set_privacy_mode(PrivacyMode mode, bool user_confirmed = false);
    
    /**
     * Get current privacy mode
     */
    PrivacyMode privacy_mode() const { return privacy_mode_; }
    
    /**
     * Check if remote providers are currently allowed
     */
    bool remote_allowed() const { return privacy_mode_ == PrivacyMode::RemoteAllowed; }
    
    /**
     * Execute a chat request respecting privacy settings
     * 
     * This is the primary entry point for LLM requests.
     * It enforces privacy controls at a single choke point.
     * 
     * @param request The LLM request
     * @return Response (with error if privacy controls block the request)
     */
    LlmResponse chat(const LlmRequest& request) const;
    
    /**
     * Execute a categorization request respecting privacy settings
     * 
     * @param filename File name to categorize
     * @param filepath Full path to file
     * @param is_directory Whether the item is a directory
     * @param consistency_context Context for consistent categorization
     * @param base_request Base request parameters
     * @return Response (with error if privacy controls block the request)
     */
    LlmResponse categorize(const std::string& filename,
                          const std::string& filepath,
                          bool is_directory,
                          const std::string& consistency_context,
                          const LlmRequest& base_request) const;
    
    /**
     * Check if a request would be allowed under current privacy settings
     * 
     * @param request The request to check
     * @return Error message if blocked, empty if allowed
     */
    std::optional<std::string> validate_request(const LlmRequest& request) const;

private:
    LlmResponse create_no_provider_error() const;
    LlmResponse create_privacy_blocked_error(const std::string& provider_id) const;
    bool is_provider_allowed(const IProvider& provider) const;
    
    std::unordered_map<std::string, ProviderPtr> providers_;
    std::string active_provider_id_;
    PrivacyMode privacy_mode_{PrivacyMode::LocalOnly};
};

#endif // PROVIDER_MANAGER_HPP
