#include "OllamaCloudProvider.hpp"
#include <stdexcept>

OllamaCloudProvider::OllamaCloudProvider(std::string api_key, std::string base_url, std::string model)
    : api_key(std::move(api_key)), 
      base_url(std::move(base_url)), 
      model(std::move(model))
{
}

std::string OllamaCloudProvider::get_name() const
{
    return "Ollama Cloud";
}

ProviderHealth OllamaCloudProvider::check_health() const
{
    // Stub implementation - health check not yet implemented
    // Future PR will add actual health check via Ollama API
    if (api_key.empty() || base_url.empty()) {
        return ProviderHealth::Unavailable;
    }
    return ProviderHealth::Unknown;
}

std::vector<ModelInfo> OllamaCloudProvider::list_models() const
{
    // Stub implementation - model listing not yet implemented
    // Future PR will query Ollama API for available models
    return {};
}

std::unique_ptr<ILLMClient> OllamaCloudProvider::create_client()
{
    // Stub implementation - client creation not yet implemented
    // Future PR will implement OllamaCloudClient that uses Ollama API
    throw std::runtime_error("Ollama Cloud provider is not yet fully implemented");
}

bool OllamaCloudProvider::requires_api_key() const
{
    return true;
}

bool OllamaCloudProvider::supports_model_listing() const
{
    return true; // Will be implemented in future PR
}
