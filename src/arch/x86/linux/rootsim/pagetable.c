#include <asm-generic/errno-base.h>

#include "rootsim.h"

// Macros to access subportions of an address
#define PML4(addr) (((long long)(addr) >> 39) & 0x1ff)
#define PDP(addr)  (((long long)(addr) >> 30) & 0x1ff)
#define PDE(addr)  (((long long)(addr) >> 21) & 0x1ff)
#define PTE(addr)  (((long long)(addr) >> 12) & 0x1ff)

#define SET_BIT(p,n) ((*(ulong *)(p)) |= (1LL << (n)))
#define CLR_BIT(p,n) ((*(ulong *)(p)) &= ~((1LL) << (n)))
#define GET_BIT(p,n) ((*(ulong *)(p)) & (1LL << (n)))

// TODO: This whole submodule should be re-engineered!

/*

void set_single_pte_sticky_flag(void *target_address) {
        void **pgd;
        void **pdp;
        void **pde;
        void **pte;

        pgd = (void **)current->mm->pgd;
        pdp = (void **)__va((ulong)pgd[PML4(target_address)] & 0xfffffffffffff000);
        pde = (void **)__va((ulong)pdp[PDP(target_address)] & 0xfffffffffffff000);
        pte = (void **)__va((ulong)pde[PDE(target_address)] & 0xfffffffffffff000);

        SET_BIT(&pte[PTE(target_address)], 9);
}

void set_pte_sticky_flags(ioctl_info *info) {
	void **pgd;
	void **pdp;
	void **pde;
	void **pte;
	int i,j;

	pgd = (void **)current->mm->pgd;
	pdp = (void **)__va((ulong)pgd[PML4(info->base_address)] & 0xfffffffffffff000);
	pde = (void **)__va((ulong)pdp[PDP(info->base_address)] & 0xfffffffffffff000);

	// This marks a LP as remote
	SET_BIT(&pdp[PDP(info->base_address)], 11);

	for(i = 0; i < 512; i++) {
		pte = (void **)__va((ulong)pde[i] & 0xfffffffffffff000);

		if(pte != NULL) {
			for(j = 0; j < 512; j++) {
				if(pte[j] != NULL) {
					if(GET_BIT(&pte[j], 0)) {
						CLR_BIT(&pte[j], 0);
						SET_BIT(&pte[j], 9);
					}
				}
			}
		}
	}
}

// is pdp!
int get_pde_sticky_bit(void *target_address) {
	void **pgd;
	void **pdp;

	pgd = (void **)current->mm->pgd;
	pdp = (void **)__va((ulong)pgd[PML4(target_address)] & 0xfffffffffffff000);

	return GET_BIT(&pdp[PDP(target_address)], 11);
}

int get_pte_sticky_bit(void *target_address) {
	void **pgd;
	void **pdp;
	void **pde;

	pgd = (void **)current->mm->pgd;
        pdp = (void **)__va((ulong)pgd[PML4(target_address)] & 0xfffffffffffff000);
        pde = (void **)__va((ulong)pdp[PDP(target_address)] & 0xfffffffffffff000);

	if(pde[PDE(target_address)] == NULL) {
		return 0;
	}

	return GET_BIT(&pde[PDE(target_address)], 9);
}

int get_presence_bit(void *target_address) {
	void **pgd;
	void **pdp;
	void **pde;
	void **pte;

	pgd = (void **)current->mm->pgd;
        pdp = (void **)__va((ulong)pgd[PML4(target_address)] & 0xfffffffffffff000);
        pde = (void **)__va((ulong)pdp[PDP(target_address)] & 0xfffffffffffff000);


	if(pde[PDE(target_address)] == NULL) {
		return 0;
	}

	pte = (void **)__va((ulong)pde[PDE(target_address)] & 0xfffffffffffff000);

	return GET_BIT(&pte[PTE(target_address)], 0);
}

void set_presence_bit(void *target_address) {
	void **pgd;
	void **pdp;
	void **pde;
	void **pte;

	pgd = (void **)current->mm->pgd;
	pdp = (void **)__va((ulong)pgd[PML4(target_address)] & 0xfffffffffffff000);
        pde = (void **)__va((ulong)pdp[PDP(target_address)] & 0xfffffffffffff000);
	pte = (void **)__va((ulong)pde[PDE(target_address)] & 0xfffffffffffff000);

	if(GET_BIT(&pte[PTE(target_address)], 9)) {
		SET_BIT(&pte[PTE(target_address)], 0);
	} else {
		printk("Oh, guarda che sto dentro all'else!!!!\n");
	}
}

void set_page_privilege(ioctl_info *info) {
        void **pgd;
        void **pdp;
        void **pde;
        void **pte;
	int i, j;

	void *base_address = info->base_address;
	void *final_address = info->base_address + info->count * PAGE_SIZE;

        pgd = (void **)current->mm->pgd;
	pdp = (void **)__va((ulong)pgd[PML4(info->base_address)] & 0xfffffffffffff000);
        pde = (void **)__va((ulong)pdp[PDP(info->base_address)] & 0xfffffffffffff000);

	for(i = PDE(base_address); i <= PDE(final_address); i++) {
		pte = (void **)__va((ulong)pde[i] & 0xfffffffffffff000);

		if(pte == NULL)
			continue;

		for(j = 0; j < 512; j++) {

			if(pte[j] == 0)
				continue;
			
			if(info->write_mode) {
				SET_BIT(&pte[j], 1);
			} else {
				CLR_BIT(&pte[j], 1);
			}
		}
	}
}

void set_single_page_privilege(ioctl_info *info) {
        void **pgd;
        void **pdp;
        void **pde;
        void **pte;

        pgd = (void **)current->mm->pgd;
	pdp = (void **)__va((ulong)pgd[PML4(info->base_address)] & 0xfffffffffffff000);
        pde = (void **)__va((ulong)pdp[PDP(info->base_address)] & 0xfffffffffffff000);
	pte = (void **)__va((ulong)pde[PDE(info->base_address)] & 0xfffffffffffff000);

        if(info->write_mode) {
                SET_BIT(&pte[PTE(info->base_address)], 1);
        } else {
		CLR_BIT(&pte[PTE(info->base_address)], 1);
	}
}


void release_pagetable(void) {
// already logged by ancestor set
	pml4 = restore_pml4; 
	involved_pml4 = restore_pml4_entries;

	for (j=0;j<SIBLING_PGD;j++){


		original_view[j] = NULL;
		continue;

		// SOme super bug down here!

		if(original_view[j]!=NULL){ // need to recover memory used for PDPs that have not been deallocated

			pgd_entry = (void **)pgd_addr[i];
			for (i=0; i<involved_pml4; i++){
			
			 	printk("\tPML4 ENTRY FOR CLOSE DEVICE IS %d\n",pml4);

				temp = pgd_entry[pml4];
				temp = (void *)((ulong) temp & 0xfffffffffffff000);	
				printk("temp is %p\n", temp);
				address = (void *)__va(temp);
				if(address!=NULL){
			//		__free_pages(address, 0); // TODO: there's a bug here! We leak memory instead! :-)
					printk("would free address at %p\n", address);
				}
				pgd_entry[pml4] = ancestor_pml4[pml4];

			}// end for i
			original_view[j]=NULL;
		}// enf if != NULL
	}// end for j
}
*/
