#!/bin/sh
# A throwaway RoMac machine with Rorth, for testing this port (David's OK, 2026-11-22).
# Its data folder is work/romac (git-ignored); David's own ~/Library/RoMac is only read
# (for !RISC3D, !MesaLibs and !SharedLibs), never written.
#
#   tools/romac-target.sh setup    make work/romac and the ro-run target "romac-n64"
#   tools/romac-target.sh start    start RoMac with no window (log: work/romac.log)
#   tools/romac-target.sh stop
#   tools/romac-target.sh status
#
# ROMAC_APP: the RoMac.app to run (default rol-run's build-mac, 0.1.229 or later: RISC3D is in the main line).
# RISC OS port of Mupen64Plus. Copyright (C) 2026 David Sharp. GPL-2.0.
set -e
repo=$(cd "$(dirname "$0")/.." && pwd)
app=${ROMAC_APP:-$HOME/code/rol-run/build-mac/RoMac.app}
res=$app/Contents/Resources
data=$repo/work/romac
pidf=$repo/work/romac.pid
log=$repo/work/romac.log
port=${ROMAC_RORTH_PORT:-7964}
target=romac-n64
mine=$HOME/Library/RoMac/hd4/!Boot/Resources
rorth_zip=$(ls "$HOME"/code/RORTH/dist/Rorth-*.zip | grep -v pi1 | sort -V | tail -1)

running() { [ -f "$pidf" ] && kill -0 "$(cat "$pidf")" 2>/dev/null; }

case "$1" in
setup)
	running && { echo "stop it first"; exit 1; }
	mkdir -p "$data"
	# the disc: the app's HardDisc4 (ditto keeps extended attributes)
	[ -d "$data/hd4" ] || ditto "$res/hd4" "$data/hd4"
	# RISC3D, Mesa and the shared libraries, as on David's disc (HardDisc4 1.0.0072)
	for d in '!RISC3D' '!MesaLibs' '!SharedLibs'; do
		rm -rf "$data/hd4/!Boot/Resources/$d"
		ditto "$mine/$d" "$data/hd4/!Boot/Resources/$d"
	done
	# Rorth: the newest release (0.36.2 or later: crash dumps no longer break GCC 10 programs on
	# RoMac, Rorth#28), its Config, and a PreDesk entry
	rm -rf "$data/hd4/!Rorth"
	python3 -I "$HOME/code/RORTH/tools/roszip.py" "$rorth_zip" "$data/hd4" >/dev/null
	tok=$(python3 -c "import secrets; print(secrets.token_hex(12))")
	cfg=$(ls "$data/hd4/!Rorth/"Config*)
	printf 'port   %s\ntoken  %s\nallow  127.0.0.1\nname   %s\nrequirelease on\n' \
		"$port" "$tok" "$target" > "$cfg"
	pd="$data/hd4/!Boot/Choices/Boot/PreDesk"
	mkdir -p "$pd"
	printf 'Run IXFS:$.riscos.HardDisc4.!Rorth.!Run\n' > "$pd/!!!Rorth,feb"
	# SharedSoundBuffer and StreamManager (John Duffell's freeware), which SDL's RISC OS audio
	# driver needs: from David's disc, as he has them
	for m in SSBuffer StreamMan; do
		ditto "$HOME/Library/RoMac/hd4/!Boot/Resources/!System/350/Modules/$m" \
			"$data/hd4/!Boot/Resources/!System/350/Modules/$m"
	done
	mkdir -p "$data/hd4/Work"
	# the ro-run target (replacing an earlier one of the same name)
	t=$HOME/.config/ro-run/targets.toml
	cp "$t" "$t.bak-romac-n64"
	python3 -I - "$t" "$target" "$port" "$tok" <<'EOF'
import re, sys
path, name, port, tok = sys.argv[1:]
s = open(path).read()
s = re.sub(r'\n\[targets\.%s\]\n(?:(?!\n\[).)*' % re.escape(name), '', s, flags=re.S)
s = s.rstrip('\n') + f'''

[targets.{name}]
# Throwaway RoMac (riscos-n64's tools/romac-target.sh), data in riscos-n64/work/romac
kind = "real"
host = "127.0.0.1"
port = {port}
token = "{tok}"
filenames = "long"
root = "IXFS:$.riscos.HardDisc4.Work"
'''
open(path, 'w').write(s)
EOF
	echo "set up $data; ro-run target $target on port $port"
	;;
start)
	running && { echo "already running (pid $(cat "$pidf"))"; exit 0; }
	[ -d "$data/hd4/!Rorth" ] || { echo "run setup first"; exit 1; }
	ROMAC_RESOURCES=$res ROMAC_DATA=$data ROMAC_BIN=$app/Contents/MacOS/RoMac \
	RUNWAY_NO_WINDOW=1 RUNWAY_ALLOW_UNTESTED=0 \
		nohup "$res/romac" > "$log" 2>&1 &
	echo $! > "$pidf"
	echo "started, pid $!; log $log"
	;;
stop)
	if running; then kill -TERM "$(cat "$pidf")"; sleep 2; running && kill -KILL "$(cat "$pidf")"; fi
	rm -f "$pidf"; echo stopped
	;;
status)
	running && echo "running, pid $(cat "$pidf")" || echo "not running"
	;;
*)
	sed -n 2,12p "$0"; exit 1 ;;
esac
