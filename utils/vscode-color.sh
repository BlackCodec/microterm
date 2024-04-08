#!/bin/bash
# Simple bash script that can convert vscode base16 
# color schema to working microterm color schema

source_file="${1}"
debug=0

if [[ "${source_file}" == "-d" ]]; then
  debug=1 
  source_file="${2}"
fi

if [[ -z "${source_file}" ]] || [[ ! -f "${source_file}" ]]; then
  echo "Usage ${0} <source_file>"
  exit 1
fi

# grep background
function extract() {
  local search_s="${1}"
  cat "${source_file}" | grep -i "${search_s}" | awk -F ":" '{print $2}' | sed 's/\"//g' | sed "s|\#|0x|g" | sed "s|,||g"
}

function translate() {
  local search="${1}"
  local key="${2}"
  local value
  [[ ${debug} == 1 ]] && echo "[TRANSLATE] Search: ${search}" && echo "[TRANSLATE] Key: ${key}"
  value=$(extract "${search}")
  [[ ${debug} == 1 ]] && ([[ -z "${value}" ]] && echo "[TRANSLATE] ${search} not found!" || echo "[TRANSLATE] extracted: ${value}")
  [[ ! -z "${value}" ]] && echo "${key} ${value}"
}

param_list="background foreground foreground:foreground_bold cursor.background:cursor cursor.foreground:cursor_foreground"
color_list="black:0 blue:1 green:2 cyan:3 red:4 magenta:5 yellow:6 white:7"
bright_list="black:8 blue:9 green:10 cyan:11 red:12 magenta:13 yellow:14 white:15"

echo "# Source: ${source_file}"

echo "# global "
for p in $(echo "${param_list}"); do
  [[ ${debug} == 1 ]] && echo "[MAIN] Param: ${p}"
  k=${p}
  v=${p}
  if [[ ${p} == *":"* ]]; then
    k=$(echo "${p}" | awk -F ":" '{print $1}')
    v=$(echo "${p}" | awk -F ":" '{print $2}')
  fi
  [[ ${k} == *"."* ]] || k=".${k}"
  [[ ${debug} == 1 ]] && echo "[MAIN] Search for ${k} -> ${v}"
  translate "terminal${k}" "${v}"
done
echo
echo "# colors"
for p in $(echo "${color_list}"); do
  [[ ${debug} == 1 ]] && echo "[MAIN] Color: ${p}"
  if [[ ${p} == *":"* ]]; then
    k=$(echo "${p}" | awk -F ":" '{print $1}')
    v=$(echo "${p}" | awk -F ":" '{print $2}')
    [[ ${debug} == 1 ]] && echo "[MAIN] Search for ${k} -> ${v}"
    translate "ansi${k}" "color${v}"
  else
    [[ ${debug} == 1 ]] && echo "[MAIN] Invalid color format!"
    exit 2
  fi
done
for p in $(echo "${bright_list}"); do
  [[ ${debug} == 1 ]] && echo "[MAIN] Bright: ${p}"
  if [[ ${p} == *":"* ]]; then
    k=$(echo "${p}" | awk -F ":" '{print $1}')
    v=$(echo "${p}" | awk -F ":" '{print $2}')
    [[ ${debug} == 1 ]] && echo "[MAIN] Search for ${k} -> ${v}"
    translate "ansiBright${k}" "color${v}"
  else
    [[ ${debug} == 1 ]] && echo "[MAIN] Invalid bright color format!"
    exit 2
  fi
done
exit 0
