#!/usr/bin/env bash
set -euo pipefail

function Q {
	find ./data/ \
		-not -path ./data/fontdata.json \
		-not -path ./data/mods/replacements.json \
		-name '*.json' \
		-exec jq "${@}" {} +
}

# One temp DIRECTORY, removed on exit. Upstream left two loose mktemp files per
# run -- free in CI where the container is discarded, but they accumulate on a
# developer machine now that tools/hooks/pre-commit runs this on every commit
# staging data JSON (~260 kB a run). The trap preserves the exit status below,
# and this drops mktemp --suffix, a GNU extension BSD and macOS lack.
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

SPECIAL_OF_TERRAIN="$TMP_DIR/special-of-terrain.json"
Q 'def skip(to_skip): select(all(. != to_skip; .));
    if type=="array" then .[] else . end
	| select(type=="object")
	| select(.type=="overmap_special")
	| .id as $id
	| .overmaps[].overmap
    | select(. != null)
	| sub("_(north|south|east|west)$"; "")
	# Skip common terrains to prevent false positives
	| skip("forest", "forest_thick", "forest_water", "field", "road_ew")
	| [., $id]' \
| jq --null-input \
	'reduce inputs as [$k, $v] ({}; .[$k] += [$v])
	| map_values(unique)' \
> "$SPECIAL_OF_TERRAIN"

MISSING_OM_SPECIAL="$TMP_DIR/missing-om-special.json"
Q --slurpfile special_of_terrain "$SPECIAL_OF_TERRAIN" \
   	'if type=="array" then .[] else . end
	| select(type=="object")
   	| select(.type=="mission_definition")
   	| .id as $id
   	| .start?.assign_mission_target?
	# NOTE: these are jq comments inside a single-quoted shell string, so no
	# apostrophes -- one would close the string and break the script.
	#
	# om_terrain may be a variable object resolved from game state at runtime --
	# no terrain name to look up, so skip it. Indexing the map with an object
	# raises a jq error, and the jq exit status reflects only its LAST input, so
	# that error is erased by the next file that parses.
   	| select(.om_terrain | type == "string")
   	| .XXX_NEEDS_SPECIAL=$special_of_terrain[][.om_terrain]
   	| select(.XXX_NEEDS_SPECIAL)
	# om_special takes the same string-or-object form, and an object never equals
	# a string, so a mission using the variable form would fall through the
	# comparison below and be reported as MISSING an om_special it plainly has.
	# An absent om_special is null, not an object, so the case this check exists
	# to catch is still reported.
	| select(.om_special | type != "object")
	| select(all(.om_special != .XXX_NEEDS_SPECIAL[]; .))
   	| {id: $id, om_special, XXX_NEEDS_SPECIAL}' \
| jq --slurp unique > "$MISSING_OM_SPECIAL"

if jq --exit-status '. != []' < "$MISSING_OM_SPECIAL" > /dev/null; then
	echo "The following missions lack .start.assign_mission_target.om_special
See https://github.com/CleverRaven/Cataclysm-DDA/blob/master/doc/MISSIONS_JSON.md#assign_mission_target
> If the om_terrain is part of an overmap special, it's essential to specify
> the om_special value as well--otherwise, the game will not know how to spawn
> the entire special." >&2
	cat "$MISSING_OM_SPECIAL"
	exit 1
fi
