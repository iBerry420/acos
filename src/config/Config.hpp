#pragma once

#include "core/Types.hpp"
#include <string>

namespace avacli {

struct Config {
    std::string model = "grok-4-1-reasoning";
    Mode        mode = Mode::Question;
    std::string session;
    std::string workspace;
    std::string task;
    bool nonInteractive = false;
    bool tuiVerbose = true;
    std::string outputFormat = "text";

    // Serve mode
    bool serveMode = false;
    int  servePort = 8080;
    // Set by main.cpp when --port was passed on the command line so
    // HttpServer can refuse to fall back to port+1 and fail fast
    // instead (the configured port is part of the deployment contract
    // with any fronting reverse proxy).
    bool servePortExplicit = false;
    std::string serveHost = "0.0.0.0";

    // Detached UI options
    std::string uiDir;
    bool useEmbeddedUI = false;
    std::string uiTheme;

    std::string xaiApiKey() const;
    bool        validate() const;
    bool        validateServe() const;
    void        resolveWorkspace();
};

} // namespace avacli
