# Daemon Codex

**Daemon Codex** is a fork of [AI File Sorter](https://github.com/hyperfield/ai-file-sorter) that enhances the original with a **plan-first, reversible** approach and a clean provider abstraction for multiple LLM backends.

## Project Goals

Daemon Codex is designed around the principle that **file organization should be safe and reversible**:

1. **No silent destructive actions** - All file moves are planned, reviewed, and reversible
2. **Privacy by default** - Local inference is the default; remote providers require explicit opt-in
3. **Auditable behavior** - Every categorization decision is logged with reasoning
4. **Undo support** - Persistent undo plans survive across sessions

## Safety Principles

### Non-Negotiable Rules

1. **Default to Local** - The app defaults to local LLM inference using llama.cpp
2. **Never exfiltrate silently** - Remote providers are opt-in and require per-session confirmation
3. **Plan before move** - Files are never moved without an explicit "From → To" plan
4. **Reversible by default** - Every sort operation creates an undo plan
5. **Bounded provider calls** - Timeouts, retries with backoff, and token limits are enforced

### Privacy Levels

| Level | Description | Use Case |
|-------|-------------|----------|
| `LocalOnly` | Never send data off-device | Default for local providers |
| `MetadataOnly` | Only filenames/extensions sent | Default for remote providers |
| `ContentExcerpt` | First N chars + metadata | Opt-in for better categorization |
| `FullContent` | All content may be sent | Requires explicit user consent |

## Provider Architecture

Daemon Codex uses a clean provider abstraction to support multiple LLM backends:

```
┌─────────────────────────────────────────────────────────────┐
│                     ProviderManager                         │
│  ┌─────────────┐  ┌─────────────────────────────────────┐  │
│  │ Privacy     │  │ Active Provider Selection            │  │
│  │ Gate        │  │ - LocalProvider (default)           │  │
│  │ - LocalOnly │  │ - OpenAIProvider                    │  │
│  │ - Remote OK │  │ - OllamaCloudProvider               │  │
│  └─────────────┘  └─────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
           ┌──────────────────┼──────────────────┐
           ▼                  ▼                  ▼
    ┌────────────┐     ┌────────────┐     ┌────────────┐
    │ Local      │     │ OpenAI     │     │ Ollama     │
    │ Provider   │     │ Provider   │     │ Cloud      │
    │            │     │            │     │ Provider   │
    │ llama.cpp  │     │ HTTPS API  │     │ HTTPS API  │
    └────────────┘     └────────────┘     └────────────┘
```

### IProvider Interface

All providers implement the `IProvider` interface:

```cpp
class IProvider {
public:
    virtual std::string id() const = 0;
    virtual std::string display_name() const = 0;
    virtual ProviderCapability capabilities() const = 0;
    virtual HealthStatus health_check() const = 0;
    virtual std::vector<ModelInfo> list_models() const = 0;
    virtual bool requires_network() const = 0;
    virtual bool is_configured() const = 0;
    virtual LlmResponse chat(const LlmRequest& request) const = 0;
    virtual LlmResponse categorize(...) const = 0;
};
```

### Request/Response Types

```cpp
struct LlmRequest {
    std::vector<ChatMessage> messages;
    std::string model;
    float temperature{0.7f};
    int max_tokens{256};
    int timeout_ms{30000};
    PrivacyLevel privacy_level{PrivacyLevel::MetadataOnly};
    bool allow_content_upload{false};
    int max_retries{3};
};

struct LlmResponse {
    std::string text;
    TokenUsage usage;
    std::chrono::milliseconds latency{0};
    bool success{false};
    bool used_remote_inference{false};
    PrivacyLevel actual_privacy_level;
};
```

## Adding a New Provider

1. Create a header in `app/include/YourProvider.hpp`:
   ```cpp
   #include "IProvider.hpp"
   
   class YourProvider : public IProvider {
   public:
       // Implement all virtual methods
   };
   ```

2. Create implementation in `app/lib/YourProvider.cpp`

3. Register in `ProviderManager`:
   ```cpp
   manager.register_provider(std::make_shared<YourProvider>(config));
   ```

4. Add tests in `tests/unit/test_provider.cpp`

### Provider Implementation Checklist

- [ ] Implement `id()` with unique identifier
- [ ] Implement `requires_network()` (true for remote, false for local)
- [ ] Respect `PrivacyLevel` in all requests
- [ ] Block `LocalOnly` requests if remote
- [ ] Require explicit consent for `FullContent`
- [ ] Implement timeout and retry logic
- [ ] Log all operations via `Logger::get_logger("core_logger")`

## Enabling Remote Inference Safely

### For Users

1. Go to Settings → LLM Provider
2. Select a remote provider (OpenAI, Ollama Cloud)
3. Read and acknowledge the privacy warning
4. Configure API key and endpoint
5. Choose privacy level (MetadataOnly recommended)

### For Developers

```cpp
ProviderManager manager;

// Register providers
manager.register_provider(std::make_shared<LocalProvider>(model_path));
manager.register_provider(std::make_shared<OpenAIProvider>(api_key));

// Enable remote with user confirmation
if (user_confirmed_remote) {
    manager.set_privacy_mode(PrivacyMode::RemoteAllowed, true);
    manager.set_active_provider("openai");
}
```

## Plan and Undo Files

### Sort Plan Format

```json
{
    "version": 1,
    "created_at_utc": "2024-01-15T12:00:00.000Z",
    "provider_id": "local",
    "model": "llama-3.2-3b",
    "privacy_level": "LocalOnly",
    "entries": [
        {
            "source": "/home/user/Downloads/invoice.pdf",
            "destination": "/home/user/Downloads/Documents/Invoices/invoice.pdf",
            "category": "Documents",
            "subcategory": "Invoices",
            "reason": "File extension .pdf and name contains 'invoice'"
        }
    ]
}
```

### Undo Plan Format

```json
{
    "version": 1,
    "base_dir": "/home/user/Downloads",
    "created_at_utc": "2024-01-15T12:00:00.000Z",
    "entries": [
        {
            "source": "/home/user/Downloads/invoice.pdf",
            "destination": "/home/user/Downloads/Documents/Invoices/invoice.pdf",
            "size": 12345,
            "mtime": 1705320000
        }
    ]
}
```

## Development Notes

### Building on macOS (Recommended)

```bash
# Install dependencies
brew install qt curl jsoncpp sqlite openssl fmt spdlog cmake git

# Configure paths
export PATH="$(brew --prefix)/opt/qt/bin:$PATH"

# Clone and build
git clone https://github.com/IraBond/daemon-codex.git
cd daemon-codex
git submodule update --init --recursive

# Build llama runtime
./app/scripts/build_llama_macos.sh

# Build the app
cd app
make -j4
```

### Replit Limitations

Due to toolchain constraints, the full Qt6/OpenGL GUI cannot be built on Replit. Development and testing should be done on local macOS or Linux environments.

For Replit, focus on:
- Code review and architecture changes
- Unit tests (if buildable without Qt GUI)
- Documentation updates

## Future Work

### PR#2: Provider Selection in UI
- Dropdown for provider selection (Local / OpenAI / Ollama Cloud)
- Privacy warnings and confirmation dialogs
- "Allow content upload" toggle

### PR#3: Content-Based Sorting
- Filetype-specific extractors (PDF, DOCX, images)
- Chunking strategies for large files
- Stable prompt templates with versioning

### PR#4: Stronger Undo Semantics
- Conflict resolution for modified files
- Partial undo handling
- Plan file signatures and integrity checks

## Upstream Syncing

This is a fork of [hyperfield/ai-file-sorter](https://github.com/hyperfield/ai-file-sorter). To sync with upstream:

```bash
git remote add upstream https://github.com/hyperfield/ai-file-sorter.git
git fetch upstream
git merge upstream/main
```

If the fork network needs to be detached for private visibility, this can be done through GitHub support.

## License

This project is licensed under the GNU Affero General Public License (AGPL-3.0). See [LICENSE](../LICENSE) for details.

Any contributions must comply with this license.
