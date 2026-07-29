#!/usr/bin/env bash
# =============================================================================
#  compare_branches.sh — compare deux versions d'EvoSwarm (zip GitHub ou dossier)
#
#  Théo a reformaté beaucoup de fichiers (tabulations -> espaces, accolades),
#  donc un diff brut noie les vraies différences sous des milliers de lignes de
#  mise en forme. Ce script ignore l'espacement par défaut : ce qui ressort est
#  un vrai changement de code.
#
#  Usage :
#    ./compare_branches.sh <A> <B>                    # A et B = .zip ou dossier
#    ./compare_branches.sh -s <A> <B>                 # résumé seul (pas de diff)
#    ./compare_branches.sh -f Debug <A> <B>           # filtre sur le nom de fichier
#    ./compare_branches.sh -o rapport.txt <A> <B>     # écrit dans un fichier
#    ./compare_branches.sh --raw <A> <B>              # n'ignore PAS la mise en forme
#
#  Exemple (les deux branches de la soutenance) :
#    ./compare_branches.sh \
#      ~/Downloads/EvoSwarm-Enzo-Soutenance.zip \
#      ~/Downloads/"EvoSwarm-theo-visuels (1).zip"
# =============================================================================
set -u

SUMMARY_ONLY=0
FILTER=""
OUT=""
DIFF_OPTS=(-u -w -B)          # -w : ignore l'espacement   -B : ignore les lignes vides
CONTEXT=3

usage() { sed -n '3,21p' "$0" | sed 's/^#\{0,1\} \{0,1\}//'; exit 1; }

# Les options sont acceptées avant OU après les deux chemins.
POS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -s|--summary) SUMMARY_ONLY=1; shift ;;
    -f|--filter)  FILTER="${2:?-f attend un motif}"; shift 2 ;;
    -o|--out)     OUT="${2:?-o attend un chemin}"; shift 2 ;;
    -c|--context) CONTEXT="${2:?-c attend un nombre}"; shift 2 ;;
    --raw)        DIFF_OPTS=(-u); shift ;;
    -h|--help)    usage ;;
    -*)           echo "option inconnue : $1" >&2; usage ;;
    *)            POS+=("$1"); shift ;;
  esac
done
[[ ${#POS[@]} -eq 2 ]] || usage
set -- "${POS[@]}"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# --- prépare un côté : dézippe si besoin, puis trouve la racine Source/ -------
prep() {                       # $1 = chemin zip|dossier, $2 = étiquette
  local src="$1" tag="$2" dst="$TMP/$2"
  mkdir -p "$dst"
  if [[ -f "$src" ]]; then
    if command -v unzip >/dev/null 2>&1; then
      unzip -qq -o "$src" -d "$dst"
    else
      powershell -NoProfile -Command \
        "Expand-Archive -LiteralPath '$(cygpath -w "$src" 2>/dev/null || echo "$src")' -DestinationPath '$(cygpath -w "$dst" 2>/dev/null || echo "$dst")' -Force" >/dev/null
    fi
  elif [[ -d "$src" ]]; then
    cp -r "$src"/. "$dst"/
  else
    echo "introuvable : $src" >&2; exit 2
  fi
  # les zips GitHub imbriquent un dossier : on cherche le vrai Source/
  local root
  root="$(find "$dst" -type d -name Source -print -quit 2>/dev/null)"
  [[ -n "$root" ]] || { echo "pas de dossier Source/ dans $src" >&2; exit 2; }
  dirname "$root"
}

A_ROOT="$(prep "$1" A)"; A_NAME="$(basename "$1")"
B_ROOT="$(prep "$2" B)"; B_NAME="$(basename "$2")"

# --- liste des fichiers de code des deux côtés -------------------------------
list() { (cd "$1" && find Source Config -type f \
          \( -name '*.h' -o -name '*.cpp' -o -name '*.cs' -o -name '*.ini' \) 2>/dev/null | sort); }

list "$A_ROOT" > "$TMP/a.lst"
list "$B_ROOT" > "$TMP/b.lst"

ONLY_A="$(comm -23 "$TMP/a.lst" "$TMP/b.lst")"
ONLY_B="$(comm -13 "$TMP/a.lst" "$TMP/b.lst")"
BOTH="$(comm -12 "$TMP/a.lst" "$TMP/b.lst")"

# --- répartit les fichiers communs : identiques vs modifiés ------------------
CHANGED=""; SAME_N=0
while IFS= read -r f; do
  [[ -n "$f" ]] || continue
  if diff -q -w -B "$A_ROOT/$f" "$B_ROOT/$f" >/dev/null 2>&1; then
    SAME_N=$((SAME_N+1))
  else
    CHANGED+="$f"$'\n'
  fi
done <<< "$BOTH"
CHANGED="$(printf '%s' "$CHANGED" | sed '/^$/d')"

# --- applique le filtre éventuel --------------------------------------------
if [[ -n "$FILTER" ]]; then
  CHANGED="$(printf '%s\n' "$CHANGED" | grep -i -- "$FILTER" || true)"
  ONLY_A="$(printf '%s\n'  "$ONLY_A"  | grep -i -- "$FILTER" || true)"
  ONLY_B="$(printf '%s\n'  "$ONLY_B"  | grep -i -- "$FILTER" || true)"
fi

n() { printf '%s\n' "$1" | sed '/^$/d' | wc -l | tr -d ' '; }

report() {
  echo "==============================================================="
  echo " COMPARAISON EVOSWARM"
  echo "   A = $A_NAME"
  echo "   B = $B_NAME"
  [[ ${DIFF_OPTS[*]} == *-w* ]] && echo "   (mise en forme ignorée — utiliser --raw pour la voir)"
  [[ -n "$FILTER" ]] && echo "   filtre : $FILTER"
  echo "==============================================================="
  echo
  echo "RÉSUMÉ"
  echo "  identiques        : $SAME_N"
  echo "  modifiés          : $(n "$CHANGED")"
  echo "  seulement dans A  : $(n "$ONLY_A")"
  echo "  seulement dans B  : $(n "$ONLY_B")"
  echo

  if [[ -n "$ONLY_A" ]]; then
    echo "--- SUPPRIMÉS dans B (présents seulement dans A) ---"
    printf '%s\n' "$ONLY_A" | sed 's/^/  - /'; echo
  fi
  if [[ -n "$ONLY_B" ]]; then
    echo "--- AJOUTÉS dans B ---"
    printf '%s\n' "$ONLY_B" | sed 's/^/  + /'; echo
  fi
  if [[ -n "$CHANGED" ]]; then
    echo "--- MODIFIÉS (ampleur : lignes de diff) ---"
    while IFS= read -r f; do
      [[ -n "$f" ]] || continue
      local_lines=$(diff "${DIFF_OPTS[@]}" -U0 "$A_ROOT/$f" "$B_ROOT/$f" 2>/dev/null \
                    | grep -cE '^[+-][^+-]' || true)
      printf '  %6s  %s\n' "$local_lines" "$f"
    done <<< "$CHANGED"
    echo
  fi

  [[ $SUMMARY_ONLY -eq 1 ]] && return

  if [[ -n "$CHANGED" ]]; then
    echo
    echo "==============================================================="
    echo " DIFFS DÉTAILLÉS"
    echo "==============================================================="
    while IFS= read -r f; do
      [[ -n "$f" ]] || continue
      echo
      echo "###############################################################"
      echo "### $f"
      echo "###############################################################"
      diff "${DIFF_OPTS[@]}" -U"$CONTEXT" \
           --label "A/$f" --label "B/$f" \
           "$A_ROOT/$f" "$B_ROOT/$f" || true
    done <<< "$CHANGED"
  fi
}

if [[ -n "$OUT" ]]; then
  report > "$OUT"
  echo "rapport écrit : $OUT  ($(wc -l < "$OUT" | tr -d ' ') lignes)"
else
  report
fi
