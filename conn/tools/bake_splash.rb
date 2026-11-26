#!/usr/bin/env ruby
# frozen_string_literal: true

# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
#
# Conn boot-splash baker.
#
# Authoring is SVG (conn/art/splash/<key>.svg); the firmware never sees SVG or
# PNG. Each logo is rasterised OFFLINE with rsvg-convert and split into:
#
#   * a palette of up to 15 colours (the SVG fills, black excluded),
#   * an index plane: per pixel, which palette colour it is,
#   * a coverage plane: per pixel, 0..255 intensity of that colour.
#
# Black is never a palette entry: a #000 fill (eyes, grins, beaks) bakes as zero
# coverage, i.e. a real hole onto the black splash background, and the
# antialiased rim between a colour and black becomes partial coverage, so those
# edges stay smooth. Edges between two non-black colours snap to the nearer one.
# At runtime ConnLaunchSplash() draws palette[index] * coverage. Both planes use
# the same RLE (raw 256x256 planes are too big for a bootloader uncompressed).
#
# Needs only rsvg-convert and Ruby's stdlib (zlib decodes the PNG).
#
# Regenerate with:  ruby conn/tools/bake_splash.rb   (or: make -C conn splash)

require "open3"
require "tempfile"
require "zlib"

SIZE = 256 # square cell (CONN_SPLASH_W == CONN_SPLASH_H)
MAX_COLOURS = 15
MIN_PIXELS = 24 # a fill must own this many opaque pixels to earn a palette slot

ART = File.expand_path("../art/splash", __dir__)
OUT = File.expand_path("../core/splash_atlas.h", __dir__)

# Order is the runtime index order. Index 0 is the generic fallback. The keys
# are matched (case-insensitive substring of the cleaned entry title) in THIS
# order by ConnLaunchSplash(), so the generic "linux" MUST come last -- else
# "Arch Linux" would match "linux" before "arch". By the same rule "omarchy"
# must precede "arch", which it contains. Unmatched OSes fall back to the
# OSType char map, then to index 0.
KEYS = %w[meridian cydia macos windows omarchy arch
          freebsd dragonfly openbsd netbsd ghostbsd 9front linux].freeze

# Coverage-plane RLE: run marker = runlen+125 (>=128) then the byte;
# literal marker = len-1 (<128) then len bytes. Runs are 3..130 bytes.
def compress_rle(raw)
  out = []
  lit = []
  flush = lambda do
    lit.each_slice(128) { |s| out << (s.size - 1) << s }
    lit = []
  end
  i = 0
  n = raw.size
  while n - i >= 3
    c = raw[i]
    if raw[i + 1] == c && raw[i + 2] == c
      run = 3
      run += 1 while run < 130 && i + run < n && raw[i + run] == c
      flush.call
      out << (run + 125) << c
      i += run
    else
      lit << c
      i += 1
    end
  end
  lit.concat(raw[i..])
  flush.call
  out.flatten
end

# Decode an 8-bit RGBA, non-interlaced PNG (what rsvg-convert writes) into a
# flat array of [r, g, b, a] pixels, row-major.
def decode_png(blob, path)
  abort "#{path}: not a PNG" unless blob.byteslice(0, 8) == "\x89PNG\r\n\x1A\n".b
  pos = 8
  idat = +"".b
  w = h = nil
  while pos < blob.bytesize
    len, type = blob.byteslice(pos, 8).unpack("Na4")
    data = blob.byteslice(pos + 8, len)
    case type
    when "IHDR"
      w, h, depth, ctype, _comp, _filter, interlace = data.unpack("NNCCCCC")
      unless depth == 8 && ctype == 6 && interlace.zero?
        abort "#{path}: need 8-bit RGBA non-interlaced, got depth=#{depth} type=#{ctype}"
      end
    when "IDAT" then idat << data
    when "IEND" then break
    end
    pos += 12 + len
  end
  raw = Zlib::Inflate.inflate(idat).bytes
  stride = w * 4
  prev = Array.new(stride, 0)
  px = []
  h.times do |y|
    base = y * (stride + 1)
    ftype = raw[base]
    line = raw[base + 1, stride]
    stride.times do |x|
      a = x >= 4 ? line[x - 4] : 0
      b = prev[x]
      c = x >= 4 ? prev[x - 4] : 0
      line[x] = (line[x] + case ftype
                           when 0 then 0
                           when 1 then a
                           when 2 then b
                           when 3 then (a + b) >> 1
                           when 4
                             p = a + b - c
                             pa = (p - a).abs
                             pb = (p - b).abs
                             pc = (p - c).abs
                             pa <= pb && pa <= pc ? a : (pb <= pc ? b : c)
                           else abort "#{path}: bad PNG filter #{ftype}"
                           end) & 0xFF
    end
    line.each_slice(4) { |q| px << q }
    prev = line
  end
  abort "#{path}: rendered #{w}x#{h}, want #{SIZE}x#{SIZE}" unless w == SIZE && h == SIZE
  px
end

def rasterise(key)
  svg = File.join(ART, "#{key}.svg")
  abort "missing splash art: #{svg}" unless File.file?(svg)
  Tempfile.create(["splash", ".png"]) do |tmp|
    _, err, st = Open3.capture3("rsvg-convert", "-w", SIZE.to_s, "-h", SIZE.to_s,
                                "-o", tmp.path, svg)
    abort "rsvg-convert failed on #{svg}: #{err}" unless st.success?
    decode_png(File.binread(tmp.path), svg)
  end
end

# The SVG fill colours: exact RGB of fully opaque, non-black pixels, most
# common first (ties keep first-seen order).
def palette_of(px, key)
  counts = {}
  px.each do |r, g, b, a|
    next unless a == 255 && r + g + b > 24

    rgb = [r, g, b]
    counts[rgb] = (counts[rgb] || 0) + 1
  end
  pal = counts.each_with_index
              .sort_by { |(_, n), i| [-n, i] }
              .filter_map { |(rgb, n), _| rgb if n >= MIN_PIXELS }
  abort "#{key}: no non-black fill found" if pal.empty?
  if pal.size > MAX_COLOURS
    abort "#{key}: #{pal.size} fill colours, max #{MAX_COLOURS}; " \
          "use fewer fills (gradients are not supported)"
  end
  pal
end

# Per pixel: best palette colour P and scale s so that s*P ~ rgb; the coverage
# is alpha*s. A pixel between P and black is just P at lower s.
def planes(px, pal)
  pp = pal.map { |c| [c, (c[0] * c[0]) + (c[1] * c[1]) + (c[2] * c[2])] }
  idx = []
  cov = []
  px.each do |r, g, b, a|
    if a.zero?
      idx << 0
      cov << 0
      next
    end
    best = nil
    bi = 0
    bs = 0.0
    pp.each_with_index do |(c, cc), i|
      s = ((r * c[0]) + (g * c[1]) + (b * c[2])).fdiv(cc).clamp(0.0, 1.0)
      e = ((r - (s * c[0]))**2) + ((g - (s * c[1]))**2) + ((b - (s * c[2]))**2)
      next unless best.nil? || e < best

      best = e
      bi = i
      bs = s
    end
    idx << bi
    cov << (a * bs).round(half: :even)
  end
  [idx, cov]
end

n = KEYS.size
pals = []
comp = []
cidx = []
KEYS.each do |k|
  px = rasterise(k)
  pal = palette_of(px, k)
  idx, cov = planes(px, pal)
  pals << pal
  comp << compress_rle(cov)
  cidx << compress_rle(idx)
end
raw_total = 2 * n * SIZE * SIZE
comp_total = comp.sum(&:size) + cidx.sum(&:size)

hex = ->(rgb) { format("0x%02X%02X%02X", *rgb) }
L = []
# REUSE-IgnoreStart -- header of the GENERATED file, not ours
L << "// SPDX-License-Identifier: GPL-3.0-or-later"
L << "// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>"
# REUSE-IgnoreEnd
L << "// GENERATED by conn/tools/bake_splash.rb -- do not edit by hand."
L << "#ifndef CONN_SPLASH_ATLAS_H"
L << "#define CONN_SPLASH_ATLAS_H"
L << "#define CONN_SPLASH_W #{SIZE}"
L << "#define CONN_SPLASH_H #{SIZE}"
L << "#define CONN_SPLASH_COUNT #{n}"
L << "// index 0 == generic fallback; matched by ConnLaunchSplash()"
L << "static const char conn_splash_key[CONN_SPLASH_COUNT][16] = {"
L << "  #{KEYS.map { |k| "\"#{k}\"" }.join(', ')}"
L << "};"
L << "// Per-logo palette, 0x00RRGGBB; black is never an entry (it bakes as"
L << "// zero coverage)."
L << "#define CONN_SPLASH_MAX_COLOURS #{MAX_COLOURS}"
L << "static const unsigned int conn_splash_pal[CONN_SPLASH_COUNT][CONN_SPLASH_MAX_COLOURS] = {"
pals.each { |pal| L << "  {#{pal.map(&hex).join(', ')}}," }
L << "};"
L << "// RLE planes; each expands to CONN_SPLASH_W*CONN_SPLASH_H bytes."
L << "// conn_splash_rle_N: 0..255 coverage. conn_splash_idx_N: palette index."
n.times do |gi|
  L << "static const unsigned char conn_splash_rle_#{gi}[] = {#{comp[gi].join(',')}};"
  L << "static const unsigned char conn_splash_idx_#{gi}[] = {#{cidx[gi].join(',')}};"
end
L << "static const unsigned char * const conn_splash_rle[CONN_SPLASH_COUNT] = {"
L << "  #{(0...n).map { |gi| "conn_splash_rle_#{gi}" }.join(', ')}"
L << "};"
L << "static const unsigned int conn_splash_rle_len[CONN_SPLASH_COUNT] = {"
L << "  #{comp.map(&:size).join(', ')}"
L << "};"
L << "static const unsigned char * const conn_splash_idx[CONN_SPLASH_COUNT] = {"
L << "  #{(0...n).map { |gi| "conn_splash_idx_#{gi}" }.join(', ')}"
L << "};"
L << "static const unsigned int conn_splash_idx_len[CONN_SPLASH_COUNT] = {"
L << "  #{cidx.map(&:size).join(', ')}"
L << "};"
L << "#endif"
File.write(OUT, "#{L.join("\n")}\n")
rel = OUT.delete_prefix("#{Dir.pwd}/")
puts format("baked %d splash logos -> %s (%dx%d); RLE %d of %d bytes (%.1f%%)",
            n, rel, SIZE, SIZE, comp_total, raw_total, 100.0 * comp_total / raw_total)
