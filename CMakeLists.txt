cmake_minimum_required(VERSION 3.22)

# Allow FetchContent dependencies with older cmake_minimum_required on CMake 4.x+
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "")

project(avacli LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# CLI11
FetchContent_Declare(
  CLI11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG        v2.4.2
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(CLI11)

# nlohmann_json
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# spdlog
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.13.0
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(spdlog)

# cpp-httplib (header-only HTTP server/client)
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.18.3
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(httplib)

# System libcurl
find_package(CURL REQUIRED)

# System OpenSSL (for VaultStorage AES-256-GCM encryption)
find_package(OpenSSL REQUIRED)

# System SQLite3
find_package(SQLite3 REQUIRED)

# Auto-regenerate src/server/EmbeddedAssets.hpp from ui/ before compiling.
# ui/ is the canonical source; EmbeddedAssets.hpp is the compiled-in fallback
# served when no on-disk UI exists (e.g. after a fresh apt install).
# See scripts/bundle-ui.py.
find_package(Python3 COMPONENTS Interpreter)
set(AVACLI_UI_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/index.html"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/css/variables.css"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/css/style.css"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/js/app.js"
)
set(AVACLI_EMBEDDED_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/src/server/EmbeddedAssets.hpp")
if(Python3_Interpreter_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/bundle-ui.py")
  add_custom_command(
    OUTPUT "${AVACLI_EMBEDDED_ASSETS}"
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/bundle-ui.py"
    DEPENDS ${AVACLI_UI_SOURCES}
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/bundle-ui.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/split_embedded_strings.py"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Bundling ui/ into EmbeddedAssets.hpp"
    VERBATIM
  )
  add_custom_target(avacli_embedded_ui DEPENDS "${AVACLI_EMBEDDED_ASSETS}")
else()
  message(WARNING
    "Python3 not found; EmbeddedAssets.hpp will NOT auto-regenerate from ui/. "
    "Run scripts/bundle-ui.py manually after editing ui/ sources."
  )
endif()

# Auto-regenerate src/server/DefaultSystemPrompt.hpp from GROK_SYSTEM_PROMPT.md
# before compiling. Same pattern as the UI bundler: the markdown file is the
# canonical source, the generated header is the compiled-in fallback used by
# AgentEngine and the /api/system/prompt/{default,reset} routes.
# See scripts/bundle-default-prompt.py.
set(AVACLI_DEFAULT_PROMPT_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/src/server/DefaultSystemPrompt.hpp")
if(Python3_Interpreter_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/bundle-default-prompt.py")
  add_custom_command(
    OUTPUT "${AVACLI_DEFAULT_PROMPT_HEADER}"
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/bundle-default-prompt.py"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/GROK_SYSTEM_PROMPT.md"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/bundle-default-prompt.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/split_embedded_strings.py"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Bundling GROK_SYSTEM_PROMPT.md into DefaultSystemPrompt.hpp"
    VERBATIM
  )
  add_custom_target(avacli_default_prompt DEPENDS "${AVACLI_DEFAULT_PROMPT_HEADER}")
else()
  message(WARNING
    "Python3 not found; DefaultSystemPrompt.hpp will NOT auto-regenerate. "
    "Run scripts/bundle-default-prompt.py manually after editing GROK_SYSTEM_PROMPT.md."
  )
endif()

# Source files
# Build id for /api/status (git short hash when available)
set(AVACLI_BUILD_ID "unknown")
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse --short=10 HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE AVACLI_BUILD_ID
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
endif()
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_info.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/build_info.h"
  @ONLY)

set(SOURCES
  src/main.cpp
  src/config/ApiKeyStorage.cpp
  src/config/VaultStorage.cpp
  src/config/ServeSettings.cpp
  src/config/PlatformSettings.cpp
  src/config/Config.cpp
  src/config/ModelRegistry.cpp
  src/client/XAIClient.cpp
  src/client/BatchClient.cpp
  src/client/VultrClient.cpp
  src/client/NodeClient.cpp
  src/infra/VultrDeployUserdata.cpp
  src/auth/AuthManager.cpp
  src/auth/MasterKeyManager.cpp
  src/core/AgentEngine.cpp
  src/core/Application.cpp
  src/server/HttpServer.cpp
  src/server/UIFileServer.cpp
  src/server/ServerHelpers.cpp
  src/server/RoutesAuth.cpp
  src/server/RoutesChat.cpp
  src/server/RoutesFiles.cpp
  src/server/RoutesTools.cpp
  src/server/RoutesData.cpp
  src/server/RoutesSettings.cpp
  src/server/RoutesInfra.cpp
  src/server/RoutesDB.cpp
  src/server/RoutesKnowledge.cpp
  src/server/RoutesApps.cpp
  src/server/RoutesAppDB.cpp
  src/server/AppTokens.cpp
  src/server/RoutesServices.cpp
  src/server/RoutesSubAgents.cpp
  src/server/RoutesBatches.cpp
  src/server/RoutesSystem.cpp
  src/server/LogBuffer.cpp
  src/db/Database.cpp
  src/db/AppDatabase.cpp
  src/services/ServiceManager.cpp
  src/services/DepInstaller.cpp
  src/services/MeshManager.cpp
  src/services/RelayManager.cpp
  src/services/RelayClient.cpp
  src/services/BatchPoller.cpp
  src/server/RoutesRelay.cpp
  src/session/SessionManager.cpp
  src/tools/ProjectNotes.cpp
  src/tools/ToolExecutor.cpp
  src/tools/ToolRegistry.cpp
  src/agents/LeaseManager.cpp
  src/agents/SubAgentManager.cpp
  src/agents/ScopedToolExecutor.cpp
  src/platform/Paths.cpp
  src/platform/ProcessRunner.cpp
  src/platform/ProcessSupervisor.cpp
  src/platform/Runtime.cpp
  src/platform/Hash.cpp
)

add_executable(avacli ${SOURCES})

if(TARGET avacli_embedded_ui)
  add_dependencies(avacli avacli_embedded_ui)
endif()
if(TARGET avacli_default_prompt)
  add_dependencies(avacli avacli_default_prompt)
endif()

target_include_directories(avacli PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated
  ${spdlog_SOURCE_DIR}/include
)

target_link_libraries(avacli PRIVATE
  CLI11::CLI11
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  httplib::httplib
  CURL::libcurl
  OpenSSL::SSL
  OpenSSL::Crypto
  SQLite::SQLite3
)

if(MSVC)
  target_compile_definitions(avacli PRIVATE NOMINMAX)
endif()

install(TARGETS avacli RUNTIME DESTINATION bin)
