class Avacli < Formula
  desc "Autonomous AI coding agent powered by xAI Grok models"
  homepage "https://avalynn.ai"
  url "https://github.com/iBerry420/acos/archive/refs/tags/v2.2.0.tar.gz"
  sha256 "PLACEHOLDER_SHA256"
  license "MIT"

  depends_on "cmake" => :build
  depends_on "pkg-config" => :build
  depends_on "curl"

  # Optional runtimes. Only needed if the user wants to host long-running
  # python/node services (e.g. a Discord bot) through the built-in
  # supervisor. Core avacli works without them.
  depends_on "python@3.12" => :optional
  depends_on "node"        => :optional

  def install
    system "cmake", "-B", "build",
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DCMAKE_PREFIX_PATH=#{Formula["curl"].opt_prefix}",
                    *std_cmake_args
    system "cmake", "--build", "build"
    bin.install "build/avacli"
  end

  test do
    assert_match "Usage", shell_output("#{bin}/avacli --help", 0)
  end
end
