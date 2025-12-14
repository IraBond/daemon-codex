#pragma once

#include "IProvider.hpp"
#include "LLMClient.hpp"
#include <string>

/**
 * Provider for OpenAI/ChatGPT API
 */
class OpenAIProvider : public IProvider {
public:
    OpenAIProvider(std::string api_key, std::string model);

    std::string get_name() const override;
    ProviderHealth check_health() const override;
    std::vector<ModelInfo> list_models() const override;
    std::unique_ptr<ILLMClient> create_client() override;
    bool requires_api_key() const override;
    bool supports_model_listing() const override;

private:
    std::string api_key;
    std::string model;
};
