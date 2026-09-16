#include <linux/module.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Felipe Peres de Almeida");
MODULE_DESCRIPTION("IR kernel module to connect with the IR hardware module for the HandsOn");
MODULE_VERSION("0.1");

#define VENDOR_ID  0x10C4
#define PRODUCT_ID 0xEA60

#define MAX_PAYLOAD_SIZE 256

/* Commands */
#define CMD_TX          0x01
#define CMD_RX          0x02
#define CMD_SET_CARRIER 0x03


struct devtitans_ir_device {
    struct usb_device *udev;
    struct usb_interface *interface;

    unsigned int carrier;

    u8 tx_buffer[MAX_PAYLOAD_SIZE + 2];
    size_t tx_length;

    u8 rx_buffer[MAX_PAYLOAD_SIZE + 2];
    size_t rx_length;
};


/*
 * Build a packet using:
 *
 * [COMM][LEN][PAYLOAD]
 *
 * Returns the total packet length, or a negative error.
 */
static int ir_build_packet(struct devtitans_ir_device *ir,
                           u8 command,
                           const u8 *payload,
                           size_t payload_len)
{
    if (payload_len > MAX_PAYLOAD_SIZE)
        return -EMSGSIZE;

    ir->tx_buffer[0] = command;
    ir->tx_buffer[1] = payload_len;

    memcpy(&ir->tx_buffer[2], payload, payload_len);

    ir->tx_length = payload_len + 2;

    return ir->tx_length;
}


/*
 * Example function for preparing an IR transmission.
 */
static int ir_prepare_transmit(struct devtitans_ir_device *ir,
                               const u8 *payload,
                               size_t payload_len)
{
    return ir_build_packet(ir, CMD_TX, payload, payload_len);
}


/*
 * Sysfs: carrier
 */

static ssize_t carrier_show(struct device *dev,
                            struct device_attribute *attr,
                            char *buf)
{
    struct devtitans_ir_device *ir = dev_get_drvdata(dev);

    return sysfs_emit(buf, "%u\n", ir->carrier);
}


static ssize_t carrier_store(struct device *dev,
                             struct device_attribute *attr,
                             const char *buf,
                             size_t count)
{
    struct devtitans_ir_device *ir = dev_get_drvdata(dev);
    unsigned int carrier;
    int ret;

    ret = kstrtouint(buf, 10, &carrier);
    if (ret)
        return ret;

    ir->carrier = carrier;

    return count;
}

static DEVICE_ATTR_RW(carrier);


/*
 * USB device identification
 */

static const struct usb_device_id ir_table[] = {
    { USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
    { }
};

MODULE_DEVICE_TABLE(usb, ir_table);


/*
 * USB probe
 */

static int ir_probe(struct usb_interface *interface,
                    const struct usb_device_id *id)
{
    struct devtitans_ir_device *ir;
    int ret;

    ir = devm_kzalloc(&interface->dev, sizeof(*ir), GFP_KERNEL);
    if (!ir)
        return -ENOMEM;

    ir->udev = interface_to_usbdev(interface);
    ir->interface = interface;
    ir->carrier = 38000;

    usb_set_intfdata(interface, ir);
    dev_set_drvdata(&interface->dev, ir);

    ret = device_create_file(&interface->dev, &dev_attr_carrier);
    if (ret) {
        dev_set_drvdata(&interface->dev, NULL);
        usb_set_intfdata(interface, NULL);
        return ret;
    }

    dev_info(&interface->dev, "DevTitans IR device connected\n");

    return 0;
}


/*
 * USB disconnect
 */

static void ir_disconnect(struct usb_interface *interface)
{
    device_remove_file(&interface->dev, &dev_attr_carrier);

    usb_set_intfdata(interface, NULL);
    dev_set_drvdata(&interface->dev, NULL);

    dev_info(&interface->dev, "DevTitans IR device disconnected\n");
}


/*
 * USB driver
 */

static struct usb_driver devtitans_ir_driver = {
    .name = "devtitans_ir_driver",
    .probe = ir_probe,
    .disconnect = ir_disconnect,
    .id_table = ir_table,
};

module_usb_driver(devtitans_ir_driver);