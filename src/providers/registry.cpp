#include "llmcli/providers/registry.hpp"
#include "llmcli/config.hpp"
#include "llmcli/providers/gemini.hpp"
#include "llmcli/providers/anthropic.hpp"
#include "llmcli/providers/openai_compatible.hpp"
#include "llmcli/providers/llamacpp.hpp"
#include "llmcli/providers/ollama.hpp"
#include "llmcli/utils/env.hpp"
#include <algorithm>
#include <iostream>

namespace llmcli::providers {

static const std::vector<ModelPreset> MODEL_PRESETS = {
    // Local
    {1, "llamacpp/default", "Local", "llama.cpp server na porta 8080 (padrão local)"},
    {2, "llamacpp/qwen2.5-coder", "Local", "llama.cpp especializado em código"},
    {3, "ollama/qwen2.5-coder:7b", "Local", "Ollama Qwen 2.5 Coder 7B"},
    {4, "ollama/deepseek-r1:latest", "Local", "Ollama DeepSeek R1 Raciocínio"},
    {5, "ollama/llama3.3:latest", "Local", "Ollama Llama 3.3 70B / 8B"},
    {6, "lmstudio/default", "Local", "LM Studio server local (porta 1234)"},
    {7, "vllm/default", "Local", "vLLM server local de alta performance (porta 8000)"},

    // Google Gemini
    {8, "gemini/gemini-2.5-flash", "Google", "Gemini 2.5 Flash (Ultrarrápido, multimodal)"},
    {9, "gemini/gemini-2.5-pro", "Google", "Gemini 2.5 Pro (Raciocínio complexo e código)"},

    // OpenAI
    {10, "gpt/codex", "OpenAI", "Codex / GPT Especializado em Código"},
    {11, "openai/gpt-4o", "OpenAI", "GPT-4o Omnimodel topo de linha"},
    {12, "openai/gpt-4o-mini", "OpenAI", "GPT-4o Mini leve e econômico"},
    {13, "openai/o3-mini", "OpenAI", "OpenAI o3-mini Raciocínio Avançado"},
    {14, "openai/o1", "OpenAI", "OpenAI o1 Raciocínio Profundo"},

    // Anthropic
    {15, "anthropic/claude-3-7-sonnet-20250219", "Anthropic", "Claude 3.7 Sonnet (Estado da arte em código)"},
    {16, "anthropic/claude-3-5-haiku-20241022", "Anthropic", "Claude 3.5 Haiku rápido e ágil"},

    // DeepSeek
    {17, "deepseek/deepseek-chat", "DeepSeek", "DeepSeek V3 Chat/Coder"},
    {18, "deepseek/deepseek-reasoner", "DeepSeek", "DeepSeek R1 Reasoner Oficial"},

    // Groq & OpenRouter
    {19, "groq/llama-3.3-70b-versatile", "Groq", "Groq LPU ultraveloz (~300 tokens/s)"},
    {20, "openrouter/anthropic/claude-3.5-sonnet", "OpenRouter", "OpenRouter Hub Multi-LLM"}
};

std::vector<ModelPreset> ProviderRegistry::get_model_presets() {
    return MODEL_PRESETS;
}

std::string ProviderRegistry::resolve_model_by_id_or_name(const std::string& input_str) {
    std::string clean = input_str;
    auto f = clean.find_first_not_of(" \t\r\n");
    auto l = clean.find_last_not_of(" \t\r\n");
    if (f == std::string::npos || l == std::string::npos) return clean;
    clean = clean.substr(f, l - f + 1);

    // Number check
    bool is_num = true;
    for (char c : clean) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            is_num = false;
            break;
        }
    }
    if (is_num) {
        try {
            int idx = std::stoi(clean);
            for (const auto& p : MODEL_PRESETS) {
                if (p.id == idx) return p.name;
            }
        } catch (...) {}
    }

    // Exact or substring match
    std::string clean_lower = clean;
    std::transform(clean_lower.begin(), clean_lower.end(), clean_lower.begin(), [](unsigned char c){ return std::tolower(c); });

    for (const auto& p : MODEL_PRESETS) {
        std::string p_lower = p.name;
        std::transform(p_lower.begin(), p_lower.end(), p_lower.begin(), [](unsigned char c){ return std::tolower(c); });
        if (p_lower == clean_lower) return p.name;
    }

    for (const auto& p : MODEL_PRESETS) {
        std::string p_lower = p.name;
        std::transform(p_lower.begin(), p_lower.end(), p_lower.begin(), [](unsigned char c){ return std::tolower(c); });
        if (p_lower.find(clean_lower) != std::string::npos) return p.name;
    }

    return clean;
}

std::shared_ptr<LLMProvider> ProviderRegistry::create_provider(const std::string& model_string) {
    Config& cfg = get_config();
    std::string model_str = model_string.empty() ? (cfg.active_model.empty() ? cfg.default_model : cfg.active_model) : model_string;

    std::string provider_type;
    std::string model_name;

    auto slash_pos = model_str.find('/');
    if (slash_pos != std::string::npos) {
        provider_type = model_str.substr(0, slash_pos);
        model_name = model_str.substr(slash_pos + 1);
        std::transform(provider_type.begin(), provider_type.end(), provider_type.begin(), [](unsigned char c){ return std::tolower(c); });
        provider_type.erase(std::remove(provider_type.begin(), provider_type.end(), '.'), provider_type.end());
        provider_type.erase(std::remove(provider_type.begin(), provider_type.end(), '-'), provider_type.end());
    } else {
        std::string raw = model_str;
        std::transform(raw.begin(), raw.end(), raw.begin(), [](unsigned char c){ return std::tolower(c); });
        // Resolve provider aliases before fuzzy inference: "ollama" contains
        // "llama" and was previously routed to the llama.cpp provider.
        if (raw == "ollama") {
            provider_type = "ollama";
            model_name = "qwen2.5-coder:latest";
        } else if (raw == "llama" || raw == "llamacpp" || raw == "llama.cpp" || raw == "llama_cpp") {
            provider_type = "llamacpp";
            model_name = "default";
        } else if (raw == "lmstudio" || raw == "lm-studio") {
            provider_type = "lmstudio";
            model_name = "default";
        } else if (raw == "vllm") {
            provider_type = "vllm";
            model_name = "default";
        } else if (raw.find("gemini") != std::string::npos) {
            provider_type = "gemini";
            model_name = model_str;
        } else if (raw.find("claude") != std::string::npos) {
            provider_type = "anthropic";
            model_name = model_str;
        } else if (raw.find("gpt") != std::string::npos || raw.find("o1") != std::string::npos || raw.find("o3") != std::string::npos || raw.find("codex") != std::string::npos) {
            provider_type = "openai";
            model_name = model_str;
        } else if (raw.find("deepseek") != std::string::npos) {
            provider_type = "deepseek";
            model_name = model_str;
        } else if (raw.find("llama.cpp") != std::string::npos || raw.find("llamacpp") != std::string::npos) {
            provider_type = "llamacpp";
            model_name = "default";
        } else if (raw.find("ollama") != std::string::npos) {
            provider_type = "ollama";
            model_name = "qwen2.5-coder:latest";
        } else {
            provider_type = "gemini";
            model_name = model_str;
        }
    }

    if (provider_type == "llamacpp" || provider_type == "llama_cpp" || provider_type == "llama") {
        return std::make_shared<LlamaCppProvider>(model_name, cfg.local_endpoints.llamacpp);
    }
    if (provider_type == "ollama") {
        return std::make_shared<OllamaProvider>(model_name, cfg.local_endpoints.ollama);
    }
    if (provider_type == "lmstudio") {
        return std::make_shared<OpenAICompatibleProvider>(model_name, cfg.local_endpoints.lmstudio, "lm-studio");
    }
    if (provider_type == "vllm") {
        return std::make_shared<OpenAICompatibleProvider>(model_name, cfg.local_endpoints.vllm, "vllm");
    }
    if (provider_type == "gemini") {
        return std::make_shared<GeminiProvider>(model_name, utils::EnvLoader::get_env("GEMINI_API_KEY", ""));
    }
    if (provider_type == "anthropic") {
        return std::make_shared<AnthropicProvider>(model_name, utils::EnvLoader::get_env("ANTHROPIC_API_KEY", ""));
    }
    if (provider_type == "openai" || provider_type == "gpt" || provider_type == "codex") {
        std::string target_model = (model_name == "codex" || model_name == "default") ? utils::EnvLoader::get_env("OPENAI_CODEX_MODEL", "gpt-4o") : model_name;
        return std::make_shared<OpenAICompatibleProvider>(
            target_model,
            utils::EnvLoader::get_env("OPENAI_BASE_URL", "https://api.openai.com/v1"),
            utils::EnvLoader::get_env("OPENAI_API_KEY", "")
        );
    }
    if (provider_type == "deepseek") {
        return std::make_shared<OpenAICompatibleProvider>(
            model_name,
            "https://api.deepseek.com",
            utils::EnvLoader::get_env("DEEPSEEK_API_KEY", "")
        );
    }
    if (provider_type == "groq") {
        return std::make_shared<OpenAICompatibleProvider>(
            model_name,
            "https://api.groq.com/openai/v1",
            utils::EnvLoader::get_env("GROQ_API_KEY", "")
        );
    }
    if (provider_type == "openrouter") {
        return std::make_shared<OpenAICompatibleProvider>(
            model_name,
            "https://openrouter.ai/api/v1",
            utils::EnvLoader::get_env("OPENROUTER_API_KEY", ""),
            std::map<std::string, std::string>{{"HTTP-Referer", "https://github.com/llmCli"}, {"X-Title", "llmCli"}}
        );
    }

    return std::make_shared<OpenAICompatibleProvider>(
        model_name,
        "https://api.openai.com/v1",
        utils::EnvLoader::get_env("OPENAI_API_KEY", "")
    );
}

std::vector<ProviderHealthItem> ProviderRegistry::get_status_overview() {
    Config& cfg = get_config();
    std::vector<ProviderHealthItem> results;

    // 1. llama.cpp
    LlamaCppProvider p_llama("default", cfg.local_endpoints.llamacpp);
    auto [ok_llama, msg_llama] = p_llama.check_health();
    auto models_llama = ok_llama ? p_llama.list_available_models() : std::vector<std::string>();
    results.push_back({
        "llama.cpp (Local)",
        cfg.local_endpoints.llamacpp,
        ok_llama ? "ONLINE" : "OFFLINE",
        msg_llama,
        models_llama,
        "llamacpp/default"
    });

    // 2. Ollama
    OllamaProvider p_ollama("default", cfg.local_endpoints.ollama);
    auto [ok_ollama, msg_ollama] = p_ollama.check_health();
    auto models_ollama = ok_ollama ? p_ollama.list_available_models() : std::vector<std::string>();
    results.push_back({
        "Ollama (Local)",
        cfg.local_endpoints.ollama,
        ok_ollama ? "ONLINE" : "OFFLINE",
        msg_ollama,
        models_ollama,
        "ollama/qwen2.5-coder:latest"
    });

    // 3. LM Studio
    OpenAICompatibleProvider p_lms("default", cfg.local_endpoints.lmstudio);
    auto [ok_lms, msg_lms] = p_lms.check_health();
    results.push_back({
        "LM Studio (Local)",
        cfg.local_endpoints.lmstudio,
        ok_lms ? "ONLINE" : "OFFLINE",
        msg_lms,
        {},
        "lmstudio/local-model"
    });

    // 4. Cloud providers
    std::vector<std::tuple<std::string, std::string, std::string>> cloud_candidates = {
        {"Google Gemini (Nuvem)", "GEMINI_API_KEY", "gemini/gemini-2.5-flash"},
        {"Anthropic Claude (Nuvem)", "ANTHROPIC_API_KEY", "anthropic/claude-3-7-sonnet-20250219"},
        {"OpenAI (Nuvem)", "OPENAI_API_KEY", "openai/gpt-4o"},
        {"DeepSeek (Nuvem)", "DEEPSEEK_API_KEY", "deepseek/deepseek-chat"},
        {"Groq (Nuvem)", "GROQ_API_KEY", "groq/llama-3.3-70b-versatile"},
        {"OpenRouter (Nuvem)", "OPENROUTER_API_KEY", "openrouter/anthropic/claude-3.5-sonnet"}
    };

    for (const auto& [name, env_var, example] : cloud_candidates) {
        bool has_key = !utils::EnvLoader::get_env(env_var, "").empty();
        results.push_back({
            name,
            "Cloud API",
            has_key ? "CONFIGURADO" : "SEM CHAVE",
            env_var + (has_key ? " presente no .env" : " não definida"),
            {},
            example
        });
    }

    return results;
}

std::string ProviderRegistry::find_backup_model(const std::string& current_model) {
    std::vector<std::pair<std::string, std::string>> candidates = {
        {"GEMINI_API_KEY", "gemini/gemini-2.5-flash"},
        {"OPENAI_API_KEY", "openai/gpt-4o"},
        {"ANTHROPIC_API_KEY", "anthropic/claude-3-7-sonnet-20250219"},
        {"DEEPSEEK_API_KEY", "deepseek/deepseek-chat"},
        {"GROQ_API_KEY", "groq/llama-3.3-70b-versatile"},
        {"OPENROUTER_API_KEY", "openrouter/anthropic/claude-3.5-sonnet"}
    };

    for (const auto& [env_var, model_name] : candidates) {
        if (!utils::EnvLoader::get_env(env_var, "").empty() && model_name != current_model) {
            return model_name;
        }
    }
    return "";
}

} // namespace llmcli::providers
