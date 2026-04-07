#include "tools/ToolRegistry.hpp"
#include "core/Types.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <unordered_set>

namespace avacli {

namespace {

std::vector<nlohmann::json> ALL_TOOLS;
bool REGISTERED = false;

// Minimum mode level required per tool: 0=Question, 1=Plan, 2=Agent
const std::unordered_map<std::string, int> TOOL_MIN_MODE = {
    // Question mode (0) — read-only, search, memory
    {"read_file", 0}, {"search_files", 0}, {"list_directory", 0},
    {"glob_files", 0}, {"read_url", 0},
    {"read_project_notes", 0}, {"add_memory", 0},
    {"search_memory", 0}, {"forget_memory", 0},
    {"web_search", 0}, {"x_search", 0},
    {"ask_user", 0}, {"search_assets", 0},
    // Plan mode (1) — write tools + todos + vault reads + tool listing
    {"edit_file", 1}, {"write_file", 1}, {"undo_edit", 1},
    {"read_todos", 1}, {"create_update_todo", 1},
    {"list_custom_tools", 1}, {"vault_list", 1},
    {"list_api_registry", 1},
    // Agent mode (2) — shell, tests, media generation, tool forge, vault writes, API research
    {"run_shell", 2}, {"run_tests", 2},
    {"generate_image", 2}, {"edit_image", 2}, {"generate_video", 2},
    {"edit_video", 2}, {"extend_video", 2},
    {"create_tool", 2}, {"modify_tool", 2}, {"delete_tool", 2},
    {"research_api", 2}, {"setup_api", 2}, {"call_api", 2},
    {"vault_store", 2}, {"vault_retrieve", 2}, {"vault_remove", 2},
    {"summarize_and_new_chat", 2},
};

int modeLevel(Mode m) {
    switch (m) {
        case Mode::Question: return 0;
        case Mode::Plan:     return 1;
        case Mode::Agent:    return 2;
    }
    return 0;
}

bool toolAllowedForMode(const std::string& name, Mode mode) {
    auto it = TOOL_MIN_MODE.find(name);
    if (it == TOOL_MIN_MODE.end()) return true;
    return modeLevel(mode) >= it->second;
}

nlohmann::json makeTool(const std::string& name, const std::string& desc,
                        const nlohmann::json& params, const std::vector<std::string>& required) {
    nlohmann::json j;
    j["type"] = "function";
    j["function"]["name"] = name;
    j["function"]["description"] = desc;
    j["function"]["parameters"]["type"] = "object";
    j["function"]["parameters"]["properties"] = params;
    j["function"]["parameters"]["required"] = required;
    return j;
}

void registerReadFileImpl() {
    auto params = nlohmann::json::object({
        {"path", {{"type", "string"}, {"description", "Absolute or workspace-relative path to the file"}}},
        {"start_line", {{"type", "integer"}, {"description", "Optional 1-based line to start reading (inclusive)"}}},
        {"end_line", {{"type", "integer"}, {"description", "Optional 1-based line to end reading (inclusive)"}}},
    });
    ALL_TOOLS.push_back(makeTool("read_file",
        "Read file contents. Optionally specify start_line and end_line (1-based) to read a range.",
        params, {"path"}));
}

void registerSearchFilesImpl() {
    auto params = nlohmann::json::object({
        {"query", {{"type", "string"}, {"description", "Search query - keywords or pattern to find in files"}}},
        {"directory", {{"type", "string"}, {"description", "Optional directory to search (default: workspace root)"}}},
        {"recursive", {{"type", "boolean"}, {"description", "Whether to search subdirectories (default: true)"}}},
    });
    ALL_TOOLS.push_back(makeTool("search_files",
        "Search for text/pattern in files. Returns matching file paths and excerpts.",
        params, {"query"}));
}

void registerListDirectoryImpl() {
    auto params = nlohmann::json::object({
        {"path", {{"type", "string"}, {"description", "Directory path (absolute or workspace-relative)"}}},
    });
    ALL_TOOLS.push_back(makeTool("list_directory",
        "List files and subdirectories in a directory.",
        params, {"path"}));
}

void registerGlobFilesImpl() {
    auto params = nlohmann::json::object({
        {"pattern", {{"type", "string"}, {"description", "Glob pattern to match (e.g. *.php, **/*.test.js, config.*)"}}},
        {"directory", {{"type", "string"}, {"description", "Directory to search (default: workspace root)"}}},
    });
    ALL_TOOLS.push_back(makeTool("glob_files",
        "Find files by name pattern. Supports * (any chars) and ? (single char). Recursively searches subdirectories.",
        params, {"pattern"}));
}

void registerReadUrlImpl() {
    auto params = nlohmann::json::object({
        {"url", {{"type", "string"}, {"description", "URL to fetch"}}},
        {"max_bytes", {{"type", "integer"}, {"description", "Max bytes of content to return (default: 8000)"}}},
    });
    ALL_TOOLS.push_back(makeTool("read_url",
        "Fetch a URL and return its text content (HTML tags stripped). Use for reading documentation, APIs, or web pages.",
        params, {"url"}));
}

void registerReadProjectNotesImpl() {
    auto params = nlohmann::json::object();
    ALL_TOOLS.push_back(makeTool("read_project_notes",
        "Read GROK_NOTES.md (project architecture reference) and your session memory. Use at the start of work on an unfamiliar codebase.",
        params, {}));
}

void registerAddMemoryImpl() {
    auto params = nlohmann::json::object({
        {"category", {{"type", "string"}, {"description", "Category: architecture, decision, gotcha, integration, or custom"}}},
        {"content", {{"type", "string"}, {"description", "The information to remember"}}},
        {"tags", {{"type", "array"}, {"description", "Optional tags for searchability"}, {"items", {{"type", "string"}}}}},
        {"pinned", {{"type", "boolean"}, {"description", "If true, always included in context (default: false)"}}},
    });
    ALL_TOOLS.push_back(makeTool("add_memory",
        "Save a piece of knowledge to your session memory. Use when you discover important architectural details, make decisions, or encounter gotchas. Memories persist across turns and are searchable.",
        params, {"category", "content"}));
}

void registerSearchMemoryImpl() {
    auto params = nlohmann::json::object({
        {"query", {{"type", "string"}, {"description", "Search query (matches content, tags, and category)"}}},
        {"category", {{"type", "string"}, {"description", "Optional category filter"}}},
    });
    ALL_TOOLS.push_back(makeTool("search_memory",
        "Search your session memory for previously saved knowledge.",
        params, {}));
}

void registerForgetMemoryImpl() {
    auto params = nlohmann::json::object({
        {"id", {{"type", "string"}, {"description", "ID of the memory entry to delete"}}},
    });
    ALL_TOOLS.push_back(makeTool("forget_memory",
        "Delete a specific memory entry by ID. Use to clean up outdated or incorrect information.",
        params, {"id"}));
}

void registerWebSearchImpl() {
    auto params = nlohmann::json::object({
        {"query", {{"type", "string"}, {"description", "Search query"}}},
        {"num_results", {{"type", "integer"}, {"description", "Rough max distinct sources/findings (default: 8)"}}},
    });
    ALL_TOOLS.push_back(makeTool("web_search",
        "Live web search via xAI (browse pages in real time). Returns a summary, citation URLs, and a results list. Uses your chat model for the Responses API.",
        params, {"query"}));
}

void registerXSearchImpl() {
    auto params = nlohmann::json::object({
        {"query", {{"type", "string"}, {"description", "What to find on X (topics, handles, events)"}}},
        {"num_results", {{"type", "integer"}, {"description", "Rough max distinct posts/findings (default: 8)"}}},
    });
    ALL_TOOLS.push_back(makeTool("x_search",
        "Live search on X (Twitter) via xAI: posts, threads, and trends in real time. Returns summary, citation URLs, and results. Uses your chat model for the Responses API.",
        params, {"query"}));
}

void registerAskUserImpl() {
    auto params = nlohmann::json::object({
        {"question", {{"type", "string"}, {"description", "Question to ask the user"}}},
    });
    ALL_TOOLS.push_back(makeTool("ask_user",
        "Ask the user a question. Use when you need clarification or confirmation.",
        params, {"question"}));
}

void registerEditFileImpl() {
    nlohmann::json editItem;
    editItem["type"] = "object";
    editItem["properties"]["search"] = {{"type", "string"}, {"description", "Exact text to find"}};
    editItem["properties"]["replace"] = {{"type", "string"}, {"description", "Replacement text"}};
    editItem["required"] = nlohmann::json::array({"search", "replace"});

    nlohmann::json params;
    params["path"] = {{"type", "string"}, {"description", "Path to the file to edit"}};
    params["edits"] = {{"type", "array"}, {"description", "Array of {search, replace} edits applied in order"}, {"items", editItem}};

    ALL_TOOLS.push_back(makeTool("edit_file",
        "Apply search-and-replace edits to a file. Each edit finds exact 'search' text and replaces with 'replace'. Previous content is saved in the undo log automatically. ALWAYS read the file first.",
        params, {"path", "edits"}));
}

void registerWriteFileImpl() {
    auto params = nlohmann::json::object({
        {"path", {{"type", "string"}, {"description", "Path to the new or existing file to create/overwrite"}}},
        {"content", {{"type", "string"}, {"description", "The complete file content to write"}}},
    });
    ALL_TOOLS.push_back(makeTool("write_file",
        "Create a completely new file or overwrite an existing file. Use for new files; prefer edit_file for modifications.",
        params, {"path", "content"}));
}

void registerUndoEditImpl() {
    auto params = nlohmann::json::object({
        {"path", {{"type", "string"}, {"description", "Path to the file to undo"}}},
    });
    ALL_TOOLS.push_back(makeTool("undo_edit",
        "Revert the last edit on a file by restoring from the undo log.",
        params, {"path"}));
}

void registerRunShellImpl() {
    auto params = nlohmann::json::object({
        {"command", {{"type", "string"}, {"description", "Shell command to execute"}}},
        {"working_dir", {{"type", "string"}, {"description", "Working directory (default: workspace root)"}}},
        {"timeout", {{"type", "integer"}, {"description", "Timeout in seconds (default: 60)"}}},
    });
    ALL_TOOLS.push_back(makeTool("run_shell",
        "Execute a shell command. Use for running scripts, build commands, git, etc.",
        params, {"command"}));
}

void registerRunTestsImpl() {
    auto params = nlohmann::json::object({
        {"working_dir", {{"type", "string"}, {"description", "Optional working directory (default: workspace root)"}}},
    });
    ALL_TOOLS.push_back(makeTool("run_tests",
        "Auto-detect and run the project's test suite (make test, cargo test, npm test, pytest, go test, etc.).",
        params, {}));
}

void registerReadTodosImpl() {
    auto params = nlohmann::json::object();
    ALL_TOOLS.push_back(makeTool("read_todos",
        "Read your current TODO list. Returns array of {id, title, status, description}.",
        params, {}));
}

void registerCreateUpdateTodoImpl() {
    nlohmann::json todoItem;
    todoItem["type"] = "object";
    todoItem["properties"]["id"] = {{"type", "string"}, {"description", "Unique ID for the todo"}};
    todoItem["properties"]["title"] = {{"type", "string"}, {"description", "Short title"}};
    todoItem["properties"]["status"] = {{"type", "string"}, {"description", "pending, in_progress, or done"}};
    todoItem["properties"]["description"] = {{"type", "string"}, {"description", "Optional longer description"}};
    todoItem["required"] = nlohmann::json::array({"id", "title", "status"});

    auto params = nlohmann::json::object({
        {"todos", {{"type", "array"}, {"description", "Array of todo items to create or update"}, {"items", todoItem}}},
    });
    ALL_TOOLS.push_back(makeTool("create_update_todo",
        "Create or update TODOs. Use for tracking multi-step work. Completed items are auto-pruned.",
        params, {"todos"}));
}

void registerGenerateImageImpl() {
    auto params = nlohmann::json::object({
        {"prompt", {{"type", "string"}, {"description", "Text description of the image to generate"}}},
        {"model", {{"type", "string"}, {"description", "Image model (default: grok-imagine-image). Options: grok-imagine-image, grok-imagine-image-pro"}}},
        {"n", {{"type", "integer"}, {"description", "Number of images (default: 1, max: 10)"}}},
        {"aspect_ratio", {{"type", "string"}, {"description", "Aspect ratio: 1:1, 16:9, 9:16, 4:3, 3:4, 3:2, 2:3, auto (default: auto)"}}},
        {"resolution", {{"type", "string"}, {"description", "Resolution: 1k or 2k (default: 1k)"}}},
    });
    ALL_TOOLS.push_back(makeTool("generate_image",
        "Generate an image from a text prompt using xAI's image generation API. Returns the local file path and web-servable URL.",
        params, {"prompt"}));
}

void registerEditImageImpl() {
    auto params = nlohmann::json::object({
        {"prompt", {{"type", "string"}, {"description", "Text description of the edit to apply"}}},
        {"image_path", {{"type", "string"}, {"description", "Path to the source image to edit"}}},
        {"model", {{"type", "string"}, {"description", "Image model (default: grok-imagine-image)"}}},
    });
    ALL_TOOLS.push_back(makeTool("edit_image",
        "Edit an existing image with a text prompt. Provide the source image path and describe the changes.",
        params, {"prompt", "image_path"}));
}

void registerGenerateVideoImpl() {
    nlohmann::json refItem;
    refItem["type"] = "object";
    refItem["properties"]["url"] = {{"type", "string"}, {"description", "URL of a reference image"}};

    auto params = nlohmann::json::object({
        {"prompt", {{"type", "string"}, {"description", "Text description of the video to generate"}}},
        {"model", {{"type", "string"}, {"description", "Video model (default: grok-imagine-video)"}}},
        {"duration", {{"type", "integer"}, {"description", "Duration in seconds, 1-15 (default: 5)"}}},
        {"aspect_ratio", {{"type", "string"}, {"description", "Aspect ratio, e.g. 16:9, 9:16, 1:1 (default: 16:9)"}}},
        {"resolution", {{"type", "string"}, {"description", "Resolution: 480p or 720p (default: 720p)"}}},
        {"image", {{"type", "string"}, {"description", "Optional source image URL or path for image-to-video (animates the image)"}}},
        {"reference_images", {{"type", "array"}, {"description", "Optional reference image URLs to guide style/content"}, {"items", refItem}}},
    });
    ALL_TOOLS.push_back(makeTool("generate_video",
        "Generate a video from a text prompt. Optionally provide an image to animate (image-to-video) or reference images for style guidance. Async, may take 1-3 minutes.",
        params, {"prompt"}));
}

void registerEditVideoImpl() {
    auto params = nlohmann::json::object({
        {"prompt", {{"type", "string"}, {"description", "Text description of the edit to apply to the video"}}},
        {"video", {{"type", "string"}, {"description", "Source video URL or workspace-relative path"}}},
        {"model", {{"type", "string"}, {"description", "Video model (default: grok-imagine-video)"}}},
    });
    ALL_TOOLS.push_back(makeTool("edit_video",
        "Edit an existing video with a text prompt. The model preserves the scene and modifies only what you describe. Max input: 8.7 seconds. Async.",
        params, {"prompt", "video"}));
}

void registerExtendVideoImpl() {
    auto params = nlohmann::json::object({
        {"prompt", {{"type", "string"}, {"description", "Text describing what should happen next in the video"}}},
        {"video", {{"type", "string"}, {"description", "Source video URL or workspace-relative path to extend"}}},
        {"model", {{"type", "string"}, {"description", "Video model (default: grok-imagine-video)"}}},
        {"duration", {{"type", "integer"}, {"description", "Duration of the extension in seconds (default: 5)"}}},
    });
    ALL_TOOLS.push_back(makeTool("extend_video",
        "Extend an existing video by appending new generated content that continues from the last frame. The extension duration is added to the original.",
        params, {"prompt", "video"}));
}

// ── Tool Forge: AI self-building tools ──────────────────

void registerCreateToolImpl() {
    nlohmann::json paramItem;
    paramItem["type"] = "object";
    paramItem["properties"]["name"] = {{"type", "string"}, {"description", "Parameter name"}};
    paramItem["properties"]["type"] = {{"type", "string"}, {"description", "JSON Schema type: string, integer, number, boolean, array, object"}};
    paramItem["properties"]["description"] = {{"type", "string"}, {"description", "What this parameter is for"}};
    paramItem["properties"]["required"] = {{"type", "boolean"}, {"description", "Whether this parameter is required (default: false)"}};
    paramItem["required"] = nlohmann::json::array({"name", "type", "description"});

    auto params = nlohmann::json::object({
        {"name", {{"type", "string"}, {"description", "Tool name (alphanumeric + underscore, will be prefixed with custom_ if not already)"}}},
        {"description", {{"type", "string"}, {"description", "What the tool does — shown to the AI when deciding to use it"}}},
        {"script", {{"type", "string"}, {"description", "Complete Python script. Must read JSON from sys.argv[1], process it, and print JSON to stdout with a 'result' key."}}},
        {"parameters", {{"type", "array"}, {"description", "Array of parameter definitions"}, {"items", paramItem}}},
    });
    ALL_TOOLS.push_back(makeTool("create_tool",
        "Create a new callable Python tool. The tool becomes immediately available for use in this and future sessions. "
        "The script reads input JSON from sys.argv[1] and prints result JSON to stdout.",
        params, {"name", "description", "script"}));
}

void registerModifyToolImpl() {
    auto params = nlohmann::json::object({
        {"name", {{"type", "string"}, {"description", "Name of the custom tool to modify"}}},
        {"description", {{"type", "string"}, {"description", "Updated description (optional, omit to keep current)"}}},
        {"script", {{"type", "string"}, {"description", "Updated Python script (optional, omit to keep current)"}}},
    });
    ALL_TOOLS.push_back(makeTool("modify_tool",
        "Modify an existing custom tool's code or description. Only custom tools (not built-in) can be modified.",
        params, {"name"}));
}

void registerDeleteToolImpl() {
    auto params = nlohmann::json::object({
        {"name", {{"type", "string"}, {"description", "Name of the custom tool to delete"}}},
    });
    ALL_TOOLS.push_back(makeTool("delete_tool",
        "Delete a custom tool. Only custom tools (not built-in) can be deleted.",
        params, {"name"}));
}

void registerListCustomToolsImpl() {
    auto params = nlohmann::json::object();
    ALL_TOOLS.push_back(makeTool("list_custom_tools",
        "List all custom (AI-created) tools with their names, descriptions, and parameter schemas.",
        params, {}));
}

// ── API Research & Registry ─────────────────────────────

void registerResearchApiImpl() {
    auto params = nlohmann::json::object({
        {"query", {{"type", "string"}, {"description", "API to research, e.g. 'Notion API', 'Stripe payments API', 'OpenWeatherMap API'"}}},
        {"focus", {{"type", "string"}, {"description", "Optional focus: 'auth' (authentication), 'endpoints' (available endpoints), 'quickstart' (getting started), or 'full' (default: full)"}}},
    });
    ALL_TOOLS.push_back(makeTool("research_api",
        "Research a third-party API using live web search. Returns structured information: base URL, authentication method, "
        "key endpoints, rate limits, and documentation links. Use this to discover how to integrate with any API.",
        params, {"query"}));
}

void registerSetupApiImpl() {
    nlohmann::json endpointItem;
    endpointItem["type"] = "object";
    endpointItem["properties"]["method"] = {{"type", "string"}, {"description", "HTTP method: GET, POST, PUT, DELETE, PATCH"}};
    endpointItem["properties"]["path"] = {{"type", "string"}, {"description", "URL path (appended to base_url), e.g. /v1/users"}};
    endpointItem["properties"]["description"] = {{"type", "string"}, {"description", "What this endpoint does"}};
    endpointItem["properties"]["params"] = {{"type", "object"}, {"description", "Parameter schema for this endpoint"}};

    auto params = nlohmann::json::object({
        {"slug", {{"type", "string"}, {"description", "Unique identifier for this API (lowercase, e.g. 'notion', 'stripe', 'openweather')"}}},
        {"name", {{"type", "string"}, {"description", "Human-readable API name"}}},
        {"base_url", {{"type", "string"}, {"description", "Base URL for API requests"}}},
        {"auth_type", {{"type", "string"}, {"description", "Authentication type: bearer, api_key_header, api_key_query, basic, custom"}}},
        {"auth_config", {{"type", "object"}, {"description", "Auth details: {header_name, query_param, token_prefix, etc.}"}}},
        {"vault_key_name", {{"type", "string"}, {"description", "Name of the vault entry holding the API key (use vault_store first)"}}},
        {"endpoints", {{"type", "array"}, {"description", "Array of endpoint definitions"}, {"items", endpointItem}}},
        {"documentation_url", {{"type", "string"}, {"description", "URL to the API documentation"}}},
        {"research_notes", {{"type", "string"}, {"description", "Notes from API research"}}},
    });
    ALL_TOOLS.push_back(makeTool("setup_api",
        "Register a third-party API in the API registry. After setup, you can use call_api to make authenticated requests. "
        "Store the API key in the vault first using vault_store, then reference it via vault_key_name.",
        params, {"slug", "name", "base_url", "auth_type"}));
}

void registerCallApiImpl() {
    auto params = nlohmann::json::object({
        {"slug", {{"type", "string"}, {"description", "API registry slug (e.g. 'notion', 'stripe')"}}},
        {"method", {{"type", "string"}, {"description", "HTTP method: GET, POST, PUT, DELETE, PATCH (default: GET)"}}},
        {"path", {{"type", "string"}, {"description", "API endpoint path (appended to base_url)"}}},
        {"body", {{"type", "object"}, {"description", "Request body for POST/PUT/PATCH (sent as JSON)"}}},
        {"query_params", {{"type", "object"}, {"description", "URL query parameters"}}},
        {"headers", {{"type", "object"}, {"description", "Additional HTTP headers"}}},
    });
    ALL_TOOLS.push_back(makeTool("call_api",
        "Make an authenticated HTTP request to a registered API. The API key is automatically retrieved from the encrypted vault "
        "and applied according to the API's auth configuration. Returns the response body and status code.",
        params, {"slug", "path"}));
}

void registerListApiRegistryImpl() {
    auto params = nlohmann::json::object();
    ALL_TOOLS.push_back(makeTool("list_api_registry",
        "List all registered APIs with their names, base URLs, auth types, and available endpoints.",
        params, {}));
}

// ── Encrypted Vault ─────────────────────────────────────

void registerVaultStoreImpl() {
    auto params = nlohmann::json::object({
        {"name", {{"type", "string"}, {"description", "Key name (e.g. 'notion_api_key', 'stripe_secret')"}}},
        {"service", {{"type", "string"}, {"description", "Service name for organization (e.g. 'Notion', 'Stripe')"}}},
        {"value", {{"type", "string"}, {"description", "The secret value to encrypt and store"}}},
    });
    ALL_TOOLS.push_back(makeTool("vault_store",
        "Store an API key or secret in the encrypted vault (AES-256-GCM). Use ask_user to get the key from the user, "
        "never hardcode secrets. The value is encrypted at rest and only decrypted when needed by call_api.",
        params, {"name", "value"}));
}

void registerVaultListImpl() {
    auto params = nlohmann::json::object();
    ALL_TOOLS.push_back(makeTool("vault_list",
        "List all entries in the encrypted vault. Returns names and services only — never the secret values.",
        params, {}));
}

void registerVaultRetrieveImpl() {
    auto params = nlohmann::json::object({
        {"name", {{"type", "string"}, {"description", "Vault entry name to retrieve"}}},
    });
    ALL_TOOLS.push_back(makeTool("vault_retrieve",
        "Retrieve a decrypted secret from the vault. Use sparingly — prefer call_api which handles auth automatically.",
        params, {"name"}));
}

void registerVaultRemoveImpl() {
    auto params = nlohmann::json::object({
        {"name", {{"type", "string"}, {"description", "Vault entry name to remove"}}},
    });
    ALL_TOOLS.push_back(makeTool("vault_remove",
        "Remove an entry from the encrypted vault.",
        params, {"name"}));
}

void registerSearchAssetsImpl() {
    auto params = nlohmann::json::object({
        {"query", {{"type", "string"}, {"description", "Search term to find assets by name, tags, or description"}}},
        {"type", {{"type", "string"}, {"description", "Filter by type: image, video, document, code (optional)"}}},
    });
    ALL_TOOLS.push_back(makeTool("search_assets",
        "Search uploaded assets (images, videos, documents, code files) by name, semantic tags, or description. Returns matching file URLs and metadata.",
        params, {"query"}));
}

void registerSummarizeAndNewChatImpl() {
    auto params = nlohmann::json::object({
        {"summary", {{"type", "string"}, {"description", "Detailed summary of the conversation so far including key decisions, code changes, and context"}}},
        {"continuation_prompt", {{"type", "string"}, {"description", "The prompt to continue with in the new chat session"}}},
    });
    ALL_TOOLS.push_back(makeTool("summarize_and_new_chat",
        "When the conversation context is getting very long, use this to create a new chat session with a condensed summary. Preserves important context while freeing up token space.",
        params, {"summary", "continuation_prompt"}));
}

} // namespace

void ToolRegistry::registerAll() {
    if (REGISTERED) return;
    registerReadFileImpl();
    registerSearchFilesImpl();
    registerListDirectoryImpl();
    registerGlobFilesImpl();
    registerReadUrlImpl();
    registerReadProjectNotesImpl();
    registerAddMemoryImpl();
    registerSearchMemoryImpl();
    registerForgetMemoryImpl();
    registerWebSearchImpl();
    registerXSearchImpl();
    registerAskUserImpl();
    registerEditFileImpl();
    registerWriteFileImpl();
    registerUndoEditImpl();
    registerRunShellImpl();
    registerRunTestsImpl();
    registerReadTodosImpl();
    registerCreateUpdateTodoImpl();
    registerGenerateImageImpl();
    registerEditImageImpl();
    registerGenerateVideoImpl();
    registerEditVideoImpl();
    registerExtendVideoImpl();
    // Tool Forge
    registerCreateToolImpl();
    registerModifyToolImpl();
    registerDeleteToolImpl();
    registerListCustomToolsImpl();
    // API Research & Registry
    registerResearchApiImpl();
    registerSetupApiImpl();
    registerCallApiImpl();
    registerListApiRegistryImpl();
    // Encrypted Vault
    registerVaultStoreImpl();
    registerVaultListImpl();
    registerVaultRetrieveImpl();
    registerVaultRemoveImpl();
    // Assets + Chat management
    registerSearchAssetsImpl();
    registerSummarizeAndNewChatImpl();
    REGISTERED = true;
    spdlog::debug("ToolRegistry: Registered {} tools", ALL_TOOLS.size());
}

std::vector<nlohmann::json> ToolRegistry::getToolsForMode(Mode mode) {
    if (!REGISTERED) registerAll();
    std::vector<nlohmann::json> filtered;
    for (const auto& t : ALL_TOOLS) {
        std::string name = t["function"]["name"];
        if (toolAllowedForMode(name, mode))
            filtered.push_back(t);
    }
    return filtered;
}

bool ToolRegistry::hasTool(const std::string& name, Mode mode) {
    if (!REGISTERED) registerAll();
    for (const auto& t : ALL_TOOLS) {
        if (t["function"]["name"] == name)
            return toolAllowedForMode(name, mode);
    }
    return false;
}

std::vector<nlohmann::json> ToolRegistry::getAllTools() {
    if (!REGISTERED) registerAll();
    return ALL_TOOLS;
}

static std::unordered_set<std::string> CUSTOM_TOOL_NAMES;

void ToolRegistry::registerCustomTool(const nlohmann::json& toolDef) {
    if (!REGISTERED) registerAll();
    std::string name = toolDef["function"]["name"];
    removeCustomTool(name);
    ALL_TOOLS.push_back(toolDef);
    CUSTOM_TOOL_NAMES.insert(name);
    spdlog::debug("ToolRegistry: Registered custom tool '{}'", name);
}

void ToolRegistry::removeCustomTool(const std::string& name) {
    ALL_TOOLS.erase(
        std::remove_if(ALL_TOOLS.begin(), ALL_TOOLS.end(),
            [&name](const nlohmann::json& t) { return t["function"]["name"] == name; }),
        ALL_TOOLS.end());
    CUSTOM_TOOL_NAMES.erase(name);
}

bool ToolRegistry::isCustomTool(const std::string& name) {
    return CUSTOM_TOOL_NAMES.count(name) > 0;
}

} // namespace avacli
