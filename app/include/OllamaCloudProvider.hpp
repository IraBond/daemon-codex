#pragma once

#include "IProvider.hpp"
#include <string>

/**
 * Provider for Ollama Cloud API
 * 
 * This is a scaffolding/stub implementation for PR #1.
 * Full implementation will be added in a future PR.
 */
class OllamaCloudProvider : public IProvider {
public:
    OllamaCloudProvider(std::string api_key, std::string base_url, std::string model);

    std::string get_name() const override;
    ProviderHealth check_health() const override;
    std::vector<ModelInfo> list_models() const override;
    std::unique_ptr<ILLMClient> create_client() override;
    bool requires_api_key() const override;
    bool supports_model_listing() const override;

private:
    std::string api_key;
    std::string base_url;
    std::string model;
};
