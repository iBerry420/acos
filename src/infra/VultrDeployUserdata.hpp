#pragma once

#include <string>

namespace avacli {

/// Plain cloud-init YAML for Vultr `user_data` (caller base64-encodes for API).
/// Returns empty string for unknown / blank profile.
std::string vultrDeployUserDataForProfile(const std::string& profileId);

} // namespace avacli
