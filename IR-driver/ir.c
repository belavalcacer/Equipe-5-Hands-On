#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");                                    
MODULE_AUTHOR("Felipe Peres de Almeida");                                 
MODULE_DESCRIPTION("IR kernel module to connect with the IR hardware module for the HandsOn");  
MODULE_VERSION("0.1");                                     


char payload[512]; // TODO: check if the payload needs to be null terminated

static void ir_payload_show(struct kobject *kobject, struct kobj_attribute *attr, char* buf);
static void ir_payload_store(void);
static void ir_set_pulse_interval(float interval);
static void set_payload_size(struct kobject *kobject, struct kobj_attribute *att, size_t size);

static ssize_t ir_payload_show(struct kobject *kobject, struct kobj_attribute *attr, char* buf){
    return sprintf(buf, "IR: %s\n", payload);
}

static ir_payload_store(struct kobject *kobject, struct kobj_attribute *attr, const char* buf, size_t count){
    ssize_t res = strscpy(payload, buf, sizeof(payload));
    if (res < 0) return res;
    strim(payload)
    return count;   
}

int init_module(void){
    printk(KERN_INFO "IR: module loaded");
    return 0;
}

void cleanup_module(void){
    printk(KERN_INFO "IR: module unloaded");
}