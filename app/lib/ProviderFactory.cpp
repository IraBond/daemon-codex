#include "ProviderFactory.hpp"
#include "OpenAIProvider.hpp"
#include "LocalProvider.hpp"
#include "OllamaCloudProvider.hpp"
#include "Settings.hpp"
#include "Utils.hpp"
#include <cstdlib>

std::unique_ptr<IProvider> ProviderFactory::create_provider_from_settings(const Settings& settings)
{
    LLMChoice choice = settings.get_llm_choice();

    switch (choice) {
        case LLMChoice::Remote: {
            const std::string api_key = settings.get_remote_api_key();
            const std::string model = settings.get_remote_model();
            return create_openai_provider(api_key, model);
        }

        case LLMChoice::Custom: {
            const auto id = settings.get_active_custom_llm_id();
            const CustomLLM custom = settings.find_custom_llm(id);
            if (custom.id.empty() || custom.path.empty()) {
                return nullptr;
            }
            return create_local_provider(custom.path);
        }

        case LLMChoice::Local_3b:
        case LLMChoice::Local_7b: {
            const char* env_var = (choice == LLMChoice::Local_3b)
                ? "LOCAL_LLM_3B_DOWNLOAD_URL"
                : "LOCAL_LLM_7B_DOWNLOAD_URL";

            const char* env_url = std::getenv(env_var);
            if (!env_url) {
                return nullptr;
            }

            std::string model_path = Utils::make_default_path_to_file_from_download_url(env_url);
            return create_local_provider(model_path);
        }

        case LLMChoice::OllamaCloud: {
            // Stub: In future PR, read Ollama config from settings
            // For now, return nullptr as it's not yet fully implemented
            return nullptr;
        }

        case LLMChoice::Unset:
        default:
            return nullptr;
    }
}

std::unique_ptr<IProvider> ProviderFactory::create_openai_provider(
    const std::string& api_key,
    const std::string& model)
{
    return std::make_unique<OpenAIProvider>(api_key, model);
}

std::unique_ptr<IProvider> ProviderFactory::create_local_provider(
    const std::string& model_path)
{
    return std::make_unique<LocalProvider>(model_path);
}

std::unique_ptr<IProvider> ProviderFactory::create_ollama_cloud_provider(
    const std::string& api_key,
    const std::string& base_url,
    const std::string& model)
{
    return std::make_unique<OllamaCloudProvider>(api_key, base_url, model);
}
