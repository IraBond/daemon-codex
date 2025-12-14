#include <catch2/catch_test_macros.hpp>
#include "IProvider.hpp"
#include "OpenAIProvider.hpp"
#include "LocalProvider.hpp"
#include "OllamaCloudProvider.hpp"
#include "ProviderFactory.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"
#include <filesystem>
#include <fstream>

TEST_CASE("OpenAIProvider basic properties") {
    OpenAIProvider provider("test_key", "gpt-4o-mini");
    
    REQUIRE(provider.get_name() == "OpenAI");
    REQUIRE(provider.requires_api_key());
    REQUIRE_FALSE(provider.supports_model_listing()); // Not implemented in PR #1
}

TEST_CASE("OpenAIProvider health check") {
    SECTION("With API key") {
        OpenAIProvider provider("test_key", "gpt-4o-mini");
        auto health = provider.check_health();
        REQUIRE(health != ProviderHealth::Unavailable);
    }
    
    SECTION("Without API key") {
        OpenAIProvider provider("", "gpt-4o-mini");
        auto health = provider.check_health();
        REQUIRE(health == ProviderHealth::Unavailable);
    }
}

TEST_CASE("LocalProvider basic properties") {
    LocalProvider provider("/tmp/test_model.gguf");
    
    REQUIRE(provider.get_name() == "Local");
    REQUIRE_FALSE(provider.requires_api_key());
    REQUIRE(provider.supports_model_listing());
}

TEST_CASE("LocalProvider health check") {
    SECTION("With non-existent model") {
        LocalProvider provider("/nonexistent/model.gguf");
        auto health = provider.check_health();
        REQUIRE(health == ProviderHealth::Unavailable);
    }
    
    SECTION("With existing model file") {
        TempDir temp_dir;
        auto model_path = temp_dir.path() / "test_model.gguf";
        {
            std::ofstream file(model_path);
            file << "dummy model content";
        }
        
        LocalProvider provider(model_path.string());
        auto health = provider.check_health();
        REQUIRE(health == ProviderHealth::Healthy);
    }
}

TEST_CASE("LocalProvider model listing") {
    SECTION("With non-existent model") {
        LocalProvider provider("/nonexistent/model.gguf");
        auto models = provider.list_models();
        REQUIRE(models.empty());
    }
    
    SECTION("With existing model file") {
        TempDir temp_dir;
        auto model_path = temp_dir.path() / "test_model.gguf";
        {
            std::ofstream file(model_path);
            file << "dummy model content";
        }
        
        LocalProvider provider(model_path.string());
        auto models = provider.list_models();
        REQUIRE(models.size() == 1);
        REQUIRE(models[0].name == "test_model.gguf");
        REQUIRE(models[0].is_available);
        REQUIRE(models[0].size_bytes > 0);
    }
}

TEST_CASE("OllamaCloudProvider basic properties") {
    OllamaCloudProvider provider("test_key", "https://api.ollama.com", "llama3");
    
    REQUIRE(provider.get_name() == "Ollama Cloud");
    REQUIRE(provider.requires_api_key());
    REQUIRE(provider.supports_model_listing()); // Will be implemented in future PR
}

TEST_CASE("OllamaCloudProvider health check") {
    SECTION("With API key and URL") {
        OllamaCloudProvider provider("test_key", "https://api.ollama.com", "llama3");
        auto health = provider.check_health();
        REQUIRE(health != ProviderHealth::Unavailable);
    }
    
    SECTION("Without API key") {
        OllamaCloudProvider provider("", "https://api.ollama.com", "llama3");
        auto health = provider.check_health();
        REQUIRE(health == ProviderHealth::Unavailable);
    }
    
    SECTION("Without base URL") {
        OllamaCloudProvider provider("test_key", "", "llama3");
        auto health = provider.check_health();
        REQUIRE(health == ProviderHealth::Unavailable);
    }
}

TEST_CASE("OllamaCloudProvider client creation throws") {
    // Stub implementation should throw until fully implemented
    OllamaCloudProvider provider("test_key", "https://api.ollama.com", "llama3");
    REQUIRE_THROWS_AS(provider.create_client(), std::runtime_error);
}

TEST_CASE("ProviderFactory creates correct provider types") {
    SECTION("Creates OpenAI provider") {
        auto provider = ProviderFactory::create_openai_provider("test_key", "gpt-4o-mini");
        REQUIRE(provider != nullptr);
        REQUIRE(provider->get_name() == "OpenAI");
    }
    
    SECTION("Creates Local provider") {
        auto provider = ProviderFactory::create_local_provider("/tmp/model.gguf");
        REQUIRE(provider != nullptr);
        REQUIRE(provider->get_name() == "Local");
    }
    
    SECTION("Creates Ollama Cloud provider") {
        auto provider = ProviderFactory::create_ollama_cloud_provider(
            "test_key", "https://api.ollama.com", "llama3");
        REQUIRE(provider != nullptr);
        REQUIRE(provider->get_name() == "Ollama Cloud");
    }
}
