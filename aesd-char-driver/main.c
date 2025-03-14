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
#include <linux/printk.h>
#include <linux/types.h>
int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Jack Center"); /** TODO: fill in your name **/
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

struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
};

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

  size_t byte_rtn = 0;
  const struct aesd_buffer_entry *buffer_entry =
      aesd_circular_buffer_find_entry_offset_for_fpos(
          &(dev_ptr->circular_buffer), (size_t)(*f_pos), &byte_rtn);
  if (buffer_entry == NULL) {
    PDEBUG("No `buffer_entry` at the requested offset: %lu", *f_pos);
    return 0;
  }

  size_t bytes_to_copy = min(count, buffer_entry->size);
  PDEBUG("Reading %lu bytes", bytes_to_copy);

  const ssize_t bytes_not_written =
      copy_to_user(buf, buffer_entry->buffptr, bytes_to_copy);
  PDEBUG("Bytes not written: %lu", bytes_written);

  const size_t bytes_written = bytes_to_copy - bytes_not_written;
  *f_pos += bytes_written;

  return bytes_written;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos) {
  PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

  /** Allocate memory to store user data */
  char *buffptr = kmalloc(count, GFP_KERNEL);
  if (!buffptr) {
    return -ENOMEM; // Memory allocation failure
  }

  /**
   * If the return value from copy_from_user is less than 0, there was an error.
   * Otherwise it is the number of bytes not copied.
   */
  const ssize_t retval = copy_from_user(buffptr, buf, count);
  if (retval < 0) {
    PDEBUG("`copy_from_user` returned %ld", retval);
    kfree(buffptr);
    return retval;
  }

  // TODO: If bytes copied does not equal the count, then store the data for later?
  const size_t bytes_copied = count - retval;

  struct aesd_dev *dev_ptr = (struct aesd_dev *)(filp->private_data);

  // NOTE: Just assume we got all of the data for now.
  
  const struct aesd_buffer_entry buffer_entry = {
      .buffptr = buffptr,
      .size = bytes_copied
  };

  aesd_circular_buffer_add_entry(&(dev_ptr->circular_buffer), &buffer_entry);

  return bytes_copied;
}

static int aesd_setup_cdev(struct aesd_dev *dev) {
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

  result = aesd_setup_cdev(&aesd_device);

  if (result) {
    unregister_chrdev_region(dev, 1);
  }
  return result;
}

void aesd_cleanup_module(void) {
  dev_t devno = MKDEV(aesd_major, aesd_minor);

  cdev_del(&aesd_device.cdev);

  /**
   * TODO: cleanup AESD specific poritions here as necessary
   */

  // Free any entries in the circular buffer.
  uint8_t index;
  struct aesd_buffer_entry *entry;
  AESD_CIRCULAR_BUFFER_FOREACH(entry, &(aesd_device.circular_buffer), index) {
    kfree(entry->buffptr);
  }

  unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
