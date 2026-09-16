#include <linux/module.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Felipe Peres de Almeida");
MODULE_DESCRIPTION("DevTitans IR USB driver");
MODULE_VERSION("0.1");

#define VENDOR_ID  0x10C4
#define PRODUCT_ID 0xEA60

#define IR_HEADER_SIZE       16
#define IR_MAX_PAYLOAD_SIZE  1036
#define IR_MAX_PACKET_SIZE   (IR_HEADER_SIZE + IR_MAX_PAYLOAD_SIZE)

#define IR_MAGIC_0  0x49  /* 'I' */
#define IR_MAGIC_1  0x52  /* 'R' */

#define IR_PROTOCOL_VERSION  1

#define IR_CMD_READ   0x01
#define IR_CMD_WRITE  0x02
#define IR_CMD_PING   0x03

/* USB endpoint number */
#define IR_ENDPOINT 0x01


struct devtitans_ir_device {
    struct usb_device *udev;
    struct usb_interface *interface;

    unsigned int carrier;

    struct cdev cdev;
    dev_t devt;
};


/*
 * Character device: open.
 */
static int ir_open(struct inode *inode, struct file *file)
{
    struct devtitans_ir_device *ir;

    ir = container_of(inode->i_cdev,
                      struct devtitans_ir_device,
                      cdev);

    file->private_data = ir;

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
    int actual_length;
    int ret;

    if (count == 0)
        return 0;

    if (count < IR_HEADER_SIZE)
        return -EINVAL;

    if (count > IR_MAX_PACKET_SIZE)
        return -EMSGSIZE;

    packet = memdup_user(buf, count);
    if (IS_ERR(packet))
        return PTR_ERR(packet);

    ret = usb_bulk_msg(
        ir->udev,
        usb_sndbulkpipe(ir->udev, IR_ENDPOINT),
        packet,
        count,
        &actual_length,
        1000
    );

    kfree(packet);

    if (ret) {
        dev_err(&ir->interface->dev,
                "Failed to transmit packet: %d\n",
                ret);
        return ret;
    }

    if (actual_length != count) {
        dev_err(&ir->interface->dev,
                "Incomplete transmission: %d/%zu bytes\n",
                actual_length,
                count);
        return -EIO;
    }

    return count;
}

/*
 * Character device: read.
 *
 * A packet is received directly from the USB IN endpoint
 * and returned to userspace unchanged.
 */
static ssize_t ir_read(struct file *file,
                       char __user *buf,
                       size_t count,
                       loff_t *ppos)
{
    struct devtitans_ir_device *ir = file->private_data;
    u8 *packet;
    int actual_length;
    int ret;

    if (count == 0)
        return 0;

    if (count < IR_MAX_PACKET_SIZE)
        return -EINVAL;

    packet = kmalloc(IR_MAX_PACKET_SIZE, GFP_KERNEL);
    if (!packet)
        return -ENOMEM;

    ret = usb_bulk_msg(
        ir->udev,
        usb_rcvbulkpipe(ir->udev, IR_ENDPOINT),
        packet,
        IR_MAX_PACKET_SIZE,
        &actual_length,
        1000
    );

    if (ret) {
        dev_err(&ir->interface->dev,
                "Failed to receive packet: %d\n",
                ret);
        kfree(packet);
        return ret;
    }

    if (copy_to_user(buf, packet, actual_length)) {
        kfree(packet);
        return -EFAULT;
    }

    kfree(packet);

    return actual_length;
}


/*
 * Character device operations.
 */
static const struct file_operations ir_fops = {
    .owner = THIS_MODULE,
    .open = ir_open,
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
     * Find and display the USB endpoints.
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

    /*
     * Allocate character device number.
     */
    ret = alloc_chrdev_region(&ir->devt, 0, 1, "devtitans_ir");
    if (ret) {
        device_remove_file(&interface->dev, &dev_attr_carrier);
        dev_set_drvdata(&interface->dev, NULL);
        usb_set_intfdata(interface, NULL);
        return ret;
    }

    /*
     * Initialize character device.
     */
    cdev_init(&ir->cdev, &ir_fops);
    ir->cdev.owner = THIS_MODULE;

    /*
     * Register character device.
     */
    ret = cdev_add(&ir->cdev, ir->devt, 1);
    if (ret) {
        unregister_chrdev_region(ir->devt, 1);
        device_remove_file(&interface->dev, &dev_attr_carrier);
        dev_set_drvdata(&interface->dev, NULL);
        usb_set_intfdata(interface, NULL);
        return ret;
    }

    /*
     * Create /dev/devtitans_ir0.
     */
    device_create(ir_class,
                  &interface->dev,
                  ir->devt,
                  NULL,
                  "devtitans_ir%d",
                  MINOR(ir->devt));

    dev_info(&interface->dev,
             "DevTitans IR device connected\n");

    return 0;
}


/*
 * Disconnect.
 */

static void ir_disconnect(struct usb_interface *interface)
{
    struct devtitans_ir_device *ir;

    ir = usb_get_intfdata(interface);

    if (!ir)
        return;

    device_destroy(ir_class, ir->devt);

    cdev_del(&ir->cdev);

    unregister_chrdev_region(ir->devt, 1);

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



/*
 * Module initialization.
 *
 * We need manual module init/exit here because the character-device
 * class has to exist before probe() can call device_create().
 */

static int __init devtitans_ir_init(void)
{
    int ret;

    ir_class = class_create(THIS_MODULE, "devtitans_ir");
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