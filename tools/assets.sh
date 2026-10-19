#!/bin/sh
# assets.sh — Open-format asset pipeline for PSXLinux games.
#
# The PNG/OBJ files are the source of truth (editable in GIMP, Blender,
# etc.); the PlayStation TIM/TMD binaries are generated from them.
#
# Usage:
#   assets.sh extract [dir]   .tim/.tmd  ->  .png/.obj   (+ .json sidecars)
#   assets.sh build   [dir]   .png/.obj  ->  .tim/.tmd  (for the game CD)
#   assets.sh verify  [dir]  round-trip both ways, byte-compare
#
# All converters are pure Python 3 (no dependencies):
#   tim2png.py  png2tim.py  tmd2obj.py  obj2tmd.py

set -e
TOOLS="$(cd "$(dirname "$0")" && pwd)"

DIR="${2:-$(dirname "$0")/../rs_race2/data}"
cd "$DIR"

TIMS="numbers.tim title.tim back.tim"
TMDS="ship.tmd"

case "${1:-}" in
extract)
	for f in $TIMS; do
		[ -f "$f" ] && python3 "$TOOLS/tim2png.py" "$f"
	done
	for f in $TMDS; do
		[ -f "$f" ] && python3 "$TOOLS/tmd2obj.py" "$f"
	done
	;;

build)
	for f in $TIMS; do
		png="${f%.tim}.png"
		[ -f "$png" ] && python3 "$TOOLS/png2tim.py" "$png"
	done
	for f in $TMDS; do
		obj="${f%.tmd}.obj"
		[ -f "$obj" ] && python3 "$TOOLS/obj2tmd.py" "$obj"
	done
	;;

verify)
	# extract to open formats, rebuild, compare
	rm -rf .verify-tmp .verify-orig
	mkdir .verify-tmp .verify-orig
	for f in $TIMS $TMDS; do
		cp "$f" .verify-orig/
	done
	(cd .verify-tmp && cp ../.verify-orig/* . && \
	 for f in $TIMS; do python3 "$TOOLS/tim2png.py" "$f" >/dev/null; done && \
	 for f in $TMDS; do python3 "$TOOLS/tmd2obj.py" "$f" >/dev/null; done && \
	 for f in $TIMS; do
		png="${f%.tim}.png"
		python3 "$TOOLS/png2tim.py" "$png" >/dev/null
	 done && \
	 for f in $TMDS; do
		obj="${f%.tmd}.obj"
		python3 "$TOOLS/obj2tmd.py" "$obj" >/dev/null
	 done)
	ok=1
	for f in $TIMS $TMDS; do
		if cmp -s ".verify-tmp/$f" ".verify-orig/$f"; then
			echo "  $f: round-trip OK"
		else
			echo "  $f: ROUND-TRIP FAILED"
			ok=0
		fi
	done
	rm -rf .verify-tmp .verify-orig
	[ "$ok" = 1 ] && echo "All assets verified." || exit 1
	;;

*)
	echo "Usage: $0 {extract|build|verify} [asset-dir]" >&2
	exit 1
	;;
esac
