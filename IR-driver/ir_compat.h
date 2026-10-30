#ifndef DEVTITANS_IR_COMPAT_H
#define DEVTITANS_IR_COMPAT_H

#include <linux/version.h>
#include <linux/device/class.h>

static inline struct class *ir_class_create(const char *name)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
    return class_create(THIS_MODULE, name);
#else
    return class_create(name);
#endif
}

#endif