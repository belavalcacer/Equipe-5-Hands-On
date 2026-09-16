#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");                                    
MODULE_AUTHOR("Felipe Peres de Almeida");                                 
MODULE_DESCRIPTION("IR kernel module to connect with the IR hardware module for the HandsOn");  
MODULE_VERSION("0.1");                                     

#define VENDOR_ID  0x10C4  /* VendorID  do CP2102 */
#define PRODUCT_ID 0xEA60  /* ProductID do CP2102 */

static const struct usb_device_id ir_table[] = {
    { USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
    { }
};

MODULE_DEVICE_TABLE(usb, ir_table);

static void ir_payload_show(struct device *dev, struct device_attribute *attr, char* buf);
static void ir_payload_store(struct device* dev, struct device_attribute *attr, char* buf, size_t count);
static void ir_probe(struct usb_interface *interface,
                      const struct usb_device_id *id);
static void ir_disconnect(struct usb_interface *interface);

static ssize_t ir_payload_show(struct kobject *kobject, struct kobj_attribute *attr, char* buf){
    return sprintf(buf, "IR: %s\n", payload);
}

static ir_payload_store(struct kobject *kobject, struct kobj_attribute *attr, const char* buf, size_t count){
    ssize_t res = strscpy(payload, buf, sizeof(payload));
    if (res < 0) return res;
    strim(payload)
    return count;   
}

typedef struct ir_device {
    struct usb_device *udev;
    struct usb_interface *interface;
    unsigned int carrier;
} devtitans_ir_device;


static int ir_probe(struct usb_interface *interface,
                      const struct usb_device_id *id)
{
    struct devtitans_ir_device *ir;

    ir = devm_kzalloc(&interface->dev, sizeof(*ir), GFP_KERNEL);
    if (!ir)
        return -ENOMEM;

    ir->udev = interface_to_usbdev(interface);
    ir->interface = interface;
    ir->carrier = 38000;

    usb_set_intfdata(interface, ir);

    dev_set_drvdata(&interface->dev, ir);

    return device_create_file(&interface->dev, &dev_attr_carrier);
}

static void myir_disconnect(struct usb_interface *interface)
{
    device_remove_file(&interface->dev, &dev_attr_carrier);
    usb_set_intfdata(interface, NULL);
}

static struct usb_driver devtitans_ir_driver = {
    .name = "devtitans_ir_driver",
    .probe = ir_probe,
    .disconnect = ir_disconnect,
    .id_table = ir_table
}

module_usb_driver(devtitans_ir_driver);