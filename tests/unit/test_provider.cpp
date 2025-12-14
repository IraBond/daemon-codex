/*
 * Unit tests for provider architecture
 * Part of Daemon Codex - a safety-first, plan-before-move file organizer
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <catch2/catch_test_macros.hpp>
#include "IProvider.hpp"
#include "LocalProvider.hpp"
#include "OpenAIProvider.hpp"
#include "OllamaCloudProvider.hpp"
#include "ProviderManager.hpp"
#include "TestHelpers.hpp"

#include <memory>
#include <string>

// =============================================================================
// Mock LLM Client for testing
// =============================================================================

class MockLLMClient : public ILLMClient {
public:
    std::string categorize_response = "Documents : Reports";
    std::string complete_response = "Test response";
    bool should_throw = false;
    std::string throw_message;
    
    std::string categorize_file(const std::string& /*file_name*/,
                               const std::string& /*file_path*/,
                               FileType /*file_type*/,
                               const std::string& /*consistency_context*/) override
    {
        if (should_throw) {
            throw std::runtime_error(throw_message);
        }
        return categorize_response;
    }
    
    std::string complete_prompt(const std::string& /*prompt*/,
                               int /*max_tokens*/) override
    {
        if (should_throw) {
            throw std::runtime_error(throw_message);
        }
        return complete_response;
    }
    
    void set_prompt_logging_enabled(bool /*enabled*/) override {}
};

// =============================================================================
// Provider Capability Tests
// =============================================================================

TEST_CASE("ProviderCapability bitwise operations work correctly") {
    auto caps = ProviderCapability::LocalInference | ProviderCapability::Vision;
    
    REQUIRE(has_capability(caps, ProviderCapability::LocalInference));
    REQUIRE(has_capability(caps, ProviderCapability::Vision));
    REQUIRE_FALSE(has_capability(caps, ProviderCapability::RemoteInference));
    REQUIRE_FALSE(has_capability(caps, ProviderCapability::Embeddings));
}

// =============================================================================
// LocalProvider Tests
// =============================================================================

TEST_CASE("LocalProvider reports correct ID and capabilities") {
    LocalProvider provider("");
    
    REQUIRE(provider.id() == "local");
    REQUIRE(provider.display_name() == "Local LLM (on-device)");
    REQUIRE(provider.requires_network() == false);
    REQUIRE(has_capability(provider.capabilities(), ProviderCapability::LocalInference));
    REQUIRE_FALSE(has_capability(provider.capabilities(), ProviderCapability::RemoteInference));
}

TEST_CASE("LocalProvider reports NotConfigured when model path is empty") {
    LocalProvider provider("");
    
    REQUIRE(provider.health_check() == HealthStatus::NotConfigured);
    REQUIRE(provider.is_configured() == false);
}

TEST_CASE("LocalProvider reports Unavailable when model file doesn't exist") {
    LocalProvider provider("/nonexistent/path/model.gguf");
    
    REQUIRE(provider.health_check() == HealthStatus::Unavailable);
    REQUIRE(provider.is_configured() == false);
}

TEST_CASE("LocalProvider with mock client returns successful response") {
    TempDir temp_dir;
    auto model_path = temp_dir.path() / "test.gguf";
    std::ofstream(model_path).put('x');
    
    auto mock_client = std::make_shared<MockLLMClient>();
    mock_client->categorize_response = "Images : Photos";
    
    auto factory = [mock_client](const std::string&) -> std::unique_ptr<ILLMClient> {
        // Create a new MockLLMClient with same settings
        auto client = std::make_unique<MockLLMClient>();
        client->categorize_response = mock_client->categorize_response;
        return client;
    };
    
    LocalProvider provider(model_path.string(), factory);
    
    REQUIRE(provider.is_configured());
    REQUIRE(provider.health_check() == HealthStatus::Healthy);
    
    LlmRequest request;
    request.privacy_level = PrivacyLevel::LocalOnly;
    
    auto response = provider.categorize("photo.jpg", "/home/user/photo.jpg", false, "", request);
    
    REQUIRE(response.success);
    REQUIRE(response.text == "Images : Photos");
    REQUIRE(response.provider_id == "local");
    REQUIRE(response.used_remote_inference == false);
    REQUIRE(response.actual_privacy_level == PrivacyLevel::LocalOnly);
}

// =============================================================================
// OpenAIProvider Tests
// =============================================================================

TEST_CASE("OpenAIProvider reports correct ID and capabilities") {
    OpenAIProvider provider("");
    
    REQUIRE(provider.id() == "openai");
    REQUIRE(provider.display_name() == "OpenAI (ChatGPT)");
    REQUIRE(provider.requires_network() == true);
    REQUIRE(has_capability(provider.capabilities(), ProviderCapability::RemoteInference));
    REQUIRE_FALSE(has_capability(provider.capabilities(), ProviderCapability::LocalInference));
}

TEST_CASE("OpenAIProvider reports NotConfigured when API key is empty") {
    OpenAIProvider provider("");
    
    REQUIRE(provider.health_check() == HealthStatus::NotConfigured);
    REQUIRE(provider.is_configured() == false);
}

TEST_CASE("OpenAIProvider uses default model when not specified") {
    OpenAIProvider provider("test-api-key", "");
    
    REQUIRE(provider.model() == "gpt-4o-mini");
}

TEST_CASE("OpenAIProvider blocks LocalOnly privacy level") {
    auto mock_client = std::make_shared<MockLLMClient>();
    
    auto factory = [mock_client](const std::string&, const std::string&) -> std::unique_ptr<ILLMClient> {
        return std::make_unique<MockLLMClient>();
    };
    
    OpenAIProvider provider("test-api-key", "gpt-4o-mini", factory);
    
    LlmRequest request;
    request.privacy_level = PrivacyLevel::LocalOnly;
    request.messages = {{MessageRole::User, "test"}};
    
    auto response = provider.chat(request);
    
    REQUIRE_FALSE(response.success);
    REQUIRE(response.error_code == 403);
    REQUIRE(response.error_message.find("Privacy control") != std::string::npos);
}

TEST_CASE("OpenAIProvider blocks FullContent without explicit consent") {
    auto factory = [](const std::string&, const std::string&) -> std::unique_ptr<ILLMClient> {
        return std::make_unique<MockLLMClient>();
    };
    
    OpenAIProvider provider("test-api-key", "gpt-4o-mini", factory);
    
    LlmRequest request;
    request.privacy_level = PrivacyLevel::FullContent;
    request.allow_content_upload = false;  // No explicit consent
    request.messages = {{MessageRole::User, "test"}};
    
    auto response = provider.chat(request);
    
    REQUIRE_FALSE(response.success);
    REQUIRE(response.error_code == 403);
}

TEST_CASE("OpenAIProvider allows MetadataOnly requests") {
    auto factory = [](const std::string&, const std::string&) -> std::unique_ptr<ILLMClient> {
        auto mock = std::make_unique<MockLLMClient>();
        mock->complete_response = "Documents : PDFs";
        return mock;
    };
    
    OpenAIProvider provider("test-api-key", "gpt-4o-mini", factory);
    
    LlmRequest request;
    request.privacy_level = PrivacyLevel::MetadataOnly;
    request.messages = {{MessageRole::User, "categorize file.pdf"}};
    
    auto response = provider.chat(request);
    
    REQUIRE(response.success);
    REQUIRE(response.used_remote_inference == true);
}

// =============================================================================
// OllamaCloudProvider Tests
// =============================================================================

TEST_CASE("OllamaCloudProvider reports correct ID and capabilities") {
    OllamaCloudConfig config;
    OllamaCloudProvider provider(config);
    
    REQUIRE(provider.id() == "ollama-cloud");
    REQUIRE(provider.display_name() == "Ollama Cloud");
    REQUIRE(provider.requires_network() == true);
    REQUIRE(has_capability(provider.capabilities(), ProviderCapability::RemoteInference));
}

TEST_CASE("OllamaCloudProvider reports NotConfigured when base_url is empty") {
    OllamaCloudConfig config;
    config.model = "llama3.2";
    OllamaCloudProvider provider(config);
    
    REQUIRE(provider.is_configured() == false);
}

TEST_CASE("OllamaCloudProvider reports NotConfigured when model is empty") {
    OllamaCloudConfig config;
    config.base_url = "https://example.com";
    OllamaCloudProvider provider(config);
    
    REQUIRE(provider.is_configured() == false);
}

TEST_CASE("OllamaCloudProvider blocks LocalOnly requests") {
    OllamaCloudConfig config;
    config.base_url = "https://example.com";
    config.model = "llama3.2";
    
    OllamaCloudProvider provider(config);
    
    LlmRequest request;
    request.privacy_level = PrivacyLevel::LocalOnly;
    request.messages = {{MessageRole::User, "test"}};
    
    auto response = provider.chat(request);
    
    REQUIRE_FALSE(response.success);
    REQUIRE(response.error_code == 403);
    REQUIRE(response.error_message.find("Privacy control") != std::string::npos);
}

TEST_CASE("OllamaCloudProvider uses mock HTTP client") {
    OllamaCloudConfig config;
    config.base_url = "https://example.com";
    config.model = "llama3.2";
    config.max_retries = 0;
    
    auto http_client = [](const std::string& url,
                          const std::string& /*method*/,
                          const std::string& /*body*/,
                          const std::vector<std::pair<std::string, std::string>>& /*headers*/,
                          int /*timeout_ms*/) -> OllamaCloudProvider::HttpResponse {
        OllamaCloudProvider::HttpResponse response;
        if (url.find("/api/chat") != std::string::npos) {
            response.status_code = 200;
            response.body = R"({"message": {"content": "Documents : Invoices"}})";
        } else {
            response.status_code = 404;
        }
        return response;
    };
    
    OllamaCloudProvider provider(config, http_client);
    
    LlmRequest request;
    request.privacy_level = PrivacyLevel::MetadataOnly;
    request.messages = {{MessageRole::User, "categorize invoice.pdf"}};
    
    auto response = provider.chat(request);
    
    REQUIRE(response.success);
    REQUIRE(response.text == "Documents : Invoices");
    REQUIRE(response.used_remote_inference == true);
}

// =============================================================================
// ProviderManager Tests
// =============================================================================

TEST_CASE("ProviderManager defaults to LocalOnly privacy mode") {
    ProviderManager manager;
    
    REQUIRE(manager.privacy_mode() == PrivacyMode::LocalOnly);
    REQUIRE(manager.remote_allowed() == false);
}

TEST_CASE("ProviderManager registers and retrieves providers") {
    ProviderManager manager;
    
    auto local = std::make_shared<LocalProvider>("");
    manager.register_provider(local);
    
    auto retrieved = manager.get_provider("local");
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->id() == "local");
}

TEST_CASE("ProviderManager allows setting local provider as active") {
    ProviderManager manager;
    
    auto local = std::make_shared<LocalProvider>("");
    manager.register_provider(local);
    
    bool result = manager.set_active_provider("local");
    REQUIRE(result == true);
    REQUIRE(manager.active_provider() != nullptr);
    REQUIRE(manager.active_provider()->id() == "local");
}

TEST_CASE("ProviderManager blocks setting remote provider as active in LocalOnly mode") {
    ProviderManager manager;
    
    auto openai = std::make_shared<OpenAIProvider>("test-key");
    manager.register_provider(openai);
    
    // Should fail because privacy mode is LocalOnly
    bool result = manager.set_active_provider("openai");
    REQUIRE(result == false);
    REQUIRE(manager.active_provider() == nullptr);
}

TEST_CASE("ProviderManager allows remote provider when RemoteAllowed with confirmation") {
    ProviderManager manager;
    
    auto openai = std::make_shared<OpenAIProvider>("test-key");
    manager.register_provider(openai);
    
    // Enable remote with user confirmation
    bool mode_result = manager.set_privacy_mode(PrivacyMode::RemoteAllowed, true);
    REQUIRE(mode_result == true);
    REQUIRE(manager.remote_allowed() == true);
    
    // Now setting remote provider should work
    bool provider_result = manager.set_active_provider("openai");
    REQUIRE(provider_result == true);
    REQUIRE(manager.active_provider()->id() == "openai");
}

TEST_CASE("ProviderManager rejects RemoteAllowed without user confirmation") {
    ProviderManager manager;
    
    bool result = manager.set_privacy_mode(PrivacyMode::RemoteAllowed, false);
    REQUIRE(result == false);
    REQUIRE(manager.privacy_mode() == PrivacyMode::LocalOnly);
}

TEST_CASE("ProviderManager deactivates remote provider when switching to LocalOnly") {
    ProviderManager manager;
    
    auto openai = std::make_shared<OpenAIProvider>("test-key");
    manager.register_provider(openai);
    
    // Enable remote and set provider
    manager.set_privacy_mode(PrivacyMode::RemoteAllowed, true);
    manager.set_active_provider("openai");
    REQUIRE(manager.active_provider() != nullptr);
    
    // Switch back to LocalOnly
    manager.set_privacy_mode(PrivacyMode::LocalOnly, false);
    
    // Remote provider should be deactivated
    REQUIRE(manager.active_provider() == nullptr);
}

TEST_CASE("ProviderManager validates request privacy level") {
    ProviderManager manager;
    
    auto openai = std::make_shared<OpenAIProvider>("test-key");
    manager.register_provider(openai);
    manager.set_privacy_mode(PrivacyMode::RemoteAllowed, true);
    manager.set_active_provider("openai");
    
    LlmRequest request;
    request.privacy_level = PrivacyLevel::LocalOnly;
    
    auto error = manager.validate_request(request);
    REQUIRE(error.has_value());
    REQUIRE(error->find("LocalOnly") != std::string::npos);
}

TEST_CASE("ProviderManager chat returns error when no provider active") {
    ProviderManager manager;
    
    LlmRequest request;
    auto response = manager.chat(request);
    
    REQUIRE_FALSE(response.success);
    REQUIRE(response.error_message.find("No active provider") != std::string::npos);
}

TEST_CASE("ProviderManager allowed_providers respects privacy mode") {
    ProviderManager manager;
    
    auto local = std::make_shared<LocalProvider>("");
    auto openai = std::make_shared<OpenAIProvider>("test-key");
    
    manager.register_provider(local);
    manager.register_provider(openai);
    
    // In LocalOnly mode, only local provider should be allowed
    auto allowed = manager.allowed_providers();
    REQUIRE(allowed.size() == 1);
    REQUIRE(allowed[0]->id() == "local");
    
    // Enable remote
    manager.set_privacy_mode(PrivacyMode::RemoteAllowed, true);
    
    allowed = manager.allowed_providers();
    REQUIRE(allowed.size() == 2);
}

// =============================================================================
// Privacy Level Tests
// =============================================================================

TEST_CASE("LlmRequest defaults to MetadataOnly privacy level") {
    LlmRequest request;
    
    REQUIRE(request.privacy_level == PrivacyLevel::MetadataOnly);
    REQUIRE(request.allow_content_upload == false);
}

TEST_CASE("LlmRequest has sensible defaults for timeouts and retries") {
    LlmRequest request;
    
    REQUIRE(request.timeout_ms == 30000);
    REQUIRE(request.max_retries == 3);
    REQUIRE(request.retry_backoff_base_ms == 1000);
}

// =============================================================================
// LlmResponse Tests
// =============================================================================

TEST_CASE("LlmResponse tracks remote inference usage") {
    LlmResponse response;
    response.used_remote_inference = true;
    response.actual_privacy_level = PrivacyLevel::MetadataOnly;
    
    REQUIRE(response.used_remote_inference == true);
    REQUIRE(response.actual_privacy_level == PrivacyLevel::MetadataOnly);
}

// =============================================================================
// Config Parsing Tests
// =============================================================================

TEST_CASE("OllamaCloudConfig has sensible defaults") {
    OllamaCloudConfig config;
    
    REQUIRE(config.timeout_ms == 30000);
    REQUIRE(config.max_retries == 3);
    REQUIRE(config.retry_backoff_base_ms == 1000);
    REQUIRE(config.base_url.empty());
    REQUIRE(config.api_key.empty());
    REQUIRE(config.model.empty());
}
