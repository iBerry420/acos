#pragma once

#include <string>
#include <vector>

namespace avacli {

/// SHA-256 of an arbitrary byte string, returned as a lowercase hex string
/// (64 chars). Used for content-addressing service dependency manifests so
/// we can skip pip/npm installs when nothing has changed.
std::string sha256Hex(const std::string& data);

/// SHA-256 of a file's contents. Returns the hex digest on success, empty
/// string if the file does not exist or could not be read. Large files are
/// streamed in 64 KiB chunks.
std::string sha256File(const std::string& path);

/// Combine the digests of several files into a single fingerprint.
/// Missing files contribute the empty string, so a manifest + lockfile
/// combination behaves gracefully when only one of the two exists.
std::string sha256Files(const std::vector<std::string>& paths);

} // namespace avacli
