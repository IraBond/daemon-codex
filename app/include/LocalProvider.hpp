/*
 * Local LLM provider for on-device inference
 * Part of Daemon Codex - a safety-first, plan-before-move file organizer
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef LOCAL_PROVIDER_HPP
#define LOCAL_PROVIDER_HPP

#include "IProvider.hpp"
#include "ILLMClient.hpp"
#include <functional>
#include <memory>
#include <string>

/**
 * Provider that uses local llama.cpp for on-device inference
 * 
 * This is the default provider that keeps all data on-device.
 * It wraps the existing LocalLLMClient implementation.
 */
class LocalProvider : public IProvider {
public:
    /**
     * Factory function type for creating LLM clients
     * This allows dependency injection for testing
     */
    using ClientFactory = std::function<std::unique_ptr<ILLMClient>(const std::string& model_path)>;
    
    /**
     * Construct provider with model path
     * @param model_path Path to the GGUF model file
     * @param client_factory Optional factory for creating clients (for testing)
     */
    explicit LocalProvider(std::string model_path,
                          ClientFactory client_factory = nullptr);
    
    ~LocalProvider() override = default;
    
    // IProvider interface
    std::string id() const override { return "local"; }
    std::string display_name() const override { return "Local LLM (on-device)"; }
    ProviderCapability capabilities() const override;
    HealthStatus health_check() const override;
    std::vector<ModelInfo> list_models() const override;
    bool requires_network() const override { return false; }
    bool is_configured() const override;
    LlmResponse chat(const LlmRequest& request) const override;
    LlmResponse categorize(const std::string& filename,
                          const std::string& filepath,
                          bool is_directory,
                          const std::string& consistency_context,
                          const LlmRequest& base_request) const override;
    
    /**
     * Set the model path (allows changing models at runtime)
     */
    void set_model_path(const std::string& path);
    
    /**
     * Get current model path
     */
    const std::string& model_path() const { return model_path_; }

private:
    std::unique_ptr<ILLMClient> create_client() const;
    std::string build_prompt(const std::vector<ChatMessage>& messages) const;
    
    std::string model_path_;
    ClientFactory client_factory_;
    mutable std::unique_ptr<ILLMClient> cached_client_;
};

#endif // LOCAL_PROVIDER_HPP
