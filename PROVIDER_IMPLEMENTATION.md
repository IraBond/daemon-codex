# Provider Foundation Implementation (PR #1)

## Overview

This PR establishes a provider abstraction layer for AI File Sorter, preparing for Ollama Cloud integration while maintaining backward compatibility with existing LLM implementations.

## Architecture

### Core Components

#### IProvider Interface (`app/include/IProvider.hpp`)
Base interface for all LLM providers with the following capabilities:
- **Health Checking**: `check_health()` returns provider availability status
- **Model Listing**: `list_models()` enumerates available models (when supported)
- **Client Creation**: `create_client()` instantiates an ILLMClient for the provider
- **Metadata**: `get_name()`, `requires_api_key()`, `supports_model_listing()`

#### Provider Implementations

1. **OpenAIProvider** (`app/include/OpenAIProvider.hpp`, `app/lib/OpenAIProvider.cpp`)
   - Wraps existing `LLMClient` for OpenAI/ChatGPT API
   - Requires API key
   - Health check validates API key presence
   - Model listing not implemented in this PR (stub returns empty vector)

2. **LocalProvider** (`app/include/LocalProvider.hpp`, `app/lib/LocalProvider.cpp`)
   - Wraps existing `LocalLLMClient` for local GGUF models
   - No API key required
   - Health check verifies model file existence and readability
   - Model listing returns single configured model with metadata

3. **OllamaCloudProvider** (`app/include/OllamaCloudProvider.hpp`, `app/lib/OllamaCloudProvider.cpp`)
   - **STUB IMPLEMENTATION** - scaffolding only for PR #1
   - Requires API key and base URL
   - Full implementation deferred to future PR
   - `create_client()` throws runtime_error until implemented

#### ProviderFactory (`app/include/ProviderFactory.hpp`, `app/lib/ProviderFactory.cpp`)
Factory class for creating providers:
- `create_provider_from_settings()`: Creates provider based on Settings configuration
- `create_openai_provider()`, `create_local_provider()`, `create_ollama_cloud_provider()`: Static factory methods

### Type Changes

#### Types.hpp
Added `OllamaCloud` to the `LLMChoice` enum:
```cpp
enum class LLMChoice {
    Unset,
    Remote,
    Local_3b,
    Local_7b,
    Custom,
    OllamaCloud  // NEW
};
```

## Tests

Unit tests are provided in `tests/unit/test_provider.cpp` covering:
- Provider basic properties (name, API key requirements, model listing support)
- Health check behavior for each provider type
- Model listing functionality
- Factory pattern usage
- Ollama Cloud stub behavior

### Running Tests

Tests use the Catch2 framework. To build and run:

```bash
cd tests
# Build test binary with Catch2
g++ -std=c++20 -I../app/include -I../external/Catch2/src \
    unit/test_provider.cpp \
    ../app/lib/OpenAIProvider.cpp \
    ../app/lib/LocalProvider.cpp \
    ../app/lib/OllamaCloudProvider.cpp \
    ../app/lib/ProviderFactory.cpp \
    -o build/test_provider
    
# Run tests
./build/test_provider
```

Note: Full integration may require additional dependencies and mock implementations.

## Non-Goals (This PR)

This PR intentionally does NOT include:
- UI changes for provider selection
- Settings persistence for Ollama Cloud configuration
- Actual Ollama Cloud API client implementation
- Migration of existing `MainApp::make_llm_client()` to use ProviderFactory
- Content-based sorting expansion

These features will be addressed in subsequent PRs.

## Future Work (PR #2+)

1. **Ollama Cloud Client Implementation**
   - Implement `OllamaCloudClient` class implementing `ILLMClient`
   - Add HTTP client for Ollama API endpoints
   - Implement model listing via `/api/tags` endpoint
   - Add health check via `/api/version` endpoint

2. **Settings Integration**
   - Add Ollama Cloud settings to Settings class
   - Add UI for Ollama provider configuration
   - Update LLM selection dialog

3. **MainApp Integration**
   - Migrate `MainApp::make_llm_client()` to use ProviderFactory
   - Add provider-based configuration flow

## Build Notes

### Dependencies
All new files integrate with existing build system:
- CMakeLists.txt auto-includes all `lib/*.cpp` files via glob pattern
- No new external dependencies added
- Uses existing C++20 standard library features

### Replit Build
The provider abstraction is header-based and should work on Replit without modifications.

### macOS Build Path
Standard Makefile targets remain unchanged. New files automatically included.

## Compatibility

- ✅ Backward compatible with existing LLMChoice values
- ✅ No changes to existing ILLMClient implementations
- ✅ Existing code continues to work unchanged
- ✅ New provider abstraction is optional/additive

## Security Considerations

- API keys stored in Settings (existing mechanism)
- No new credential storage mechanisms
- Provider health checks don't expose sensitive information
- Ollama Cloud stub safely throws before attempting network calls
