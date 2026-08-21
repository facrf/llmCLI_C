#pragma once

#include "llmcli/types.hpp"
#include <string>
#include <vector>

namespace llmcli::providers {

class HostScanner {
public:
    explicit HostScanner(const std::string& host, double timeout_seconds = 3.0);

    std::vector<DiscoveredService> scan();

private:
    std::string host_;
    double timeout_seconds_{3.0};

    std::optional<DiscoveredService> probe_ollama(int port);
    std::optional<DiscoveredService> probe_llamacpp(int port, const std::string& name);
    std::optional<DiscoveredService> probe_openai_compatible(int port, const std::string& name, const std::string& ptype);
};

} // namespace llmcli::providers
