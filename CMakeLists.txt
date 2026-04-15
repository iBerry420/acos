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

# Source files
set(SOURCES
  src/main.cpp
  src/config/ApiKeyStorage.cpp
  src/config/VaultStorage.cpp
  src/config/ServeSettings.cpp
  src/config/Config.cpp
  src/config/ModelRegistry.cpp
  src/client/XAIClient.cpp
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
  src/server/RoutesServices.cpp
  src/server/RoutesSystem.cpp
  src/server/LogBuffer.cpp
  src/db/Database.cpp
  src/services/ServiceManager.cpp
  src/services/MeshManager.cpp
  src/session/SessionManager.cpp
  src/tools/ProjectNotes.cpp
  src/tools/ToolExecutor.cpp
  src/tools/ToolRegistry.cpp
  src/platform/Paths.cpp
  src/platform/ProcessRunner.cpp
)

add_executable(avacli ${SOURCES})

target_include_directories(avacli PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include
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
