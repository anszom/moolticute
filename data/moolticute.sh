#!/bin/bash
##
# Wrapper for the moolticute AppImage
##

me="Moolticute.AppImage"

help_msg="Moolticute for linux
Usage:
$me <arg>

Arguments:
-d --daemon         Runs daemon only mode.
-a --agent          Runs mc-agent only, with passed arguments
-l --cli            Runs mc-cli only, with passed arguments
-i --install        Install moolticute UDev rules
-c --check          Check moolticute UDev rules
-u --uninstall      Uninstall moolticute UDev rules
-h --help           This help

Example:
Run Moolticute normaly
./$me

Run only the moolticuted daemon
./$me --daemon

Get help on how to use mc-cli:
./$me --cli --help

Get a password via mc-cli:
./$me --cli login get site1.com user1

Run mc-agent:
./$me --agent --address /tmp/mc-agent.socket
";

UDEV_RULES_FILE_PATH="/etc/udev/rules.d/50-mooltipass.rules"
UDEV_RULE=="$(cat <<-EOF
## File managed by Appimage, do not change
#version:2
# udev rules for allowing console user(s) and hidraw access to Mooltipass Mini devices

ACTION!="add|change", GOTO="mooltipass_end"

# console user
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="16d0", ATTRS{idProduct}=="09a0", MODE="0660", SYMLINK+="mooltipass_keyboard", TAG+="uaccess", TAG+="udev-acl"
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="1209", ATTRS{idProduct}=="4321", MODE="0660", SYMLINK+="mooltipass_keyboard", TAG+="uaccess", TAG+="udev-acl"
SUBSYSTEM=="hidraw", KERNELS=="*1209:4321*", MODE="0660", SYMLINK+="mooltipass_keyboard", TAG+="uaccess", TAG+="udev-acl"
# libusb
SUBSYSTEM=="usb", ATTRS{idVendor}=="16d0", ATTRS{idProduct}=="09a0", MODE="0660", SYMLINK+="mooltipass_device", TAG+="uaccess"
SUBSYSTEM=="usb", ATTRS{idVendor}=="1209", ATTRS{idProduct}=="4321", MODE="0660", SYMLINK+="mooltipass_device", TAG+="uaccess"

LABEL="mooltipass_end"
EOF
)"

function install_udev_rule()
{
    tmpfile=$(mktemp /tmp/mc-udev.XXXXXX)
    cat > "$tmpfile" <<- EOF
${UDEV_RULE}
EOF

chmod +r $tmpfile

pkexec env DISPLAY="${DISPLAY}" XAUTHORITY="${XAUTHORITY}" sh << EOF
cp "$tmpfile" "${UDEV_RULES_FILE_PATH}" && \
    udevadm control --reload-rules
EOF

    rm "$tmpfile"
}

function check_udev_installed()
{
    if [ -f ${UDEV_RULES_FILE_PATH} ] && [ "x$UDEV_RULE" = "x$(cat ${UDEV_RULES_FILE_PATH})" ]
    then
        return 0
    fi
    return 1
}

SINGLE_EXE=""
INSTALL=0
UNINSTALL=0
CHECK=0

case "$1" in
    -d|--daemon)
        SINGLE_EXE="moolticuted"
        ;;
    -a|--agent)
        SINGLE_EXE="mc-agent"
        ;;
    -l|--cli)
        SINGLE_EXE="mc-cli"
        ;;
    -i|--install)
        INSTALL=1
    ;;
        -c|--check)
        CHECK=1
    ;;
    -u|--uninstall)
        UNINSTALL=1
    ;;
    -h|-\?|--help)
        printf "$help_msg"
        exit
    ;;
    -?*)
        printf 'WARN: Unknown option (ignored): %s\n' "$1" >&2
        exit 1
    ;;

esac

# Check if UDEV rules are set on normal run
if (( $# == 0 )) && ! check_udev_installed
then
    install_udev_rule
fi

#fix https://github.com/mooltipass/moolticute/issues/245
export QT_XKB_CONFIG_ROOT=/usr/share/X11/xkb

# support for running extracted AppImage like this:
# $ ./Moolticute-x86_64.AppImage --appimage-extract
# $ cd squashfs-root
# $ ./AppRun
if [ -z "$APPDIR" ] ; then
    APPDIR=`dirname -- "$0"`
fi

# Point our lib OpenSSL to its bundled config:
export OPENSSL_CONF="$APPDIR/usr/lib/ssl/openssl.cnf"


# Search root certificates on a system in well-known locations:
# https://github.com/darealshinji/vlc-AppImage/issues/1#issuecomment-321041496
# https://serverfault.com/questions/62496/ssl-certificate-location-on-unix-linux
# https://www.happyassassin.net/2015/01/12/a-note-about-ssltls-trusted-certificate-stores-and-platforms/
if [ -s "/etc/ssl/ca-bundle.pem" ] ; then
    # OpenSuSE
    export SSL_CERT_FILE="/etc/ssl/ca-bundle.pem"
elif [ -s "/var/lib/ca-certificates/ca-bundle.pem" ] ; then
    # alternative OpenSuSE path (different versions have different locations)
    export SSL_CERT_FILE="/var/lib/ca-certificates/ca-bundle.pem"
elif [ -s "/etc/pki/tls/cacert.pem" ] ; then
    # OpenELEC
    export SSL_CERT_FILE="/etc/pki/tls/cacert.pem"
elif [ -s "/etc/ssl/certs/ca-certificates.crt" ] ; then
    # Debian/Ubuntu/Gentoo etc.
    export SSL_CERT_FILE="/etc/ssl/certs/ca-certificates.crt"
elif [ -s "/etc/pki/tls/cert.pem" ] ; then
    export SSL_CERT_FILE="/etc/pki/tls/cert.pem"
elif [ -s "/usr/local/share/certs/ca-root-nss.crt" ] ; then
    export SSL_CERT_FILE="/usr/local/share/certs/ca-root-nss.crt"
elif [ -s "/etc/ssl/cert.pem" ] ; then
    export SSL_CERT_FILE="/etc/ssl/cert.pem"
elif [ -s "/etc/pki/tls/certs/ca-bundle.crt" ] ; then
    # Fedora/RHEL 6
    export SSL_CERT_FILE="/etc/pki/tls/certs/ca-bundle.crt"
elif [ -s "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem" ] ; then
    # CentOS/RHEL 7
    export SSL_CERT_FILE="/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem"
elif [ -s "/usr/local/ssl/cert.pem" ] ; then
    # OpenSSL library default
    export SSL_CERT_FILE="/usr/local/ssl/cert.pem"
fi

if [ -d "/usr/local/ssl" ] ; then
    # OpenSSL library default
    export SSL_CERT_DIR="/usr/local/ssl"
elif [ -d "/etc/ssl/certs" ] ; then
    export SSL_CERT_DIR="/etc/ssl/certs"
fi


if [ -n "$SINGLE_EXE" ] ;
then
    test "SINGLE_EXE" = "moolticuted" && echo "Running daemon only"
    shift
    $APPDIR/usr/bin/$SINGLE_EXE "$@"
elif (( $INSTALL == 0 )) && (( $UNINSTALL == 0 )) && (( $CHECK == 0 ));
then
    $APPDIR/usr/bin/moolticute
elif (( $INSTALL == 1 ));
then
    echo "Installing moolticute UDEV rules"
    install_udev_rule
elif (( $CHECK == 1 ));
then
    if check_udev_installed
    then
        echo "Moolticute Udev rules are installed."
        exit 0;
    else
        echo "Moolticute Udev rules are NOT installed."
        exit 1;
    fi
elif (( $UNINSTALL == 1 ));
then
    echo "Uninstalling moolticuted UDEV rules"
    sudo rm $UDEV_RULES_FILE_PATH
    sudo udevadm control --reload-rules
fi
