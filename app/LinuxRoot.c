//
//  LinuxRoot.c
//  libiSHLinux
//
//  Created by Theodore Dubois on 12/29/21.
//

#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/errname.h>
#include <linux/device.h>
#include <uapi/linux/mount.h>
#include "LinuxInterop.h"

static __init int ish_rootfs(void) {
    rootfs_mounted = true;

    const char *fakefs_path = DefaultRootPath();
    int err = do_mount(fakefs_path, "/root", "fakefs", MS_SILENT, NULL);
    if (err < 0) {
        pr_emerg("fakefs: failed to mount root from %s: %s\n", fakefs_path, errname(err));
        return err;
    }
    ksys_chdir("/root");

    devtmpfs_mount();
    err = do_mount("proc", "proc", "proc", MS_SILENT, NULL);
    if (err < 0) {
        pr_warn("procfs: failed to mount: %s", errname(err));
    }

    do_mount(".", "/", NULL, MS_MOVE, NULL);
    ksys_chroot(".");
    return 0;
}

rootfs_initcall(ish_rootfs);
