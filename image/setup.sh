#!/bin/sh
#
# Product setup script - Loki Entertainment Software

# Go to the proper setup directory (if not already there)
cd `dirname $0`

# Return the appropriate architecture string
function DetectARCH {
	status=1
	case `uname -m` in
		i?86)  echo "x86"
			status=0;;
		*)     echo "`uname -m`"
			status=0;;
	esac
	return $status
}

# Return the appropriate version string
function DetectLIBC {
      status=1
      if [ -f `echo /lib/libc.so.6* | tail -1` ]; then
	      if fgrep GLIBC_2.1 /lib/libc.so.6* 2>&1 >/dev/null; then
	              echo "glibc-2.1"
	              status=0
	      else    
	              echo "glibc-2.0"
	              status=0
	      fi        
      elif [ -f /lib/libc.so.5 ]; then
	      echo "libc5"
	      status=0
      else
	      echo "unknown"
      fi
      return $status
}

# Detect the Linux environment
arch=`DetectARCH`
libc=`DetectLIBC`

# Find the installation program
function try_run
{
    setup=$1
    shift
    fatal=$1
    if [ "$1" != "" ]; then
        shift
    fi

    # First find the binary we want to run
    failed=0
    setup_bin="setup.data/bin/$arch/$libc/$setup"
    if [ ! -f "$setup_bin" ]; then
        setup_bin="setup.data/bin/$arch/$setup"
        if [ ! -f "$setup_bin" ]; then
            failed=1
        fi
    fi
    if [ "$failed" -eq 1 ]; then
        if [ "$fatal" != "" ]; then
            cat <<__EOF__
This installation doesn't support $libc on $arch

Please contact Loki Technical Support at support@lokigames.com
__EOF__
            exit 1
        fi
        return $failed
    fi

    # Try to run the binary
    # The executable is here but we can't execute it from CD
    setup="$HOME/.setup$$"
    cp "$setup_bin" "$setup"
    chmod 700 "$setup"
    if [ "$fatal" != "" ]; then
        "$setup" $*
        failed=$?
    else
        "$setup" $* 2>/dev/null
        failed=$?
    fi
    rm -f "$setup"
    return $failed
}


# Try to run the setup program
status=0
rm -f "$setup"
if ! try_run setup.gtk && ! try_run setup -fatal; then
    echo "The setup program seems to have failed on $arch/$libc"
    echo
    echo "Please contact Loki Technical Support at support@lokigames.com"
    status=1
fi
exit $status
