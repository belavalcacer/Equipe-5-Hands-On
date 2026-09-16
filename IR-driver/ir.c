#include <linux/module.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Felipe Peres de Almeida");
MODULE_DESCRIPTION("DevTitans IR USB driver");
MODULE_VERSION("0.1");

#define VENDOR_ID  0x10C4
#define PRODUCT_ID 0xEA60

#define MAX_PAYLOAD_SIZE 256

/* Protocol commands */
#define CMD_TX 0x01
#define CMD_RX 0x02

/* USB endpoint number */
#define IR_ENDPOINT 0x01


struct devtitans_ir_device {
    struct usb_device *udev;
    struct usb_interface *interface;

    unsigned int carrier;

    /* TX */
    u8 tx_buffer[MAX_PAYLOAD_SIZE + 2];
    size_t tx_length;

    /* RX */
    u8 rx_buffer[MAX_PAYLOAD_SIZE + 2];
    size_t rx_length;
};


/*
 * Build:
 *
 * [COMM][LEN][PAYLOAD]
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

    return 0;
}


/*
 * Transmit the packet to the ESP32.
 */
static int ir_transmit_packet(struct devtitans_ir_device *ir)
{
    int actual_length;
    int ret;

    ret = usb_bulk_msg(
        ir->udev,
        usb_sndbulkpipe(ir->udev, IR_ENDPOINT),
        ir->tx_buffer,
        ir->tx_length,
        &actual_length,
        1000
    );

    if (ret) {
        dev_err(&ir->interface->dev,
                "Failed to transmit IR packet: %d\n",
                ret);
        return ret;
    }

    if (actual_length != ir->tx_length) {
        dev_err(&ir->interface->dev,
                "Incomplete transmission: %d/%zu bytes\n",
                actual_length,
                ir->tx_length);
        return -EIO;
    }

    return 0;
}


/*
 * Build and transmit an IR packet.
 */
static int ir_transmit(struct devtitans_ir_device *ir,
                       const u8 *payload,
                       size_t payload_len)
{
    int ret;

    ret = ir_build_packet(ir, CMD_TX, payload, payload_len);
    if (ret)
        return ret;

    return ir_transmit_packet(ir);
}


/*
 * Receive a packet from the ESP32.
 *
 * Expected format:
 *
 * [COMM][LEN][PAYLOAD]
 */
static int ir_receive_packet(struct devtitans_ir_device *ir)
{
    int actual_length;
    int ret;
    u8 command;
    u8 payload_length;

    ret = usb_bulk_msg(
        ir->udev,
        usb_rcvbulkpipe(ir->udev, IR_ENDPOINT),
        ir->rx_buffer,
        sizeof(ir->rx_buffer),
        &actual_length,
        1000
    );

    if (ret) {
        dev_err(&ir->interface->dev,
                "Failed to receive IR packet: %d\n",
                ret);
        return ret;
    }

    if (actual_length < 2) {
        dev_err(&ir->interface->dev,
                "Received packet is too short: %d bytes\n",
                actual_length);
        return -EINVAL;
    }

    command = ir->rx_buffer[0];
    payload_length = ir->rx_buffer[1];

    if (payload_length > MAX_PAYLOAD_SIZE) {
        dev_err(&ir->interface->dev,
                "Invalid payload length: %u\n",
                payload_length);
        return -EINVAL;
    }

    if (actual_length != payload_length + 2) {
        dev_err(&ir->interface->dev,
                "Invalid packet size: received %d, expected %u\n",
                actual_length,
                payload_length + 2);
        return -EINVAL;
    }

    ir->rx_length = actual_length;

    dev_info(&ir->interface->dev,
             "Received command 0x%02X, payload length %u\n",
             command,
             payload_length);

    return 0;
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
 * USB device IDs.
 */

static const struct usb_device_id ir_table[] = {
    { USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
    { }
};

MODULE_DEVICE_TABLE(usb, ir_table);


/*
 * Probe.
 */

static int ir_probe(struct usb_interface *interface,
                    const struct usb_device_id *id)
{
    struct devtitans_ir_device *ir;
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    int i;
    int ret;

    ir = devm_kzalloc(&interface->dev, sizeof(*ir), GFP_KERNEL);
    if (!ir)
        return -ENOMEM;

    ir->udev = interface_to_usbdev(interface);
    ir->interface = interface;
    ir->carrier = 38000;

    /*
     * Find the endpoint.
     */
    iface_desc = interface->cur_altsetting;

    for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
        endpoint = &iface_desc->endpoint[i].desc;

        dev_info(&interface->dev,
                 "Endpoint 0x%02X, attributes 0x%02X\n",
                 endpoint->bEndpointAddress,
                 endpoint->bmAttributes);
    }

    /*
     * Store driver data.
     */
    usb_set_intfdata(interface, ir);
    dev_set_drvdata(&interface->dev, ir);

    /*
     * Create carrier sysfs attribute.
     */
    ret = device_create_file(&interface->dev, &dev_attr_carrier);
    if (ret) {
        dev_set_drvdata(&interface->dev, NULL);
        usb_set_intfdata(interface, NULL);
        return ret;
    }

    dev_info(&interface->dev,
             "DevTitans IR device connected\n");

    return 0;
}


/*
 * Disconnect.
 */

static void ir_disconnect(struct usb_interface *interface)
{
    device_remove_file(&interface->dev, &dev_attr_carrier);

    usb_set_intfdata(interface, NULL);
    dev_set_drvdata(&interface->dev, NULL);

    dev_info(&interface->dev,
             "DevTitans IR device disconnected\n");
}


/*
 * USB driver.
 */

static struct usb_driver devtitans_ir_driver = {
    .name = "devtitans_ir_driver",
    .probe = ir_probe,
    .disconnect = ir_disconnect,
    .id_table = ir_table,
};

module_usb_driver(devtitans_ir_driver);