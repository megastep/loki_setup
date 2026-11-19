#! /bin/sh
#
# Product setup script
#
uid=$(/usr/bin/id -u) || exit 1
if [ "$uid" = 0 ]; then
    PATH=/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/local/sbin
    export PATH
fi

# The same check runs on both sides of the authorization boundary.
trusted_path_code='
trusted_path()
(
    path=$1
    kind=$2
    trust_owner=0
    case "$path" in /*) ;; *) return 1 ;; esac
    [ "$kind" = directory ] && [ -d "$path" ] ||
        { [ "$kind" = file ] && [ -f "$path" ]; } || return 1
    while :; do
        [ ! -L "$path" ] || return 1
        metadata=$(/usr/bin/stat -c "%u %a" "$path" 2>/dev/null) ||
            metadata=$(/usr/bin/stat -f "%u %Op" "$path" 2>/dev/null) || return 1
        set -- $metadata
        [ "$#" = 2 ] && [ "$1" = "$trust_owner" ] || return 1
        case "$2" in ""|*[!0-7]*) return 1 ;; esac
        mode=$((0$2))
        if [ $((mode & 022)) -ne 0 ]; then
            [ "$kind" = ancestor ] && [ $((mode & 01000)) -ne 0 ] || return 1
        fi
        [ "$path" = / ] && return 0
        path=${path%/*}
        path=${path:-/}
        kind=ancestor
    done
)
'
eval "$trusted_path_code"

# Go to the proper setup directory (if not already there)
CDPATH= cd "$(dirname "$0")" || exit 1
media_dir=$(pwd -P) || exit 1
launcher=$media_dir/${0##*/}
if [ "$uid" = 0 ] && ! trusted_path "$launcher" file; then
    printf '%s\n' "Administrator access requires protected installation media." >&2
    exit 1
fi

# defaults
FATAL_ERROR="Fatal error, no tech support email configured in this setup"
# try to get root prior to running setup?
# 0: no
# 1: prompt, but run anyway if fails
# 2: require, abort if root fails
GET_ROOT=0
# See if we have the XPG4 utilities (Solaris)
if test -d /usr/xpg4/bin; then
	PATH=/usr/xpg4/bin:$PATH
fi

# Return the appropriate architecture string
DetectARCH()
{
	status=1
	case `uname -m` in
	    arm64 | aarch64)
		echo "aarch64"
		status=0;;
	    amd64 | x86_64)
		echo "x86_64"
		status=0;;
	    i?86 | i86*)
		echo "x86"
		status=0;;
	    90*/*) 
		echo "hppa"
		status=0;;
	    *)
		case `uname -s` in
		    IRIX*)
			echo "mips"
			status=0;;
            AIX*)
            echo "ppc"
            status=0;;
		    *)
			arch=`uname -p 2> /dev/null || uname -m`
			if test "$arch" = powerpc; then
				echo "ppc"
			else
				echo $arch
			fi
			status=0;;
		esac
	esac
	return $status
}

# Return the appropriate libc string - completely obsolete, assume glibc-2.1 for compatibility
DetectLIBC()
{
	echo "glibc-2.1"
	return 1
}

DetectOS()
{
	os=`uname -s`
	if test "$os" = "OpenUNIX"; then
		echo SCO_SV
	else
		echo $os
	fi
	return 0
}

# Detect the environment
arch=`DetectARCH`
libc=`DetectLIBC`
os=`DetectOS`
args=""

# Import preferences from a secondary script
if [ -f setup.data/config.sh ]; then
    [ "$uid" != 0 ] || trusted_path "$media_dir/setup.data/config.sh" file || exit 1
    . setup.data/config.sh
elif [ -f SETUP.DAT/CONFIG.SH\;1 ]; then
	# HP-UX and other systems unable to get LFN correctly
    [ "$uid" != 0 ] || trusted_path "$media_dir/SETUP.DAT/CONFIG.SH;1" file || exit 1
	. SETUP.DAT/CONFIG.SH\;1
fi

# Add some standard paths for compatibility
PATH=$PATH:/usr/ucb

# Prefer the platform's usual authorization tool, using system paths.
find_root_helper()
{
    if [ "$os" = OpenBSD ] && [ -x /usr/bin/doas ] && trusted_path /usr/bin/doas file; then
        printf '%s\n' /usr/bin/doas
        return
    fi
    for helper in /usr/bin/sudo /usr/local/bin/sudo /usr/bin/doas /usr/local/bin/doas; do
        if [ -x "$helper" ] && trusted_path "$helper" file; then
            printf '%s\n' "$helper"
            return
        fi
    done
    return 1
}

console_only=0
if [ "${1-}" = "-auth" ]; then
    # Private re-entry flag. The authorization helper must actually grant root.
    [ "$uid" = 0 ] || exit 1
    console_only=1
    shift
fi

case "$GET_ROOT" in
    0) ;;
    1|2)
        if [ "$uid" = 0 ]; then
            console_only=1
        else
            if [ -t 0 ] && trusted_path "$launcher" file && helper=$(find_root_helper); then
                home_option=
                case "$helper" in */sudo) home_option=-H ;; esac
                # Both helpers return 1 for authorization failures. Keep a
                # launched installer's status 1 distinct so it cannot retry.
                "$helper" ${home_option:+"$home_option"} -u root -- /bin/sh -c "$trusted_path_code"'
                    [ "$(/usr/bin/id -u)" = 0 ] || exit 1
                    unset SETUP_EXEC_TMPDIR TMPDIR
                    trusted_path "$0" file || exit 125
                    /bin/sh "$0" -auth "$@"
                    status=$?
                    [ "$status" -ne 1 ] || exit 125
                    exit "$status"
                ' "$launcher" "$@"
                status=$?
                case "$status" in
                    1) ;;
                    125) exit 1 ;;
                    *) exit "$status" ;;
                esac
            fi
            if [ "$GET_ROOT" = 2 ]; then
                printf '%s\n' "Administrator access is required. Run this installer from a terminal with sudo or doas." >&2
                exit 1
            fi
            printf '%s\n' "Continuing without administrator access." >&2
        fi
        ;;
    *)
        printf '%s\n' "GET_ROOT must be 0, 1, or 2." >&2
        exit 1
        ;;
esac

# Optionally override the detected architecture. 

if [ "${1-}" = "-arch" ]
then
  shift
  if [ -z "${1-}" ] || echo "$1" | grep "^-" >/dev/null 2>&1
  then
    echo "$0: -arch needs an argument"
    exit 1
  fi
  # some handy conversion copied from DetectARCH. 
  case "$1" in
  arm64 | aarch64)
    arch="aarch64" ;;
  amd64)
    arch="x86_64" ;;
  i?86 | i86*)
    arch="x86" ;;
  90*/*) 
    arch="hppa" ;;
  *)
    arch="$1" ;;
  esac
  shift
fi

case "$arch" in
    ''|*[!a-zA-Z0-9_.-]*|.|..) exit 1 ;;
esac


# Copy the selected binary off media that may be mounted noexec.
try_run()
{
    setup=$1
    shift
    setup_bin="setup.data/bin/$os/$arch/$libc/$setup"
    if [ ! -f "$setup_bin" ]; then
        setup_bin="setup.data/bin/$os/$arch/$setup"
        [ -f "$setup_bin" ] || return 1
    fi

    tmpbase=${SETUP_EXEC_TMPDIR:-$HOME}
    if [ "$uid" = 0 ]; then
        trusted_path "$media_dir/$setup_bin" file || return 1
        tmpbase=$(CDPATH= cd "$HOME" && pwd -P) || return 1
        trusted_path "$tmpbase" directory || return 1
    fi
    setup=$(mktemp "$tmpbase/.setup.XXXXXX") || return 1
    if ! cp "$setup_bin" "$setup" || ! chmod 700 "$setup"; then
        rm -f "$setup"
        return 1
    fi
    trap 'rm -f "$setup"; exit 1' 1 2 3 15
    "$setup" "$@"
    failed=$?
    rm -f "$setup"
    trap - 1 2 3 15
    return "$failed"
}

if [ "$console_only" = 1 ]; then
    try_run setup $args -n "$@"
    status=$?
else
    try_run setup.gtk2 $args "$@" 2> /dev/null
    status=$?
    if [ "$status" -ne 0 ] && [ "$status" -ne 2 ]; then
        try_run setup.gtk $args "$@"
        status=$?
        if [ "$status" -ne 0 ] && [ "$status" -ne 2 ]; then
            try_run setup $args "$@"
            status=$?
        fi
    fi
fi

if [ "$status" -ne 0 ] && [ "$status" -ne 2 ]; then
    printf '%s\n\n%s\n' "The setup program seems to have failed on $arch/$libc" "$FATAL_ERROR" >&2
fi

exit "$status"
