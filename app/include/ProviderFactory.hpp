#pragma once

#include "IProvider.hpp"
#include "Types.hpp"
#include <memory>
#include <string>

class Settings;

/**
 * Factory for creating LLM providers based on configuration
 */
class ProviderFactory {
public:
    /**
     * Create a provider based on the current settings
     * @param settings Application settings containing provider configuration
     * @return Provider instance, or nullptr if no valid provider is configured
     */
    static std::unique_ptr<IProvider> create_provider_from_settings(const Settings& settings);

    /**
     * Create an OpenAI provider
     */
    static std::unique_ptr<IProvider> create_openai_provider(
        const std::string& api_key, 
        const std::string& model);

    /**
     * Create a local provider
     */
    static std::unique_ptr<IProvider> create_local_provider(
        const std::string& model_path);

    /**
     * Create an Ollama Cloud provider
     */
    static std::unique_ptr<IProvider> create_ollama_cloud_provider(
        const std::string& api_key,
        const std::string& base_url,
        const std::string& model);
};
