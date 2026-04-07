#pragma once

#include <string>

namespace avacli {

struct ProcessRunResult {
    std::string capturedOutput;
    int exitCode = -1;
    bool timedOut = false;
};

/// Runs `userCommand` in a shell with working directory `cwd` (empty = current).
/// Stdout and stderr are merged into capturedOutput. Kills the process if it runs past timeoutSeconds.
ProcessRunResult runShellCommand(const std::string& cwd, const std::string& userCommand, int timeoutSeconds);

} // namespace avacli
