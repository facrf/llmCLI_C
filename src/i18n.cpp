#include "llmcli/i18n.hpp"
#include <cstdlib>
#include <algorithm>
#include <iostream>

namespace llmcli::i18n {

static std::string g_active_language = "pt-BR";

static const std::map<std::string, LanguageInfo> SUPPORTED_LANGUAGES = {
    {"pt-BR", {"Português (Brasil)", "🇧🇷"}},
    {"en-US", {"English (US)", "🇺🇸"}},
    {"es-ES", {"Español", "🇪🇸"}},
    {"de-DE", {"Deutsch", "🇩🇪"}},
    {"fr-FR", {"Français", "🇫🇷"}},
    {"zh-CN", {"简体中文", "🇨🇳"}},
    {"ru-RU", {"Русский", "🇷🇺"}},
    {"hi-IN", {"हिन्दी (Hindi)", "🇮🇳"}}
};

static const std::map<std::string, std::string> LANGUAGE_ALIASES = {
    {"pt", "pt-BR"}, {"pt-br", "pt-BR"}, {"pt_br", "pt-BR"}, {"portugues", "pt-BR"}, {"portuguese", "pt-BR"},
    {"en", "en-US"}, {"en-us", "en-US"}, {"en_us", "en-US"}, {"ingles", "en-US"}, {"english", "en-US"},
    {"es", "es-ES"}, {"es-es", "es-ES"}, {"es_es", "es-ES"}, {"espanhol", "es-ES"}, {"spanish", "es-ES"},
    {"de", "de-DE"}, {"de-de", "de-DE"}, {"de_de", "de-DE"}, {"alemao", "de-DE"}, {"german", "de-DE"}, {"deutsch", "de-DE"},
    {"fr", "fr-FR"}, {"fr-fr", "fr-FR"}, {"fr_fr", "fr-FR"}, {"frances", "fr-FR"}, {"french", "fr-FR"}, {"francais", "fr-FR"},
    {"zh", "zh-CN"}, {"zh-cn", "zh-CN"}, {"zh_cn", "zh-CN"}, {"chines", "zh-CN"}, {"chinese", "zh-CN"}, {"mandarin", "zh-CN"},
    {"ru", "ru-RU"}, {"ru-ru", "ru-RU"}, {"ru_ru", "ru-RU"}, {"russo", "ru-RU"}, {"russian", "ru-RU"},
    {"hi", "hi-IN"}, {"hi-in", "hi-IN"}, {"hi_in", "hi-IN"}, {"indiano", "hi-IN"}, {"hindi", "hi-IN"}, {"indian", "hi-IN"},
    {"auto", "auto"}, {"default", "auto"}
};

static const std::map<std::string, std::map<std::string, std::string>> TRANSLATIONS = {
    {"pt-BR", {
        {"banner_subtitle", "llmCli - Assistente IA de Código Híbrido em C++ (Local & Nuvem)"},
        {"banner_desc", "Suporte nativo a llama.cpp (porta 8080), Ollama, LM Studio, vLLM, Gemini, OpenAI, Anthropic e DeepSeek.\nDigite /help para ver comandos ou /yolo para alternar o modo autônomo."},
        {"yolo_on", "⚡ MODO YOLO ATIVADO para {model} (preferência salva)!"},
        {"yolo_off", "🛡️ MODO YOLO DESATIVADO para {model} (preferência salva)."},
        {"model_switched", "✓ Modelo ativo alterado para: {model}"},
        {"arch_on", "🏛️ MODO ARQUITETO ATIVADO: Arquiteto: {arch} | Editor: {editor}"},
        {"arch_off", "🛡️ MODO ARQUITETO DESATIVADO: Usando modelo único padrão."},
        {"arch_planning", "🏛️ [Arquiteto: {arch}] Planejando solução..."},
        {"arch_applying", "⚡ [Editor: {editor}] Aplicando alterações nos arquivos..."},
        {"confirm_tool", "Deseja executar a ferramenta '{tool}' com os argumentos: {args}?"},
        {"confirm_editor", "Deseja que o Editor ({editor}) aplique o plano nos arquivos?"},
        {"confirm_commit", "Deseja criar o commit com esta mensagem?"},
        {"commit_success", "Commit criado com sucesso [{hash}]: {msg}"},
        {"commit_suggested", "Mensagem de commit sugerida:"},
        {"commit_no_diff", "Nenhuma alteração Git não commitada encontrada para gerar commit."},
        {"review_no_diff", "Nenhuma alteração Git detectada para Code Review."},
        {"review_running", "Executando Code Review nas alterações Git pendentes..."},
        {"compact_success", "✓ Histórico compactado com sucesso! Estimativa atual: ~{tokens} tokens"},
        {"compact_empty", "Histórico da conversa está vazio, nada a compactar."},
        {"temp_current", "Temperatura atual para {model}: {temp} (padrão: 0.2)"},
        {"temp_changed", "✓ Temperatura para {model} alterada para: {temp} (preferência salva)"},
        {"temp_invalid", "Valor de temperatura inválido. Use um número float entre 0.0 e 2.0."},
        {"sys_current", "System Prompt Ativo:\n{prompt}"},
        {"sys_reset", "System prompt redefinido para o padrão com sucesso."},
        {"sys_custom", "✓ System prompt personalizado configurado para esta sessão."},
        {"paste_active", "📋 Modo Multilinha ativado. Digite ':done' em uma linha para enviar ou ':cancel' para abortar."},
        {"paste_cancel", "Modo multilinha cancelado."},
        {"session_reset", "Sessão reiniciada (histórico e arquivos limpos)."},
        {"prefs_reset", "✓ Todas as preferências salvas (globais e por LLM) foram redefinidas para o padrão."},
        {"all_reset", "✓ Sessão e preferências de usuário totalmente reiniciadas para o padrão."},
        {"lang_current", "Idioma ativo: {flag} {name} ({code})"},
        {"lang_changed", "✓ Idioma alterado com sucesso para: {flag} {name} ({code})"},
        {"exit_msg", "Encerrando llmCli C++. Até logo!"},
        {"unknown_cmd", "Comando desconhecido: '{cmd}'. Digite /help para ver os comandos disponíveis."},
        {"ambiguous_cmd", "Comando ambíguo '{cmd}'. Opções possíveis: {options}"},
        {"prompt_ai_instruction", "Responda sempre em Português do Brasil com explicações claras e código bem documentado."}
    }},
    {"en-US", {
        {"banner_subtitle", "llmCli - Hybrid AI Coding Assistant in C++ (Local & Cloud)"},
        {"banner_desc", "Native support for llama.cpp (port 8080), Ollama, LM Studio, vLLM, Gemini, OpenAI, Anthropic, and DeepSeek.\nType /help to see commands or /yolo to toggle autonomous mode."},
        {"yolo_on", "⚡ YOLO MODE ENABLED for {model} (preference saved)!"},
        {"yolo_off", "🛡️ YOLO MODE DISABLED for {model} (preference saved)."},
        {"model_switched", "✓ Active model switched to: {model}"},
        {"arch_on", "🏛️ ARCHITECT MODE ENABLED: Architect: {arch} | Editor: {editor}"},
        {"arch_off", "🛡️ ARCHITECT MODE DISABLED: Using single default model."},
        {"arch_planning", "🏛️ [Architect: {arch}] Planning solution..."},
        {"arch_applying", "⚡ [Editor: {editor}] Applying changes to files..."},
        {"confirm_tool", "Execute tool '{tool}' with arguments: {args}?"},
        {"confirm_editor", "Do you want the Editor ({editor}) to apply the plan to files?"},
        {"confirm_commit", "Create commit with this message?"},
        {"commit_success", "Commit successfully created [{hash}]: {msg}"},
        {"commit_suggested", "Suggested commit message:"},
        {"commit_no_diff", "No uncommitted Git changes found to generate a commit."},
        {"review_no_diff", "No Git changes detected for Code Review."},
        {"review_running", "Running Code Review on pending Git changes..."},
        {"compact_success", "✓ Conversation history compacted! Current estimate: ~{tokens} tokens"},
        {"compact_empty", "Conversation history is empty, nothing to compact."},
        {"temp_current", "Current temperature for {model}: {temp} (default: 0.2)"},
        {"temp_changed", "✓ Temperature for {model} changed to: {temp} (preference saved)"},
        {"temp_invalid", "Invalid temperature value. Please use a float between 0.0 and 2.0."},
        {"sys_current", "Active System Prompt:\n{prompt}"},
        {"sys_reset", "System prompt successfully reset to default."},
        {"sys_custom", "✓ Custom system prompt set for this session."},
        {"paste_active", "📋 Multiline paste mode active. Type ':done' on an empty line to submit or ':cancel' to abort."},
        {"paste_cancel", "Multiline mode canceled."},
        {"session_reset", "Session reset (history and files cleared)."},
        {"prefs_reset", "✓ All saved preferences (global and per-LLM) have been reset to default."},
        {"all_reset", "✓ Session and user preferences fully reset to default."},
        {"lang_current", "Active language: {flag} {name} ({code})"},
        {"lang_changed", "✓ Language successfully changed to: {flag} {name} ({code})"},
        {"exit_msg", "Exiting llmCli C++. Goodbye!"},
        {"unknown_cmd", "Unknown command: '{cmd}'. Type /help to see available commands."},
        {"ambiguous_cmd", "Ambiguous command '{cmd}'. Possible options: {options}"},
        {"prompt_ai_instruction", "Always reply in English with clear technical explanations and well-structured code."}
    }},
    {"es-ES", {
        {"banner_subtitle", "llmCli - Asistente IA de Código Híbrido en C++ (Local y Nube)"},
        {"banner_desc", "Soporte nativo para llama.cpp (puerto 8080), Ollama, LM Studio, vLLM, Gemini, OpenAI, Anthropic y DeepSeek.\nEscriba /help para ver comandos o /yolo para alternar el modo autónomo."},
        {"yolo_on", "⚡ MODO YOLO ACTIVADO para {model} (preferencia guardada)!"},
        {"yolo_off", "🛡️ MODO YOLO DESACTIVADO para {model} (preferencia guardada)."},
        {"model_switched", "✓ Modelo activo cambiado a: {model}"},
        {"arch_on", "🏛️ MODO ARQUITECTO ACTIVADO: Arquitecto: {arch} | Editor: {editor}"},
        {"arch_off", "🛡️ MODO ARQUITECTO DESACTIVADO: Usando modelo predeterminado."},
        {"prompt_ai_instruction", "Responde siempre en Español con explicaciones claras y código bien documentado."}
    }},
    {"de-DE", {
        {"banner_subtitle", "llmCli - Hybrider KI-Programmierassistent in C++ (Lokal & Cloud)"},
        {"banner_desc", "Native Unterstützung für llama.cpp (Port 8080), Ollama, LM Studio, vLLM, Gemini, OpenAI, Anthropic und DeepSeek.\nTippen Sie /help für Befehle oder /yolo für den Autonomiemodus."},
        {"yolo_on", "⚡ YOLO-MODUS AKTIVIERT für {model} (Einstellung gespeichert)!"},
        {"yolo_off", "🛡️ YOLO-MODUS DEAKTIVIERT für {model} (Einstellung gespeichert)."},
        {"model_switched", "✓ Aktives Modell gewechselt zu: {model}"},
        {"prompt_ai_instruction", "Antworte immer auf Deutsch mit klaren technischen Erklärungen und sauberem Code."}
    }},
    {"fr-FR", {
        {"banner_subtitle", "llmCli - Assistant de Code IA Hybride en C++ (Local & Cloud)"},
        {"banner_desc", "Support natif pour llama.cpp (port 8080), Ollama, LM Studio, vLLM, Gemini, OpenAI, Anthropic et DeepSeek.\nTapez /help pour les commandes ou /yolo pour le mode autonome."},
        {"yolo_on", "⚡ MODE YOLO ACTIVÉ pour {model} (préférence enregistrée)!"},
        {"yolo_off", "🛡️ MODE YOLO DÉSACTIVÉ pour {model} (préférence enregistrée)."},
        {"model_switched", "✓ Modèle actif changé pour: {model}"},
        {"prompt_ai_instruction", "Répondez toujours en Français avec des explications claires et un code bien documenté."}
    }},
    {"zh-CN", {
        {"banner_subtitle", "llmCli - 混合 AI 代码助手 C++ 版 (本地与云端)"},
        {"banner_desc", "原生支持 llama.cpp (8080端口), Ollama, LM Studio, vLLM, Gemini, OpenAI, Anthropic 和 DeepSeek。\n输入 /help 查看命令，或输入 /yolo 切换自主模式。"},
        {"yolo_on", "⚡ {model} 的 YOLO 模式已启用 (偏好已保存)！"},
        {"yolo_off", "🛡️ {model} 的 YOLO 模式已禁用 (偏好已保存)。"},
        {"model_switched", "✓ 当前模型已切换为: {model}"},
        {"prompt_ai_instruction", "请始终使用中文回答，并提供清晰的代码和解释。"}
    }},
    {"ru-RU", {
        {"banner_subtitle", "llmCli - Гибридный ИИ-ассистент разработчика на C++ (Локальный и Облачный)"},
        {"banner_desc", "Поддержка llama.cpp (порт 8080), Ollama, LM Studio, vLLM, Gemini, OpenAI, Anthropic и DeepSeek.\nВведите /help для команд или /yolo для переключения автономного режима."},
        {"yolo_on", "⚡ РЕЖИМ YOLO ВКЛЮЧЕН для {model} (настройка сохранена)!"},
        {"yolo_off", "🛡️ РЕЖИМ YOLO ВЫКЛЮЧЕН для {model} (настройка сохранена)."},
        {"model_switched", "✓ Активная модель изменена на: {model}"},
        {"prompt_ai_instruction", "Всегда отвечайте на русском языке с понятными объяснениями и чистым кодом."}
    }},
    {"hi-IN", {
        {"banner_subtitle", "llmCli - हाइब्रिड AI कोडिंग सहायक C++ में (लोकल और क्लाउड)"},
        {"banner_desc", "llama.cpp, Ollama, LM Studio, Gemini, OpenAI, Anthropic और DeepSeek के लिए समर्थन।\nकमांड के लिए /help या स्वायत्त मोड के लिए /yolo टाइप करें।"},
        {"yolo_on", "⚡ {model} के लिए YOLO मोड सक्षम!"},
        {"yolo_off", "🛡️ {model} के लिए YOLO मोड अक्षम।"},
        {"model_switched", "✓ सक्रिय मॉडल बदला गया: {model}"},
        {"prompt_ai_instruction", "हमेशा स्पष्ट व्याख्या और साफ़ कोड के साथ हिन्दी में उत्तर दें।"}
    }}
};

const std::map<std::string, LanguageInfo>& get_supported_languages() {
    return SUPPORTED_LANGUAGES;
}

std::string get_active_language() {
    return g_active_language;
}

std::string detect_system_language() {
    const char* env_lang = std::getenv("LC_ALL");
    if (!env_lang) env_lang = std::getenv("LC_MESSAGES");
    if (!env_lang) env_lang = std::getenv("LANG");

    if (env_lang) {
        std::string raw(env_lang);
        std::transform(raw.begin(), raw.end(), raw.begin(), [](unsigned char c){ return std::tolower(c); });

        if (raw.find("pt") != std::string::npos) return "pt-BR";
        if (raw.find("es") != std::string::npos) return "es-ES";
        if (raw.find("de") != std::string::npos) return "de-DE";
        if (raw.find("fr") != std::string::npos) return "fr-FR";
        if (raw.find("zh") != std::string::npos) return "zh-CN";
        if (raw.find("ru") != std::string::npos) return "ru-RU";
        if (raw.find("hi") != std::string::npos) return "hi-IN";
        if (raw.find("en") != std::string::npos) return "en-US";
    }
    return "pt-BR";
}

std::string set_active_language(const std::string& lang_input) {
    std::string clean = lang_input;
    std::transform(clean.begin(), clean.end(), clean.begin(), [](unsigned char c){ return std::tolower(c); });

    auto alias_it = LANGUAGE_ALIASES.find(clean);
    std::string resolved = (alias_it != LANGUAGE_ALIASES.end()) ? alias_it->second : lang_input;

    if (resolved == "auto") {
        resolved = detect_system_language();
    }

    if (SUPPORTED_LANGUAGES.find(resolved) != SUPPORTED_LANGUAGES.end()) {
        g_active_language = resolved;
    } else {
        g_active_language = "pt-BR";
    }
    return g_active_language;
}

std::string t(const std::string& key, const std::map<std::string, std::string>& replacements) {
    std::string text = "";

    auto lang_it = TRANSLATIONS.find(g_active_language);
    if (lang_it != TRANSLATIONS.end()) {
        auto key_it = lang_it->second.find(key);
        if (key_it != lang_it->second.end()) {
            text = key_it->second;
        }
    }

    // Fallback to en-US if missing in active language
    if (text.empty()) {
        auto en_it = TRANSLATIONS.find("en-US");
        if (en_it != TRANSLATIONS.end()) {
            auto key_it = en_it->second.find(key);
            if (key_it != en_it->second.end()) {
                text = key_it->second;
            }
        }
    }

    if (text.empty()) {
        text = key;
    }

    // Replace {placeholder} with replacements
    for (const auto& [param, val] : replacements) {
        std::string target = "{" + param + "}";
        size_t pos = 0;
        while ((pos = text.find(target, pos)) != std::string::npos) {
            text.replace(pos, target.length(), val);
            pos += val.length();
        }
    }

    return text;
}

} // namespace llmcli::i18n
