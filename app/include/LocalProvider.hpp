#pragma once

#include "IProvider.hpp"
#include "LocalLLMClient.hpp"
#include <string>

/**
 * Provider for local LLM models (llama.cpp based)
 */
class LocalProvider : public IProvider {
public:
    explicit LocalProvider(std::string model_path);

    std::string get_name() const override;
    ProviderHealth check_health() const override;
    std::vector<ModelInfo> list_models() const override;
    std::unique_ptr<ILLMClient> create_client() override;
    bool requires_api_key() const override;
    bool supports_model_listing() const override;

private:
    std::string model_path;
};
