#!/bin/bash

set -e

# Kernel module name and path relative to this script
MODULE_NAME="ir"
MODULE_FILE="./ir.ko"

# USB driver names and the FT232R USB ID
FTDI_DRIVER="ftdi_sio"
IR_USB_DRIVER="devtitans_ir_driver"
FTDI_VID="0403"
FTDI_PID="6001"

DEVICE_NODE="/dev/devtitans_ir0"


# Print the bound driver path, or nothing when the interface is unbound.
get_bound_driver()
{
    local interface="$1"
    local driver_link="/sys/bus/usb/devices/$interface/driver"

    if [ -L "$driver_link" ]; then
        readlink -f "$driver_link"
    fi
}


# Reattach the interface to the standard FTDI serial driver after a failure.
restore_ftdi_binding()
{
    local interface="$1"

    [ -d "/sys/bus/usb/drivers/$FTDI_DRIVER" ] || return 1
    echo "$interface" > "/sys/bus/usb/drivers/$FTDI_DRIVER/bind"
}


# Find the USB interface whose parent device matches the configured VID/PID.
find_ftdi_interface()
{
    for interface in /sys/bus/usb/devices/*:*; do

        [ -f "$interface/../idVendor" ] || continue
        [ -f "$interface/../idProduct" ] || continue

        vid=$(cat "$interface/../idVendor")
        pid=$(cat "$interface/../idProduct")

        if [ "$vid" = "$FTDI_VID" ] &&
           [ "$pid" = "$FTDI_PID" ]; then

            echo "$(basename "$interface")"
            return 0
        fi
    done

    return 1
}


# Unbind ftdi_sio, load the custom module, bind the interface, and verify the node.
load_driver()
{
    echo "=== Carregando DevTitans IR ==="
    echo

    # Track rollback state so a failed load does not leave the FT232 detached.
    restore_ftdi=0
    module_was_loaded=0
    already_bound=0

    if [ ! -f "$MODULE_FILE" ]; then
        echo "ERRO: módulo não encontrado:"
        echo "  $MODULE_FILE"
        exit 1
    fi

    echo "[1/4] Procurando FT232..."

    interface=$(find_ftdi_interface) || {
        echo "ERRO: FT232 $FTDI_VID:$FTDI_PID não encontrado."
        exit 1
    }

    echo "      Interface encontrada: $interface"

    driver=$(get_bound_driver "$interface")

    # Release the interface from ftdi_sio before the custom driver claims it.
    if [ "$driver" = "/sys/bus/usb/drivers/$FTDI_DRIVER" ]; then

        echo "[2/4] Removendo binding do $FTDI_DRIVER..."

        echo "$interface" > \
            "/sys/bus/usb/drivers/$FTDI_DRIVER/unbind"
        restore_ftdi=1

        echo "      OK"

    elif [ -z "$driver" ]; then

        echo "[2/4] Interface sem driver."
        restore_ftdi=1

    elif [ "$driver" = "/sys/bus/usb/drivers/$IR_USB_DRIVER" ]; then

        echo "[2/4] Interface já ligada ao driver customizado."
        already_bound=1

    else

        echo "ERRO: interface já está ligada a:"
        echo "  $driver"
        exit 1
    fi

    echo "[3/4] Carregando $MODULE_FILE..."

    if lsmod | grep -q "^${MODULE_NAME} "; then
        module_was_loaded=1
        echo "      Módulo já está carregado."
    elif ! insmod "$MODULE_FILE"; then
        echo "ERRO: não foi possível carregar $MODULE_FILE."
        if [ "$restore_ftdi" -eq 1 ]; then
            echo "Restaurando binding do $FTDI_DRIVER..."
            if ! restore_ftdi_binding "$interface"; then
                echo "AVISO: não foi possível restaurar $FTDI_DRIVER automaticamente."
            fi
        fi
        exit 1
    fi

    # A module that was already loaded will not necessarily probe after unbind,
    # so explicitly bind the matching interface when it is still unbound.
    driver=$(get_bound_driver "$interface")
    if [ "$driver" != "/sys/bus/usb/drivers/$IR_USB_DRIVER" ] &&
       [ "$already_bound" -eq 0 ]; then
        echo "Vinculando a interface ao $IR_USB_DRIVER..."
        if ! echo "$interface" > "/sys/bus/usb/drivers/$IR_USB_DRIVER/bind"; then
            echo "ERRO: falha ao vincular ao $IR_USB_DRIVER."
            if [ "$module_was_loaded" -eq 0 ]; then
                rmmod "$MODULE_NAME" 2>/dev/null || true
            fi
            if [ "$restore_ftdi" -eq 1 ]; then
                echo "Restaurando binding do $FTDI_DRIVER..."
                restore_ftdi_binding "$interface" || \
                    echo "AVISO: não foi possível restaurar $FTDI_DRIVER automaticamente."
            fi
            exit 1
        fi
    fi

    echo "[4/4] Verificando dispositivo..."

    sleep 1

    # Probe creates the character device only after FTDI setup succeeds.
    if [ -e "$DEVICE_NODE" ]; then
        echo
        echo "OK!"
        echo
        echo "Driver: $MODULE_NAME"
        echo "Dispositivo: $DEVICE_NODE"
    else
        echo
        echo "ERRO: $DEVICE_NODE não foi encontrado."
        echo "Verifique: sudo dmesg | tail -50"

        if [ "$module_was_loaded" -eq 0 ]; then
            rmmod "$MODULE_NAME" 2>/dev/null || true
        fi
        if [ "$restore_ftdi" -eq 1 ]; then
            echo "Restaurando binding do $FTDI_DRIVER..."
            if restore_ftdi_binding "$interface"; then
                echo "      $FTDI_DRIVER restaurado."
            else
                echo "AVISO: não foi possível restaurar $FTDI_DRIVER automaticamente."
            fi
        fi
        return 1
    fi
}


# Remove the custom module and restore the standard ftdi_sio binding.
unload_driver()
{
    echo "=== Removendo DevTitans IR ==="
    echo

    echo "[1/4] Removendo módulo..."

    if lsmod | grep -q "^${MODULE_NAME} "; then
        rmmod "$MODULE_NAME"
        echo "      OK"
    else
        echo "      Módulo não está carregado."
    fi

    echo "[2/4] Procurando FT232..."

    interface=$(find_ftdi_interface) || {
        echo "ERRO: FT232 $FTDI_VID:$FTDI_PID não encontrado."
        exit 1
    }

    echo "      Interface: $interface"

    driver=$(get_bound_driver "$interface")
    if [ "$driver" = "/sys/bus/usb/drivers/$FTDI_DRIVER" ]; then
        echo "[3/4] $FTDI_DRIVER já está vinculado."
    elif [ -z "$driver" ]; then
        echo "[3/4] Restaurando $FTDI_DRIVER..."
        echo "$interface" > \
            "/sys/bus/usb/drivers/$FTDI_DRIVER/bind"
        echo "      OK"
    else
        echo "ERRO: interface ainda está ligada a:"
        echo "  $driver"
        exit 1
    fi

    echo "[4/4] Verificando..."

    sleep 1

    driver=$(get_bound_driver "$interface")

    if [ "$driver" = "/sys/bus/usb/drivers/$FTDI_DRIVER" ]; then
        echo
        echo "OK!"
        echo "FT232 voltou para $FTDI_DRIVER."
    else
        echo
        echo "AVISO: FT232 não está vinculado ao $FTDI_DRIVER."
        echo
        echo "Driver atual:"
        echo "  ${driver:-nenhum}"
    fi
}


# Perform a complete unload followed by a fresh load.
restart_driver()
{
    unload_driver

    echo
    echo "================================"
    echo

    load_driver
}


# Report the USB binding, module state, and character-device node.
status_driver()
{
    echo "=== Status DevTitans IR ==="
    echo

    echo "FT232:"
    echo "  VID:PID = $FTDI_VID:$FTDI_PID"

    interface=$(find_ftdi_interface 2>/dev/null || true)

    if [ -n "$interface" ]; then

        echo "  Interface: $interface"

        driver=$(get_bound_driver "$interface")

        echo "  Driver: ${driver:-nenhum}"

    else

        echo "  FT232 não encontrado."
    fi

    echo
    echo "Módulo $MODULE_NAME:"

    if lsmod | grep -q "^${MODULE_NAME} "; then
        echo "  carregado"
    else
        echo "  não carregado"
    fi

    echo
    echo "Device node:"

    if [ -e "$DEVICE_NODE" ]; then
        echo "  $DEVICE_NODE"
    else
        echo "  não encontrado"
    fi
}


# Print the supported command-line operations.
usage()
{
    echo "Uso:"
    echo
    echo "  sudo $0 load"
    echo "  sudo $0 unload"
    echo "  sudo $0 restart"
    echo "  sudo $0 status"
}


# Binding USB drivers and loading kernel modules require root privileges.
if [ "$EUID" -ne 0 ]; then
    echo "Execute como root:"
    echo
    echo "  sudo $0 $*"
    exit 1
fi


# Dispatch the requested operation.
case "$1" in

    load)
        load_driver
        ;;

    unload)
        unload_driver
        ;;

    restart)
        restart_driver
        ;;

    status)
        status_driver
        ;;

    *)
        usage
        exit 1
        ;;

esac