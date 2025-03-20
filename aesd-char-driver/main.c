/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include "aesdchar.h"
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/types.h>

#include "aesd_ioctl.h"

int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Jack Center");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

static int aesd_open(struct inode *inode, struct file *filp);
static int aesd_release(struct inode *inode, struct file *filp);
static ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                         loff_t *f_pos);
static ssize_t aesd_write(struct file *filp, const char __user *buf,
                          size_t count, loff_t *f_pos);
static int aesd_setup_cdev(struct aesd_dev *dev);
static int aesd_init_module(void);
static void aesd_cleanup_module(void);
static loff_t aesd_llseek(struct file *filp, loff_t offset, int whence);
static long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

/**
 * @brief Adjust the file offset `f_pos` parameter of `filp` based on the
 * location specified by `write_cmd` and `write_cmd_offset`
 * @return 0 if successful
 * @return -ERESTARTSYS if mutex could not be obtained
 * @return -EINVAL if write command or write_cmd_offset was out of range
 */
static long aesd_adjust_file_offset(struct file *filp, unsigned int write_cmd,
                                    unsigned int write_cmd_offset);

struct file_operations aesd_fops = {.owner = THIS_MODULE,
                                    .read = aesd_read,
                                    .write = aesd_write,
                                    .open = aesd_open,
                                    .release = aesd_release,
                                    .llseek = aesd_llseek,
                                    .unlocked_ioctl = aesd_ioctl};

static long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
  PDEBUG("ioctl with command %u", cmd);

  switch (cmd) {
  case AESDCHAR_IOCSEEKTO:
    struct aesd_seekto seekto;
    if (copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)) !=
        0) {
      return -EFAULT;
    }

    return aesd_adjust_file_offset(filp, seekto.write_cmd,
                                   seekto.write_cmd_offset);

  default:
    PDEBUG("unhandled ioctl command: %u", cmd);
    return -EINVAL;
  }
}

static long aesd_adjust_file_offset(struct file *filp, unsigned int write_cmd,
                                    unsigned int write_cmd_offset) {
  PDEBUG("aesd_adujust_file_offset with index %u and offset %u", write_cmd,
         write_cmd_offset);
  struct aesd_dev *dev_ptr = (struct aesd_dev *)(filp->private_data);
  if (mutex_lock_interruptible(&dev_ptr->device_mutex)) {
    return -ERESTARTSYS;
  }

  const struct aesd_buffer_entry *buffer_entry =
      aesd_circular_buffer_get_entry_by_index(write_cmd,
                                              &(dev_ptr->circular_buffer));
  if (NULL == buffer_entry || buffer_entry->size <= write_cmd_offset) {
    mutex_unlock(&dev_ptr->device_mutex);
    PDEBUG("No data a requested index (%u) and offset (%u)", write_cmd,
           write_cmd_offset);
    return -EINVAL;
  }

  const ssize_t entry_pos = aesd_circular_buffer_get_entry_offset(
      buffer_entry, &(dev_ptr->circular_buffer));
  if (entry_pos < 0) {
    mutex_unlock(&dev_ptr->device_mutex);
    return -EINVAL;
  }
  mutex_unlock(&dev_ptr->device_mutex);

  const size_t file_offset = entry_pos + write_cmd_offset;
  filp->f_pos += file_offset;
  return 0;
}

static loff_t aesd_llseek(struct file *filp, loff_t offset, int whence) {
  PDEBUG("llseek with offset %lld", offset);

  struct aesd_dev *dev_ptr = (struct aesd_dev *)(filp->private_data);
  mutex_lock(&dev_ptr->device_mutex);

  ssize_t circular_buffer_size =
      aesd_circular_buffer_get_size(&(dev_ptr->circular_buffer));
  if (circular_buffer_size < 0) {
    PDEBUG("aesd_circular_buffer_get_size returned an error");
    mutex_unlock(&dev_ptr->device_mutex);
    return -EINVAL;
  }

  const loff_t new_file_position =
      fixed_size_llseek(filp, offset, whence, (loff_t)circular_buffer_size);

  // TODO: validate new_file_position
  if (new_file_position < 0) {
    PDEBUG("seek position (%lld) resulted in a negative value", offset);
    mutex_unlock(&dev_ptr->device_mutex);
    return -EINVAL;
  }

  if (new_file_position > circular_buffer_size) {
    PDEBUG("seek position (%lld) exceeds circular buffer size", offset);
    mutex_unlock(&dev_ptr->device_mutex);
    return -EINVAL;
  }

  mutex_unlock(&dev_ptr->device_mutex);
  return new_file_position;
}

int aesd_open(struct inode *inode, struct file *filp) {
  PDEBUG("open");
  // Retrive `aesd_dev` based on position of cdev
  struct aesd_dev *device = container_of(inode->i_cdev, struct aesd_dev, cdev);

  // Set file `private_data` to our device structure pointer
  filp->private_data = device;

  return 0;
}

int aesd_release(struct inode *inode, struct file *filp) {
  PDEBUG("release");
  /**
   * TODO: handle release (deallocate anything `aesd_open` allocated)
   */
  return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                  loff_t *f_pos) {

  PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

  struct aesd_dev *dev_ptr = (struct aesd_dev *)(filp->private_data);
  mutex_lock(&dev_ptr->device_mutex);

  size_t byte_rtn = 0;
  const struct aesd_buffer_entry *buffer_entry =
      aesd_circular_buffer_find_entry_offset_for_fpos(
          &(dev_ptr->circular_buffer), (size_t)(*f_pos), &byte_rtn);
  if (buffer_entry == NULL) {
    PDEBUG("No `buffer_entry` at the requested offset: %lu", (size_t)(*f_pos));
    mutex_unlock(&dev_ptr->device_mutex);
    return 0;
  }

  // `remaining_bytes` is the number of bytes in the `buffer_entry->buffptr`
  // after the `byte_rtn`
  const size_t remaining_bytes = buffer_entry->size - byte_rtn;
  size_t bytes_to_copy = min(count, remaining_bytes);
  PDEBUG("Reading %lu bytes", bytes_to_copy);

  // TODO: I don't think this coppies right for no-zero offset
  const ssize_t bytes_not_written =
      copy_to_user(buf, buffer_entry->buffptr + byte_rtn, bytes_to_copy);
  PDEBUG("Bytes not read: %lu", bytes_not_written);

  const size_t bytes_written = bytes_to_copy - bytes_not_written;
  *f_pos += bytes_written;
  mutex_unlock(&dev_ptr->device_mutex);

  return bytes_written;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos) {
  PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

  struct aesd_dev *dev_ptr = (struct aesd_dev *)(filp->private_data);
  mutex_lock(&dev_ptr->device_mutex);

  /** Allocate memory to store user data */
  size_t staging_offset = 0;
  if (dev_ptr->buffer_entry_staging.size == 0) {
    PDEBUG("Allocating a new staging buffer");
    dev_ptr->buffer_entry_staging.buffptr = kmalloc(count, GFP_KERNEL);
    if (NULL == dev_ptr->buffer_entry_staging.buffptr) {
      mutex_unlock(&dev_ptr->device_mutex);
      return -ENOMEM; // Memory allocation failure
    }
    dev_ptr->buffer_entry_staging.size = count;
  } else {
    PDEBUG("Reallocating the staging buffer from %lu to %lu",
           dev_ptr->buffer_entry_staging.size,
           dev_ptr->buffer_entry_staging.size + count);
    dev_ptr->buffer_entry_staging.buffptr =
        krealloc(dev_ptr->buffer_entry_staging.buffptr,
                 dev_ptr->buffer_entry_staging.size + count, GFP_KERNEL);
    if (NULL == dev_ptr->buffer_entry_staging.buffptr) {
      kfree(dev_ptr->buffer_entry_staging.buffptr);
      mutex_unlock(&dev_ptr->device_mutex);
      return -ENOMEM; // Memory allocation failure
    }
    staging_offset = dev_ptr->buffer_entry_staging.size;
    dev_ptr->buffer_entry_staging.size += count;
  }
  PDEBUG("Staging buffer size: %lu", dev_ptr->buffer_entry_staging.size);

  char *write_ptr = dev_ptr->buffer_entry_staging.buffptr + staging_offset;
  /**
   * If the return value from copy_from_user is less than 0, there was an error.
   * Otherwise it is the number of bytes not copied.
   */
  const ssize_t retval = copy_from_user(write_ptr, buf, count);
  if (retval < 0) {
    PDEBUG("`copy_from_user` returned %ld", retval);
    kfree(dev_ptr->buffer_entry_staging.buffptr);
    mutex_unlock(&dev_ptr->device_mutex);
    return retval;
  }

  const size_t bytes_copied = count - retval;
  *f_pos += bytes_copied;

  // Only write if we have the line termination character
  const char *end_of_line_ptr = memchr(write_ptr, '\n', count);
  if (NULL == end_of_line_ptr) {
    PDEBUG("Write is incomplete, the termination character was not found.");
    mutex_unlock(&dev_ptr->device_mutex);
    return bytes_copied;
  }

  const char *replaced_buffptr = aesd_circular_buffer_add_entry(
      &(dev_ptr->circular_buffer), &(dev_ptr->buffer_entry_staging));

  if (NULL != replaced_buffptr) {
    kfree(replaced_buffptr);
  }

  // Clear the staging data
  dev_ptr->buffer_entry_staging.buffptr = NULL;
  dev_ptr->buffer_entry_staging.size = 0;

  mutex_unlock(&dev_ptr->device_mutex);

  return bytes_copied;
}

static int aesd_setup_cdev(struct aesd_dev *dev) {
  PDEBUG("aesd_setup_cdev");
  int err, devno = MKDEV(aesd_major, aesd_minor);

  cdev_init(&dev->cdev, &aesd_fops);
  dev->cdev.owner = THIS_MODULE;
  dev->cdev.ops = &aesd_fops;
  err = cdev_add(&dev->cdev, devno, 1);
  if (err) {
    printk(KERN_ERR "Error %d adding aesd cdev", err);
  }
  return err;
}

int aesd_init_module(void) {
  PDEBUG("aesd_init_module");
  dev_t dev = 0;
  int result;
  result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
  aesd_major = MAJOR(dev);
  if (result < 0) {
    printk(KERN_WARNING "Can't get major %d\n", aesd_major);
    return result;
  }
  memset(&aesd_device, 0, sizeof(struct aesd_dev));

  aesd_circular_buffer_init(&(aesd_device.circular_buffer));
  mutex_init(&(aesd_device.device_mutex));

  result = aesd_setup_cdev(&aesd_device);

  if (result) {
    unregister_chrdev_region(dev, 1);
  }
  return result;
}

void aesd_cleanup_module(void) {
  PDEBUG("aesd_cleanup_module");
  dev_t devno = MKDEV(aesd_major, aesd_minor);

  cdev_del(&aesd_device.cdev);

  // Free any entries in the circular buffer.
  uint8_t index;
  struct aesd_buffer_entry *entry;
  AESD_CIRCULAR_BUFFER_FOREACH(entry, &(aesd_device.circular_buffer), index) {
    if (entry->buffptr != NULL) {
      kfree(entry->buffptr);
    }
  }

  if (aesd_device.buffer_entry_staging.buffptr != NULL) {
    kfree(aesd_device.buffer_entry_staging.buffptr);
  }

  unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
