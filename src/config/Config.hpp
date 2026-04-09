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
