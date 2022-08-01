#include <string.h>
#include "kernel/calls.h"

extern bool doEnableExtraLocking;
extern pthread_mutex_t extra_lock;

static int __user_read_task(struct task *task, addr_t addr, void *buf, size_t count) {
    char *cbuf = (char *) buf;
    addr_t p = addr;
    ////modify_critical_region_counter(task, 1, __FILE__, __LINE__); // Everyone who calls this function sets alrady
    while (p < addr + count) {
        addr_t chunk_end = (PAGE(p) + 1) << PAGE_BITS;
        if (chunk_end > addr + count)
            chunk_end = addr + count;
        const char *ptr = mem_ptr(task->mem, p, MEM_READ);
        if (ptr == NULL) {
            // //modify_critical_region_counter(task, -1, __FILE__, __LINE__);
            return 1;
	}
        memcpy(&cbuf[p - addr], ptr, chunk_end - p);
        p = chunk_end;
    }
    ////modify_critical_region_counter(task, -1, __FILE__, __LINE__);
    return 0;
}

static int __user_write_task(struct task *task, addr_t addr, const void *buf, size_t count) {
<<<<<<< HEAD
    task->critical_region_count++;
=======
    //modify_critical_region_counter(task, 1, __FILE__, __LINE__);
>>>>>>> 2eebde1688b242d9ec29a6af5d1374758e1b1f41
    const char *cbuf = (const char *) buf;
    addr_t p = addr;
    while (p < addr + count) {
        addr_t chunk_end = (PAGE(p) + 1) << PAGE_BITS;
        if (chunk_end > addr + count)
            chunk_end = addr + count;
        char *ptr = mem_ptr(task->mem, p, MEM_WRITE);
        if (ptr == NULL) {
<<<<<<< HEAD
            task->critical_region_count--;
=======
            //modify_critical_region_counter(task, -1, __FILE__, __LINE__);
>>>>>>> 2eebde1688b242d9ec29a6af5d1374758e1b1f41
            return 1;
        }
        memcpy(ptr, &cbuf[p - addr], chunk_end - p);
        p = chunk_end;
    }
    
<<<<<<< HEAD
    task->critical_region_count--;
=======
    //modify_critical_region_counter(task, -1, __FILE__, __LINE__);
>>>>>>> 2eebde1688b242d9ec29a6af5d1374758e1b1f41
    return 0;
}

int user_read_task(struct task *task, addr_t addr, void *buf, size_t count) {
<<<<<<< HEAD
    task->critical_region_count++;
    read_lock(&task->mem->lock);
=======
    read_lock(&task->mem->lock, __FILE__, __LINE__);
>>>>>>> 2eebde1688b242d9ec29a6af5d1374758e1b1f41

    //modify_critical_region_counter(task, 1, __FILE__, __LINE__);
    int res = __user_read_task(task, addr, buf, count);
    //modify_critical_region_counter(task, -1, __FILE__, __LINE__);

<<<<<<< HEAD
    read_unlock(&task->mem->lock);
    task->critical_region_count--;
=======
    read_unlock(&task->mem->lock, __FILE__, __LINE__);
>>>>>>> 2eebde1688b242d9ec29a6af5d1374758e1b1f41
    return res;
}

int user_read(addr_t addr, void *buf, size_t count) {
    return user_read_task(current, addr, buf, count);
}

int user_write_task(struct task *task, addr_t addr, const void *buf, size_t count) { // This function has 'write' in the name, yet uses a read lock?  -mke
    //modify_critical_region_counter(task, 1, __FILE__, __LINE__);
    read_lock(&task->mem->lock, __FILE__, __LINE__);
    int res = __user_write_task(task, addr, buf, count);
    read_unlock(&task->mem->lock, __FILE__, __LINE__);
    //modify_critical_region_counter(task, -1, __FILE__, __LINE__);
    return res;
}

int user_write(addr_t addr, const void *buf, size_t count) {
    return user_write_task(current, addr, buf, count);
}

int user_read_string(addr_t addr, char *buf, size_t max) {
<<<<<<< HEAD
    current->critical_region_count++;
    if (addr == 0) {
        current->critical_region_count--;
=======
    ////modify_critical_region_counter(current, 1, __FILE__, __LINE__);
    if (addr == 0) {
        ////modify_critical_region_counter(current, -1, __FILE__, __LINE__);
>>>>>>> 2eebde1688b242d9ec29a6af5d1374758e1b1f41
        return 1;
    }
    //modify_critical_region_counter(current, 1, __FILE__, __LINE__);
    read_lock(&current->mem->lock, __FILE__, __LINE__);
    size_t i = 0;
    while (i < max) {
        if (__user_read_task(current, addr + i, &buf[i], sizeof(buf[i]))) {
<<<<<<< HEAD
            read_unlock(&current->mem->lock);
            current->critical_region_count--;
=======
            read_unlock(&current->mem->lock, __FILE__, __LINE__);
            ////modify_critical_region_counter(current, -1, __FILE__, __LINE__);
>>>>>>> 2eebde1688b242d9ec29a6af5d1374758e1b1f41
            return 1;
        }
        if (buf[i] == '\0')
            break;
        i++;
    }
<<<<<<< HEAD
    read_unlock(&current->mem->lock);
    current->critical_region_count--;
=======
    read_unlock(&current->mem->lock, __FILE__, __LINE__);
    //modify_critical_region_counter(current, -1, __FILE__, __LINE__);
>>>>>>> 2eebde1688b242d9ec29a6af5d1374758e1b1f41
    return 0;
}

int user_write_string(addr_t addr, const char *buf) {
    ////modify_critical_region_counter(current, 1, __FILE__, __LINE__);
    if (addr == 0) {
        ////modify_critical_region_counter(current, -1, __FILE__, __LINE__);
        return 1;
    }
    read_lock(&current->mem->lock, __FILE__, __LINE__);
    size_t i = 0;
    do {
        //modify_critical_region_counter(current, 1, __FILE__, __LINE__);
        if (__user_write_task(current, addr + i, &buf[i], sizeof(buf[i]))) {
            read_unlock(&current->mem->lock, __FILE__, __LINE__);
            //modify_critical_region_counter(current, -1, __FILE__, __LINE__);
            return 1;
        }
        //modify_critical_region_counter(current, -1, __FILE__, __LINE__);
        i++;
    } while (buf[i - 1] != '\0');
    read_unlock(&current->mem->lock, __FILE__, __LINE__);
    ////modify_critical_region_counter(current, -1, __FILE__, __LINE__);
    return 0;
}
