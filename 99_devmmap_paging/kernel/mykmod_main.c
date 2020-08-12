#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <mydev.h>

MODULE_DESCRIPTION("My kernel module - mykmod");
MODULE_AUTHOR("maruthisi.inukonda [at] gmail.com");
MODULE_LICENSE("GPL");

// Dynamically allocate major no
#define MYKMOD_MAX_DEVS 256
#define MYKMOD_DEV_MAJOR 0

static int mykmod_init_module(void);
static void mykmod_cleanup_module(void);

static int mykmod_open(struct inode *inode, struct file *filp);
static int mykmod_close(struct inode *inode, struct file *filp);
static int mykmod_mmap(struct file *filp, struct vm_area_struct *vma);

module_init(mykmod_init_module);
module_exit(mykmod_cleanup_module);

static struct file_operations mykmod_fops = {
    .owner = THIS_MODULE,    /* owner (struct module *) */
    .open = mykmod_open,     /* open */
    .release = mykmod_close, /* release */
    .mmap = mykmod_mmap,     /* mmap */
};

static void mykmod_vm_open(struct vm_area_struct *vma);
static void mykmod_vm_close(struct vm_area_struct *vma);
// static int mykmod_vm_fault(struct vm_fault *vmf);
static int mykmod_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf);

// TODO Data-structure to keep per device info
struct dev_file_info {
  char *Data;  // pointer to data to be read/write
  size_t Size; // size of the data pointed by Data
};

// TODO Device table data-structure to keep all devices
// I am using array of (struct dev_file_info)pointers to store device table
struct dev_file_info *device_table[MYKMOD_MAX_DEVS];

// TODO Data-structure to keep per VMA info
struct vm_using_struct {
  int npagefaults;
  struct dev_file_info *dev_info; // pointer to device-info
};

static const struct vm_operations_struct mykmod_vm_ops = {
    .open = mykmod_vm_open, .close = mykmod_vm_close, .fault = mykmod_vm_fault};

int mykmod_major;
int device_count; // initially device count is 0

static int mykmod_init_module(void) {
  printk("mykmod loaded\n");
  printk("mykmod initialized at=%p\n", init_module);

  if ((mykmod_major =
           register_chrdev(MYKMOD_DEV_MAJOR, "mykmod", &mykmod_fops)) < 0) {
    printk(KERN_WARNING "Failed to register character device\n");
    return 1;
  } else {
    printk("register character device %d\n", mykmod_major);
  }
  // TODO initialize device table
  // it is initialised globally(array of pointers to device-info)

  return 0;
}

static void mykmod_cleanup_module(void) {
  int i = 0;
  printk("mykmod unloaded\n");
  unregister_chrdev(mykmod_major, "mykmod");
  // TODO free device info structures from device table
  while ((i < MYKMOD_MAX_DEVS) && (device_table[i] != NULL)) {
    kfree(device_table[i]->Data); // freeing the data in device info
    kfree(device_table[i]);       // freeing the srtuct storing the device info
    i++;                          // checking every device
  }
  return;
}

static int mykmod_open(struct inode *inodep, struct file *filep) {
  printk("mykmod_open: filep=%p f->private_data=%p "
         "inodep=%p i_private=%p i_rdev=%x maj:%d min:%d\n",
         filep, filep->private_data, inodep, inodep->i_private, inodep->i_rdev,
         MAJOR(inodep->i_rdev), MINOR(inodep->i_rdev));

  // TODO: Allocate memory for devinfo and store in device table and i_private.
  // done
  if (inodep->i_private == NULL) {
    struct dev_file_info *info;
    info = kmalloc(sizeof(struct dev_file_info), GFP_KERNEL);
    // printk("mykmod_open: virtual to physical = 0x%llx\n",(1llu *
    // virt_to_phys((void*)info)));
    info->Data = kzalloc(MYDEV_LEN, GFP_KERNEL);
    info->Size = MYDEV_LEN;
    inodep->i_private = (void *)info;
    device_table[device_count] = info; // storing the dev_info in device table
    device_count++;
  }

  // Store device info in file's private_data aswell
  filep->private_data = inodep->i_private;

  return 0;
}

static int mykmod_close(struct inode *inodep, struct file *filep) {
  // TODO: Release memory allocated for data-structures.

  // memory should not be cleared here itself,
  // It must be cleaned in cleanup module, or else we get seg-faults
  printk("mykmod_close: inodep=%p filep=%p\n", inodep, filep);
  return 0;
}

static int mykmod_mmap(struct file *filep, struct vm_area_struct *vma) {
  struct vm_using_struct
      *info; // for storing the dev info and  keep track of no. of pagefaults
  printk("mykmod_mmap: filp=%p vma=%p flags=%lx\n", filep, vma, vma->vm_flags);

  // TODO setup vma's flags, save private data (devinfo, npagefaults) in
  // vm_private_data
  // done
  vma->vm_ops =
      &mykmod_vm_ops; // using the functionalities of struct mykmod_vm_ops
  vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;

  info = kmalloc(sizeof(struct vm_using_struct), GFP_KERNEL);
  info->dev_info = (struct dev_file_info *)filep->private_data;
  vma->vm_private_data = (void *)info;
  mykmod_vm_open(vma);

  // return -ENOSYS; // Remove this once mmap is implemented.
  return 0;
}

static void mykmod_vm_open(
    struct vm_area_struct *vma) { // typecasting the void pointer to stuct of
                                  // necessary type and accessing its elements
  ((struct vm_using_struct *)(vma->vm_private_data))->npagefaults = 0;
  // initialising the pagefaults to 0, when we enter mykmod_	vm_open
  printk("mykmod_vm_open: vma=%p npagefaults:%lu\n", vma,
         (1lu) *
             (((struct vm_using_struct *)(vma->vm_private_data))->npagefaults));
}

static void mykmod_vm_close(struct vm_area_struct *vma) {
  printk("mykmod_vm_close: vma=%p npagefaults:%lu\n", vma,
         (1lu) *
             (((struct vm_using_struct *)(vma->vm_private_data))->npagefaults));
  kfree((struct vm_using_struct *)
            vma->vm_private_data); // freeing the struct created to keep track
                                   // of page faults
}

static int mykmod_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf) {
  unsigned long dist_from_base_address, total_offset_in_bytes, vir_add;
  struct page *Page;
  (((struct vm_using_struct *)(vma->vm_private_data))->npagefaults) += 1;
  // incrementing the pagefaults when we enter mykmod_vm_fault

  // TODO: build virt->phys mappings
  dist_from_base_address = vmf->pgoff + vma->vm_pgoff;

  total_offset_in_bytes = dist_from_base_address << PAGE_SHIFT;
  // moving page shift bits will multiply it by page size(4096)

  vir_add = ((unsigned long)((((struct vm_using_struct *)(vma->vm_private_data))
                                  ->dev_info)
                                 ->Data)) +
            total_offset_in_bytes;

  Page = virt_to_page(vir_add);
  get_page(Page);
  vmf->page = Page;
  printk("mykmod_vm_fault: vma=%p vmf=%p pgoff=%lu page=%p\n", vma, vmf,
         vmf->pgoff, vmf->page);
  return 0;
}
