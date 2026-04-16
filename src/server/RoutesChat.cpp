#include "server/Routes.hpp"
#include "server/ServerContext.hpp"
#include "server/ServerHelpers.hpp"
#include "server/LogBuffer.hpp"
#include "client/XAIClient.hpp"
#include "core/AgentEngine.hpp"
#include "core/TokenTracker.hpp"
#include "session/SessionManager.hpp"
#include "tools/ToolExecutor.hpp"
#include "tools/ToolRegistry.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <queue>
#include <thread>
#include <chrono>

namespace avacli {

namespace fs = std::filesystem;

namespace {

std::string base64Encode(const unsigned char* data, size_t len) {
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((len * 4 / 3) + 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned int n = static_cast<unsigned int>(data[i]) << 16;
        if (i + 1 < len) n |= static_cast<unsigned int>(data[i + 1]) << 8;
        if (i + 2 < len) n |= static_cast<unsigned int>(data[i + 2]);
        out += b64[(n >> 18) & 0x3F];
        out += b64[(n >> 12) & 0x3F];
        out += (i + 1 < len) ? b64[(n >> 6) & 0x3F] : '=';
        out += (i + 2 < len) ? b64[n & 0x3F] : '=';
    }
    return out;
}

std::string mimeFromPath(const std::string& path) {
    auto ext = fs::path(path).extension().string();
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".webp") return "image/webp";
    return "image/jpeg";
}

std::string fileToDataUri(const std::string& filePath) {
    if (!fs::exists(filePath)) return "";
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs) return "";
    std::string data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    std::string mime = mimeFromPath(filePath);
    std::string b64 = base64Encode(reinterpret_cast<const unsigned char*>(data.data()), data.size());
    return "data:" + mime + ";base64," + b64;
}

} // anonymous namespace

static thread_local AskUserState* _threadAskUserState = nullptr;

void registerChatRoutes(httplib::Server& svr, ServerContext ctx) {

    svr.Post("/api/chat/ask_user", [ctx](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }
        std::string answer = body.value("answer", "");
        auto* state = ctx.askUserState;
        if (!state) {
            res.status = 400;
            res.set_content(R"({"error":"no pending question"})", "application/json");
            return;
        }
        {
            std::lock_guard<std::mutex> lk(state->mu);
            if (!state->pending) {
                res.status = 400;
                res.set_content(R"({"error":"no pending question"})", "application/json");
                return;
            }
            state->response = answer;
            state->responded = true;
        }
        state->cv.notify_all();
        res.set_content(R"({"ok":true})", "application/json");
    });

    svr.Get("/api/chat/status", [ctx](const httplib::Request&, httplib::Response& res) {
        nlohmann::json j;
        j["busy"] = ctx.chatBusyFlag->load();
        j["cancelled"] = ctx.chatCancelFlag->load();
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/chat/stop", [ctx](const httplib::Request&, httplib::Response& res) {
        ctx.chatCancelFlag->store(true);
        res.set_content(R"({"stopped":true})", "application/json");
    });

    svr.Post("/api/chat", [ctx](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        try {
            body = nlohmann::json::parse(req.body);
        } catch (const std::exception&) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }

        std::string message = body.value("message", "");
        std::string model = body.value("model", ctx.config->model);
        std::string sessionName = body.value("session", ctx.config->session);
        if (sessionName.empty())
            sessionName = generateChatSessionName();
        std::string modeStr = body.value("mode", "agent");
        bool useHistory = body.value("use_history", true);
        int maxTokensOverride = body.value("max_tokens", 0);
        std::string reasoningEffort = body.value("reasoning_effort", "");

        std::vector<std::string> notes;
        if (body.contains("notes") && body["notes"].is_array()) {
            for (const auto& n : body["notes"]) {
                if (n.is_string()) notes.push_back(n.get<std::string>());
            }
        }

        if (message.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"message required"})", "application/json");
            return;
        }

        nlohmann::json mediaSettings;
        if (body.contains("media_settings") && body["media_settings"].is_object())
            mediaSettings = body["media_settings"];
        std::vector<nlohmann::json> contextMedia;
        if (body.contains("context_media") && body["context_media"].is_array()) {
            for (const auto& cm : body["context_media"])
                if (cm.is_object()) contextMedia.push_back(cm);
        }

        bool isMediaModel = (model.find("grok-imagine-image") != std::string::npos ||
                             model.find("grok-imagine-video") != std::string::npos);

        ctx.chatCancelFlag->store(false);
        ctx.chatBusyFlag->store(true);
        LogBuffer::instance().info("chat", "Chat request started", {{"model", model}});

        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_header("Access-Control-Allow-Origin", "*");

        struct EventQueue {
            std::mutex mu;
            std::condition_variable cv;
            std::queue<std::string> events;
            bool done = false;
        };
        auto eq = std::make_shared<EventQueue>();

        auto pushEvent = [eq](const nlohmann::json& j) {
            std::string data = "data: " + j.dump() + "\n\n";
            std::lock_guard<std::mutex> lock(eq->mu);
            eq->events.push(std::move(data));
            eq->cv.notify_one();
        };

        auto cancelFlag = ctx.chatCancelFlag;
        auto promptCounter = ctx.sessionPromptTokens;
        auto completionCounter = ctx.sessionCompletionTokens;

        std::string chatEndpointUrl;
        std::string apiKey;
        resolveXaiAuth(*ctx.config, apiKey, chatEndpointUrl);

        // Media model path (image/video generation)
        if (isMediaModel) {
            std::string workspace = ctx.config->workspace;
            auto busyFlag = ctx.chatBusyFlag;
            std::thread mediaThread([eq, pushEvent, message, model, sessionName,
                                     mediaSettings, contextMedia, apiKey, cancelFlag, busyFlag, workspace]() {
                bool isVideo = (model.find("video") != std::string::npos);
                std::string mediaType = isVideo ? "video" : "image";

                pushEvent({{"type", "session_init"}, {"session", sessionName}});
                pushEvent({{"type", "media_start"}, {"media_type", mediaType}, {"prompt", message}});

                auto toolExec = std::make_unique<ToolExecutor>(workspace, Mode::Agent, nullptr);

                SessionManager sessionMgr;
                SessionData sessionData;
                if (!sessionName.empty() && sessionMgr.exists(sessionName))
                    sessionMgr.load(sessionName, sessionData);
                sessionData.messages.push_back({"user", message});

                nlohmann::json toolArgs;
                toolArgs["prompt"] = message;
                toolArgs["model"] = model;

                std::string toolName;
                bool hasContextImage = false;
                bool hasContextVideo = false;
                for (const auto& cm : contextMedia) {
                    std::string t = cm.value("type", "");
                    if (t == "image" || t.find("image") != std::string::npos) hasContextImage = true;
                    if (t == "video" || t.find("video") != std::string::npos) hasContextVideo = true;
                }

                if (isVideo) {
                    if (hasContextVideo) {
                        toolName = "edit_video";
                        std::string vidUrl = contextMedia[0].value("url", "");
                        if (vidUrl.rfind("/uploads/", 0) == 0) {
                            std::string localPath = (fs::path(workspace) / vidUrl.substr(1)).string();
                            toolArgs["video"] = localPath;
                        } else {
                            toolArgs["video"] = vidUrl;
                        }
                    } else if (hasContextImage) {
                        toolName = "generate_video";
                        std::string imgUrl = contextMedia[0].value("url", "");
                        if (imgUrl.rfind("/uploads/", 0) == 0) {
                            std::string localPath = (fs::path(workspace) / imgUrl.substr(1)).string();
                            toolArgs["image"] = localPath;
                        } else {
                            toolArgs["image"] = imgUrl;
                        }
                    } else {
                        toolName = "generate_video";
                    }
                    toolArgs["duration"] = mediaSettings.value("duration", 5);
                    toolArgs["aspect_ratio"] = mediaSettings.value("ratio", "16:9");
                    toolArgs["resolution"] = mediaSettings.value("resolution", "720p");
                } else {
                    if (hasContextImage) {
                        toolName = "edit_image";
                        std::string imgUrl = contextMedia[0].value("url", "");
                        if (imgUrl.find("/uploads/") == 0) {
                            std::string localPath = (fs::path(workspace) / imgUrl.substr(1)).string();
                            toolArgs["image_path"] = localPath;
                        } else {
                            toolArgs["image_path"] = imgUrl;
                        }
                    } else {
                        toolName = "generate_image";
                        toolArgs["n"] = mediaSettings.value("count", 1);
                    }
                    toolArgs["aspect_ratio"] = mediaSettings.value("ratio", "");
                    toolArgs["resolution"] = mediaSettings.value("resolution", "");
                    toolArgs["quality"] = mediaSettings.value("quality", "");
                }

                pushEvent({{"type", "media_progress"}, {"status", "generating"},
                           {"media_type", mediaType}, {"tool", toolName}});

                std::string result = toolExec->execute(toolName, toolArgs.dump());

                nlohmann::json resultJson;
                try { resultJson = nlohmann::json::parse(result); } catch (...) {
                    resultJson = {{"error", result}};
                }

                bool success = !resultJson.contains("error");
                auto urls = nlohmann::json::array();

                if (!success) {
                    try {
                        LogBuffer::instance().error(
                            "chat", "Media generation failed",
                            {{"model", model},
                             {"tool", toolName},
                             {"error", resultJson.value("error", result)}});
                    } catch (...) {}
                }

                if (success) {
                    if (resultJson.contains("images") && resultJson["images"].is_array()) {
                        for (const auto& img : resultJson["images"])
                            urls.push_back({{"url", img.value("url","")}, {"type", "image"}});
                    } else if (resultJson.contains("url")) {
                        urls.push_back({{"url", resultJson.value("url","")}, {"type", mediaType}});
                    }
                }

                pushEvent({{"type", "media_done"}, {"media_type", mediaType},
                           {"success", success}, {"urls", urls},
                           {"result", resultJson}, {"prompt", message}});

                std::string assistantContent;
                if (success) {
                    for (const auto& u : urls)
                        assistantContent += "![" + mediaType + "](" + u.value("url","") + ")\n";
                } else {
                    assistantContent = "Media generation failed: " + resultJson.value("error", "unknown error");
                }
                sessionData.messages.push_back({"assistant", assistantContent});
                sessionMgr.save(sessionName, sessionData);

                LogBuffer::instance().info("chat", "Chat completed");
                pushEvent({{"type", "done"}, {"success", success}, {"cancelled", false},
                           {"session", sessionName}, {"prompt_tokens", 0}, {"completion_tokens", 0}});

                busyFlag->store(false);
                std::lock_guard<std::mutex> lock(eq->mu);
                eq->done = true;
                eq->cv.notify_one();
            });
            mediaThread.detach();

            res.set_chunked_content_provider(
                "text/event-stream",
                [eq](size_t, httplib::DataSink& sink) -> bool {
                    std::unique_lock<std::mutex> lock(eq->mu);
                    eq->cv.wait(lock, [&] { return !eq->events.empty() || eq->done; });
                    while (!eq->events.empty()) {
                        std::string event = std::move(eq->events.front());
                        eq->events.pop();
                        lock.unlock();
                        if (!sink.write(event.data(), event.size())) return false;
                        lock.lock();
                    }
                    if (eq->done && eq->events.empty()) { sink.done(); return false; }
                    return true;
                });
            return;
        }

        // Normal chat model path
        std::string workspace = ctx.config->workspace;
        auto busyFlag2 = ctx.chatBusyFlag;
        auto askState = ctx.askUserState;
        std::thread agentThread([eq, pushEvent, message, model, sessionName, modeStr,
                                 cancelFlag, busyFlag2, promptCounter, completionCounter, useHistory, notes,
                                 chatEndpointUrl, apiKey, workspace, contextMedia, maxTokensOverride, reasoningEffort, askState]() {
          _threadAskUserState = askState;
          try {
            Mode mode = modeFromString(modeStr);
            ToolRegistry::registerAll();

            auto sessionMgr = std::make_shared<SessionManager>();
            auto sessionData = std::make_shared<SessionData>();
            sessionData->mode = mode;
            if (!sessionName.empty() && sessionMgr->exists(sessionName))
                sessionMgr->load(sessionName, *sessionData);

            pushEvent({{"type", "session_init"}, {"session", sessionName}});

            auto toolExecutor = std::make_unique<ToolExecutor>(workspace, mode, nullptr);
            toolExecutor->setSessionId(sessionName);
            toolExecutor->setSearchModel(model);

            toolExecutor->setAskUser([pushEvent, cancelFlag](const std::string& question) -> std::string {
                pushEvent({{"type", "ask_user"}, {"question", question}});

                auto* state = _threadAskUserState;
                if (!state) return "{\"error\": \"ask_user not available\"}";

                {
                    std::lock_guard<std::mutex> lk(state->mu);
                    state->pending = true;
                    state->question = question;
                    state->responded = false;
                    state->response.clear();
                }

                std::unique_lock<std::mutex> lk(state->mu);
                state->cv.wait(lk, [&] { return state->responded || cancelFlag->load(); });

                state->pending = false;
                if (cancelFlag->load()) return "User cancelled.";
                return state->response;
            });

            struct ThoughtState {
                bool inThought = false;
                std::string buffer;
            };
            auto ts = std::make_shared<ThoughtState>();

            auto engine = std::make_unique<AgentEngine>(
                workspace, mode, toolExecutor.get(),
                [pushEvent, ts](const std::string& delta) {
                    ts->buffer += delta;
                    while (!ts->buffer.empty()) {
                        if (!ts->inThought) {
                            auto pos = ts->buffer.find("<thought>");
                            if (pos != std::string::npos) {
                                if (pos > 0)
                                    pushEvent({{"type", "content_delta"}, {"content", ts->buffer.substr(0, pos)}});
                                ts->inThought = true;
                                ts->buffer.erase(0, pos + 9);
                            } else {
                                size_t safe = ts->buffer.size() > 8 ? ts->buffer.size() - 8 : 0;
                                if (safe > 0) {
                                    pushEvent({{"type", "content_delta"}, {"content", ts->buffer.substr(0, safe)}});
                                    ts->buffer.erase(0, safe);
                                }
                                break;
                            }
                        } else {
                            auto pos = ts->buffer.find("</thought>");
                            if (pos != std::string::npos) {
                                if (pos > 0)
                                    pushEvent({{"type", "thinking_delta"}, {"content", ts->buffer.substr(0, pos)}});
                                pushEvent({{"type", "thinking_done"}});
                                ts->inThought = false;
                                ts->buffer.erase(0, pos + 10);
                            } else {
                                size_t safe = ts->buffer.size() > 9 ? ts->buffer.size() - 9 : 0;
                                if (safe > 0) {
                                    pushEvent({{"type", "thinking_delta"}, {"content", ts->buffer.substr(0, safe)}});
                                    ts->buffer.erase(0, safe);
                                }
                                break;
                            }
                        }
                    }
                },
                [](TranscriptChannel, const std::string&) {},
                nullptr,
                [](Mode) {});

            engine->setToolStartCallback(
                [pushEvent](const std::string& id, const std::string& name, const std::string& args) {
                    nlohmann::json j;
                    j["type"] = "tool_start";
                    j["tool_call_id"] = id;
                    j["name"] = name;
                    try { j["arguments"] = nlohmann::json::parse(args); }
                    catch (...) { j["arguments"] = args; }
                    pushEvent(j);
                    LogBuffer::instance().info("chat", "Tool started: " + name, {{"tool_call_id", id}});
                });

            engine->setToolDoneCallback(
                [pushEvent, sessionMgr, sessionData, sessionName](const std::string& id, const std::string& name, bool success, const std::string& result) {
                    nlohmann::json j;
                    j["type"] = "tool_done";
                    j["tool_call_id"] = id;
                    j["name"] = name;
                    j["success"] = success;
                    std::string trunc = result.size() > 4000 ? result.substr(0, 4000) + "..." : result;
                    j["result"] = trunc;
                    pushEvent(j);
                    LogBuffer::instance().info("chat", "Tool done: " + name, {{"success", success}});
                    try { sessionMgr->save(sessionName, *sessionData); } catch (...) {}
                });

            if (cancelFlag->load()) {
                LogBuffer::instance().info("chat", "Chat cancelled");
                pushEvent({{"type", "done"}, {"success", false}, {"cancelled", true},
                           {"session", sessionName},
                           {"prompt_tokens", 0}, {"completion_tokens", 0}});
                busyFlag2->store(false);
                std::lock_guard<std::mutex> lock(eq->mu);
                eq->done = true;
                eq->cv.notify_one();
                return;
            }

            if (!notes.empty()) {
                std::string notesStr = "Additional instructions from the user:\n";
                for (const auto& n : notes) notesStr += "- " + n + "\n";
                sessionData->messages.insert(sessionData->messages.begin(),
                    Message{"system", notesStr});
            }

            if (!useHistory) {
                sessionData->messages.clear();
            }

            {
                Message userMsg;
                userMsg.role = "user";
                userMsg.content = message;
                if (!contextMedia.empty()) {
                    userMsg.content_parts.push_back({"text", message, ""});
                    for (const auto& cm : contextMedia) {
                        std::string url = cm.value("url", "");
                        std::string ctype = cm.value("type", "");
                        if ((ctype == "image" || ctype.find("image") != std::string::npos) && !url.empty()) {
                            std::string dataUri;
                            if (url.rfind("/uploads/", 0) == 0) {
                                std::string localPath = (fs::path(workspace) / url.substr(1)).string();
                                dataUri = fileToDataUri(localPath);
                            } else {
                                dataUri = url;
                            }
                            if (!dataUri.empty())
                                userMsg.content_parts.push_back({"image_url", "", dataUri});
                        } else if ((ctype == "video" || ctype.find("video") != std::string::npos) && !url.empty()) {
                            std::string localPath;
                            if (url.rfind("/uploads/", 0) == 0)
                                localPath = (fs::path(workspace) / url.substr(1)).string();
                            else
                                localPath = url;
                            userMsg.content_parts.push_back({"text", "[Attached video: " + localPath + " — use tools to analyze if needed]", ""});
                        } else if ((ctype == "audio" || ctype.find("audio") != std::string::npos) && !url.empty()) {
                            std::string localPath;
                            if (url.rfind("/uploads/", 0) == 0)
                                localPath = (fs::path(workspace) / url.substr(1)).string();
                            else
                                localPath = url;
                            userMsg.content_parts.push_back({"text", "[Attached audio: " + localPath + " — use tools to analyze if needed]", ""});
                        } else if (!url.empty() && ctype != "image") {
                            std::string localPath;
                            if (url.rfind("/uploads/", 0) == 0)
                                localPath = (fs::path(workspace) / url.substr(1)).string();
                            else
                                localPath = url;
                            userMsg.content_parts.push_back({"text", "[Attached file: " + localPath + "]", ""});
                        }
                    }
                }
                sessionData->messages.push_back(userMsg);
            }

            XAIClient client(apiKey, chatEndpointUrl);
            auto tools = ToolRegistry::getToolsForMode(mode);

            TokenTracker tracker;
            auto onUsage = [pushEvent, &tracker, promptCounter, completionCounter, &model,
                            &sessionName](const Usage& u) {
                tracker.addUsage(u.promptTokens, u.completionTokens);
                promptCounter->fetch_add(u.promptTokens);
                completionCounter->fetch_add(u.completionTokens);
                pushEvent({{"type", "usage"},
                           {"prompt_tokens", u.promptTokens},
                           {"completion_tokens", u.completionTokens},
                           {"billing", "xai_direct"}});

                auto now = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                nlohmann::json record;
                record["timestamp"] = now;
                record["model"] = model;
                record["session"] = sessionName;
                record["prompt_tokens"] = u.promptTokens;
                record["completion_tokens"] = u.completionTokens;
                record["billing"] = "xai_direct";
                try { appendUsageRecord(record); } catch (...) {}
            };

            engine->setCancelFlag(cancelFlag);
            if (maxTokensOverride > 0) engine->setMaxTokensOverride(maxTokensOverride);
            if (!reasoningEffort.empty()) engine->setReasoningEffort(reasoningEffort);

            bool ok = engine->run(client, model, sessionData->messages, tools, onUsage);

            if (ts->inThought && !ts->buffer.empty()) {
                pushEvent({{"type", "thinking_delta"}, {"content", ts->buffer}});
                pushEvent({{"type", "thinking_done"}});
            } else if (!ts->buffer.empty()) {
                pushEvent({{"type", "content_delta"}, {"content", ts->buffer}});
            }

            sessionData->messages.erase(
                std::remove_if(sessionData->messages.begin(), sessionData->messages.end(),
                    [](const Message& m) {
                        return m.role == "system";
                    }),
                sessionData->messages.end());

            sessionData->totalPromptTokens = tracker.promptTokens();
            sessionData->totalCompletionTokens = tracker.completionTokens();
            sessionMgr->save(sessionName, *sessionData);

            if (cancelFlag->load())
                LogBuffer::instance().info("chat", "Chat cancelled");
            else
                LogBuffer::instance().info("chat", "Chat completed");

            pushEvent({{"type", "done"},
                        {"success", ok},
                        {"cancelled", cancelFlag->load()},
                        {"session", sessionName},
                        {"prompt_tokens", tracker.promptTokens()},
                        {"completion_tokens", tracker.completionTokens()}});

            busyFlag2->store(false);
            std::lock_guard<std::mutex> lock(eq->mu);
            eq->done = true;
            eq->cv.notify_one();
          } catch (const std::exception& ex) {
            spdlog::error("Chat thread exception: {}", ex.what());
            pushEvent({{"type", "content_delta"}, {"content", std::string("*Error: ") + ex.what() + "*"}});
            LogBuffer::instance().info("chat", "Chat completed");
            pushEvent({{"type", "done"}, {"success", false}, {"cancelled", false},
                        {"session", sessionName}, {"prompt_tokens", 0}, {"completion_tokens", 0}});
            busyFlag2->store(false);
            std::lock_guard<std::mutex> lock(eq->mu);
            eq->done = true;
            eq->cv.notify_one();
          } catch (...) {
            spdlog::error("Chat thread unknown exception");
            pushEvent({{"type", "content_delta"}, {"content", "*Error: unknown internal error*"}});
            LogBuffer::instance().info("chat", "Chat completed");
            pushEvent({{"type", "done"}, {"success", false}, {"cancelled", false},
                        {"session", sessionName}, {"prompt_tokens", 0}, {"completion_tokens", 0}});
            busyFlag2->store(false);
            std::lock_guard<std::mutex> lock(eq->mu);
            eq->done = true;
            eq->cv.notify_one();
          }
        });
        agentThread.detach();

        res.set_chunked_content_provider(
            "text/event-stream",
            [eq](size_t, httplib::DataSink& sink) -> bool {
                std::unique_lock<std::mutex> lock(eq->mu);
                eq->cv.wait(lock, [&] { return !eq->events.empty() || eq->done; });

                while (!eq->events.empty()) {
                    std::string event = std::move(eq->events.front());
                    eq->events.pop();
                    lock.unlock();
                    if (!sink.write(event.data(), event.size()))
                        return false;
                    lock.lock();
                }

                if (eq->done && eq->events.empty()) {
                    sink.done();
                    return false;
                }
                return true;
            });
    });
}

} // namespace avacli
