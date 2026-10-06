#include <linux/module.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/cdev.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include "ir_compat.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Felipe Peres de Almeida");
MODULE_DESCRIPTION("DevTitans IR USB driver");
MODULE_VERSION("0.1");

#define VENDOR_ID  0x0403
#define PRODUCT_ID 0x6001

#define IR_HEADER_SIZE       16
#define IR_MAX_PAYLOAD_SIZE  1036
#define IR_MAX_PACKET_SIZE   (IR_HEADER_SIZE + IR_MAX_PAYLOAD_SIZE)

#define IR_MAGIC_0            0x49  /* 'I' */
#define IR_MAGIC_1            0x52  /* 'R' */

#define IR_PROTOCOL_VERSION   1

#define IR_CMD_READ            0x01
#define IR_CMD_WRITE           0x02
#define IR_CMD_PING            0x03

/* USB bulk endpoints */
#define IR_ENDPOINT_IN        0x81
#define IR_ENDPOINT_OUT       0x02
#define FTDI_STATUS_SIZE      2
#define FTDI_PACKET_SIZE      64

#define FTDI_SIO_RESET             0
#define FTDI_SIO_SET_MODEM_CTRL    1
#define FTDI_SIO_SET_FLOW_CTRL     2
#define FTDI_SIO_SET_BAUD_RATE     3
#define FTDI_SIO_SET_DATA          4
#define FTDI_SIO_SET_LATENCY_TIMER 9
#define FTDI_REQTYPE_OUT           0x40
#define FTDI_BAUD_115200_DIVISOR   26 /* 3 MHz / 115200, FT232R encoding */
#define FTDI_DATA_8N1              0x0008
#define FTDI_LATENCY_MS            1


struct devtitans_ir_device {
    struct kref kref;
    struct usb_device *udev;
    unsigned int interface_number;
    bool disconnected;

    struct mutex read_mutex;
    struct mutex write_mutex;
    u8 rx_data[FTDI_PACKET_SIZE - FTDI_STATUS_SIZE];
    size_t rx_len;
    size_t rx_offset;

    unsigned int carrier;

    struct cdev cdev;
    dev_t devt;
};

static void ir_delete(struct kref *kref)
{
    struct devtitans_ir_device *ir =
        container_of(kref, struct devtitans_ir_device, kref);

    usb_put_dev(ir->udev);
    kfree(ir);
}

static int ir_ftdi_control(struct devtitans_ir_device *ir, u8 request,
                           u16 value, const char *name)
{
    int ret;

    ret = usb_control_msg(ir->udev, usb_sndctrlpipe(ir->udev, 0), request,
                          FTDI_REQTYPE_OUT, value, ir->interface_number,
                          NULL, 0, 1000);
    if (ret < 0) {
        dev_err(&ir->udev->dev, "FTDI %s request failed: %d\n", name, ret);
        return ret;
    }
    if (ret != 0) {
        dev_err(&ir->udev->dev,
                "FTDI %s returned unexpected length %d\n", name, ret);
        return -EIO;
    }
    return 0;
}

static int ir_ftdi_reset(struct devtitans_ir_device *ir)
{
    return ir_ftdi_control(ir, FTDI_SIO_RESET, 0, "reset");
}

static int ir_ftdi_set_baudrate(struct devtitans_ir_device *ir)
{
    return ir_ftdi_control(ir, FTDI_SIO_SET_BAUD_RATE,
                           FTDI_BAUD_115200_DIVISOR, "baud rate 115200");
}

static int ir_ftdi_set_data(struct devtitans_ir_device *ir)
{
    return ir_ftdi_control(ir, FTDI_SIO_SET_DATA, FTDI_DATA_8N1, "8N1");
}

static int ir_ftdi_disable_flow_control(struct devtitans_ir_device *ir)
{
    int ret;

    ret = ir_ftdi_control(ir, FTDI_SIO_SET_FLOW_CTRL, 0,
                          "disable flow control");
    if (ret)
        return ret;

    /* FTDI modem control bit 8/9 selects DTR/RTS; bit 0/1 clears the line. */
    ret = ir_ftdi_control(ir, FTDI_SIO_SET_MODEM_CTRL, 0x0100,
                          "deassert DTR");
    if (ret)
        return ret;
    return ir_ftdi_control(ir, FTDI_SIO_SET_MODEM_CTRL, 0x0200,
                           "deassert RTS");
}

static int ir_ftdi_set_latency(struct devtitans_ir_device *ir)
{
    return ir_ftdi_control(ir, FTDI_SIO_SET_LATENCY_TIMER,
                           FTDI_LATENCY_MS, "latency timer");
}

static int ir_ftdi_configure(struct devtitans_ir_device *ir)
{
    int ret;

    ret = ir_ftdi_reset(ir);
    if (ret)
        return ret;
    ret = ir_ftdi_disable_flow_control(ir);
    if (ret)
        return ret;
    ret = ir_ftdi_set_baudrate(ir);
    if (ret)
        return ret;
    ret = ir_ftdi_set_data(ir);
    if (ret)
        return ret;
    ret = ir_ftdi_set_latency(ir);
    if (ret)
        return ret;

    dev_info(&ir->udev->dev,
             "FT232R configured: 115200 8N1, flow control off, latency %u ms\n",
             FTDI_LATENCY_MS);
    return 0;
}

/*
 * Character device: open.
 */
static int ir_open(struct inode *inode, struct file *file)
{
    struct devtitans_ir_device *ir;

    ir = container_of(inode->i_cdev,
                      struct devtitans_ir_device,
                      cdev);

    kref_get(&ir->kref);
    file->private_data = ir;

    return 0;
}

static int ir_release(struct inode *inode, struct file *file)
{
    struct devtitans_ir_device *ir = file->private_data;

    kref_put(&ir->kref, ir_delete);
    return 0;
}


/*
 * Character device: write.
 *
 * The userspace buffer is sent directly to the USB OUT endpoint.
 *
 * Userspace is responsible for constructing the packet:
 */
static ssize_t ir_write(struct file *file,
                        const char __user *buf,
                        size_t count,
                        loff_t *ppos)
{
    struct devtitans_ir_device *ir = file->private_data;
    u8 *packet;
    int actual_length = 0;
    int ret;

    if (count == 0)
        return 0;
    if (count > IR_MAX_PACKET_SIZE)
        return -EMSGSIZE;

    packet = memdup_user(buf, count);
    if (IS_ERR(packet))
        return PTR_ERR(packet);

    mutex_lock(&ir->write_mutex);
    if (READ_ONCE(ir->disconnected)) {
        ret = -ENODEV;
        goto out;
    }

    ret = usb_bulk_msg(ir->udev,
                       usb_sndbulkpipe(ir->udev, IR_ENDPOINT_OUT),
                       packet, count, &actual_length, 1000);
    if (!ret && actual_length != count) {
        dev_err(&ir->udev->dev,
                "Incomplete transmission: %d/%zu bytes\n",
                actual_length, count);
        ret = -EIO;
    }
out:
    mutex_unlock(&ir->write_mutex);
    kfree(packet);
    if (ret) {
        if (ret != -ENODEV)
            dev_err(&ir->udev->dev, "Failed to transmit packet: %d\n", ret);
        return ret;
    }
    return count;
}

/*
 * Character device: read.
 *
 * Strip each FTDI packet's two status bytes and return the remaining UART
 * data as a byte stream, independent of USB packet and protocol boundaries.
 */
static ssize_t ir_read(struct file *file,
                       char __user *buf,
                       size_t count,
                       loff_t *ppos)
{
    struct devtitans_ir_device *ir = file->private_data;
    u8 *packet;
    size_t copied = 0;
    int actual_length = 0;
    int ret = 0;

    if (count == 0)
        return 0;

    /* usb_bulk_msg() DMA-maps its transfer buffer; it must not be on-stack. */
    packet = kmalloc(FTDI_PACKET_SIZE, GFP_KERNEL);
    if (!packet)
        return -ENOMEM;

    mutex_lock(&ir->read_mutex);
    while (copied < count) {
        size_t available;
        size_t take;

        if (ir->rx_offset < ir->rx_len) {
            available = ir->rx_len - ir->rx_offset;
            take = min(count - copied, available);
            if (copy_to_user(buf + copied, ir->rx_data + ir->rx_offset,
                             take)) {
                mutex_unlock(&ir->read_mutex);
                kfree(packet);
                return copied ? copied : -EFAULT;
            }
            ir->rx_offset += take;
            copied += take;
            continue;
        }

        ir->rx_len = 0;
        ir->rx_offset = 0;
        if (READ_ONCE(ir->disconnected)) {
            ret = -ENODEV;
            break;
        }

        /* A 64-byte receive buffer can contain at most one 64-byte USB packet. */
        ret = usb_bulk_msg(ir->udev,
                           usb_rcvbulkpipe(ir->udev, IR_ENDPOINT_IN),
                           packet, FTDI_PACKET_SIZE, &actual_length, 1000);
        if (ret)
            break;
        if (actual_length < FTDI_STATUS_SIZE) {
            dev_err(&ir->udev->dev,
                    "Short FTDI packet without complete status: %d bytes\n",
                    actual_length);
            ret = -EIO;
            break;
        }

        /* Every packet starts with modem/line status, even within one stream. */
        ir->rx_len = actual_length - FTDI_STATUS_SIZE;
        if (ir->rx_len)
            memcpy(ir->rx_data, packet + FTDI_STATUS_SIZE, ir->rx_len);

        /* Ignore status-only packets and continue until UART data or timeout. */
        if (!ir->rx_len && !copied)
            continue;
        if (!ir->rx_len)
            break;
    }

    mutex_unlock(&ir->read_mutex);
    kfree(packet);
    return copied ? copied : ret;
}

/*
 * Character device operations.
 */
static const struct file_operations ir_fops = {
    .owner = THIS_MODULE,
    .open = ir_open,
    .release = ir_release,
    .read = ir_read,
    .write = ir_write,
};


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
 * Character device class.
 *
 * This is a Linux class for the /dev character device.
 * It is NOT the USB device class.
 */
static struct class *ir_class;


/*
 * Probe.
 */

static int ir_probe(struct usb_interface *interface,
                    const struct usb_device_id *id)
{
    struct devtitans_ir_device *ir;
    struct usb_host_interface *iface_desc = interface->cur_altsetting;
    struct usb_endpoint_descriptor *endpoint;
    bool have_in = false;
    bool have_out = false;
    int i;
    int ret;

    ir = kzalloc(sizeof(*ir), GFP_KERNEL);
    if (!ir)
        return -ENOMEM;

    kref_init(&ir->kref);
    ir->udev = usb_get_dev(interface_to_usbdev(interface));
    ir->interface_number = iface_desc->desc.bInterfaceNumber;
    ir->carrier = 38000;
    mutex_init(&ir->read_mutex);
    mutex_init(&ir->write_mutex);

    for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
        endpoint = &iface_desc->endpoint[i].desc;
        dev_info(&interface->dev,
                 "Endpoint 0x%02X, attributes 0x%02X, max packet %u\n",
                 endpoint->bEndpointAddress, endpoint->bmAttributes,
                 usb_endpoint_maxp(endpoint));

        if (endpoint->bEndpointAddress == IR_ENDPOINT_IN &&
            usb_endpoint_is_bulk_in(endpoint) &&
            usb_endpoint_maxp(endpoint) == FTDI_PACKET_SIZE)
            have_in = true;
        if (endpoint->bEndpointAddress == IR_ENDPOINT_OUT &&
            usb_endpoint_is_bulk_out(endpoint) &&
            usb_endpoint_maxp(endpoint) == FTDI_PACKET_SIZE)
            have_out = true;
    }
    if (!have_in || !have_out) {
        dev_err(&interface->dev,
                "Expected bulk endpoints 0x81/0x02 with 64-byte packets\n");
        ret = -ENODEV;
        goto err_put;
    }

    ret = ir_ftdi_configure(ir);
    if (ret)
        goto err_put;

    usb_set_intfdata(interface, ir);
    dev_set_drvdata(&interface->dev, ir);

    ret = device_create_file(&interface->dev, &dev_attr_carrier);
    if (ret)
        goto err_data;

    ret = alloc_chrdev_region(&ir->devt, 0, 1, "devtitans_ir");
    if (ret)
        goto err_attribute;

    cdev_init(&ir->cdev, &ir_fops);
    ir->cdev.owner = THIS_MODULE;
    ret = cdev_add(&ir->cdev, ir->devt, 1);
    if (ret)
        goto err_region;

    {
        struct device *chardev;

        chardev = device_create(ir_class, &interface->dev, ir->devt, NULL,
                                "devtitans_ir%d", MINOR(ir->devt));
        if (IS_ERR(chardev)) {
            ret = PTR_ERR(chardev);
            goto err_cdev;
        }
    }

    dev_info(&interface->dev, "DevTitans IR device connected\n");
    return 0;

err_cdev:
    cdev_del(&ir->cdev);
err_region:
    unregister_chrdev_region(ir->devt, 1);
err_attribute:
    device_remove_file(&interface->dev, &dev_attr_carrier);
err_data:
    usb_set_intfdata(interface, NULL);
    dev_set_drvdata(&interface->dev, NULL);
err_put:
    kref_put(&ir->kref, ir_delete);
    return ret;
}

/*
 * Disconnect.
 */

static void ir_disconnect(struct usb_interface *interface)
{
    struct devtitans_ir_device *ir = usb_get_intfdata(interface);

    if (!ir)
        return;

    WRITE_ONCE(ir->disconnected, true);
    usb_set_intfdata(interface, NULL);
    dev_set_drvdata(&interface->dev, NULL);
    device_destroy(ir_class, ir->devt);
    cdev_del(&ir->cdev);
    unregister_chrdev_region(ir->devt, 1);
    device_remove_file(&interface->dev, &dev_attr_carrier);

    dev_info(&interface->dev, "DevTitans IR device disconnected\n");
    /* Open file descriptors keep the object and usb_device reference alive. */
    kref_put(&ir->kref, ir_delete);
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



/*
 * Module initialization.
 *
 * We need manual module init/exit here because the character-device
 * class has to exist before probe() can call device_create().
 */

static int __init devtitans_ir_init(void)
{
    int ret;

    ir_class = ir_class_create("devtitans_ir");
    if (IS_ERR(ir_class))
        return PTR_ERR(ir_class);

    ret = usb_register(&devtitans_ir_driver);
    if (ret) {
        class_destroy(ir_class);
        return ret;
    }

    return 0;
}


static void __exit devtitans_ir_exit(void)
{
    usb_deregister(&devtitans_ir_driver);

    class_destroy(ir_class);
}

module_init(devtitans_ir_init);
module_exit(devtitans_ir_exit);