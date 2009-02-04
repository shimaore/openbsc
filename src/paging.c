/* Paging helper and manager.... */
/* (C) 2009 by Holger Hans Peter Freyther <zecke@selfish.org>
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

/*
 * Relevant specs:
 *     12.21:
 *       - 9.4.12 for CCCH Local Threshold
 *
 *     05.58:
 *       - 8.5.2 CCCH Load indication
 *       - 9.3.15 Paging Load
 *
 * Approach:
 *       - Send paging command to subscriber
 *       - On Channel Request we will remember the reason
 *       - After the ACK we will request the identity
 *	 - Then we will send assign the gsm_subscriber and
 *	 - and call a callback
 */

#include <stdio.h>
#include <stdlib.h>

#include <openbsc/paging.h>
#include <openbsc/debug.h>
#include <openbsc/abis_rsl.h>
#include <openbsc/gsm_04_08.h>

#define PAGING_TIMEOUT 1, 75000
#define MAX_PAGING_REQUEST 750

static LLIST_HEAD(managed_bts);

static unsigned int calculate_group(struct gsm_bts *bts, struct gsm_subscriber *subscr)
{
	int ccch_conf;
	int bs_cc_chans;
	int blocks;
	unsigned int group;
	
	ccch_conf = bts->chan_desc.ccch_conf;
	bs_cc_chans = rsl_ccch_conf_to_bs_cc_chans(ccch_conf);
	/* code word + 2, as 2 channels equals 0x0 */
	blocks = bts->chan_desc.bs_pa_mfrms + 2;
	group = get_paging_group(str_to_imsi(subscr->imsi),
					bs_cc_chans, blocks);
	return group;
}

/*
 * Kill one paging request update the internal list...
 */
static void page_remove_request(struct paging_bts *paging_bts) {
	struct paging_request *to_be_deleted = paging_bts->last_request;
	paging_bts->last_request =
		(struct paging_request *)paging_bts->last_request->entry.next;
	if (&to_be_deleted->entry == &paging_bts->pending_requests)
		paging_bts->last_request = NULL;
	llist_del(&to_be_deleted->entry);
	free(to_be_deleted);
}


static void page_handle_pending_requests(void *data) {
	u_int8_t mi[128];
	unsigned long int tmsi;
	unsigned int mi_len;
	unsigned int pag_group;
	struct paging_bts *paging_bts = (struct paging_bts *)data;
	struct paging_request *request = NULL;

	if (!paging_bts->last_request)
		paging_bts->last_request =
			(struct paging_request *)paging_bts->pending_requests.next; 
	if (&paging_bts->last_request->entry == &paging_bts->pending_requests) {
		paging_bts->last_request = NULL;
		return;
	}

	/* handle the paging request now */
	request = paging_bts->last_request;
	DEBUGP(DPAG, "Going to send paging commands: '%s'\n",
		request->subscr->imsi);
	++request->requests;

	pag_group = calculate_group(paging_bts->bts, request->subscr);
	tmsi = strtoul(request->subscr->tmsi, NULL, 10);
	mi_len = generate_mid_from_tmsi(mi, tmsi);
	rsl_paging_cmd(paging_bts->bts, pag_group, mi_len, mi,
			request->chan_type);

	if (request->requests > MAX_PAGING_REQUEST) {
		page_remove_request(paging_bts);
	} else {
		/* move to the next item */
		paging_bts->last_request =
			(struct paging_request *)paging_bts->last_request->entry.next;
		if (&paging_bts->last_request->entry == &paging_bts->pending_requests)
			paging_bts->last_request = NULL;
	}

	schedule_timer(&paging_bts->page_timer, PAGING_TIMEOUT);
}

static int page_pending_request(struct paging_bts *bts,
				struct gsm_subscriber *subscr) {
	struct paging_request *req;

	llist_for_each_entry(req, &bts->pending_requests, entry) {
		if (subscr == req->subscr)
			return 1;
	}

	return 0;	
}

struct paging_bts* page_allocate(struct gsm_bts *bts) {
	struct paging_bts *page;

	page = (struct paging_bts *)malloc(sizeof(*page));
	memset(page, 0, sizeof(*page));
	page->bts = bts;
	INIT_LLIST_HEAD(&page->pending_requests);
	page->page_timer.cb = page_handle_pending_requests;
	page->page_timer.data = page;

	llist_add_tail(&page->bts_list, &managed_bts);

	return page;
}

void page_request(struct gsm_bts *bts, struct gsm_subscriber *subscr, int type) {
	struct paging_bts *bts_entry;
	struct paging_request *req;

	req = (struct paging_request *)malloc(sizeof(*req));
	memset(req, 0, sizeof(*req));
	req->subscr = subscr_get(subscr);
	req->bts = bts;
	req->chan_type = type;

	llist_for_each_entry(bts_entry, &managed_bts, bts_list) {
		if (bts == bts_entry->bts) {
			if (!page_pending_request(bts_entry, subscr)) {
				llist_add_tail(&req->entry, &bts_entry->pending_requests);
				if (!timer_pending(&bts_entry->page_timer))
					schedule_timer(&bts_entry->page_timer, PAGING_TIMEOUT);
			} else {
				DEBUGP(DPAG, "Paging request already pending\n");
			}

			return;
		}
	}

	DEBUGP(DPAG, "Paging request for not managed BTS\n");
	free(req);
	return;
}
