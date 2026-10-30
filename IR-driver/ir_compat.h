#ifndef DEVTITANS_IR_COMPAT_H
#define DEVTITANS_IR_COMPAT_H

#include <linux/version.h>

// If the kernel version is older than 6.3.0, use the monolithic device.h
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0)
#include <linux/device.h>
#else
#include <linux/device/class.h>
#endif

static inline struct class *ir_class_create(const char *name)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
    return class_create(THIS_MODULE, name);
#else
    return class_create(name);
#endif
}

#endif