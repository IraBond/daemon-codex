/*
 * OpenAI API provider for remote inference
 * Part of Daemon Codex - a safety-first, plan-before-move file organizer
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef OPENAI_PROVIDER_HPP
#define OPENAI_PROVIDER_HPP

#include "IProvider.hpp"
#include "ILLMClient.hpp"
#include <functional>
#include <memory>
#include <string>

/**
 * Provider that uses OpenAI API for remote inference
 * 
 * WARNING: This provider sends data to remote servers.
 * It MUST be explicitly enabled by the user and respects privacy controls.
 */
class OpenAIProvider : public IProvider {
public:
    /**
     * Factory function type for creating LLM clients
     * This allows dependency injection for testing
     */
    using ClientFactory = std::function<std::unique_ptr<ILLMClient>(const std::string& api_key, const std::string& model)>;
    
    /**
     * Construct provider with API key and model
     * @param api_key OpenAI API key
     * @param model Model to use (e.g., "gpt-4o-mini")
     * @param client_factory Optional factory for creating clients (for testing)
     */
    explicit OpenAIProvider(std::string api_key,
                           std::string model = "gpt-4o-mini",
                           ClientFactory client_factory = nullptr);
    
    ~OpenAIProvider() override = default;
    
    // IProvider interface
    std::string id() const override { return "openai"; }
    std::string display_name() const override { return "OpenAI (ChatGPT)"; }
    ProviderCapability capabilities() const override;
    HealthStatus health_check() const override;
    std::vector<ModelInfo> list_models() const override;
    bool requires_network() const override { return true; }
    bool is_configured() const override;
    LlmResponse chat(const LlmRequest& request) const override;
    LlmResponse categorize(const std::string& filename,
                          const std::string& filepath,
                          bool is_directory,
                          const std::string& consistency_context,
                          const LlmRequest& base_request) const override;
    
    /**
     * Update API key
     */
    void set_api_key(const std::string& key);
    
    /**
     * Update model
     */
    void set_model(const std::string& model);
    
    /**
     * Get current model
     */
    const std::string& model() const { return model_; }

private:
    std::unique_ptr<ILLMClient> create_client() const;
    LlmResponse create_privacy_error(const std::string& reason) const;
    
    std::string api_key_;
    std::string model_;
    ClientFactory client_factory_;
};

#endif // OPENAI_PROVIDER_HPP
