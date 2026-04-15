#include "client/XAIClient.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

namespace avacli {

namespace {

std::string buildRequestBody(const std::vector<Message>& messages,
                             const ChatOptions& opts,
                             const std::vector<nlohmann::json>& tools) {
    nlohmann::json j;
    j["model"] = opts.model;
    j["max_tokens"] = opts.maxTokens;
    j["stream"] = opts.stream;
    if (opts.stream)
        j["stream_options"] = {{"include_usage", true}};

    auto msgs = nlohmann::json::array();
    for (const auto& m : messages) {
        nlohmann::json msg;
        msg["role"] = m.role;
        if (m.role == "tool") {
            msg["tool_call_id"] = m.tool_call_id.value_or("");
            msg["content"] = m.content;
        } else if (m.role == "assistant" && !m.tool_calls.empty()) {
            if (!m.content.empty()) msg["content"] = m.content;
            auto arr = nlohmann::json::array();
            for (const auto& tc : m.tool_calls) {
                arr.push_back({
                    {"id", tc.id},
                    {"type", "function"},
                    {"function", {{"name", tc.name}, {"arguments", tc.arguments}}}
                });
            }
            msg["tool_calls"] = arr;
        } else if (!m.content_parts.empty()) {
            auto parts = nlohmann::json::array();
            for (const auto& p : m.content_parts) {
                nlohmann::json part;
                part["type"] = p.type;
                if (p.type == "text")
                    part["text"] = p.text;
                else if (p.type == "image_url")
                    part["image_url"] = {{"url", p.image_url}, {"detail", "high"}};
                parts.push_back(part);
            }
            msg["content"] = parts;
        } else {
            msg["content"] = m.content;
        }
        msgs.push_back(msg);
    }
    j["messages"] = msgs;

    if (!tools.empty()) {
        j["tools"] = tools;
        j["tool_choice"] = "auto";
    }

    return j.dump();
}

// Trim whitespace from both ends
std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/// Rough JWT shape: header.payload.signature (three dot-separated segments).
bool looksLikeJwt(const std::string& key) {
    const auto d1 = key.find('.');
    if (d1 == std::string::npos || d1 == 0) return false;
    const auto d2 = key.find('.', d1 + 1);
    if (d2 == std::string::npos || d2 <= d1 + 1) return false;
    if (key.find('.', d2 + 1) != std::string::npos) return false;
    return true;
}

} // namespace

XAIClient::XAIClient(const std::string& apiKey, const std::string& chatUrl) : apiKey_(apiKey), chatUrl_(chatUrl) {}

bool XAIClient::chatStream(const std::vector<Message>& messages,
                           const ChatOptions& opts,
                           const StreamCallbacks& callbacks) {
    return chatStream(messages, opts, {}, callbacks);
}

bool XAIClient::chatStream(const std::vector<Message>& messages,
                           const ChatOptions& opts,
                           const std::vector<nlohmann::json>& tools,
                           const StreamCallbacks& callbacks) {
    lastError_.clear();

    if (apiKey_.empty()) {
        lastError_ = "XAI_API_KEY not set";
        return false;
    }

    const std::string url = chatUrl_;
    const std::string body = buildRequestBody(messages, opts, tools);

    struct StreamState {
        std::map<size_t, ToolCall> toolCalls;
        const StreamCallbacks* cbs = nullptr;
        bool inReasoning = false;
    } state;
    state.cbs = &callbacks;

    auto parseSSEChunk = [&state](const std::string& data) {
        if (data.empty()) return;
        const std::string trimmed = trim(data);
        if (trimmed == "[DONE]") {
            if (state.cbs->onDone) state.cbs->onDone();
            return;
        }

        try {
            auto j = nlohmann::json::parse(trimmed);

            // Content delta
            if (j.contains("choices") && !j["choices"].empty()) {
                const auto& choice = j["choices"][0];
                if (choice.contains("delta")) {
                    const auto& delta = choice["delta"];

                    // Handle reasoning_content from reasoning models (grok-4-*-reasoning)
                    if (delta.contains("reasoning_content") && delta["reasoning_content"].is_string()) {
                        std::string rc = delta["reasoning_content"].get<std::string>();
                        if (!rc.empty() && state.cbs->onContent) {
                            if (!state.inReasoning) {
                                state.inReasoning = true;
                                state.cbs->onContent("<thought>");
                            }
                            state.cbs->onContent(rc);
                        }
                    }

                    if (delta.contains("content") && delta["content"].is_string()) {
                        std::string content = delta["content"].get<std::string>();
                        if (!content.empty()) {
                            if (state.inReasoning) {
                                state.inReasoning = false;
                                if (state.cbs->onContent) state.cbs->onContent("</thought>");
                            }
                            if (state.cbs->onContent) state.cbs->onContent(content);
                        }
                    }
                    // Tool calls - stream incrementally by index
                    if (delta.contains("tool_calls") && state.cbs->onToolCalls) {
                        for (const auto& tc : delta["tool_calls"]) {
                            size_t idx = tc.contains("index") ? tc["index"].get<size_t>() : 0;
                            if (!state.toolCalls.count(idx))
                                state.toolCalls[idx] = ToolCall{};

                            auto& call = state.toolCalls[idx];
                            if (tc.contains("id"))
                                call.id = tc["id"].get<std::string>();
                            if (tc.contains("function")) {
                                const auto& fn = tc["function"];
                                if (fn.contains("name")) {
                                    std::string n = fn["name"].get<std::string>();
                                    if (!n.empty()) call.name = std::move(n);
                                }
                                if (fn.contains("arguments"))
                                    call.arguments += fn["arguments"].get<std::string>();
                            }
                        }
                    }
                }
            }

            // Usage (usually on final chunk) - support XAI/OpenAI field names
            if (j.contains("usage") && state.cbs->onUsage) {
                Usage u;
                const auto& usage = j["usage"];
                if (usage.contains("prompt_tokens"))
                    u.promptTokens = usage["prompt_tokens"].get<size_t>();
                else if (usage.contains("input_tokens"))
                    u.promptTokens = usage["input_tokens"].get<size_t>();
                if (usage.contains("completion_tokens"))
                    u.completionTokens = usage["completion_tokens"].get<size_t>();
                else if (usage.contains("output_tokens"))
                    u.completionTokens = usage["output_tokens"].get<size_t>();
                if (usage.contains("reasoning_tokens"))
                    u.completionTokens += usage["reasoning_tokens"].get<size_t>();
                else if (usage.contains("output_tokens_details") && usage["output_tokens_details"].contains("reasoning_tokens"))
                    u.completionTokens += usage["output_tokens_details"]["reasoning_tokens"].get<size_t>();
                state.cbs->onUsage(u);
            }
        } catch (const nlohmann::json::exception& e) {
            spdlog::debug("XAIClient: JSON parse error in SSE chunk: {}", e.what());
        }
    };

    auto flushToolCalls = [&state]() {
        // Close any open reasoning block before flushing
        if (state.inReasoning && state.cbs->onContent) {
            state.inReasoning = false;
            state.cbs->onContent("</thought>");
        }
        if (state.cbs->onToolCalls && !state.toolCalls.empty()) {
            std::vector<ToolCall> calls;
            for (size_t i = 0; i < state.toolCalls.size(); ++i) {
                if (state.toolCalls.count(i))
                    calls.push_back(state.toolCalls[i]);
            }
            if (!calls.empty())
                state.cbs->onToolCalls(calls);
        }
    };

    struct WriteContext {
        std::string buffer;
        StreamState* state = nullptr;
        std::atomic<bool>* cancelFlag = nullptr;
        std::function<void(const std::string&)> parse;
        std::function<void()> flush;
    } ctx;
    ctx.cancelFlag = opts.cancelFlag;
    ctx.state = &state;
    ctx.parse = parseSSEChunk;
    ctx.flush = flushToolCalls;

    // Use a static function - lambdas passed to C may have ABI issues on some systems
    static auto parseFromBuffer = [](WriteContext* uc) {
        size_t pos = 0;
        while (pos < uc->buffer.size()) {
            size_t lineEnd = uc->buffer.find('\n', pos);
            if (lineEnd == std::string::npos) break;

            std::string line = uc->buffer.substr(pos, lineEnd - pos);
            pos = lineEnd + 1;

            std::string trimmed = trim(line);
            if (trimmed.empty()) continue;

            if (trimmed.size() >= 5 && trimmed.compare(0, 5, "data:") == 0) {
                std::string payload = (trimmed.size() > 5) ? trim(trimmed.substr(5)) : "";
                if (payload == "[DONE]") {
                    uc->flush();
                    if (uc->state->cbs->onDone)
                        uc->state->cbs->onDone();
                } else if (!payload.empty()) {
                    uc->parse(payload);
                }
            }
        }
        if (pos > 0) uc->buffer.erase(0, pos);
    };

    auto writeCb = +[](char* ptr, size_t size, size_t nmemb, void* user) -> size_t {
        const size_t total = size * nmemb;
        auto* uc = static_cast<WriteContext*>(user);
        if (uc->cancelFlag && uc->cancelFlag->load(std::memory_order_relaxed))
            return 0;
        uc->buffer.append(ptr, total);
        parseFromBuffer(uc);
        return total;
    };

    CURL* curl = curl_easy_init();
    if (!curl) {
        lastError_ = "Failed to init curl";
        return false;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    {
        std::string auth = "Authorization: Bearer " + apiKey_;
        headers = curl_slist_append(headers, auth.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3600L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, nullptr);  // Disable compression; can cause write errors
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        if (res == CURLE_WRITE_ERROR && opts.cancelFlag && opts.cancelFlag->load(std::memory_order_relaxed)) {
            lastError_ = "Cancelled";
            return false;
        }
        lastError_ = curl_easy_strerror(res);
        spdlog::error("XAIClient: {}", lastError_);
        return false;
    }

    return true;
}

bool XAIClient::responsesStream(const std::string& instructions,
                                const std::vector<nlohmann::json>& input,
                                const ChatOptions& opts,
                                const std::vector<nlohmann::json>& tools,
                                const StreamCallbacks& callbacks) {
    lastError_.clear();

    if (apiKey_.empty()) {
        lastError_ = "XAI_API_KEY not set";
        return false;
    }

    nlohmann::json body;
    body["model"] = opts.model;
    body["stream"] = opts.stream;

    if (!instructions.empty())
        body["instructions"] = instructions;

    body["input"] = input;

    if (!tools.empty())
        body["tools"] = tools;

    if (!opts.reasoningEffort.empty())
        body["reasoning"] = {{"effort", opts.reasoningEffort}};

    if (opts.maxTokens > 0)
        body["max_output_tokens"] = opts.maxTokens;

    const std::string url = responsesUrl_;
    const std::string jsonBody = body.dump();

    struct RespStreamState {
        std::map<size_t, ToolCall> toolCalls;
        const StreamCallbacks* cbs = nullptr;
        bool inReasoning = false;
        std::string currentEventType;
    } state;
    state.cbs = &callbacks;

    auto parseSSELine = [&state](const std::string& eventType, const std::string& data) {
        if (data.empty() || data == "[DONE]") {
            if (!state.toolCalls.empty() && state.cbs->onToolCalls) {
                std::vector<ToolCall> calls;
                for (size_t i = 0; i <= state.toolCalls.rbegin()->first; ++i) {
                    if (state.toolCalls.count(i))
                        calls.push_back(state.toolCalls[i]);
                }
                if (!calls.empty())
                    state.cbs->onToolCalls(calls);
                state.toolCalls.clear();
            }
            if (state.inReasoning && state.cbs->onContent) {
                state.inReasoning = false;
                state.cbs->onContent("</thought>");
            }
            if (state.cbs->onDone) state.cbs->onDone();
            return;
        }

        try {
            auto j = nlohmann::json::parse(data);
            std::string type = j.value("type", "");

            if (type == "response.output_text.delta") {
                std::string delta = j.value("delta", "");
                if (!delta.empty() && state.cbs->onContent) {
                    if (state.inReasoning) {
                        state.inReasoning = false;
                        state.cbs->onContent("</thought>");
                    }
                    state.cbs->onContent(delta);
                }
            }
            else if (type == "response.reasoning.delta" || type == "response.reasoning_summary_text.delta") {
                std::string delta = j.value("delta", "");
                if (!delta.empty() && state.cbs->onContent) {
                    if (!state.inReasoning) {
                        state.inReasoning = true;
                        state.cbs->onContent("<thought>");
                    }
                    state.cbs->onContent(delta);
                }
            }
            else if (type == "response.function_call_arguments.delta") {
                size_t idx = j.value("output_index", (size_t)0);
                if (!state.toolCalls.count(idx))
                    state.toolCalls[idx] = ToolCall{};
                state.toolCalls[idx].arguments += j.value("delta", "");
            }
            else if (type == "response.function_call_arguments.done") {
                size_t idx = j.value("output_index", (size_t)0);
                if (!state.toolCalls.count(idx))
                    state.toolCalls[idx] = ToolCall{};
                auto& tc = state.toolCalls[idx];
                if (j.contains("call_id")) tc.id = j["call_id"].get<std::string>();
                if (j.contains("name")) tc.name = j["name"].get<std::string>();
                if (j.contains("arguments")) tc.arguments = j["arguments"].get<std::string>();
            }
            else if (type == "response.output_item.added") {
                if (j.contains("item") && j["item"].is_object()) {
                    auto& item = j["item"];
                    if (item.value("type", "") == "function_call") {
                        size_t idx = j.value("output_index", (size_t)0);
                        if (!state.toolCalls.count(idx))
                            state.toolCalls[idx] = ToolCall{};
                        auto& tc = state.toolCalls[idx];
                        if (item.contains("call_id")) tc.id = item["call_id"].get<std::string>();
                        if (item.contains("name")) tc.name = item["name"].get<std::string>();
                    }
                }
            }
            else if (type == "response.completed" || type == "response.done") {
                if (state.inReasoning && state.cbs->onContent) {
                    state.inReasoning = false;
                    state.cbs->onContent("</thought>");
                }

                if (!state.toolCalls.empty() && state.cbs->onToolCalls) {
                    std::vector<ToolCall> calls;
                    for (size_t i = 0; i <= state.toolCalls.rbegin()->first; ++i) {
                        if (state.toolCalls.count(i))
                            calls.push_back(state.toolCalls[i]);
                    }
                    if (!calls.empty())
                        state.cbs->onToolCalls(calls);
                    state.toolCalls.clear();
                }

                if (j.contains("response") && j["response"].is_object()) {
                    const auto& resp = j["response"];
                    if (resp.contains("usage") && state.cbs->onUsage) {
                        Usage u;
                        const auto& usage = resp["usage"];
                        u.promptTokens = usage.value("input_tokens", (size_t)0);
                        u.completionTokens = usage.value("output_tokens", (size_t)0);
                        state.cbs->onUsage(u);
                    }
                }
            }
        } catch (const nlohmann::json::exception& e) {
            spdlog::debug("XAIClient::responsesStream: JSON parse error: {}", e.what());
        }
    };

    struct WriteContext {
        std::string buffer;
        std::string currentEvent;
        RespStreamState* state = nullptr;
        std::atomic<bool>* cancelFlag = nullptr;
        std::function<void(const std::string&, const std::string&)> parse;
    } ctx;
    ctx.cancelFlag = opts.cancelFlag;
    ctx.state = &state;
    ctx.parse = parseSSELine;

    static auto processBuffer = [](WriteContext* uc) {
        size_t pos = 0;
        while (pos < uc->buffer.size()) {
            size_t lineEnd = uc->buffer.find('\n', pos);
            if (lineEnd == std::string::npos) break;

            std::string line = uc->buffer.substr(pos, lineEnd - pos);
            pos = lineEnd + 1;

            if (line.empty() || line == "\r") continue;
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (line.compare(0, 6, "event:") == 0) {
                std::string ev = line.substr(6);
                auto s = ev.find_first_not_of(" \t");
                uc->currentEvent = (s != std::string::npos) ? ev.substr(s) : "";
            }
            else if (line.compare(0, 5, "data:") == 0) {
                std::string payload = line.substr(5);
                auto s = payload.find_first_not_of(" \t");
                payload = (s != std::string::npos) ? payload.substr(s) : "";

                if (payload == "[DONE]") {
                    uc->parse(uc->currentEvent, "[DONE]");
                } else if (!payload.empty()) {
                    uc->parse(uc->currentEvent, payload);
                }
            }
        }
        if (pos > 0) uc->buffer.erase(0, pos);
    };

    auto writeCb = +[](char* ptr, size_t size, size_t nmemb, void* user) -> size_t {
        const size_t total = size * nmemb;
        auto* uc = static_cast<WriteContext*>(user);
        if (uc->cancelFlag && uc->cancelFlag->load(std::memory_order_relaxed))
            return 0;
        uc->buffer.append(ptr, total);
        processBuffer(uc);
        return total;
    };

    CURL* curl = curl_easy_init();
    if (!curl) {
        lastError_ = "Failed to init curl";
        return false;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    {
        std::string auth = "Authorization: Bearer " + apiKey_;
        headers = curl_slist_append(headers, auth.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3600L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, nullptr);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

    CURLcode res = curl_easy_perform(curl);

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        if (res == CURLE_WRITE_ERROR && opts.cancelFlag && opts.cancelFlag->load(std::memory_order_relaxed)) {
            lastError_ = "Cancelled";
            return false;
        }
        lastError_ = curl_easy_strerror(res);
        spdlog::error("XAIClient::responsesStream: {}", lastError_);
        return false;
    }

    if (httpCode != 200) {
        std::string errBody = ctx.buffer;
        try {
            auto j = nlohmann::json::parse(errBody);
            if (j.contains("error") && j["error"].is_object())
                lastError_ = "HTTP " + std::to_string(httpCode) + ": " + j["error"].value("message", errBody);
            else if (j.contains("error") && j["error"].is_string())
                lastError_ = "HTTP " + std::to_string(httpCode) + ": " + j["error"].get<std::string>();
            else
                lastError_ = "HTTP " + std::to_string(httpCode) + ": " + errBody.substr(0, 500);
        } catch (...) {
            lastError_ = "HTTP " + std::to_string(httpCode) + ": " + errBody.substr(0, 500);
        }
        spdlog::error("XAIClient::responsesStream: {}", lastError_);
        return false;
    }

    return true;
}

} // namespace avacli
