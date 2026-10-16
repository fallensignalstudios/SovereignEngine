#!/usr/bin/env bash
# G0-01 workspace isolation probe. Records the resolved repository and build paths, searches the
# host for the production projects named in the handoff and for Unreal Engine installations, and
# records `git status` + HEAD of every git repository found outside this one. Run it before and
# after a work session and compare the two files (scripts/collect_evidence.sh does).
#   scripts/isolation_check.sh <out-file> <label>
set -uo pipefail
out="$1"
label="$2"
repo="$(cd "$(dirname "$0")/.." && pwd -P)"
{
  echo "# isolation probe: $label"
  echo "# $(date -u +%Y-%m-%dT%H:%M:%SZ) host $(hostname) user $(id -un)"
  echo
  echo "## resolved paths"
  echo "repository: $repo"
  for d in "$repo"/build/*/; do [ -d "$d" ] && echo "build dir:  $(cd "$d" && pwd -P)"; done
  echo "repository git dir: $(git -C "$repo" rev-parse --absolute-git-dir 2>/dev/null)"
  echo "remotes: $(git -C "$repo" remote -v | wc -l) configured"
  git -C "$repo" remote -v | sed 's/^/  /'
  echo
  echo "## production projects and Unreal installations named in the handoff"
  roots=(/home /root /opt /srv /mnt /media /workspace /data /usr/local /tmp)
  existing=()
  for r in "${roots[@]}"; do [ -d "$r" ] && existing+=("$r"); done
  # Distinctive names match case-insensitively; "Origins" is a common word, so it must match exactly.
  # "the MMO" has no project name to search for: covered only by the repository list below.
  names='ProjectVelkorran|Velkorran|Battles ?of ?the ?Reach|BattlesOfTheReach|GASPALS|Cinemafy|Holofy|Grungify|UnrealEngine|UE_5[^/]*|UE_4[^/]*|Epic Games|UnrealEditor'
  hits=$(find "${existing[@]}" -xdev \( -path "$repo" -o -path /proc -o -path /sys \) -prune -o \
           -maxdepth 6 -print 2>/dev/null | { grep -Ei "/($names)[^/]*$"; true; } | grep -v "^$repo" || true)
  hits+=$(find "${existing[@]}" -xdev -maxdepth 6 -name Origins 2>/dev/null | grep -v "^$repo" || true)
  if [ -z "$hits" ]; then
    echo "none found under: ${existing[*]} (depth 6; /$names/ case-insensitive; 'Origins' exact)"
  else
    echo "$hits" | sed 's/^/FOUND: /'
  fi
  echo
  echo "## git repositories on this host other than this one (status recorded, never modified)"
  repos=$(find "${existing[@]}" -xdev -maxdepth 6 -name .git \( -type d -o -type f \) 2>/dev/null | grep -v "^$repo/" |
            grep -v '/build/[^/]*/_deps/' | sed 's#/\.git$##' | sort -u || true)
  if [ -z "$repos" ]; then
    echo "none"
  else
    while IFS= read -r r; do
      echo "$r: HEAD $(git -C "$r" rev-parse --verify -q HEAD 2>/dev/null || echo none) status entries $(git -C "$r" status --porcelain 2>/dev/null | wc -l)"
    done <<< "$repos"
  fi
} > "$out"
echo "isolation probe ($label) written to $out"
