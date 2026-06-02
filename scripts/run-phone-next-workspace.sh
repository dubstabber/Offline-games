#!/bin/sh
set -eu

usage() {
    cat <<'EOF'
Usage: scripts/run-phone-next-workspace.sh [options] [--] [app args...]

Run offline-games on the next unused numeric Sway workspace.

Options:
  --release       Run build/release/offline-games.
  --dev           Run build/dev/offline-games.
  --debug         Run build/debug/offline-games.
  --exe PATH      Run a specific executable.
  --dry-run       Print what would run without switching workspace.
  -h, --help      Show this help.
EOF
}

die() {
    printf '%s\n' "$*" >&2
    exit 1
}

command_exists() {
    command -v "$1" >/dev/null 2>&1
}

repo_root() {
    script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
    dirname "$script_dir"
}

workspace_nums_with_jq() {
    jq -r '.[] | .num | select(. > 0)'
}

focused_num_with_jq() {
    jq -r '.[] | select(.focused == true) | .num' | sed -n '1p'
}

workspace_nums_with_awk() {
    awk '
        BEGIN { RS = "[{}]" }

        function workspace_num(record, value) {
            if (match(record, /"num"[[:space:]]*:[[:space:]]*-?[0-9]+/)) {
                value = substr(record, RSTART, RLENGTH)
                sub(/.*:/, "", value)
                gsub(/[[:space:]]/, "", value)
                return value
            }
            return ""
        }

        {
            num = workspace_num($0)
            if (num > 0) {
                print num
            }
        }
    '
}

focused_num_with_awk() {
    awk '
        BEGIN { RS = "[{}]" }

        function workspace_num(record, value) {
            if (match(record, /"num"[[:space:]]*:[[:space:]]*-?[0-9]+/)) {
                value = substr(record, RSTART, RLENGTH)
                sub(/.*:/, "", value)
                gsub(/[[:space:]]/, "", value)
                return value
            }
            return ""
        }

        /"focused"[[:space:]]*:[[:space:]]*true/ {
            num = workspace_num($0)
            if (num != "") {
                print num
                exit
            }
        }
    '
}

is_positive_number() {
    case "${1:-}" in
        ""|*[!0-9]*)
            return 1
            ;;
    esac

    [ "$1" -gt 0 ]
}

next_unused_workspace() {
    used_nums=$1
    focused_num=$2

    if is_positive_number "$focused_num"; then
        candidate=$((focused_num + 1))
    else
        candidate=$(printf '%s\n' "$used_nums" |
            awk 'BEGIN { max = 0 } $1 > max { max = $1 } END { print max + 1 }')
    fi

    while printf '%s\n' "$used_nums" | grep -qx "$candidate"; do
        candidate=$((candidate + 1))
    done

    printf '%s\n' "$candidate"
}

preset=
exe=
dry_run=0

while [ "$#" -gt 0 ]; do
    case "$1" in
        --release)
            preset=release
            ;;
        --dev)
            preset=dev
            ;;
        --debug)
            preset=debug
            ;;
        --exe)
            shift
            [ "$#" -gt 0 ] || die "--exe requires a path"
            exe=$1
            ;;
        --dry-run)
            dry_run=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            break
            ;;
        -*)
            die "unknown option: $1"
            ;;
        *)
            break
            ;;
    esac
    shift
done

root=$(repo_root)

if [ -n "$exe" ]; then
    :
elif [ -n "$preset" ]; then
    exe=$root/build/$preset/offline-games
else
    for candidate in \
        "$root/build/release/offline-games" \
        "$root/build/dev/offline-games" \
        "$root/build/debug/offline-games"
    do
        if [ -x "$candidate" ]; then
            exe=$candidate
            break
        fi
    done
fi

[ -n "$exe" ] || die "no executable found; build with: cmake --build build/release"
[ -x "$exe" ] || die "not executable: $exe"
command_exists swaymsg || die "swaymsg not found; run this inside sxmo-de-sway"

workspace_json=$(swaymsg -t get_workspaces) ||
    die "could not query Sway workspaces; run this inside an active sxmo-de-sway session"

if command_exists jq; then
    used_nums=$(printf '%s\n' "$workspace_json" | workspace_nums_with_jq)
    focused_num=$(printf '%s\n' "$workspace_json" | focused_num_with_jq)
else
    used_nums=$(printf '%s\n' "$workspace_json" | workspace_nums_with_awk)
    focused_num=$(printf '%s\n' "$workspace_json" | focused_num_with_awk)
fi

target_workspace=$(next_unused_workspace "$used_nums" "$focused_num")

if [ "$dry_run" -eq 1 ]; then
    printf 'workspace number %s\n' "$target_workspace"
    printf 'exec %s' "$exe"
    for arg in "$@"; do
        printf ' %s' "$arg"
    done
    printf '\n'
    exit 0
fi

swaymsg "workspace number $target_workspace" >/dev/null
exec "$exe" "$@"
