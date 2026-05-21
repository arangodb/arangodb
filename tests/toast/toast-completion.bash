# Toast CLI — shell function and bash completion
# Setup: /path/to/tests/toast/toast --setup-completion

# Guard: silently no-op if this file's directory no longer exists
# (e.g., repo was moved or deleted after setup)
[[ -d "$(dirname "${BASH_SOURCE[0]}")" ]] || return 0

# --- Shell function: finds the toast script in the current repo clone ---

toast() {
  local repo_root
  repo_root="$(git rev-parse --show-toplevel 2>/dev/null)" || {
    echo "toast: not inside a git repository" >&2
    return 1
  }
  local toast_script="$repo_root/tests/toast/toast"
  if [[ ! -x "$toast_script" ]]; then
    echo "toast: script not found at $toast_script" >&2
    return 1
  fi
  "$toast_script" "$@"
}

# --- Bash completion ---

# Fallback if bash-completion package is not installed
if ! declare -F _init_completion &>/dev/null; then
  _init_completion() {
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    words=("${COMP_WORDS[@]}")
    cword=$COMP_CWORD
  }
fi

_toast_suites_dir() {
  local repo_root
  repo_root="$(git rev-parse --show-toplevel 2>/dev/null)" || return 1
  echo "$repo_root/tests/toast/suites"
}

_toast_complete_suites() {
  local cur="$1"
  local suites_dir
  suites_dir="$(_toast_suites_dir)" || return

  if [[ "$cur" == */* ]]; then
    # Complete test files within a suite: smoke/test_<TAB>
    local suite_name="${cur%%/*}"
    local suite_path="$suites_dir/$suite_name"
    if [[ -d "$suite_path" ]]; then
      local files
      files=$(cd "$suites_dir" && compgen -f "$cur" 2>/dev/null | while read -r f; do
        [[ "$f" == */test_*.exs ]] && echo "$f"
      done)
      COMPREPLY=( $(compgen -W "$files" -- "$cur") )
    fi
  else
    # Complete suite directory names
    local suites=""
    if [[ -d "$suites_dir" ]]; then
      suites=$(for d in "$suites_dir"/*/; do
        [[ -d "$d" ]] && basename "$d"
      done 2>/dev/null)
    fi
    COMPREPLY=( $(compgen -W "$suites" -- "$cur") )
    # Append / for single match to invite file completion
    if [[ ${#COMPREPLY[@]} -eq 1 ]]; then
      COMPREPLY=( "${COMPREPLY[0]}/" )
      compopt -o nospace
    fi
  fi
}

_toast_complete_run() {
  local cur="$1" prev="$2"

  # Options that expect a value — don't offer further completions for numeric ones
  case "$prev" in
    -b|--build-dir|--base-dir|--result-dir)
      compopt -o dirnames; COMPREPLY=(); return ;;
    --sanitizer)
      COMPREPLY=( $(compgen -W "tsan alubsan" -- "$cur") ); return ;;
    --rr)
      COMPREPLY=( $(compgen -W "default all" -- "$cur") ); return ;;
    --global-timeout|--test-timeout|--startup-timeout|--shutdown-timeout|\
    --timeout-factor|--memory-budget|--timeout|--max-failures|\
    --cluster-agents|--cluster-dbservers|--cluster-coordinators|\
    --replication-factor|--test|--test-buckets|--include|-i|--exclude|-e|--only)
      return ;;
  esac

  if [[ "$cur" == -* ]]; then
    local opts="
      --build-dir --base-dir --result-dir
      --cluster --single --show-server-logs
      --global-timeout --test-timeout --startup-timeout --shutdown-timeout
      --timeout-factor --memory-budget
      --keep-data --sanitizer --attach-debugger --rr --http2
      --cluster-agents --cluster-dbservers --cluster-coordinators
      --replication-factor --test-buckets
      --test --no-agency-dump --ci --force-all-tiers --help
      --include --exclude --only --trace
      --timeout --max-failures
      --color --no-color --no-compile --no-start
    "
    COMPREPLY=( $(compgen -W "$opts" -- "$cur") )
    return
  fi

  _toast_complete_suites "$cur"
}

_toast_complete_analyze() {
  local cur="$1" prev="$2"
  local words=("${COMP_WORDS[@]}")
  local cword=$COMP_CWORD

  # Find the analyze subcommand position and detect which subcommand follows
  local analyze_idx=0 subcmd=""
  for (( i=1; i < cword; i++ )); do
    if [[ "${words[i]}" == "analyze" ]]; then
      analyze_idx=$i
      break
    fi
  done

  # Look for subcommand after "analyze"
  for (( i=analyze_idx+1; i < cword; i++ )); do
    case "${words[i]}" in
      -*) continue ;;
      issues|detail|details|info|perf|weights|help)
        subcmd="${words[i]}"
        break ;;
      *) break ;;
    esac
  done

  # Handle options expecting values
  case "$prev" in
    --result-dir)
      compopt -o dirnames; COMPREPLY=(); return ;;
    --type)
      COMPREPLY=( $(compgen -W "crash test_failure sanitizer_report timeout" -- "$cur") ); return ;;
    --suite)
      _toast_complete_suites "$cur"; return ;;
    --threads)
      COMPREPLY=( $(compgen -W "relevant all" -- "$cur") ); return ;;
    --log-events)
      COMPREPLY=( $(compgen -W "none basic full" -- "$cur") ); return ;;
    --log-min-level)
      COMPREPLY=( $(compgen -W "debug info warn error" -- "$cur") ); return ;;
    --top|--backtrace-frames|--log-servers|--log-window|--log-exclude|--module)
      return ;;
  esac

  if [[ "$cur" == -* ]]; then
    local opts="--result-dir --no-color --type --suite --help"
    case "$subcmd" in
      detail|details)
        opts="$opts --logs --log-servers --log-window --log-min-level --log-exclude --log-events"
        opts="$opts --coredumps --no-coredumps --threads --backtrace-frames"
        opts="$opts --disassembly --no-disassembly"
        ;;
      perf)
        opts="$opts --top --module"
        ;;
      weights)
        opts="$opts --module"
        ;;
    esac
    COMPREPLY=( $(compgen -W "$opts" -- "$cur") )
    return
  fi

  # If no subcommand yet, complete subcommand names
  if [[ -z "$subcmd" ]]; then
    COMPREPLY=( $(compgen -W "issues detail details info perf weights help" -- "$cur") )
    return
  fi

  # After detail subcommand, complete issue specs
  if [[ "$subcmd" == "detail" || "$subcmd" == "details" ]]; then
    COMPREPLY=( $(compgen -W "all crashes test_failures sanitizer timeouts" -- "$cur") )
  fi
}

_toast_completions() {
  local cur prev words cword
  _init_completion || return

  # Detect subcommand
  local subcmd=""
  for (( i=1; i < cword; i++ )); do
    case "${words[i]}" in
      run) subcmd="run"; break ;;
      analyze) subcmd="analyze"; break ;;
      -*) continue ;;
      *) break ;;
    esac
  done

  case "$subcmd" in
    run)     _toast_complete_run "$cur" "$prev" ;;
    analyze) _toast_complete_analyze "$cur" "$prev" ;;
    *)
      # Top-level: complete subcommands (and --setup-completion)
      if [[ "$cur" == -* ]]; then
        COMPREPLY=( $(compgen -W "--help --setup-completion" -- "$cur") )
      else
        COMPREPLY=( $(compgen -W "run analyze" -- "$cur") )
      fi
      ;;
  esac
}

complete -F _toast_completions toast
