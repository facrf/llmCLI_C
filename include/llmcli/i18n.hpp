#pragma once

#include <string>
#include <map>
#include <vector>

namespace llmcli::i18n {

struct LanguageInfo {
    std::string name;
    std::string flag;
};

// Available languages
const std::map<std::string, LanguageInfo>& get_supported_languages();

// Active language functions
std::string get_active_language();
std::string set_active_language(const std::string& lang_input);
std::string detect_system_language();

// Translate a key with optional string parameter replacements
std::string t(const std::string& key, const std::map<std::string, std::string>& replacements = {});

} // namespace llmcli::i18n
