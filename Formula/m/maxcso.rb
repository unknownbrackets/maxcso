class Maxcso < Formula

  desc "Fast cso compressor"
  homepage "https://github.com/unknownbrackets/maxcso"
  url "https://github.com/unknownbrackets/maxcso/archive/refs/tags/v1.13.0.zip"
  sha256 "09df940ddd48a299aa9a1aed85c495aea1be8b9185cab5b893f6cd6bdb3fc822"
  license "ISC License"

  depends_on "lz4"
  depends_on "libuv"
  depends_on "libdeflate"
  depends_on "pkg-config"

  def install
    system "make"
    bin.install "maxcso"
  end

  test do
    system "#{bin}/maxcso", "--help"
  end

end
