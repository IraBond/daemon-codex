#include "LocalProvider.hpp"
#include <filesystem>
#include <stdexcept>

LocalProvider::LocalProvider(std::string model_path)
    : model_path(std::move(model_path))
{
}

std::string LocalProvider::get_name() const
{
    return "Local";
}

ProviderHealth LocalProvider::check_health() const
{
    // Check if model file exists
    if (!std::filesystem::exists(model_path)) {
        return ProviderHealth::Unavailable;
    }
    
    // Check if file is readable
    std::filesystem::file_status status = std::filesystem::status(model_path);
    if ((status.permissions() & std::filesystem::perms::owner_read) == std::filesystem::perms::none) {
        return ProviderHealth::Unavailable;
    }
    
    return ProviderHealth::Healthy;
}

std::vector<ModelInfo> LocalProvider::list_models() const
{
    // For local provider, we return the single configured model
    std::vector<ModelInfo> models;
    
    if (std::filesystem::exists(model_path)) {
        ModelInfo info;
        info.id = model_path;
        info.name = std::filesystem::path(model_path).filename().string();
        info.description = "Local GGUF model";
        
        std::error_code ec;
        info.size_bytes = std::filesystem::file_size(model_path, ec);
        if (ec) {
            info.size_bytes = 0;
        }
        
        info.is_available = true;
        models.push_back(info);
    }
    
    return models;
}

std::unique_ptr<ILLMClient> LocalProvider::create_client()
{
    if (!std::filesystem::exists(model_path)) {
        throw std::runtime_error("Local model file not found: " + model_path);
    }
    
    return std::make_unique<LocalLLMClient>(model_path);
}

bool LocalProvider::requires_api_key() const
{
    return false;
}

bool LocalProvider::supports_model_listing() const
{
    return true;
}
