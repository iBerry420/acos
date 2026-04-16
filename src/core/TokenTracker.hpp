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
    void addCached(size_t n)     { cachedPromptTokens_.fetch_add(n); }
    void addReasoning(size_t n)  { reasoningTokens_.fetch_add(n); }

    /// Backwards-compatible 2-arg form: no cache/reasoning information available.
    void addUsage(size_t prompt, size_t completion) {
        promptTokens_.fetch_add(prompt);
        completionTokens_.fetch_add(completion);
    }
    /// Full form covering all four fields on the Usage struct.
    void addUsage(size_t prompt, size_t completion, size_t cached, size_t reasoning) {
        promptTokens_.fetch_add(prompt);
        completionTokens_.fetch_add(completion);
        cachedPromptTokens_.fetch_add(cached);
        reasoningTokens_.fetch_add(reasoning);
    }
    void reset() {
        promptTokens_.store(0);
        completionTokens_.store(0);
        cachedPromptTokens_.store(0);
        reasoningTokens_.store(0);
    }

    size_t promptTokens()       const { return promptTokens_.load(); }
    size_t completionTokens()   const { return completionTokens_.load(); }
    size_t cachedPromptTokens() const { return cachedPromptTokens_.load(); }
    size_t reasoningTokens()    const { return reasoningTokens_.load(); }
    size_t totalTokens()        const { return promptTokens_.load() + completionTokens_.load(); }
    size_t billablePromptTokens() const {
        size_t p = promptTokens_.load();
        size_t c = cachedPromptTokens_.load();
        return p > c ? p - c : 0;
    }
    int cacheHitPercent() const {
        size_t p = promptTokens_.load();
        if (p == 0) return 0;
        return static_cast<int>((cachedPromptTokens_.load() * 100) / p);
    }

    // "I: 12k (8k cached, 67%) | O: 3k | Total: 15k"
    std::string formatForDisplay() const {
        std::string out = "I: " + formatTokens(promptTokens());
        if (cachedPromptTokens() > 0) {
            out += " (" + formatTokens(cachedPromptTokens()) + " cached, " +
                   std::to_string(cacheHitPercent()) + "%)";
        }
        out += " | O: " + formatTokens(completionTokens());
        out += " | Total: " + formatTokens(totalTokens());
        return out;
    }

    std::string formatForDisplayDetailed() const {
        std::string out = "in " + std::to_string(promptTokens());
        if (cachedPromptTokens() > 0) {
            out += " (cached " + std::to_string(cachedPromptTokens()) +
                   ", " + std::to_string(cacheHitPercent()) + "%)";
        }
        out += " | out " + std::to_string(completionTokens());
        out += " | Σ " + std::to_string(totalTokens());
        return out;
    }

private:
    std::atomic<size_t> promptTokens_{0};
    std::atomic<size_t> completionTokens_{0};
    std::atomic<size_t> cachedPromptTokens_{0};
    std::atomic<size_t> reasoningTokens_{0};
};

} // namespace avacli
