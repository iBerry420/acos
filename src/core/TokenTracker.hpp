#pragma once

#include <atomic>
#include <cstddef>
#include <string>

namespace avacli {

inline std::string formatTokens(size_t n) {
    if (n >= 1000000) return std::to_string(n / 1000000) + "M";
    if (n >= 1000)   return std::to_string(n / 1000) + "k";
    return std::to_string(n);
}

class TokenTracker {
public:
    void addPrompt(size_t n)     { promptTokens_.fetch_add(n); }
    void addCompletion(size_t n) { completionTokens_.fetch_add(n); }
    void addUsage(size_t prompt, size_t completion) {
        promptTokens_.fetch_add(prompt);
        completionTokens_.fetch_add(completion);
    }
    void reset() {
        promptTokens_.store(0);
        completionTokens_.store(0);
    }

    size_t promptTokens()     const { return promptTokens_.load(); }
    size_t completionTokens() const { return completionTokens_.load(); }
    size_t totalTokens()      const { return promptTokens_.load() + completionTokens_.load(); }

    // Format for display: "I: xxk | O: xxk | Total: xxk"
    std::string formatForDisplay() const {
        return "I: " + formatTokens(promptTokens()) +
               " | O: " + formatTokens(completionTokens()) +
               " | Total: " + formatTokens(totalTokens());
    }

    // Full integers for TUI status bar
    std::string formatForDisplayDetailed() const {
        return "in " + std::to_string(promptTokens()) +
               " | out " + std::to_string(completionTokens()) +
               " | Σ " + std::to_string(totalTokens());
    }

private:
    std::atomic<size_t> promptTokens_{0};
    std::atomic<size_t> completionTokens_{0};
};

} // namespace avacli
