#!/usr/bin/env bash


ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MYSH="$ROOT/mysh"
WORK="$(mktemp -d)"
trap 'pkill -P $$ sleep 2>/dev/null; rm -rf "$WORK"' EXIT

if [[ ! -x "$MYSH" ]]; then
    echo "mysh binary not found - run 'make' first" >&2
    exit 1
fi

cd "$WORK" || exit 1

pass=0
fail=0

ok()  { pass=$((pass + 1)); printf '  \033[32mPASS\033[0m  %s\n' "$1"; }
bad() { fail=$((fail + 1)); printf '  \033[31mFAIL\033[0m  %s\n' "$1"; shift
        printf '        %s\n' "$@"; }


run_sh() { printf '%s\n' "$@" | "$MYSH" 2>&1; }


run_out() { printf '%s\n' "$@" | "$MYSH" 2>/dev/null; }


expect_eq() {
    if [[ "$3" == "$2" ]]; then ok "$1"
    else bad "$1" "expected: $(printf %q "$2")" "     got: $(printf %q "$3")"; fi
}


expect_has() {
    if [[ "$3" == *"$2"* ]]; then ok "$1"
    else bad "$1" "expected to contain: $2" "     got: $(printf %q "$3")"; fi
}


expect_lacks() {
    if [[ "$3" != *"$2"* ]]; then ok "$1"
    else bad "$1" "should not contain: $2" "     got: $(printf %q "$3")"; fi
}


wait_for_text() {
    local i
    for ((i = 0; i < ${3:-40}; i++)); do
        grep -q -- "$2" "$1" 2>/dev/null && return 0
        sleep 0.1
    done
    return 1
}

echo "== basic execution =="
expect_eq  "runs a command"                 "hello"        "$(run_sh 'echo hello')"
expect_eq  "passes arguments"               "a b c"        "$(run_sh 'echo a b c')"
expect_eq  "extra whitespace ignored"       "x y"          "$(run_sh '   echo    x     y  ')"
expect_eq  "blank lines ignored"            "ok"           "$(run_sh '' '   ' 'echo ok')"
expect_eq  "double quotes keep spaces"      "a   b"        "$(run_sh 'echo "a   b"')"
expect_eq  "single quotes keep operators"   "x | y > z"    "$(run_sh "echo 'x | y > z'")"
expect_eq  "waits for foreground command"   $'first\nsecond' \
           "$(run_sh "sh -c 'sleep 0.3; echo first'" 'echo second')"
expect_has "unknown command reported"       "nosuchcmd: command not found" "$(run_sh 'nosuchcmd')"
expect_eq  "shell survives bad command"     "still here"   "$(run_out 'nosuchcmd' 'echo still here')"
expect_has "non-executable file reported"   "Permission denied" "$(touch noexec; run_sh './noexec')"

echo "== built-ins: pwd / cd / exit =="
expect_eq  "pwd matches system pwd"         "$(pwd -P)"    "$(run_sh 'pwd')"
mkdir -p d1/d2
expect_eq  "cd into directory"              "$WORK/d1"     "$(run_sh 'cd d1' 'pwd')"
expect_eq  "cd .. goes up"                  "$WORK"        "$(run_sh 'cd d1/d2' 'cd ..' 'cd ..' 'pwd')"
expect_eq  "cd with no args goes to HOME"   "$WORK/d1"     "$(HOME="$WORK/d1" run_sh 'cd' 'pwd')"
expect_eq  "cd ~ expands HOME"              "$WORK/d1"     "$(HOME="$WORK/d1" run_sh 'cd ~' 'pwd')"
expect_eq  "cd ~/sub expands HOME"          "$WORK/d1/d2"  "$(HOME="$WORK/d1" run_sh 'cd ~/d2' 'pwd')"
expect_eq  "cd - returns and prints dir"    "$WORK"        "$(run_sh 'cd /tmp' 'cd -')"
expect_eq  "cd - twice toggles"             "$WORK/d1"     "$(run_sh 'cd d1' 'cd ..' 'cd -' 'pwd' | tail -1)"
expect_has "cd to missing dir errors"       "cd: /nonexistent: No such file or directory" "$(run_sh 'cd /nonexistent')"
expect_eq  "cd failure keeps directory"     "$WORK"        "$(run_out 'cd /nonexistent' 'pwd')"
expect_has "cd too many args"               "too many arguments" "$(run_sh 'cd a b')"
expect_has "cd - without OLDPWD"            "OLDPWD not set" "$(env -u OLDPWD "$MYSH" <<< 'cd -' 2>&1)"
expect_has "cd on a file errors"            "Not a directory" "$(run_sh 'cd noexec')"
expect_eq  "child sees updated PWD"         "$WORK/d1"     "$(run_sh 'cd d1' "sh -c 'echo \$PWD'")"
expect_eq  "cd inside a pipeline is local"  "$WORK"        "$(run_sh 'cd d1 | cat' 'pwd')"
expect_eq  "pwd can be redirected"          "$WORK"        "$(run_sh 'pwd > pwd.out' 'cat pwd.out')"
"$MYSH" <<< 'exit 3' ; expect_eq "exit N sets status" "3" "$?"
"$MYSH" <<< 'exit'   ; expect_eq "exit sets status 0" "0" "$?"
"$MYSH" <<< $'false\nexit'; expect_eq "exit keeps last status" "1" "$?"
"$MYSH" < /dev/null  ; expect_eq "EOF exits cleanly" "0" "$?"
expect_eq  "commands after exit not run"    ""             "$(run_sh 'exit' 'echo nope')"

echo "== output redirection (> and >>) =="
expect_eq  "> creates file"                 "hi"           "$(run_sh 'echo hi > o1.txt' 'cat o1.txt')"
expect_eq  "> overwrites"                   "second"       "$(run_sh 'echo first > o2.txt' 'echo second > o2.txt' 'cat o2.txt')"
expect_eq  ">> appends"                     $'one\ntwo'    "$(run_sh 'echo one > o3.txt' 'echo two >> o3.txt' 'cat o3.txt')"
expect_eq  ">> creates missing file"        "new"          "$(run_sh 'echo new >> o4.txt' 'cat o4.txt')"
expect_eq  "no space around >"              "nospace"      "$(run_sh 'echo nospace>o5.txt' 'cat o5.txt')"
expect_eq  "nothing printed when redirected" ""            "$(run_sh 'echo hidden > o6.txt')"
expect_has "> into missing dir errors"      "No such file or directory" "$(run_sh 'echo x > nodir/f')"
expect_eq  "shell survives redirect error"  "alive"        "$(run_out 'echo x > nodir/f' 'echo alive')"

echo "== input redirection (<) =="
printf 'b\na\nc\n' > in.txt
expect_eq  "< feeds stdin"                  $'a\nb\nc'     "$(run_sh 'sort < in.txt')"
expect_eq  "< and > together"               $'a\nb\nc'     "$(run_sh 'sort < in.txt > sorted.txt' 'cat sorted.txt')"
expect_has "< missing file errors"          "missing.txt: No such file or directory" "$(run_sh 'cat < missing.txt')"
expect_eq  "wc -l < file"                   "3"            "$(run_sh 'wc -l < in.txt')"

echo "== pipes =="
expect_eq  "two-command pipe"               "3"            "$(run_sh 'cat in.txt | wc -l')"
expect_eq  "three-command pipe"             "2"            "$(run_sh 'echo one two | tr a-z A-Z | wc -w')"
expect_eq  "four-command pipe"              "A"            "$(run_sh 'sort in.txt | head -1 | tr a-z A-Z | cat')"
expect_eq  "pipe with < on first"           "3"            "$(run_sh 'cat < in.txt | wc -l')"
expect_eq  "pipe with > on last"            "3"            "$(run_sh 'cat in.txt | wc -l > cnt.txt' 'cat cnt.txt')"
expect_eq  "pipe with no spaces"            "3"            "$(run_sh 'cat in.txt|wc -l')"
expect_eq  "large data through pipe"        "20000"        "$(run_sh 'seq 1 20000 | cat | wc -l')"
expect_eq  "pipe stops on SIGPIPE quietly"  "1"            "$(run_sh 'seq 1 1000000 | head -1')"
expect_eq  "builtin in a pipeline"          "$WORK"        "$(run_sh 'pwd | cat')"

echo "== background jobs (&) =="
start=$(date +%s%N)
out="$(run_out 'sleep 2 > /dev/null &' 'echo prompt-came-back')"
elapsed=$(( ($(date +%s%N) - start) / 1000000 ))
expect_has "& returns to prompt at once"    "prompt-came-back" "$out"
if (( elapsed < 1500 )); then ok "& did not wait for the job (${elapsed} ms)"
else bad "& did not wait for the job" "took ${elapsed} ms"; fi
expect_has "& reports the pid"              "[bg] pid " "$(run_out 'true &')"
out="$(run_sh 'echo A & echo B' | grep -v '^\[bg\]' | sort | tr '\n' ' ')"
expect_eq  "a & b runs both"                "A B " "$out"
out="$(run_sh 'echo X & echo Y &' | grep -c '^\[bg\]')"
expect_eq  "a & b & starts two jobs"        "2" "$out"
expect_eq  "background pipeline"            "3" "$(run_out 'cat in.txt | wc -l > bgp.txt &' 'sleep 0.3' 'cat bgp.txt' | grep -v '^\[bg\]')"
expect_eq  "background with redirect"       "bgr" "$(run_out 'echo bgr > bgr.txt &' 'sleep 0.3' 'cat bgr.txt' | grep -v '^\[bg\]')"

echo "== ps / kill =="
expect_eq  "ps with nothing running"        "    PID  COMMAND" "$(run_sh 'ps')"
expect_has "ps rejects arguments"           "takes no arguments" "$(run_sh 'ps aux')"
expect_has "kill without pid: usage"        "usage: kill <pid>" "$(run_sh 'kill')"
expect_has "kill non-number rejected"       "positive process id" "$(run_sh 'kill abc')"
expect_has "kill 0 rejected"                "positive process id" "$(run_sh 'kill 0')"
expect_has "kill negative rejected"         "positive process id" "$(run_sh 'kill -9')"
expect_has "kill unknown pid refused"       "no such process started by mysh" "$(run_sh 'kill 1')"
expect_has "kill won't touch foreign pid"   "no such process started by mysh" "$(run_sh "kill $$")"


mkfifo session.in
"$MYSH" < session.in > session.out 2>&1 &
shell_pid=$!
exec 3> session.in
echo 'sleep 60 &'       >&3
echo 'sleep 61 &'       >&3
if wait_for_text session.out 'pid'; then
    pid1=$(grep -m1 '^\[bg\] pid' session.out | awk '{print $3}')
    echo 'ps'           >&3
    wait_for_text session.out 'COMMAND'
    kill -0 "$pid1" 2>/dev/null && alive_before=yes || alive_before=no
    echo "kill $pid1"   >&3
    echo 'ps'           >&3
    echo "kill $pid1"   >&3
    echo 'exit'         >&3
    exec 3>&-
    wait "$shell_pid"
    kill -0 "$pid1" 2>/dev/null && alive_after=yes || alive_after=no
    session=$(<session.out)
    first_ps=${session%%COMMAND*}
    rest=${session#*COMMAND}
    expect_eq  "job is alive before kill"        "yes" "$alive_before"
    expect_has "ps lists the sleep 60 job"       "sleep 60" "$session"
    expect_has "ps shows its pid"                "$pid1  sleep 60" "$session"
    expect_has "ps lists both jobs"              "sleep 61" "$rest"
    expect_eq  "kill terminated the process"     "no" "$alive_after"
    second_ps=${rest#*COMMAND}
    second_ps=${second_ps%%no such process*}
    expect_lacks "kill removed its ps record"    "$pid1  sleep 60" "$second_ps"
    expect_has "killing twice is an error"       "no such process started by mysh" "$session"
else
    exec 3>&-
    kill "$shell_pid" 2>/dev/null
    bad "live ps/kill session" "background job never reported its pid"
fi
pkill -f 'sleep 6[01]' 2>/dev/null

expect_eq  "finished job leaves ps"         "    PID  COMMAND" "$(run_sh 'sleep 0.1 &' 'sleep 0.5' 'ps' | grep -v '^\[bg\]')"

echo "== syntax errors and invalid input =="
expect_has "trailing pipe"                  "missing command after '|'"  "$(run_sh 'ls |')"
expect_has "leading pipe"                   "missing command before '|'" "$(run_sh '| ls')"
expect_has "double pipe"                    "missing command before '|'" "$(run_sh 'ls | | wc')"
expect_has "lone &"                         "missing command before '&'" "$(run_sh '&')"
expect_has "redirect without file"          "expected a file name after '>'" "$(run_sh 'ls >')"
expect_has "input redirect without file"    "expected a file name after '<'" "$(run_sh 'ls <')"
expect_has "redirect without command"       "redirection without a command"  "$(run_sh '> f')"
expect_has "unterminated quote"             "unterminated double quote" "$(run_sh 'echo "abc')"
expect_eq  "shell survives syntax errors"   "fine" "$(run_out 'ls |' '&' 'echo "x' 'echo fine')"
expect_eq  "very long line"                 "5000" "$(run_sh "echo $(printf 'a%.0s' $(seq 1 5000)) | wc -c | awk '{print \$1-1}'")"
expect_eq  "many arguments"                 "1000" "$(run_sh "echo $(seq -s' ' 1 1000) | wc -w")"
expect_has "mysh rejects arguments"         "usage:" "$("$MYSH" extra 2>&1)"

echo "== terminal behaviour (Ctrl-C / Ctrl-D) =="
if command -v python3 >/dev/null 2>&1; then
    tty_out=$(python3 "$ROOT/tests/test_tty.py" "$MYSH" 2>&1); tty_rc=$?
    echo "$tty_out" | sed 's/^/  /'
    if (( tty_rc == 0 )); then pass=$((pass + 1)); else fail=$((fail + 1)); fi
else
    echo "  (python3 not found - skipped)"
fi

echo
echo "functional tests: $pass passed, $fail failed"
(( fail == 0 ))
