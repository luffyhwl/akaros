/* Copyright 2015 Google Inc.
 * 
 * See LICENSE for details.
 */

/* We're not going to falll into the trap of only compiling support
 * for AMD OR Intel for an image. It all gets compiled in, and which
 * one you use depends on on cpuinfo, not a compile-time
 * switch. That's proven to be the best strategy.  Conditionally
 * compiling in support is the path to hell.
 */
#include <assert.h>
#include <pmap.h>
#include <smp.h>
#include <kmalloc.h>

#include "intel/vmx.h"
#include "vmm.h"

/* TODO: have better cpuid info storage and checks */
bool x86_supports_vmx = FALSE;

/* Figure out what kind of CPU we are on, and if it supports any reasonable
 * virtualization. For now, if we're not some sort of newer intel, don't
 * bother. This does all cores. Again, note, we make these decisions at runtime,
 * to avoid getting into the problems that compile-time decisions can cause. 
 * At this point, of course, it's still all intel.
 */
void vmm_init(void)
{
	int ret;
	/* Check first for intel capabilities. This is hence two back-to-back
	 * implementationd-dependent checks. That's ok, it's all msr dependent.
	 */
	ret = intel_vmm_init();
	if (! ret) {
		printd("intel_vmm_init worked\n");
		x86_supports_vmx = TRUE;
		return;
	}

	/* TODO: AMD. Will we ever care? It's not clear. */
	printk("vmm_init failed, ret %d\n", ret);
	return;
}

void vmm_pcpu_init(void)
{
	if (!x86_supports_vmx)
		return;
	if (! intel_vmm_pcpu_init()) {
		printd("vmm_pcpu_init worked\n");
		return;
	}
	/* TODO: AMD. Will we ever care? It's not clear. */
	printk("vmm_pcpu_init failed\n");
}

int vm_run(uint64_t rip, uint64_t rsp, uint64_t cr3)
{
	int vmx_launch(uint64_t rip, uint64_t rsp, uint64_t cr3);
	if (current->vmm.amd) {
		return -1;
	} else {
		return vmx_launch(rip, rsp, cr3);
	}
	return -1;
}

/* Initializes a process to run virtual machine contexts, returning the number
 * initialized, optionally setting errno */
int vmm_struct_init(struct proc *p, unsigned int nr_guest_pcores)
{
	struct vmm *vmm = &p->vmm;
	unsigned int i;
	if (!x86_supports_vmx) {
		set_errno(ENODEV);
		return 0;
	}
	qlock(&vmm->qlock);
	if (vmm->vmmcp) {
		set_errno(EINVAL);
		qunlock(&vmm->qlock);
		return 0;
	}
	/* Set this early, so cleanup checks the gpc array */
	vmm->vmmcp = TRUE;
	nr_guest_pcores = MIN(nr_guest_pcores, num_cpus);
	vmm->amd = 0;
	vmm->guest_pcores = kzmalloc(sizeof(void*) * nr_guest_pcores, KMALLOC_WAIT);
	for (i = 0; i < nr_guest_pcores; i++) {
		vmm->guest_pcores[i] = vmx_create_vcpu(p);
		/* If we failed, we'll clean it up when the process dies */
		if (!vmm->guest_pcores[i]) {
			set_errno(ENOMEM);
			break;
		}
	}
	vmm->nr_guest_pcores = i;
	qunlock(&vmm->qlock);
	return i;
}

/* Has no concurrency protection - only call this when you know you have the
 * only ref to vmm.  For instance, from __proc_free, where there is only one ref
 * to the proc (and thus proc.vmm). */
void __vmm_struct_cleanup(struct proc *p)
{
	struct vmm *vmm = &p->vmm;
	if (!vmm->vmmcp)
		return;
	for (int i = 0; i < vmm->nr_guest_pcores; i++) {
		if (vmm->guest_pcores[i])
			vmx_destroy_vcpu(vmm->guest_pcores[i]);
	}
	kfree(vmm->guest_pcores);
	ept_flush(p->env_pgdir.eptp);
	vmm->vmmcp = FALSE;
}
