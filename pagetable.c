#include <stdio.h>
#include <stdlib.h>
#include "vm.h"
#include "API.h"
#include "list.h"

struct Node * head = NULL;

bool clock_bits[256];

int fifo()
{
		return head->data;
}

int lru()
{
		return head->data;
}

int clock()
{
		// iterate through FIFO list until bit is false
		struct Node * temp = head;
		int PFN = temp->data;
		while (temp->next) {

			if (clock_bits[PFN]) {
				clock_bits[PFN] = false;
			} else {
				return PFN;
			}

		}

		return head->data;
}

/*========================================================================*/

int find_replacement()
{
		int PFN;
		if(replacementPolicy == ZERO)  PFN = 0;
		else if(replacementPolicy == FIFO)  PFN = fifo();
		else if(replacementPolicy == LRU) PFN = lru();
		else if(replacementPolicy == CLOCK) PFN = clock();

		return PFN;
}

int pagefault_handler(int pid, int VPN, char type)
{

		int PFN;

		// find a free PFN.
		PFN = get_freeframe();
		
		// no free frame available. find a victim using page replacement. ;
		if(PFN < 0) {
				PFN = find_replacement();
				/* ---- */
				// swap out
				IPTE ipte = read_IPTE(PFN);
				PTE temp = read_PTE(ipte.pid, ipte.VPN);
				temp.valid = false;
				write_PTE(ipte.pid, ipte.VPN, temp);
				if (temp.dirty) {
					swap_out(ipte.pid, ipte.VPN, PFN);
				}

				// editing list based on replacement policy
				if (replacementPolicy == FIFO) {
					head = list_remove_head(head);
				} else if (replacementPolicy == LRU) {
					head = list_remove(head, PFN);
				} else if (replacementPolicy == CLOCK) {
					head = list_remove_head(head);	
					clock_bits[PFN] = false;
				}
		}
		
		/* ---- */
		// swap in
		swap_in(pid, VPN, PFN);

		// update table
		PTE pte;
		pte.PFN = PFN;
		pte.valid = true;
		pte.dirty = (type == 'W');
		write_PTE(pid, VPN, pte);

		IPTE ipte;
		ipte.pid = pid;
		ipte.VPN = VPN;
		write_IPTE(PFN, ipte);

		// updating list based on replacement policy
		if (replacementPolicy == FIFO) {
			head = list_insert_tail(head, PFN);
		} else if (replacementPolicy == LRU) {
			head = list_insert_tail(head, PFN);
		} else if (replacementPolicy == CLOCK) {
			head = list_insert_tail(head, PFN);
			clock_bits[PFN] = false;
		}

		return PFN;
}

int is_page_hit(int pid, int VPN, char type)
{
		/* Read page table entry for (pid, VPN) */
		PTE pte = read_PTE(pid, VPN);

		/* if PTE is valid, it is a page hit. Return physical frame number (PFN) */
		if(pte.valid) {
		/* Mark the page dirty, if it is a write request */
				if(type == 'W') {
						pte.dirty = true;
						write_PTE(pid, VPN, pte);
				}
		/* Need to take care of a page replacement data structure (LRU, CLOCK) for the page hit*/
		/* ---- */
			if (replacementPolicy == LRU) {
				head = list_remove(head, pte.PFN);
				head = list_insert_tail(head, pte.PFN);
			} else if (replacementPolicy == CLOCK) {
				clock_bits[pte.PFN] = true;
			}

				return pte.PFN;
		}
		
		/* PageFault, if the PTE is invalid. Return -1 */
		return -1;
}

int MMU(int pid, int VPN, char type, bool *hit)
{
		int PFN;

		// hit
		PFN = is_page_hit(pid, VPN, type);
		if(PFN >= 0) {
				stats.hitCount++;
				*hit = true;
				return PFN; 
		}

		stats.missCount++;
		*hit = false;
				
		// miss -> pagefault
		PFN = pagefault_handler(pid, VPN, type);

		return PFN;
}

