#include "OpenAIProvider.hpp"
#include "CategorizationSession.hpp"
#include <stdexcept>

OpenAIProvider::OpenAIProvider(std::string api_key, std::string model)
    : api_key(std::move(api_key)), model(std::move(model))
{
}

std::string OpenAIProvider::get_name() const
{
    return "OpenAI";
}

ProviderHealth OpenAIProvider::check_health() const
{
    // Basic check - if we have an API key, assume available
    // A more sophisticated check would make a test API call
    if (api_key.empty()) {
        return ProviderHealth::Unavailable;
    }
    return ProviderHealth::Unknown;
}

std::vector<ModelInfo> OpenAIProvider::list_models() const
{
    // Model listing not implemented for OpenAI in this PR
    // Would require /v1/models API call
    return {};
}

std::unique_ptr<ILLMClient> OpenAIProvider::create_client()
{
    if (api_key.empty()) {
        throw std::runtime_error("OpenAI API key is missing");
    }
    
    CategorizationSession session(api_key, model);
    return std::make_unique<LLMClient>(session.create_llm_client());
}

bool OpenAIProvider::requires_api_key() const
{
    return true;
}

bool OpenAIProvider::supports_model_listing() const
{
    return false; // Not implemented in this PR
}
