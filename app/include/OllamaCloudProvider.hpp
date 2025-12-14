/*
 * Ollama Cloud provider for remote inference
 * Part of Daemon Codex - a safety-first, plan-before-move file organizer
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef OLLAMA_CLOUD_PROVIDER_HPP
#define OLLAMA_CLOUD_PROVIDER_HPP

#include "IProvider.hpp"
#include <functional>
#include <memory>
#include <string>

/**
 * Configuration for Ollama Cloud provider
 */
struct OllamaCloudConfig {
    std::string base_url;           // e.g., "https://your-instance.ollama.ai"
    std::string api_key;            // API key if required by instance
    std::string model;              // e.g., "llama3.2"
    int timeout_ms{30000};          // Request timeout
    int max_retries{3};             // Retry count on failure
    int retry_backoff_base_ms{1000}; // Exponential backoff base
};

/**
 * Provider that uses Ollama Cloud for remote inference
 * 
 * WARNING: This provider sends data to remote servers.
 * It MUST be explicitly enabled by the user and respects privacy controls.
 * 
 * TODO(PR#1): Finalize endpoint paths after reviewing Ollama Cloud API docs.
 * Current implementation is scaffolding only.
 */
class OllamaCloudProvider : public IProvider {
public:
    /**
     * HTTP client interface for testability
     */
    struct HttpResponse {
        int status_code{0};
        std::string body;
        std::string error;
        bool success() const { return status_code >= 200 && status_code < 300; }
    };
    
    using HttpClient = std::function<HttpResponse(
        const std::string& url,
        const std::string& method,
        const std::string& body,
        const std::vector<std::pair<std::string, std::string>>& headers,
        int timeout_ms
    )>;
    
    /**
     * Construct provider with configuration
     * @param config Configuration for the provider
     * @param http_client Optional HTTP client for testing
     */
    explicit OllamaCloudProvider(OllamaCloudConfig config,
                                 HttpClient http_client = nullptr);
    
    ~OllamaCloudProvider() override = default;
    
    // IProvider interface
    std::string id() const override { return "ollama-cloud"; }
    std::string display_name() const override { return "Ollama Cloud"; }
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
     * Update configuration
     */
    void set_config(const OllamaCloudConfig& config);
    
    /**
     * Get current configuration
     */
    const OllamaCloudConfig& config() const { return config_; }

private:
    // Endpoint paths (TODO: finalize after API doc review)
    static constexpr const char* kChatEndpoint = "/api/chat";
    static constexpr const char* kModelsEndpoint = "/api/tags";
    static constexpr const char* kHealthEndpoint = "/api/version";
    
    HttpResponse make_request(const std::string& endpoint,
                             const std::string& method,
                             const std::string& body,
                             int timeout_ms) const;
    HttpResponse default_http_client(const std::string& url,
                                    const std::string& method,
                                    const std::string& body,
                                    const std::vector<std::pair<std::string, std::string>>& headers,
                                    int timeout_ms) const;
    std::string build_chat_payload(const LlmRequest& request) const;
    LlmResponse parse_chat_response(const HttpResponse& response,
                                   std::chrono::milliseconds latency) const;
    LlmResponse create_privacy_error(const std::string& reason) const;
    LlmResponse create_config_error(const std::string& reason) const;
    
    OllamaCloudConfig config_;
    HttpClient http_client_;
};

#endif // OLLAMA_CLOUD_PROVIDER_HPP
