#pragma once

#include "ILLMClient.hpp"
#include <memory>
#include <string>
#include <vector>

/**
 * Provider health status
 */
enum class ProviderHealth {
    Healthy,
    Degraded,
    Unavailable,
    Unknown
};

/**
 * Information about an available model
 */
struct ModelInfo {
    std::string id;
    std::string name;
    std::string description;
    size_t size_bytes{0};
    bool is_available{true};
};

/**
 * Base interface for LLM providers (Local, Remote, Ollama Cloud, etc.)
 * 
 * Providers abstract the underlying implementation and provide
 * capabilities like model listing, health checks, and client creation.
 */
class IProvider {
public:
    virtual ~IProvider() = default;

    /**
     * Get the name of this provider (e.g., "OpenAI", "Local", "Ollama Cloud")
     */
    virtual std::string get_name() const = 0;

    /**
     * Check the health/availability of this provider
     */
    virtual ProviderHealth check_health() const = 0;

    /**
     * List available models for this provider
     * May return empty vector if listing is not supported
     */
    virtual std::vector<ModelInfo> list_models() const = 0;

    /**
     * Create an LLM client for this provider
     * @throws std::runtime_error if client cannot be created
     */
    virtual std::unique_ptr<ILLMClient> create_client() = 0;

    /**
     * Check if this provider requires an API key
     */
    virtual bool requires_api_key() const = 0;

    /**
     * Check if this provider supports model listing
     */
    virtual bool supports_model_listing() const = 0;
};
