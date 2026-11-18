#!/bin/bash
# Build a raw SD card image with a FAT file system and a fixed set of example files.
#
#   make_sd_image.sh <output.img> [fat32|fat16] [mbr|superfloppy]
#
# The file set is fixed so the C64 directory listing can be checked automatically
# (see SD_DIR_EXPECT in rp2_test_runner.c).
set -o errexit -o nounset -o pipefail

out=${1:?usage: $0 <output.img> [fat32|fat16] [mbr|superfloppy]}
fat=${2:-fat32}
layout=${3:-mbr}

here=$(cd "$(dirname "$0")" && pwd)

case "$fat" in
  fat32) size_mib=64; fat_bits=32 ;;
  fat16) size_mib=16; fat_bits=16 ;;
  *) echo "unknown FAT type $fat" >&2; exit 1 ;;
esac

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

rm -f "$out"
truncate -s "${size_mib}M" "$out"

if [ "$layout" = mbr ]; then
  # One partition starting at 1 MiB, type 0x0c (FAT32 LBA) or 0x0e (FAT16 LBA)
  part_type=$([ "$fat" = fat32 ] && echo c || echo e)
  echo "2048,,${part_type}" | sfdisk --quiet "$out"
  offset_sectors=2048
else
  offset_sectors=0
fi

fs_sectors=$(( size_mib * 2048 - offset_sectors ))
mkfs.fat -F "$fat_bits" -n SD2IECTEST --offset="$offset_sectors" "$out" $(( fs_sectors / 2 )) >/dev/null
target="$out@@$(( offset_sectors * 512 ))"

# Deterministic file contents. Sizes are chosen for distinct block counts
# (sd2iec shows ceil(size / 254) blocks).
make_prg() { # name size
  { printf '\x01\x08'; head -c $(( $2 - 2 )) /dev/zero | tr '\0' 'A'; } > "$tmp/$1"
}
make_prg HELLO.PRG 200            # 1 block
make_prg BIG.PRG 10160            # 40 blocks
head -c 600 /dev/zero | tr '\0' 'R' > "$tmp/README"          # 3 blocks, no CBM extension
make_prg "Long Mixed Name.prg" 520                           # 3 blocks, LFN entry
cp "$here/../docs/1541_test_demo.d64" "$tmp/1541_test_demo.d64"
cp "$here/../docs/1541_test_demo.d64" "$tmp/IMAGE.D64"       # 8.3 name for CD:IMAGE.D64

export MTOOLS_SKIP_CHECK=1
mcopy -i "$target" "$tmp/HELLO.PRG" "$tmp/BIG.PRG" "$tmp/README" \
      "$tmp/Long Mixed Name.prg" "$tmp/1541_test_demo.d64" "$tmp/IMAGE.D64" ::/
mmd -i "$target" ::/GAMES
make_prg INSIDE.PRG 300
mcopy -i "$target" "$tmp/INSIDE.PRG" ::/GAMES/

echo "$out: $fat, $layout, ${size_mib} MiB"
mdir -i "$target" ::/
