#!/usr/bin/env bash


ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

if ! command -v valgrind >/dev/null 2>&1; then
    echo "valgrind is not installed (sudo apt install valgrind)" >&2
    exit 1
fi

LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT

timeout 120 valgrind --leak-check=full --show-leak-kinds=all \
         --errors-for-leak-kinds=all --track-fds=yes --error-exitcode=99 \
         ./mysh < tests/gdb/stress_input.txt > /dev/null 2> "$LOG"

fail=0
check() {   # check "description" <grep-pattern> <expected: present|absent>
    if grep -Eq -- "$2" "$LOG"; then found=present; else found=absent; fi
    if [[ $found == "$3" ]]; then
        printf '  \033[32mPASS\033[0m  %s\n' "$1"
    else
        printf '  \033[31mFAIL\033[0m  %s\n' "$1"; fail=1
    fi
}

echo "== valgrind: scripted session =="
check "no memory errors"         'ERROR SUMMARY: [1-9]'               absent
check "summary was produced"     'ERROR SUMMARY: 0 errors'            present
check "no invalid reads/writes"  'Invalid (read|write)'               absent
check "no uninitialised values"  'uninitialised value'                absent
check "nothing definitely lost"  'definitely lost: [1-9]'             absent
check "nothing indirectly lost"  'indirectly lost: [1-9]'             absent
check "nothing still reachable"  'still reachable: [1-9]'             absent
check "no leaked descriptors"    'Open file descriptor [3-9]'         absent

if (( fail )); then
    echo "--- valgrind log ---"; cat "$LOG"
    exit 1
fi
echo "valgrind: clean"
