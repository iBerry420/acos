class Avacli < Formula
  desc "Autonomous AI coding agent powered by xAI Grok models"
  homepage "https://avalynn.ai"
  url "https://github.com/iBerry420/avacli/archive/refs/tags/v1.3.1.tar.gz"
  sha256 "PLACEHOLDER_SHA256"
  license "MIT"

  depends_on "cmake" => :build
  depends_on "pkg-config" => :build
  depends_on "curl"

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
