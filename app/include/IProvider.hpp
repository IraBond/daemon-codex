/*
 * Provider abstraction for LLM inference
 * Part of Daemon Codex - a safety-first, plan-before-move file organizer
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef I_PROVIDER_HPP
#define I_PROVIDER_HPP

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

/**
 * Provider capabilities flags
 */
enum class ProviderCapability : uint32_t {
    None          = 0,
    LocalInference   = 1 << 0,   // Runs entirely on device
    RemoteInference  = 1 << 1,   // Sends data to remote server
    Vision           = 1 << 2,   // Can process images
    Embeddings       = 1 << 3,   // Can generate embeddings
    Streaming        = 1 << 4,   // Supports streaming responses
};

inline ProviderCapability operator|(ProviderCapability a, ProviderCapability b) {
    return static_cast<ProviderCapability>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline ProviderCapability operator&(ProviderCapability a, ProviderCapability b) {
    return static_cast<ProviderCapability>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool has_capability(ProviderCapability caps, ProviderCapability flag) {
    return (static_cast<uint32_t>(caps) & static_cast<uint32_t>(flag)) != 0;
}

/**
 * Health check result for a provider
 */
enum class HealthStatus {
    Healthy,        // Provider is ready
    Degraded,       // Provider works but with issues
    Unavailable,    // Provider cannot be used
    NotConfigured,  // Provider lacks required configuration
};

/**
 * Privacy level for requests - controls what data can be sent off-device
 */
enum class PrivacyLevel {
    LocalOnly,           // Never send content off-device
    MetadataOnly,        // Only send filename/extension, never content
    ContentExcerpt,      // Send limited content (first N chars, no sensitive patterns)
    FullContent,         // User explicitly allows full content upload
};

/**
 * Chat message role
 */
enum class MessageRole {
    System,
    User,
    Assistant,
};

/**
 * A single message in a chat conversation
 */
struct ChatMessage {
    MessageRole role;
    std::string content;
};

/**
 * Request to an LLM provider
 */
struct LlmRequest {
    std::vector<ChatMessage> messages;
    std::string model;                       // Model identifier
    float temperature{0.7f};
    float top_p{1.0f};
    int max_tokens{256};
    int timeout_ms{30000};                   // 30 second default
    PrivacyLevel privacy_level{PrivacyLevel::MetadataOnly};
    bool allow_content_upload{false};        // Explicit user consent
    int content_excerpt_budget{200};         // Max chars if using excerpt mode
    
    // Retry policy
    int max_retries{3};
    int retry_backoff_base_ms{1000};         // Exponential backoff base
};

/**
 * Token usage information
 */
struct TokenUsage {
    int prompt_tokens{0};
    int completion_tokens{0};
    int total_tokens{0};
};

/**
 * Response from an LLM provider
 */
struct LlmResponse {
    std::string text;
    TokenUsage usage;
    std::string provider_id;
    std::string model_used;
    std::chrono::milliseconds latency{0};
    
    // Error handling
    bool success{false};
    int error_code{0};
    std::string error_message;
    
    // Safety metadata
    bool used_remote_inference{false};
    PrivacyLevel actual_privacy_level{PrivacyLevel::LocalOnly};
};

/**
 * Model information
 */
struct ModelInfo {
    std::string id;
    std::string name;
    std::string description;
    bool is_local{false};
    bool supports_vision{false};
    int64_t parameter_count{0};
    int context_length{0};
};

/**
 * Abstract interface for LLM providers
 * 
 * Daemon Codex uses this interface to support multiple inference backends:
 * - Local: llama.cpp for on-device inference (default, privacy-safe)
 * - OpenAI: Remote API (requires explicit user consent)
 * - Ollama Cloud: Remote Ollama instance (requires explicit user consent)
 */
class IProvider {
public:
    virtual ~IProvider() = default;
    
    /**
     * Unique identifier for this provider (e.g., "local", "openai", "ollama-cloud")
     */
    virtual std::string id() const = 0;
    
    /**
     * Human-readable name for display in UI
     */
    virtual std::string display_name() const = 0;
    
    /**
     * Provider capabilities bitmap
     */
    virtual ProviderCapability capabilities() const = 0;
    
    /**
     * Check if provider is healthy and ready for requests
     */
    virtual HealthStatus health_check() const = 0;
    
    /**
     * List available models (optional - may return empty for some providers)
     */
    virtual std::vector<ModelInfo> list_models() const = 0;
    
    /**
     * Whether this provider requires network access
     */
    virtual bool requires_network() const = 0;
    
    /**
     * Whether this provider is configured and ready for use
     */
    virtual bool is_configured() const = 0;
    
    /**
     * Main chat completion method
     * 
     * SAFETY: Implementations MUST respect the privacy_level in the request.
     * If allow_content_upload is false, remote providers MUST reject requests
     * that would send file content off-device.
     */
    virtual LlmResponse chat(const LlmRequest& request) const = 0;
    
    /**
     * Convenience method for categorization that handles privacy automatically
     */
    virtual LlmResponse categorize(const std::string& filename,
                                   const std::string& filepath,
                                   bool is_directory,
                                   const std::string& consistency_context,
                                   const LlmRequest& base_request) const = 0;
};

/**
 * Type alias for provider pointers
 */
using ProviderPtr = std::shared_ptr<IProvider>;

#endif // I_PROVIDER_HPP
