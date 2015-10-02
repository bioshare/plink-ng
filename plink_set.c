#include "plink_common.h"

#include "plink_set.h"

void set_init(Set_info* sip, Annot_info* aip) {
  sip->fname = NULL;
  sip->setnames_flattened = NULL;
  sip->subset_fname = NULL;
  sip->merged_set_name = NULL;
  sip->genekeep_flattened = NULL;
  sip->ct = 0;
  sip->modifier = 0;
  sip->set_r2 = 0.5;
  sip->set_p = 0.05;
  sip->set_test_lambda = 0.0;
  sip->set_max = 5;
  aip->fname = NULL;
  aip->attrib_fname = NULL;
  aip->ranges_fname = NULL;
  aip->filter_fname = NULL;
  aip->snps_fname = NULL;
  aip->subset_fname = NULL;
  aip->snpfield = NULL;
  aip->modifier = 0;
  aip->border = 0;
}

void set_cleanup(Set_info* sip, Annot_info* aip) {
  free_cond(sip->fname);
  free_cond(sip->setnames_flattened);
  free_cond(sip->subset_fname);
  free_cond(sip->merged_set_name);
  free_cond(sip->genekeep_flattened);
  free_cond(aip->fname);
  free_cond(aip->attrib_fname);
  free_cond(aip->ranges_fname);
  free_cond(aip->filter_fname);
  free_cond(aip->snps_fname);
  free_cond(aip->subset_fname);
  free_cond(aip->snpfield);
}

uint32_t in_setdef(uint32_t* setdef, uint32_t marker_idx) {
  uint32_t range_ct = setdef[0];
  uint32_t idx_base;
  if (range_ct != 0xffffffffU) {
    if (!range_ct) {
      return 0;
    }
    return (uint32arr_greater_than(&(setdef[1]), range_ct * 2, marker_idx + 1) & 1);
  } else {
    idx_base = setdef[1];
    if ((marker_idx < idx_base) || (marker_idx >= idx_base + setdef[2])) {
      return setdef[3];
    }
    return is_set((uintptr_t*)(&(setdef[4])), marker_idx - idx_base);
  }
}

uint32_t in_setdef_dist(uint32_t* setdef, uint32_t pos, uint32_t border, int32_t* dist_ptr) {
  uint32_t range_ct = setdef[0];
  uint32_t uii;
  int32_t ii;
  uii = uint32arr_greater_than(&(setdef[1]), range_ct * 2, pos + 1);
  if (uii & 1) {
    *dist_ptr = 0;
    return 1;
  }
  // check distance from two nearest interval boundaries
  if (!uii) {
    if (pos + border >= setdef[1]) {
      *dist_ptr = ((int32_t)pos) - ((int32_t)setdef[1]);
      return 1;
    }
    return 0;
  } else if (uii == range_ct * 2) {
    if (setdef[uii] + border > pos) {
      *dist_ptr = pos + 1 - setdef[uii];
      return 1;
    }
    return 0;
  } else {
    // this is actually the uncommon case, since range_ct is usually 1
    // ties broken in favor of negative numbers, to match PLINK 1.07 annot.cpp
    if (setdef[uii] + border > pos) {
      ii = pos + 1 - setdef[uii];
      if (pos + ((uint32_t)ii) > setdef[uii + 1]) {
	ii = ((int32_t)pos) - ((int32_t)setdef[uii + 1]);
      }
      *dist_ptr = ii;
      return 1;
    } else if (pos + border >= setdef[uii + 1]) {
      *dist_ptr = ((int32_t)pos) - ((int32_t)setdef[uii + 1]);
      return 1;
    }
    return 0;
  }
}

uint32_t interval_in_setdef(uint32_t* setdef, uint32_t marker_idx_start, uint32_t marker_idx_end) {
  // expects half-open interval coordinates as input
  // assumes interval is nonempty
  uint32_t range_ct = setdef[0];
  uint32_t idx_base;
  uint32_t uii;
  if (range_ct != 0xffffffffU) {
    if (!range_ct) {
      return 0;
    }
    uii = uint32arr_greater_than(&(setdef[1]), range_ct * 2, marker_idx_start + 1);
    if (uii & 1) {
      return 1;
    } else if (uii == range_ct * 2) {
      return 0;
    }
    return uint32arr_greater_than(&(setdef[1 + uii]), range_ct * 2 - uii, marker_idx_end);
  } else {
    idx_base = setdef[1];
    if ((marker_idx_end <= idx_base) || (marker_idx_start >= idx_base + setdef[2])) {
      return setdef[3];
    }
    if (marker_idx_start < idx_base) {
      if (setdef[3]) {
	return 1;
      }
      marker_idx_start = 0;
    } else {
      marker_idx_start -= idx_base;
    }
    if (marker_idx_end > idx_base + setdef[2]) {
      if (setdef[3]) {
	return 1;
      }
      marker_idx_end = setdef[2];
    } else {
      marker_idx_end -= idx_base;
    }
    uii = next_set((uintptr_t*)(&(setdef[4])), marker_idx_start, marker_idx_end);
    return (uii < marker_idx_end);
  }
}

uint32_t setdef_size(uint32_t* setdef, uint32_t marker_ct) {
  uint32_t range_ct = setdef[0];
  uint32_t total_size = 0;
  uint32_t uii;
  if (range_ct != 0xffffffffU) {
    for (uii = 0; uii < range_ct; uii++) {
      total_size += setdef[uii * 2 + 2] - setdef[uii * 2 + 1];
    }
  } else {
    if (setdef[3]) {
      total_size = marker_ct - setdef[2];
    }
    total_size += popcount_bit_idx((uintptr_t*)(&(setdef[4])), 0, setdef[2]);
  }
  return total_size;
}

void setdef_iter_init(uint32_t* setdef, uint32_t marker_ct, uint32_t start_idx, uint32_t* cur_idx_ptr, uint32_t* aux_ptr) {
  uint32_t range_ct = setdef[0];
  uint32_t uii;
  if (range_ct != 0xffffffffU) {
    if (!range_ct) {
      *cur_idx_ptr = 0;
      *aux_ptr = 0;
      return;
    }
    uii = uint32arr_greater_than(&(setdef[1]), range_ct * 2, start_idx + 1);
    if (uii % 2) {
      *cur_idx_ptr = start_idx;
      *aux_ptr = uii + 1;
    } else if (uii < range_ct * 2) {
      *cur_idx_ptr = setdef[uii + 1];
      *aux_ptr = uii + 2;
    } else {
      *cur_idx_ptr = setdef[uii];
      *aux_ptr = uii;
    }
  } else {
    // aux value may be redefined; just needs to be compatible with
    // setdef_iter()
    if (setdef[3]) {
      *cur_idx_ptr = start_idx;
      *aux_ptr = marker_ct - setdef[1];
    } else {
      if (start_idx <= setdef[1]) {
	*cur_idx_ptr = setdef[1];
      } else {
        *cur_idx_ptr = start_idx;
      }
      *aux_ptr = setdef[2];
    }
  }
}

uint32_t setdef_iter(uint32_t* setdef, uint32_t* cur_idx_ptr, uint32_t* aux_ptr) {
  // Iterator.  Returns 0 if end of set, 1 if *not* end.
  // Assumes cur_idx and aux were initialized with setdef_iter_init(), and
  // (after the first call) cur_idx is incremented right before this is called.
  uint32_t range_ct = setdef[0];
  uint32_t cur_idx = *cur_idx_ptr;
  uint32_t aux = *aux_ptr;
  if (range_ct != 0xffffffffU) {
    if (cur_idx < setdef[aux]) {
      return 1;
    } else if (aux < range_ct * 2) {
      *cur_idx_ptr = setdef[aux + 1];
      *aux_ptr = aux + 2;
      return 1;
    } else {
      return 0;
    }
  } else if (cur_idx < setdef[1]) {
    return 1; // only possible if setdef[3] set
  } else {
    cur_idx -= setdef[1];
    if (cur_idx < setdef[2]) {
      if (is_set((uintptr_t*)(&(setdef[4])), cur_idx)) {
        return 1;
      }
      cur_idx = next_set((uintptr_t*)(&(setdef[4])), cur_idx, setdef[2]);
      *cur_idx_ptr = cur_idx + setdef[1];
      if (cur_idx < setdef[2]) {
	return 1;
      }
    }
    return (cur_idx < aux)? 1 : 0;
  }
}

uint32_t alloc_and_populate_nonempty_set_incl(Set_info* sip, uint32_t* nonempty_set_ct_ptr, uintptr_t** nonempty_set_incl_ptr) {
  uint32_t raw_set_ct = sip->ct;
  uint32_t raw_set_ctl = (raw_set_ct + (BITCT - 1)) / BITCT;
  uint32_t nonempty_set_ct = 0;
  uintptr_t* nonempty_set_incl;
  uint32_t set_uidx;
  if (wkspace_alloc_ul_checked(nonempty_set_incl_ptr, raw_set_ctl * sizeof(intptr_t))) {
    return 1;
  }
  nonempty_set_incl = *nonempty_set_incl_ptr;
  fill_ulong_zero(nonempty_set_incl, raw_set_ctl);
  for (set_uidx = 0; set_uidx < raw_set_ct; set_uidx++) {
    if (sip->setdefs[set_uidx][0]) {
      set_bit(nonempty_set_incl, set_uidx);
      nonempty_set_ct++;
    }
  }
  *nonempty_set_ct_ptr = nonempty_set_ct;
  return 0;
}

int32_t load_range_list(FILE* infile, uint32_t track_set_names, uint32_t border_extend, uint32_t collapse_group, uint32_t fail_on_no_sets, uint32_t c_prefix, uintptr_t subset_ct, char* sorted_subset_ids, uintptr_t max_subset_id_len, uint32_t* marker_pos, Chrom_info* chrom_info_ptr, uintptr_t* topsize_ptr, uintptr_t* set_ct_ptr, char** set_names_ptr, uintptr_t* max_set_id_len_ptr, Make_set_range*** make_set_range_arr_ptr, uint64_t** range_sort_buf_ptr, const char* file_descrip) {
  // Called directly by extract_exclude_range(), define_sets(), and indirectly
  // by annotate(), gene_report(), and clump_reports().
  // Assumes topsize has not been subtracted off wkspace_left.  (This remains
  // true on exit.)
  Ll_str* make_set_ll = NULL;
  char* set_names = NULL;
  uintptr_t set_ct = 0;
  uintptr_t max_set_id_len = 0;
  uintptr_t line_idx = 0;
  uint32_t chrom_start = 0;
  uint32_t chrom_end = 0;
  int32_t retval = 0;
  Make_set_range** make_set_range_arr;
  Make_set_range* msr_tmp;
  Ll_str* ll_tmp;
  char* bufptr;
  char* bufptr2;
  char* bufptr3;
  uintptr_t set_idx;
  uintptr_t ulii;
  uint32_t chrom_idx;
  uint32_t range_first;
  uint32_t range_last;
  uint32_t uii;
  uint32_t ujj;
  int32_t ii;
  tbuf[MAXLINELEN - 1] = ' ';
  // if we need to track set names, put together a sorted list
  if (track_set_names) {
    while (fgets(tbuf, MAXLINELEN, infile)) {
      line_idx++;
      if (!tbuf[MAXLINELEN - 1]) {
	sprintf(logbuf, "Error: Line %" PRIuPTR " of %s file is pathologically long.\n", line_idx, file_descrip);
	goto load_range_list_ret_INVALID_FORMAT_2;
      }
      bufptr = skip_initial_spaces(tbuf);
      if (is_eoln_kns(*bufptr)) {
	continue;
      }
      bufptr2 = next_token_mult(bufptr, 3);
      if (!collapse_group) {
	bufptr3 = bufptr2;
      } else {
	bufptr3 = next_token(bufptr2);
      }
      if (no_more_tokens_kns(bufptr3)) {
	sprintf(logbuf, "Error: Line %" PRIuPTR " of %s file has fewer tokens than expected.\n", line_idx, file_descrip);
	goto load_range_list_ret_INVALID_FORMAT_2;
      }
      ii = get_chrom_code(chrom_info_ptr, bufptr);
      if (ii < 0) {
	sprintf(logbuf, "Error: Invalid chromosome code on line %" PRIuPTR " of %s file.\n", line_idx, file_descrip);
	goto load_range_list_ret_INVALID_FORMAT_2;
      }
      // chrom_mask check removed, we want to track empty sets
      uii = strlen_se(bufptr2);
      bufptr2[uii] = '\0';
      if (subset_ct) {
	if (bsearch_str(bufptr2, uii, sorted_subset_ids, max_subset_id_len, subset_ct) == -1) {
	  continue;
	}
      }
      if (collapse_group) {
	uii = strlen_se(bufptr3);
	bufptr3[uii] = '\0';
      }
      // when there are repeats, they are likely to be next to each other
      if (make_set_ll && (!strcmp(make_set_ll->ss, bufptr3))) {
	continue;
      }
      uii++;
      // argh, --clump counts positional overlaps which don't include any
      // variants in the dataset.  So we prefix set IDs with a chromosome index
      // in that case (with leading zeroes) and treat cross-chromosome sets as
      // distinct.
      if (!marker_pos) {
	uii += 4;
      }
      if (uii > max_set_id_len) {
	max_set_id_len = uii;
      }
      ll_tmp = top_alloc_llstr(topsize_ptr, uii);
      ll_tmp->next = make_set_ll;
      if (marker_pos) {
        memcpy(ll_tmp->ss, bufptr3, uii);
      } else {
	uint32_write4(ll_tmp->ss, (uint32_t)ii);
	// if first character of gene name is a digit, natural sort has strange
	// effects unless we force [3] to be nonnumeric...
	ll_tmp->ss[3] -= 15;
	memcpy(&(ll_tmp->ss[4]), bufptr3, uii - 4);
      }
      make_set_ll = ll_tmp;
      set_ct++;
    }
    if (!set_ct) {
      if (fail_on_no_sets) {
	if (marker_pos) {
	  // okay, this is a kludge
	  logerrprint("Error: All variants excluded by --gene{-all}, since no sets were defined from\n--make-set file.\n");
	  retval = RET_ALL_MARKERS_EXCLUDED;
	} else {
	  if (subset_ct) {
	    logerrprint("Error: No --gene-subset genes present in --gene-report file.\n");
	  } else {
	    logerrprint("Error: Empty --gene-report file.\n");
	  }
	  retval = RET_INVALID_FORMAT;
	}
	goto load_range_list_ret_1;
      }
      LOGERRPRINTF("Warning: No valid ranges in %s file.\n", file_descrip);
      goto load_range_list_ret_1;
    }
    max_set_id_len += c_prefix;
    if (max_set_id_len > MAX_ID_LEN_P1) {
      logerrprint("Error: Set IDs are limited to " MAX_ID_LEN_STR " characters.\n");
      goto load_range_list_ret_INVALID_FORMAT;
    }
    wkspace_left -= *topsize_ptr;
    if (wkspace_alloc_c_checked(set_names_ptr, set_ct)) {
      goto load_range_list_ret_NOMEM2;
    }
    wkspace_left += *topsize_ptr;
    set_names = *set_names_ptr;
    if (!c_prefix) {
      for (ulii = 0; ulii < set_ct; ulii++) {
	strcpy(&(set_names[ulii * max_set_id_len]), make_set_ll->ss);
	make_set_ll = make_set_ll->next;
      }
    } else {
      for (ulii = 0; ulii < set_ct; ulii++) {
	memcpy(&(set_names[ulii * max_set_id_len]), "C_", 2);
	strcpy(&(set_names[ulii * max_set_id_len + 2]), make_set_ll->ss);
	make_set_ll = make_set_ll->next;
      }
    }
    qsort(set_names, set_ct, max_set_id_len, strcmp_natural);
    set_ct = collapse_duplicate_ids(set_names, set_ct, max_set_id_len, NULL);
    wkspace_shrink_top(set_names, set_ct * max_set_id_len);
    rewind(infile);
  } else {
    set_ct = 1;
  }
  make_set_range_arr = (Make_set_range**)top_alloc(topsize_ptr, set_ct * sizeof(intptr_t));
  for (set_idx = 0; set_idx < set_ct; set_idx++) {
    make_set_range_arr[set_idx] = NULL;
  }
  line_idx = 0;
  while (fgets(tbuf, MAXLINELEN, infile)) {
    line_idx++;
    if (!tbuf[MAXLINELEN - 1]) {
      sprintf(logbuf, "Error: Line %" PRIuPTR " of %s file is pathologically long.\n", line_idx, file_descrip);
      goto load_range_list_ret_INVALID_FORMAT_2;
    }
    bufptr = skip_initial_spaces(tbuf);
    if (is_eoln_kns(*bufptr)) {
      continue;
    }
    bufptr2 = next_token_mult(bufptr, 3);
    if (!collapse_group) {
      bufptr3 = bufptr2;
    } else {
      bufptr3 = next_token(bufptr2);
    }
    if (no_more_tokens_kns(bufptr3)) {
      sprintf(logbuf, "Error: Line %" PRIuPTR " of %s file has fewer tokens than expected.\n", line_idx, file_descrip);
      goto load_range_list_ret_INVALID_FORMAT_2;
    }
    ii = get_chrom_code(chrom_info_ptr, bufptr);
    if (ii < 0) {
      sprintf(logbuf, "Error: Invalid chromosome code on line %" PRIuPTR " of %s file.\n", line_idx, file_descrip);
      goto load_range_list_ret_INVALID_FORMAT_2;
    }
    if (!is_set(chrom_info_ptr->chrom_mask, ii)) {
      continue;
    }
    chrom_idx = ii;
    if (marker_pos) {
      chrom_start = chrom_info_ptr->chrom_start[chrom_idx];
      chrom_end = chrom_info_ptr->chrom_end[chrom_idx];
      if (chrom_end == chrom_start) {
	continue;
      }
      // might need to move this outside the if-statement later
      if (subset_ct && (bsearch_str(bufptr2, strlen_se(bufptr2), sorted_subset_ids, max_subset_id_len, subset_ct) == -1)) {
	continue;
      }
    }
    bufptr = next_token(bufptr);
    if (scan_uint_defcap(bufptr, &range_first)) {
      sprintf(logbuf, "Error: Invalid range start position on line %" PRIuPTR " of %s file.\n", line_idx, file_descrip);
      goto load_range_list_ret_INVALID_FORMAT_2;
    }
    bufptr = next_token(bufptr);
    if (scan_uint_defcap(bufptr, &range_last)) {
      sprintf(logbuf, "Error: Invalid range end position on line %" PRIuPTR " of %s file.\n", line_idx, file_descrip);
      goto load_range_list_ret_INVALID_FORMAT_2;
    }
    if (range_last < range_first) {
      sprintf(logbuf, "Error: Range end position smaller than range start on line %" PRIuPTR " of %s file.\n", line_idx, file_descrip);
      wordwrap(logbuf, 0);
      goto load_range_list_ret_INVALID_FORMAT_2;
    }
    if (border_extend > range_first) {
      range_first = 0;
    } else {
      range_first -= border_extend;
    }
    range_last += border_extend;
    if (set_ct > 1) {
      // bugfix: bsearch_str_natural requires null-terminated string
      uii = strlen_se(bufptr3);
      bufptr3[uii] = '\0';
      if (c_prefix) {
	bufptr3 = &(bufptr3[-2]);
	memcpy(bufptr3, "C_", 2);
      } else if (!marker_pos) {
	bufptr3 = &(bufptr3[-4]);
	uint32_write4(bufptr3, chrom_idx);
	bufptr3[3] -= 15;
      }
      // this should never fail
      set_idx = (uint32_t)bsearch_str_natural(bufptr3, set_names, max_set_id_len, set_ct);
    } else {
      set_idx = 0;
    }
    if (marker_pos) {
      // translate to within-chromosome uidx
      range_first = uint32arr_greater_than(&(marker_pos[chrom_start]), chrom_end - chrom_start, range_first);
      range_last = uint32arr_greater_than(&(marker_pos[chrom_start]), chrom_end - chrom_start, range_last + 1);
      if (range_last > range_first) {
	msr_tmp = (Make_set_range*)top_alloc(topsize_ptr, sizeof(Make_set_range));
	msr_tmp->next = make_set_range_arr[set_idx];
	// normally, I'd keep chrom_idx here since that enables by-chromosome
	// sorting, but that's probably not worth bloating Make_set_range from
	// 16 to 32 bytes
	msr_tmp->uidx_start = chrom_start + range_first;
	msr_tmp->uidx_end = chrom_start + range_last;
	make_set_range_arr[set_idx] = msr_tmp;
      }
    } else {
      msr_tmp = (Make_set_range*)top_alloc(topsize_ptr, sizeof(Make_set_range));
      msr_tmp->next = make_set_range_arr[set_idx];
      msr_tmp->uidx_start = range_first;
      msr_tmp->uidx_end = range_last + 1;
      make_set_range_arr[set_idx] = msr_tmp;
    }
  }
  // allocate buffer for sorting ranges later
  uii = 0;
  for (set_idx = 0; set_idx < set_ct; set_idx++) {
    ujj = 0;
    msr_tmp = make_set_range_arr[set_idx];
    while (msr_tmp) {
      ujj++;
      msr_tmp = msr_tmp->next;
    }
    if (ujj > uii) {
      uii = ujj;
    }
  }
  if (range_sort_buf_ptr) {
    *range_sort_buf_ptr = (uint64_t*)top_alloc(topsize_ptr, uii * sizeof(int64_t));
  }
  if (set_ct_ptr) {
    *set_ct_ptr = set_ct;
  }
  if (max_set_id_len_ptr) {
    *max_set_id_len_ptr = max_set_id_len;
  }
  *make_set_range_arr_ptr = make_set_range_arr;
  while (0) {
  load_range_list_ret_NOMEM2:
    wkspace_left += *topsize_ptr;
    *topsize_ptr = 0;
    retval = RET_NOMEM;
    break;
  load_range_list_ret_INVALID_FORMAT_2:
    logerrprintb();
  load_range_list_ret_INVALID_FORMAT:
    retval = RET_INVALID_FORMAT;
    break;
  }
 load_range_list_ret_1:
  return retval;
}

int32_t extract_exclude_range(char* fname, uint32_t* marker_pos, uintptr_t unfiltered_marker_ct, uintptr_t* marker_exclude, uintptr_t* marker_exclude_ct_ptr, uint32_t is_exclude, Chrom_info* chrom_info_ptr) {
  unsigned char* wkspace_mark = wkspace_base;
  uintptr_t unfiltered_marker_ctl = (unfiltered_marker_ct + (BITCT - 1)) / BITCT;
  FILE* infile = NULL;
  uintptr_t topsize = 0;
  uintptr_t orig_marker_exclude_ct = *marker_exclude_ct_ptr;
  Make_set_range** range_arr = NULL;
  int32_t retval = 0;
  Make_set_range* msr_tmp;
  uintptr_t* marker_exclude_new;
  if (fopen_checked(&infile, fname, "r")) {
    goto extract_exclude_range_ret_OPEN_FAIL;
  }
  retval = load_range_list(infile, 0, 0, 0, 0, 0, 0, NULL, 0, marker_pos, chrom_info_ptr, &topsize, NULL, NULL, NULL, &range_arr, NULL, is_exclude? "--exclude range" : "--extract range");
  if (retval) {
    goto extract_exclude_range_ret_1;
  }
  if (fclose_null(&infile)) {
    goto extract_exclude_range_ret_READ_FAIL;
  }
  msr_tmp = range_arr[0];
  if (is_exclude) {
    while (msr_tmp) {
      fill_bits(marker_exclude, msr_tmp->uidx_start, msr_tmp->uidx_end - msr_tmp->uidx_start);
      msr_tmp = msr_tmp->next;
    }
  } else {
    wkspace_base -= topsize;
    marker_exclude_new = (uintptr_t*)wkspace_alloc(unfiltered_marker_ctl * sizeof(intptr_t));
    wkspace_base += topsize;
    if (!marker_exclude_new) {
      goto extract_exclude_range_ret_NOMEM;
    }
    fill_all_bits(marker_exclude_new, unfiltered_marker_ct);
    while (msr_tmp) {
      clear_bits(marker_exclude_new, msr_tmp->uidx_start, msr_tmp->uidx_end - msr_tmp->uidx_start);
      msr_tmp = msr_tmp->next;
    }
    bitfield_or(marker_exclude, marker_exclude_new, unfiltered_marker_ctl);
  }
  *marker_exclude_ct_ptr = popcount_longs(marker_exclude, unfiltered_marker_ctl);
  if (*marker_exclude_ct_ptr == unfiltered_marker_ct) {
    LOGERRPRINTF("Error: All variants excluded by '--%s range'.\n", is_exclude? "exclude" : "extract");
    retval = RET_ALL_MARKERS_EXCLUDED;
  } else if (*marker_exclude_ct_ptr == orig_marker_exclude_ct) {
    LOGERRPRINTF("Warning: No variants excluded by '--%s range'.\n", is_exclude? "exclude" : "extract");
  } else {
    orig_marker_exclude_ct = *marker_exclude_ct_ptr - orig_marker_exclude_ct;
    LOGPRINTF("--%s range: %" PRIuPTR " variant%s excluded.\n", is_exclude? "exclude" : "extract", orig_marker_exclude_ct, (orig_marker_exclude_ct == 1)? "" : "s");
  }
  while (0) {
  extract_exclude_range_ret_NOMEM:
    retval = RET_NOMEM;
    break;
  extract_exclude_range_ret_OPEN_FAIL:
    retval = RET_OPEN_FAIL;
    break;
  extract_exclude_range_ret_READ_FAIL:
    retval = RET_READ_FAIL;
    break;
  }
 extract_exclude_range_ret_1:
  fclose_cond(infile);
  wkspace_reset(wkspace_mark);
  return retval;
}

uint32_t save_set_bitfield(uintptr_t* marker_bitfield_tmp, uint32_t marker_ct, uint32_t range_start, uint32_t range_end, uint32_t complement_sets, uint32_t** set_range_pp) {
  uintptr_t mem_req = ((marker_ct + 255) / 128) * 16;
  uint32_t bound_bottom_d128 = range_start / 128;
  uint32_t bound_top_d128 = (range_end - 1) / 128;
  uint32_t set_bits_outer = complement_sets;
  uint32_t do_flip = 0;
  uint32_t range_idx = 0;
  uint32_t* uiptr;
  uint32_t range_ct_ceil;
  uint32_t bit_idx;
  uint32_t uii;
  uint32_t ujj;
  uint32_t ukk;
  if (wkspace_left < mem_req) {
    return 1;
  }
  *set_range_pp = (uint32_t*)wkspace_base;
  if (range_start == marker_ct) {
    // empty or full set
  save_set_bitfield_degen:
    wkspace_left -= 16;
    wkspace_base = &(wkspace_base[16]);
    if (complement_sets) {
      (*set_range_pp)[0] = 1;
      (*set_range_pp)[1] = 0;
      (*set_range_pp)[2] = marker_ct;
    } else {
      (*set_range_pp)[0] = 0;
    }
    return 0;
  }
  // profitable to invert?
  ukk = bound_top_d128 - bound_bottom_d128;
  if (!range_start) {
    uii = next_unset(marker_bitfield_tmp, 0, range_end);
    if (range_end == marker_ct) {
      if (uii != marker_ct) {
	ujj = prev_unset_unsafe(marker_bitfield_tmp, range_end - 1);
	if ((ujj / 128) - (uii / 128) < ukk) {
	  do_flip = 1;
          range_start = uii;
	  range_end = ujj + 1;
	}
      } else {
	complement_sets = 1 - complement_sets;
	goto save_set_bitfield_degen;
      }
    } else {
      if (((marker_ct - 1) / 128) - (uii / 128) < ukk) {
	do_flip = 1;
        range_start = uii;
	range_end = marker_ct;
      }
    }
  } else {
    if (range_end == marker_ct) {
      uii = prev_unset_unsafe(marker_bitfield_tmp, range_end - 1);
      if ((uii / 128) < ukk) {
	do_flip = 1;
	range_start = 0;
        range_end = uii + 1;
      }
    }
  }
  if (do_flip) {
    set_bits_outer = 1 - set_bits_outer;
    bound_bottom_d128 = range_start / 128;
    bound_top_d128 = (range_end - 1) / 128;
  }
  bound_top_d128++;
  // equal or greater than this -> use bitfield
  range_ct_ceil = 2 * (1 + bound_top_d128 - bound_bottom_d128);
  mem_req = range_ct_ceil * 8;
  // try to compress as sequence of ranges
  uiptr = &((*set_range_pp)[1]);
  bit_idx = bound_bottom_d128 * 128;
  if (set_bits_outer && bit_idx) {
    if (!complement_sets) {
      bit_idx = next_unset_unsafe(marker_bitfield_tmp, bit_idx);
    } else {
      bit_idx = next_set_unsafe(marker_bitfield_tmp, bit_idx);
    }
    *uiptr++ = 0;
    *uiptr++ = bit_idx;
    range_idx++;
  }
  ujj = bound_top_d128 * 128;
  if (!complement_sets) {
    do {
      if (++range_idx == range_ct_ceil) {
	goto save_set_bitfield_standard;
      }
      uii = next_set_unsafe(marker_bitfield_tmp, bit_idx);
      bit_idx = uii + 1;
      next_unset_unsafe_ck(marker_bitfield_tmp, &bit_idx);
      *uiptr++ = uii;
      *uiptr++ = bit_idx;
    } while (bit_idx < range_end);
  } else {
    set_bit(marker_bitfield_tmp, ujj);
    clear_bit(marker_bitfield_tmp, ujj + 1);
    ukk = ujj;
    if (ukk > marker_ct) {
      ukk = marker_ct;
    }
    while (1) {
      uii = bit_idx + 1;
      next_unset_unsafe_ck(marker_bitfield_tmp, &uii);
      if (uii >= ukk) {
	break;
      }
      if (++range_idx == range_ct_ceil) {
	goto save_set_bitfield_standard;
      }
      bit_idx = next_set_unsafe(marker_bitfield_tmp, uii);
      *uiptr++ = uii;
      *uiptr++ = bit_idx;
    }
    if (uiptr[-1] > marker_ct) {
      uiptr[-1] = marker_ct;
    }
  }
  if (set_bits_outer && (ujj < marker_ct)) {
    if (uiptr[-1] == ujj) {
      uiptr[-1] = marker_ct;
    } else {
      if (++range_idx == range_ct_ceil) {
	goto save_set_bitfield_standard;
      }
      *uiptr++ = ujj;
      *uiptr++ = marker_ct;
    }
  }
  (*set_range_pp)[0] = range_idx;
  mem_req = (1 + (range_idx / 2)) * 16;
  while (0) {
  save_set_bitfield_standard:
    bound_bottom_d128 *= 128;
    bound_top_d128 *= 128;
    (*set_range_pp)[0] = 0xffffffffU;
    (*set_range_pp)[1] = bound_bottom_d128;
    (*set_range_pp)[2] = bound_top_d128 - bound_bottom_d128;
    (*set_range_pp)[3] = set_bits_outer;
    memcpy(&((*set_range_pp)[4]), &(marker_bitfield_tmp[bound_bottom_d128 / BITCT]), mem_req - 16);
    if (complement_sets) {
      bitfield_invert((uintptr_t*)(&((*set_range_pp)[4])), bound_top_d128 - bound_bottom_d128);
    }
  }
  wkspace_left -= mem_req;
  wkspace_base = &(wkspace_base[mem_req]);
  return 0;
}

uint32_t save_set_range(uint64_t* range_sort_buf, uint32_t marker_ct, uint32_t rsb_last_idx, uint32_t complement_sets, uint32_t** set_range_pp) {
  uint32_t* uiptr = (uint32_t*)wkspace_base;
  uint32_t range_start = (uint32_t)(range_sort_buf[0] >> 32);
  uint32_t range_end = (uint32_t)(range_sort_buf[rsb_last_idx]);
  uint32_t bound_bottom_d128 = range_start / 128;
  uint32_t bound_top_d128 = (range_end - 1) / 128;
  uint32_t range_ct = bound_top_d128 - bound_bottom_d128;
  uint32_t set_bits_outer = complement_sets;
  uint32_t do_flip = 0; // flip set_bits_outer since that's more compact?
  uint32_t rsb_idx = 0;
  uintptr_t* bitfield_ptr = (uintptr_t*)(&(uiptr[4]));
  uint64_t ullii;
  uintptr_t mem_req;
  uintptr_t ulii;
  uint32_t uii;
  uint32_t ujj;
  if (wkspace_left < (rsb_last_idx / 2) * 16 + 32) {
    return 1;
  }
  *set_range_pp = uiptr;
  if (!range_start) {
    uii = (uint32_t)range_sort_buf[0];
    if (range_end == marker_ct) {
      if (uii != marker_ct) {
	do_flip = 1;
        range_start = uii;
	range_end = (uint32_t)(range_sort_buf[rsb_last_idx] >> 32);
      } else {
	wkspace_left -= 16;
	wkspace_base = &(wkspace_base[16]);
	if (!complement_sets) {
	  uiptr[0] = 1;
	  uiptr[1] = 0;
	  uiptr[2] = marker_ct;
	} else {
	  uiptr[0] = 0;
	}
	return 0;
      }
    } else {
      if (((marker_ct - 1) / 128) - (uii / 128) < range_ct) {
	do_flip = 1;
	range_start = uii;
        range_end = marker_ct;
      }
    }
  } else {
    if (range_end == marker_ct) {
      if ((((uint32_t)(range_sort_buf[rsb_last_idx] - 1)) / 128) < range_ct) {
	do_flip = 1;
	range_start = 0;
	range_end = range_sort_buf[rsb_last_idx];
      }
    }
  }
  if (do_flip) {
    set_bits_outer = 1 - set_bits_outer;
    bound_bottom_d128 = range_start / 128;
    bound_top_d128 = (range_end - 1) / 128;
  }
  bound_top_d128++;
  range_end = bound_top_d128 * 128;
  if (range_end > marker_ct) {
    range_end = marker_ct;
  }
  mem_req = 16 * (1 + bound_top_d128 - bound_bottom_d128);
  if (!complement_sets) {
    ulii = ((rsb_last_idx + 1) / 2) + 1;
    ulii *= 16;
    if (ulii > mem_req) {
      fill_ulong_zero(bitfield_ptr, (bound_top_d128 - bound_bottom_d128) * (128 / BITCT));
      range_start = bound_bottom_d128 * 128;
      if (do_flip) {
	rsb_last_idx--;
	if (range_start) {
	  // first range must begin at bit 0
	  uii = range_start;
	  ujj = (uint32_t)(range_sort_buf[0]);
	  goto save_set_range_late_start_1;
	}
      }
      for (; rsb_idx <= rsb_last_idx; rsb_idx++) {
	ullii = range_sort_buf[rsb_idx];
	uii = (uint32_t)(ullii >> 32);
	ujj = (uint32_t)ullii;
      save_set_range_late_start_1:
	fill_bits(bitfield_ptr, uii - range_start, ujj - uii);
      }
      if (do_flip) {
	// last range may go past bitfield end
        ullii = range_sort_buf[rsb_idx];
	uii = (uint32_t)(ullii >> 32);
	ujj = (uint32_t)ullii;
        if (ujj > range_end) {
	  ujj = range_end;
	}
	fill_bits(bitfield_ptr, uii - range_start, ujj - uii);
      }
      goto save_set_range_bitfield_finish_encode;
    }
    wkspace_left -= ulii;
    wkspace_base = &(wkspace_base[ulii]);
    *uiptr++ = rsb_last_idx + 1;
    for (; rsb_idx <= rsb_last_idx; rsb_idx++) {
      ullii = range_sort_buf[rsb_idx];
      *uiptr++ = (uint32_t)(ullii >> 32);
      *uiptr++ = (uint32_t)ullii;
    }
  } else {
    range_ct = rsb_last_idx + 2;
    if (((uint32_t)range_sort_buf[rsb_last_idx]) == marker_ct) {
      range_ct--;
    }
    ullii = range_sort_buf[0];
    range_start = (uint32_t)(ullii >> 32);
    if (range_start) {
      ulii = (range_ct / 2) + 1;
    } else {
      ulii = ((range_ct - 1) / 2) + 1;
    }
    ulii *= 16;
    if (ulii > mem_req) {
      range_start = bound_bottom_d128 * 128;
      fill_all_bits(bitfield_ptr, range_end - range_start);
      if (do_flip) {
	rsb_last_idx--;
	if (range_start) {
	  // first raw range must begin at bit 0, so complemented range must
	  // begin later
	  uii = range_start;
	  ujj = (uint32_t)(range_sort_buf[0]);
	  goto save_set_range_late_start_2;
	}
      }
      for (; rsb_idx <= rsb_last_idx; rsb_idx++) {
	ullii = range_sort_buf[rsb_idx];
	uii = (uint32_t)(ullii >> 32);
        ujj = (uint32_t)ullii;
      save_set_range_late_start_2:
        clear_bits(bitfield_ptr, uii - range_start, ujj - uii);
      }
      if (do_flip) {
        ullii = range_sort_buf[rsb_idx];
	uii = (uint32_t)(ullii >> 32);
	ujj = (uint32_t)ullii;
        if (ujj > range_end) {
	  ujj = range_end;
	}
	clear_bits(bitfield_ptr, uii - range_start, ujj - uii);
      }
    save_set_range_bitfield_finish_encode:
      wkspace_left -= mem_req;
      wkspace_base = &(wkspace_base[mem_req]);
      uiptr[0] = 0xffffffffU;
      uiptr[1] = range_start;
      uiptr[2] = range_end - range_start;
      uiptr[3] = set_bits_outer;
    } else {
      wkspace_left -= ulii;
      wkspace_base = &(wkspace_base[ulii]);
      if (range_start) {
	*uiptr++ = range_ct;
	*uiptr++ = 0;
	*uiptr++ = range_start;
      } else {
	*uiptr++ = range_ct - 1;
      }
      for (rsb_idx = 1; rsb_idx <= rsb_last_idx; rsb_idx++) {
	*uiptr++ = (uint32_t)ullii;
	ullii = range_sort_buf[rsb_idx];
	*uiptr++ = (uint32_t)(ullii >> 32);
      }
      if (range_ct == rsb_last_idx + 2) {
	*uiptr++ = (uint32_t)ullii;
	*uiptr++ = marker_ct;
      }
    }
  }
  return 0;
}

int32_t define_sets(Set_info* sip, uintptr_t unfiltered_marker_ct, uintptr_t* marker_exclude, uint32_t* marker_pos, uintptr_t* marker_exclude_ct_ptr, char* marker_ids, uintptr_t max_marker_id_len, Chrom_info* chrom_info_ptr) {
  FILE* infile = NULL;
  uintptr_t topsize = 0;
  char* sorted_marker_ids = NULL;
  char* sorted_genekeep_ids = NULL;
  uintptr_t unfiltered_marker_ctl = (unfiltered_marker_ct + (BITCT - 1)) / BITCT;
  uintptr_t marker_exclude_ct = *marker_exclude_ct_ptr;
  uintptr_t marker_ct = unfiltered_marker_ct - marker_exclude_ct;
  uintptr_t set_ct = 0;
  uint32_t make_set = sip->modifier & SET_MAKE_FROM_RANGES;
  uint32_t complement_sets = (sip->modifier / SET_COMPLEMENTS) & 1;
  uint32_t c_prefix = 2 * ((sip->modifier / SET_C_PREFIX) & 1);
  uint32_t gene_all = sip->modifier & SET_GENE_ALL;
  uint32_t curtoklen = 0;
  uint32_t in_set = 0;
  int32_t retval = 0;
  uintptr_t subset_ct = 0;
  uintptr_t max_subset_id_len = 0;
  uintptr_t genekeep_ct = 0;
  uintptr_t max_genekeep_len = 0;
  uintptr_t max_set_id_len = 0;
  Make_set_range** make_set_range_arr = NULL;
  char* midbuf = &(tbuf[MAXLINELEN]);
  char* sorted_subset_ids = NULL;
  char* set_names = NULL;
  char* bufptr = NULL;
  uint64_t* range_sort_buf = NULL;
  char* bufptr2;
  char* bufptr3;
  char* buf_end;
  Make_set_range* msr_tmp;
  uint32_t* marker_id_map;
  uint32_t* marker_uidx_to_idx;
  uint32_t** all_setdefs;
  uintptr_t* marker_exclude_new;
  uintptr_t* marker_bitfield_tmp;
  uintptr_t set_idx;
  uintptr_t bufsize;
  uintptr_t topsize_bak;
  uintptr_t marker_ctp2l;
  uintptr_t ulii;
  uint64_t ullii;
  uint32_t range_first;
  uint32_t range_last;
  uint32_t slen;
  uint32_t uii;
  uint32_t ujj;
  uint32_t ukk;
  uint32_t umm;
  int32_t ii;
  // 1. validate and sort --gene parameter(s)
  if (sip->genekeep_flattened) {
    bufptr = sip->genekeep_flattened;
    if (sip->merged_set_name) {
      // degenerate case: --gene-all if merged set name present, fail
      // otherwise
      uii = strlen(sip->merged_set_name);
      while (1) {
	slen = strlen(bufptr);
	if ((slen == uii) && (!memcmp(bufptr, sip->merged_set_name, uii))) {
	  break;
	}
	bufptr = &(bufptr[slen + 1]);
	if (!(*bufptr)) {
	  goto define_sets_ret_ALL_MARKERS_EXCLUDED;
	}
      }
      free(sip->genekeep_flattened);
      sip->genekeep_flattened = NULL;
      gene_all = 1;
    } else {
      do {
	slen = strlen(bufptr) + 1;
	if ((!c_prefix) || (!memcmp(bufptr, "C_", 2))) {
	  if (slen > max_genekeep_len) {
	    max_genekeep_len = slen;
	  }
	  genekeep_ct++;
	}
	bufptr = &(bufptr[slen]);
      } while (*bufptr);
      if (!genekeep_ct) {
	logerrprint("Error: All variants excluded by --gene.\n");
	goto define_sets_ret_ALL_MARKERS_EXCLUDED_2;
      }
      sorted_genekeep_ids = (char*)top_alloc(&topsize, genekeep_ct * max_genekeep_len);
      if (!sorted_genekeep_ids) {
	goto define_sets_ret_NOMEM;
      }
      bufptr = sip->genekeep_flattened;
      ulii = 0;
      do {
	slen = strlen(bufptr) + 1;
	if ((!c_prefix) || (!memcmp(bufptr, "C_", 2))) {
	  memcpy(&(sorted_genekeep_ids[ulii * max_genekeep_len]), bufptr, slen);
	  ulii++;
	}
	bufptr = &(bufptr[slen]);
      } while (*bufptr);
      qsort(sorted_genekeep_ids, genekeep_ct, max_genekeep_len, strcmp_casted);
    }
  }
  // 2. if --set-names and/or --subset is present, (load and) sort those lists
  if (sip->setnames_flattened || sip->subset_fname) {
    if (sip->subset_fname) {
      if (fopen_checked(&infile, sip->subset_fname, "rb")) {
	goto define_sets_ret_OPEN_FAIL;
      }
      retval = scan_token_ct_len(infile, tbuf, MAXLINELEN, &subset_ct, &max_subset_id_len);
      if (retval) {
	if (retval == RET_INVALID_FORMAT) {
	  logerrprint("Error: Pathologically long token in --subset file.\n");
	}
	goto define_sets_ret_1;
      }
    }
    ulii = subset_ct;
    if (sip->setnames_flattened) {
      subset_ct += count_and_measure_multistr(sip->setnames_flattened, &max_subset_id_len);
    }
    if (!subset_ct) {
      if ((gene_all || sip->genekeep_flattened) && ((!sip->merged_set_name) || (!complement_sets))) {
	if (sip->subset_fname) {
	  logerrprint("Error: All variants excluded, since --subset file is empty.\n");
	} else {
	  logerrprint("Error: All variants excluded, since --set-names was given no parameters.\n");
	}
	goto define_sets_ret_ALL_MARKERS_EXCLUDED_2;
      }
      if (sip->merged_set_name) {
	goto define_sets_merge_nothing;
      } else {
	if (sip->subset_fname) {
          logerrprint("Warning: Empty --subset file; no sets defined.\n");
	} else {
          logerrprint("Warning: No sets defined since --set-names was given no parameters.\n");
	}
        goto define_sets_ret_1;
      }
    }
    if (max_subset_id_len > MAX_ID_LEN_P1) {
      logerrprint("Error: Subset IDs are limited to " MAX_ID_LEN_STR " characters.\n");
      goto define_sets_ret_INVALID_FORMAT;
    }
    sorted_subset_ids = (char*)top_alloc(&topsize, subset_ct * max_subset_id_len);
    if (!sorted_subset_ids) {
      goto define_sets_ret_NOMEM;
    }
    if (sip->subset_fname) {
      if (ulii) {
	rewind(infile);
	retval = read_tokens(infile, tbuf, MAXLINELEN, ulii, max_subset_id_len, sorted_subset_ids);
	if (retval) {
	  goto define_sets_ret_1;
	}
      }
      if (fclose_null(&infile)) {
	goto define_sets_ret_READ_FAIL;
      }
    }
    if (sip->setnames_flattened) {
      bufptr = sip->setnames_flattened;
      while (*bufptr) {
	slen = strlen(bufptr) + 1;
        memcpy(&(sorted_subset_ids[ulii * max_subset_id_len]), bufptr, slen);
	ulii++;
	bufptr = &(bufptr[slen]);
      }
    }
    qsort(sorted_subset_ids, subset_ct, max_subset_id_len, strcmp_casted);
    subset_ct = collapse_duplicate_ids(sorted_subset_ids, subset_ct, max_subset_id_len, NULL);
  }
  if (fopen_checked(&infile, sip->fname, make_set? "r" : "rb")) {
    goto define_sets_ret_OPEN_FAIL;
  }
  // 3. load --make-set range list
  if (make_set) {
    retval = load_range_list(infile, !sip->merged_set_name, sip->make_set_border, sip->modifier & SET_MAKE_COLLAPSE_GROUP, gene_all || sip->genekeep_flattened, c_prefix, subset_ct, sorted_subset_ids, max_subset_id_len, marker_pos, chrom_info_ptr, &topsize, &set_ct, &set_names, &max_set_id_len, &make_set_range_arr, &range_sort_buf, "--make-set");
    if (retval) {
      goto define_sets_ret_1;
    }
  }

  // 4. if --gene or --gene-all is present, pre-filter variants.
  if (gene_all || sip->genekeep_flattened) {
    topsize_bak = topsize;
    marker_bitfield_tmp = (uintptr_t*)top_alloc(&topsize, unfiltered_marker_ctl * sizeof(intptr_t));
    if (!marker_bitfield_tmp) {
      goto define_sets_ret_NOMEM;
    }
    marker_exclude_new = (uintptr_t*)top_alloc(&topsize, unfiltered_marker_ctl * sizeof(intptr_t));
    if (!marker_exclude_new) {
      goto define_sets_ret_NOMEM;
    }
    fill_ulong_zero(marker_bitfield_tmp, unfiltered_marker_ctl);
    fill_all_bits(marker_exclude_new, unfiltered_marker_ct);
    // then include every variant that appears, or include every variant that
    // fails to appear in a fully loaded set in the complement case
    if (make_set) {
      for (set_idx = 0; set_idx < set_ct; set_idx++) {
	if (gene_all || (bsearch_str_nl(&(set_names[set_idx * max_set_id_len]), sorted_genekeep_ids, max_genekeep_len, genekeep_ct) != -1)) {
	  msr_tmp = make_set_range_arr[set_idx];
	  while (msr_tmp) {
	    fill_bits(marker_bitfield_tmp, msr_tmp->uidx_start, msr_tmp->uidx_end - msr_tmp->uidx_start);
	    msr_tmp = msr_tmp->next;
	  }
	}
        if (complement_sets) {
	  bitfield_and(marker_exclude_new, marker_bitfield_tmp, unfiltered_marker_ctl);
          fill_ulong_zero(marker_bitfield_tmp, unfiltered_marker_ctl);
	}
      }
    } else {
      sorted_marker_ids = (char*)top_alloc(&topsize, marker_ct * max_marker_id_len);
      if (!sorted_marker_ids) {
	goto define_sets_ret_NOMEM;
      }
      marker_id_map = (uint32_t*)top_alloc(&topsize, marker_ct * sizeof(int32_t));
      if (!marker_id_map) {
	goto define_sets_ret_NOMEM;
      }
      wkspace_left -= topsize;
      retval = sort_item_ids_noalloc(sorted_marker_ids, marker_id_map, unfiltered_marker_ct, marker_exclude, marker_ct, marker_ids, max_marker_id_len, 0, 0, strcmp_deref);
      wkspace_left += topsize;
      if (retval) {
	goto define_sets_ret_NOMEM;
      }
      // similar to read_tokens(), since it may be important to support very
      // long lines.
      while (1) {
	if (fread_checked(midbuf, MAXLINELEN, infile, &bufsize)) {
          goto define_sets_ret_READ_FAIL;
	}
        if (!bufsize) {
	  if (curtoklen) {
	    if ((curtoklen != 3) || memcmp(bufptr, "END", 3)) {
	      goto define_sets_ret_INVALID_FORMAT_NO_END;
	    } else if (!in_set) {
	      goto define_sets_ret_INVALID_FORMAT_EXTRA_END;
	    }
	  } else if (in_set) {
	    goto define_sets_ret_INVALID_FORMAT_NO_END;
	  }
	  break;
	}
        buf_end = &(midbuf[bufsize]);
        *buf_end = ' ';
        buf_end[1] = '0';
        bufptr = &(tbuf[MAXLINELEN - curtoklen]);
        bufptr2 = midbuf;
        if (curtoklen) {
          goto define_sets_tok_start_1;
	}
        while (1) {
          while (*bufptr <= ' ') {
	    bufptr++;
	  }
          if (bufptr >= buf_end) {
	    curtoklen = 0;
	    break;
	  }
	  bufptr2 = &(bufptr[1]);
	define_sets_tok_start_1:
          while (*bufptr2 > ' ') {
	    bufptr2++;
	  }
          curtoklen = (uintptr_t)(bufptr2 - bufptr);
          if (bufptr2 == &(tbuf[MAXLINELEN * 2])) {
	    if (curtoklen > MAXLINELEN) {
	      logerrprint("Error: Excessively long token in --set file.\n");
	      goto define_sets_ret_INVALID_FORMAT;
	    }
            bufptr3 = &(tbuf[MAXLINELEN - curtoklen]);
            memcpy(bufptr3, bufptr, curtoklen);
            bufptr = bufptr3;
	    break;
	  }
          if ((curtoklen == 3) && (!memcmp(bufptr, "END", 3))) {
	    if (!in_set) {
	      goto define_sets_ret_INVALID_FORMAT_EXTRA_END;
	    }
            if (complement_sets) {
	      bitfield_and(marker_exclude_new, marker_bitfield_tmp, unfiltered_marker_ctl);
              fill_ulong_zero(marker_bitfield_tmp, unfiltered_marker_ctl);
	    }
            in_set = 0;
	  } else if (!in_set) {
	    if (subset_ct && (bsearch_str(bufptr, (uintptr_t)(bufptr2 - bufptr), sorted_subset_ids, max_subset_id_len, subset_ct) == -1)) {
	      in_set = 2; // ignore this set
	      bufptr = &(bufptr2[1]);
	      continue;
	    }
	    if (curtoklen >= max_set_id_len) {
	      max_set_id_len = curtoklen + 1;
	    }
	    set_ct++;
	    in_set = 1;
	  } else if (in_set == 1) {
	    ii = bsearch_str(bufptr, (uintptr_t)(bufptr2 - bufptr), sorted_marker_ids, max_marker_id_len, marker_ct);
	    if (ii != -1) {
	      set_bit(marker_bitfield_tmp, marker_id_map[(uint32_t)ii]);
	    }
	  }
	  bufptr = &(bufptr2[1]);
	}
      }
      if (!feof(infile)) {
	goto define_sets_ret_READ_FAIL;
      }
      if (!set_ct) {
	if (!complement_sets) {
	  logerrprint("Error: All variants excluded by --gene{-all}, since no sets were defined from\n--set file.\n");
	  goto define_sets_ret_ALL_MARKERS_EXCLUDED_2;
	}
	logerrprint("Warning: No sets defined from --set file.\n");
	goto define_sets_ret_1;
      }
    }
    if (!complement_sets) {
      bitfield_andnot(marker_exclude_new, marker_bitfield_tmp, unfiltered_marker_ctl);
    }
    bitfield_or(marker_exclude, marker_exclude_new, unfiltered_marker_ctl);
    marker_exclude_ct = popcount_longs(marker_exclude, unfiltered_marker_ctl);
    if (marker_exclude_ct == unfiltered_marker_ct) {
      goto define_sets_ret_ALL_MARKERS_EXCLUDED;
    }
    *marker_exclude_ct_ptr = marker_exclude_ct;
    marker_ct = unfiltered_marker_ct - marker_exclude_ct;
    rewind(infile);
    topsize = topsize_bak;
  } else if ((!make_set) && (!sip->merged_set_name)) {
    // 5. otherwise, with --set and no --set-collapse-all, count number of sets
    //    and max_name_len.
    while (1) {
      if (fread_checked(midbuf, MAXLINELEN, infile, &bufsize)) {
	goto define_sets_ret_READ_FAIL;
      }
      if (!bufsize) {
	if (curtoklen) {
	  if ((curtoklen != 3) || (memcmp(bufptr, "END", 3))) {
	    goto define_sets_ret_INVALID_FORMAT_NO_END;
	  } else if (!in_set) {
	    goto define_sets_ret_INVALID_FORMAT_EXTRA_END;
	  }
	} else if (in_set) {
          goto define_sets_ret_INVALID_FORMAT_NO_END;
	}
	break;
      }
      buf_end = &(midbuf[bufsize]);
      *buf_end = ' ';
      buf_end[1] = '0';
      bufptr = &(tbuf[MAXLINELEN - curtoklen]);
      bufptr2 = midbuf;
      if (curtoklen) {
	goto define_sets_tok_start_2;
      }
      while (1) {
	while (*bufptr <= ' ') {
	  bufptr++;
	}
        if (bufptr >= buf_end) {
          curtoklen = 0;
	  break;
	}
        bufptr2 = &(bufptr[1]);
      define_sets_tok_start_2:
        while (*bufptr2 > ' ') {
	  bufptr2++;
	}
        curtoklen = (uintptr_t)(bufptr2 - bufptr);
        if (bufptr2 == &(tbuf[MAXLINELEN * 2])) {
          bufptr3 = &(tbuf[MAXLINELEN - curtoklen]);
          memcpy(bufptr3, bufptr, curtoklen);
	  bufptr = bufptr3;
	  break;
	}
        if ((curtoklen == 3) && (!memcmp(bufptr, "END", 3))) {
          if (!in_set) {
	    goto define_sets_ret_INVALID_FORMAT_EXTRA_END;
	  }
	  in_set = 0;
	} else if (!in_set) {
	  in_set = 1;
	  if (subset_ct && (bsearch_str(bufptr, (uintptr_t)(bufptr2 - bufptr), sorted_subset_ids, max_subset_id_len, subset_ct) == -1)) {
	    // no need for in_set = 2, just don't adjust set_ct/id_len
	    bufptr = &(bufptr2[1]);
	    continue;
	  }
	  if (curtoklen >= max_set_id_len) {
	    max_set_id_len = curtoklen + 1;
	  }
	  set_ct++;
	}
	bufptr = &(bufptr2[1]);
      }
    }
    if (!set_ct) {
      logerrprint("Warning: No sets defined from --set file.\n");
      goto define_sets_ret_1;
    }
    rewind(infile);
  }
  // 6. allocate sip->names[], setdefs[] on stack
  marker_uidx_to_idx = (uint32_t*)top_alloc(&topsize, unfiltered_marker_ct * sizeof(int32_t));
  if (!marker_uidx_to_idx) {
    goto define_sets_ret_NOMEM;
  }
  fill_uidx_to_idx(marker_exclude, unfiltered_marker_ct, marker_ct, marker_uidx_to_idx);
  wkspace_left -= topsize;
  if (!set_names) {
    if (sip->merged_set_name) {
      set_ct = 1;
      max_set_id_len = strlen(sip->merged_set_name) + 1;
      if (max_set_id_len > MAX_ID_LEN_P1) {
	logerrprint("Error: Set IDs are limited to " MAX_ID_LEN_STR " characters.\n");
	goto define_sets_ret_INVALID_FORMAT;
      }
      if (wkspace_alloc_c_checked(&set_names, max_set_id_len)) {
	goto define_sets_ret_NOMEM2;
      }
      memcpy(set_names, sip->merged_set_name, max_set_id_len);
    } else {
      if (max_set_id_len > MAX_ID_LEN_P1) {
	logerrprint("Error: Set IDs are limited to " MAX_ID_LEN_STR " characters.\n");
	goto define_sets_ret_INVALID_FORMAT;
      }
      if (wkspace_alloc_c_checked(&set_names, set_ct * max_set_id_len)) {
	goto define_sets_ret_NOMEM2;
      }
    }
  }
  all_setdefs = (uint32_t**)wkspace_alloc(set_ct * sizeof(intptr_t));
  if (!all_setdefs) {
    goto define_sets_ret_NOMEM2;
  }
  if (make_set) {
    // 7. If --make-set, allocate entries on stack
    for (set_idx = 0; set_idx < set_ct; set_idx++) {
      msr_tmp = make_set_range_arr[set_idx];
      // sort and merge intervals in O(n log n) instead of O(n^2) time
      uii = 0;
      while (msr_tmp) {
	range_first = msr_tmp->uidx_start;
	range_last = msr_tmp->uidx_end;
	msr_tmp = msr_tmp->next;
	if (IS_SET(marker_exclude, range_first)) {
	  range_first = next_unset(marker_exclude, range_first, unfiltered_marker_ct);
	  if (range_first == unfiltered_marker_ct) {
	    continue;
	  }
	}
	range_first = marker_uidx_to_idx[range_first];
	if (IS_SET(marker_exclude, range_last)) {
          range_last = next_unset(marker_exclude, range_last, unfiltered_marker_ct);
	}
	if (range_last == unfiltered_marker_ct) {
	  range_last = marker_ct;
	} else {
          range_last = marker_uidx_to_idx[range_last];
	  if (range_last == range_first) {
	    continue;
	  }
	}
	range_sort_buf[uii++] = (((uint64_t)range_first) << 32) | ((uint64_t)range_last);
      }
      if (!uii) {
	// special case: empty set
	if (wkspace_left < 16) {
	  goto define_sets_ret_NOMEM;
	}
	all_setdefs[set_idx] = (uint32_t*)wkspace_base;
	wkspace_left -= 16;
	wkspace_base = &(wkspace_base[16]);
	if (!complement_sets) {
	  all_setdefs[set_idx][0] = 0;
	} else {
	  all_setdefs[set_idx][0] = 1;
	  all_setdefs[set_idx][1] = 0;
	  all_setdefs[set_idx][2] = marker_ct;
	}
	continue;
      }
#ifdef __cplusplus
      std::sort((int64_t*)range_sort_buf, (int64_t*)(&(range_sort_buf[uii])));
#else
      qsort((int64_t*)range_sort_buf, uii, sizeof(int64_t), llcmp);
#endif
      ukk = 0; // current end of sorted interval list
      range_last = (uint32_t)range_sort_buf[0];
      for (ujj = 1; ujj < uii; ujj++) {
	ullii = range_sort_buf[ujj];
	range_first = (uint32_t)(ullii >> 32);
	if (range_first <= range_last) {
	  umm = (uint32_t)ullii;
	  if (umm > range_last) {
	    range_last = umm;
	    range_sort_buf[ukk] = (range_sort_buf[ukk] & 0xffffffff00000000LLU) | (ullii & 0xffffffffLLU);
	  }
	} else {
	  if (++ukk < ujj) {
	    range_sort_buf[ukk] = ullii;
	  }
	  range_last = (uint32_t)ullii;
	}
      }
      if (save_set_range(range_sort_buf, marker_ct, ukk, complement_sets, &(all_setdefs[set_idx]))) {
	goto define_sets_ret_NOMEM;
      }
    }
  } else {
    // 8. If --set, load sets and allocate on stack
    set_idx = 0;
    in_set = 0;
    curtoklen = 0;
    topsize_bak = topsize;
    range_first = marker_ct;
    range_last = 0;
    // guarantee two free bits at end to simplify loop termination checks (may
    // want to default to doing this...)
    marker_ctp2l = (marker_ct + (BITCT + 1)) / BITCT;
    marker_bitfield_tmp = (uintptr_t*)top_alloc(&topsize, marker_ctp2l * sizeof(intptr_t));
    if (!marker_bitfield_tmp) {
      goto define_sets_ret_NOMEM2;
    }
    sorted_marker_ids = (char*)top_alloc(&topsize, marker_ct * max_marker_id_len);
    if (!sorted_marker_ids) {
      wkspace_left += topsize_bak;
      goto define_sets_ret_NOMEM;
    }
    marker_id_map = (uint32_t*)top_alloc(&topsize, marker_ct * sizeof(int32_t));
    if (!marker_id_map) {
      wkspace_left += topsize_bak;
      goto define_sets_ret_NOMEM;
    }
    wkspace_left -= topsize - topsize_bak;
    retval = sort_item_ids_noalloc(sorted_marker_ids, marker_id_map, unfiltered_marker_ct, marker_exclude, marker_ct, marker_ids, max_marker_id_len, 0, 1, strcmp_deref);
    if (retval) {
      goto define_sets_ret_NOMEM2;
    }
#ifdef __LP64__
    fill_ulong_zero(marker_bitfield_tmp, (marker_ctp2l + 1) & (~1));
#else
    fill_ulong_zero(marker_bitfield_tmp, (marker_ctp2l + 3) & (~3));
#endif
    while (1) {
      if (fread_checked(midbuf, MAXLINELEN, infile, &bufsize)) {
	goto define_sets_ret_READ_FAIL;
      }
      if (!bufsize) {
        if (curtoklen) {
	  if ((curtoklen != 3) || (memcmp(bufptr, "END", 3))) {
	    goto define_sets_ret_INVALID_FORMAT_NO_END;
	  } else if (!in_set) {
            goto define_sets_ret_INVALID_FORMAT_EXTRA_END;
	  }
	} else if (in_set) {
	  goto define_sets_ret_INVALID_FORMAT_NO_END;
	}
	break;
      }
      buf_end = &(midbuf[bufsize]);
      *buf_end = ' ';
      buf_end[1] = '0';
      bufptr = &(tbuf[MAXLINELEN - curtoklen]);
      bufptr2 = midbuf;
      if (curtoklen) {
	goto define_sets_tok_start_3;
      }
      while (1) {
        while (*bufptr <= ' ') {
	  bufptr++;
	}
        if (bufptr >= buf_end) {
	  curtoklen = 0;
	  break;
	}
	bufptr2 = &(bufptr[1]);
      define_sets_tok_start_3:
        while (*bufptr2 > ' ') {
	  bufptr2++;
	}
	curtoklen = (uintptr_t)(bufptr2 - bufptr);
        if ((bufptr2 == buf_end) && (buf_end == &(tbuf[MAXLINELEN * 2]))) {
	  bufptr3 = &(tbuf[MAXLINELEN - curtoklen]);
          memcpy(bufptr3, bufptr, curtoklen);
          bufptr = bufptr3;
          break;
	}
        if ((curtoklen == 3) && (!memcmp(bufptr, "END", 3))) {
	  if (!in_set) {
	    goto define_sets_ret_INVALID_FORMAT_EXTRA_END;
	  }
	  if ((!sip->merged_set_name) && (in_set == 1)) {
	    if (save_set_bitfield(marker_bitfield_tmp, marker_ct, range_first, range_last + 1, complement_sets, &(all_setdefs[set_idx]))) {
	      goto define_sets_ret_NOMEM;
	    }
	    set_idx++;
	    fill_ulong_zero(marker_bitfield_tmp, marker_ctp2l);
	    range_first = marker_ct;
	    range_last = 0;
	  }
	  in_set = 0;
	} else if (!in_set) {
	  in_set = 1;
	  if (subset_ct && (bsearch_str(bufptr, (uintptr_t)(bufptr2 - bufptr), sorted_subset_ids, max_subset_id_len, subset_ct) == -1)) {
	    in_set = 2;
	    bufptr = &(bufptr2[1]);
	    continue;
	  }
	  if (!sip->merged_set_name) {
	    memcpyx(&(set_names[set_idx * max_set_id_len]), bufptr, bufptr2 - bufptr, '\0');
	  }
	} else if (in_set == 1) {
	  ii = bsearch_str(bufptr, (uintptr_t)(bufptr2 - bufptr), sorted_marker_ids, max_marker_id_len, marker_ct);
	  if (ii != -1) {
	    uii = marker_id_map[(uint32_t)ii];
	    if (uii < range_first) {
	      range_first = uii;
	    }
	    if (uii > range_last) {
	      range_last = uii;
	    }
	    set_bit(marker_bitfield_tmp, uii);
	  }
	}
	bufptr = &(bufptr2[1]);
      }
    }
    if (sip->merged_set_name) {
      if (save_set_bitfield(marker_bitfield_tmp, marker_ct, range_first, range_last + 1, complement_sets, all_setdefs)) {
	goto define_sets_ret_NOMEM;
      }
    }
  }
  wkspace_left += topsize;
  if (fclose_null(&infile)) {
    goto define_sets_ret_READ_FAIL;
  }
  sip->ct = set_ct;
  sip->names = set_names;
  sip->max_name_len = max_set_id_len;
  sip->setdefs = all_setdefs;
  LOGPRINTF("--%sset: %" PRIuPTR " set%s defined.\n", make_set? "make-" : "", set_ct, (set_ct == 1)? "" : "s");
  while (0) {
  define_sets_merge_nothing:
    sip->ct = 1;
    uii = strlen(sip->merged_set_name) + 1;
    // topsize = 0;
    sip->setdefs = (uint32_t**)wkspace_alloc(sizeof(intptr_t));
    if (!sip->setdefs) {
      goto define_sets_ret_NOMEM;
    }
    if (wkspace_alloc_c_checked(&sip->names, uii) ||
	wkspace_alloc_ui_checked(&(sip->setdefs[0]), (1 + 2 * complement_sets) * sizeof(int32_t))) {
      goto define_sets_ret_NOMEM;
    }
    memcpy(sip->names, sip->merged_set_name, uii);
    sip->max_name_len = uii;
    if (complement_sets) {
      sip->setdefs[0][0] = 1;
      sip->setdefs[0][1] = 0;
      sip->setdefs[0][2] = marker_ct;
    } else {
      sip->setdefs[0][0] = 0;
    }
    LOGPRINTF("--%sset: 1 set defined.\n", make_set? "make-" : "");
    break;
  define_sets_ret_NOMEM2:
    wkspace_left += topsize;
  define_sets_ret_NOMEM:
    retval = RET_NOMEM;
    break;
  define_sets_ret_OPEN_FAIL:
    retval = RET_OPEN_FAIL;
    break;
  define_sets_ret_READ_FAIL:
    retval = RET_READ_FAIL;
    break;
  define_sets_ret_ALL_MARKERS_EXCLUDED:
    logerrprint("Error: All variants excluded by --gene/--gene-all.\n");
  define_sets_ret_ALL_MARKERS_EXCLUDED_2:
    retval = RET_ALL_MARKERS_EXCLUDED;
    break;
  define_sets_ret_INVALID_FORMAT_EXTRA_END:
    logerrprint("Error: Extra 'END' token in --set file.\n");
    retval = RET_INVALID_FORMAT;
    break;
  define_sets_ret_INVALID_FORMAT_NO_END:
    logerrprint("Error: Last token in --set file isn't 'END'.\n");
  define_sets_ret_INVALID_FORMAT:
    retval = RET_INVALID_FORMAT;
    break;
  }
 define_sets_ret_1:
  fclose_cond(infile);
  return retval;
}

int32_t write_set(Set_info* sip, char* outname, char* outname_end, uint32_t marker_ct, uintptr_t unfiltered_marker_ct, uintptr_t* marker_exclude, char* marker_ids, uintptr_t max_marker_id_len, uint32_t* marker_pos, Chrom_info* chrom_info_ptr) {
  unsigned char* wkspace_mark = wkspace_base;
  FILE* outfile = NULL;
  uintptr_t set_ct = sip->ct;
  uintptr_t max_set_name_len = sip->max_name_len;
  uintptr_t set_idx = 0;
  uint32_t chrom_idx = 0;
  int32_t retval = 0;
  uintptr_t* ulptr;
  uint32_t* marker_idx_to_uidx;
  uint32_t* last_idx;
  uint32_t* next_adj;
  uint32_t* cur_set_ptr;
  char* cur_setting;
  char* writebuf;
  char* bufptr;
  char* cptr;
  uintptr_t marker_uidx;
  uint32_t marker_idx;
  uint32_t chrom_end;
  uint32_t range_ct;
  uint32_t range_start;
  uint32_t uii;
  uint32_t ujj;
  uint32_t ukk;
  if (sip->modifier & SET_WRITE_TABLE) {
    memcpy(outname_end, ".set.table", 11);
    if (fopen_checked(&outfile, outname, "w")) {
      goto write_set_ret_OPEN_FAIL;
    }
    fputs("SNP\tCHR\tBP", outfile);
    for (set_idx = 0; set_idx < set_ct; set_idx++) {
      putc('\t', outfile);
      fputs(&(sip->names[set_idx * max_set_name_len]), outfile);
    }
    if (putc_checked('\n', outfile)) {
      goto write_set_ret_WRITE_FAIL;
    }
    if (wkspace_alloc_ui_checked(&last_idx, set_ct * sizeof(int32_t)) ||
        wkspace_alloc_ui_checked(&next_adj, set_ct * sizeof(int32_t)) ||
        wkspace_alloc_c_checked(&cur_setting, set_ct) ||
        wkspace_alloc_c_checked(&writebuf, 2 * set_ct)) {
      goto write_set_ret_NOMEM;
    }
    fill_uint_zero(last_idx, set_ct);
    fill_uint_zero(next_adj, set_ct);
    marker_uidx = 0;
    tbuf[0] = '\t';
    chrom_end = 0;
    for (set_idx = 1; set_idx < set_ct; set_idx++) {
      writebuf[2 * set_idx - 1] = '\t';
    }
    writebuf[2 * set_ct - 1] = '\n';
    for (marker_idx = 0; marker_idx < marker_ct; marker_uidx++, marker_idx++) {
      next_unset_ul_unsafe_ck(marker_exclude, &marker_uidx);
      if (marker_uidx >= chrom_end) {
	uii = get_marker_chrom_fo_idx(chrom_info_ptr, marker_uidx);
        chrom_idx = chrom_info_ptr->chrom_file_order[uii];
        chrom_end = chrom_info_ptr->chrom_file_order_marker_idx[uii];
      }
      fputs(&(marker_ids[marker_uidx * max_marker_id_len]), outfile);
      bufptr = chrom_name_write(&(tbuf[1]), chrom_info_ptr, chrom_idx);
      *bufptr++ = '\t';
      bufptr = uint32_writex(bufptr, marker_pos[marker_uidx], '\t');
      // do not keep double-tab (if it was intentional, it should have been in
      // the header line too...)
      fwrite(tbuf, 1, bufptr - tbuf, outfile);
      bufptr = writebuf;
      cptr = cur_setting;
      for (set_idx = 0; set_idx < set_ct; set_idx++) {
        if (next_adj[set_idx] <= marker_idx) {
	  cur_set_ptr = sip->setdefs[set_idx];
	  range_ct = cur_set_ptr[0];
	  if (range_ct != 0xffffffffU) {
	    uii = last_idx[set_idx];
	    if (uii < range_ct) {
	      range_start = sip->setdefs[set_idx][2 * uii + 1];
	      if (range_start > marker_idx) {
		*cptr = '0';
		next_adj[set_idx] = range_start;
	      } else {
		*cptr = '1';
		next_adj[set_idx] = sip->setdefs[set_idx][2 * uii + 2];
		last_idx[set_idx] = uii + 1;
	      }
	    } else {
              *cptr = '0';
              next_adj[set_idx] = marker_ct;
	    }
	  } else {
	    range_start = cur_set_ptr[1];
	    uii = cur_set_ptr[3];
	    if (marker_idx >= range_start) {
	      ujj = cur_set_ptr[2];
              if (marker_idx < ujj + range_start) {
                ulptr = (uintptr_t*)(&(cur_set_ptr[4]));
		ukk = marker_idx - range_start;
                if (IS_SET(ulptr, ukk)) {
		  *cptr = '1';
		  ukk++;
		  next_unset_ck(ulptr, &ukk, ujj);
		} else {
                  *cptr = '0';
                  ukk = next_set(ulptr, ukk, ujj);
		}
		next_adj[set_idx] = range_start + ukk;
	      } else {
		*cptr = '0' + uii;
		next_adj[set_idx] = marker_ct;
	      }
	    } else {
              *cptr = '0' + uii;
	      next_adj[set_idx] = range_start;
	    }
	  }
	}
	*bufptr = *cptr++;
        bufptr = &(bufptr[2]);
      }
      if (fwrite_checked(writebuf, 2 * set_ct, outfile)) {
	goto write_set_ret_WRITE_FAIL;
      }
    }
    if (fclose_null(&outfile)) {
      goto write_set_ret_WRITE_FAIL;
    }
    LOGPRINTFWW("--set-table: %s written.\n", outname);
  }
  if (sip->modifier & SET_WRITE_LIST) {
    memcpy(outname_end, ".set", 5);
    if (fopen_checked(&outfile, outname, "w")) {
      goto write_set_ret_OPEN_FAIL;
    }
    if (wkspace_alloc_ui_checked(&marker_idx_to_uidx, marker_ct * sizeof(int32_t))) {
      goto write_set_ret_NOMEM;
    }
    fill_idx_to_uidx(marker_exclude, unfiltered_marker_ct, marker_ct, marker_idx_to_uidx);
    for (set_idx = 0; set_idx < set_ct; set_idx++) {
      fputs(&(sip->names[set_idx * max_set_name_len]), outfile);
      putc('\n', outfile);
      cur_set_ptr = sip->setdefs[set_idx];
      range_ct = cur_set_ptr[0];
      if (range_ct != 0xffffffffU) {
        for (uii = 0; uii < range_ct; uii++) {
	  ujj = cur_set_ptr[uii * 2 + 2];
          for (marker_idx = cur_set_ptr[uii * 2 + 1]; marker_idx < ujj; marker_idx++) {
            fputs(&(marker_ids[marker_idx_to_uidx[marker_idx] * max_marker_id_len]), outfile);
	    putc('\n', outfile);
	  }
	}
      } else {
	range_start = cur_set_ptr[1];
	uii = cur_set_ptr[2];
	ulptr = (uintptr_t*)(&(cur_set_ptr[4]));
	if (cur_set_ptr[3]) {
	  for (marker_idx = 0; marker_idx < range_start; marker_idx++) {
	    fputs(&(marker_ids[marker_idx_to_uidx[marker_idx] * max_marker_id_len]), outfile);
	    putc('\n', outfile);
	  }
	}
	marker_idx = 0;
	while (1) {
	  next_set_ck(ulptr, &marker_idx, uii);
	  if (marker_idx == uii) {
	    break;
	  }
          fputs(&(marker_ids[marker_idx_to_uidx[marker_idx] * max_marker_id_len]), outfile);
	  putc('\n', outfile);
	  marker_idx++;
	}
	if ((range_start + uii < marker_ct) && cur_set_ptr[3]) {
          for (marker_idx = range_start + uii; marker_idx < marker_ct; marker_idx++) {
	    fputs(&(marker_ids[marker_idx_to_uidx[marker_idx] * max_marker_id_len]), outfile);
	    putc('\n', outfile);
	  }
	}
      }
      if (fputs_checked("END\n\n", outfile)) {
	goto write_set_ret_WRITE_FAIL;
      }
    }
    if (fclose_null(&outfile)) {
      goto write_set_ret_WRITE_FAIL;
    }
    LOGPRINTFWW("--write-set: %s written.\n", outname);
  }
  while (0) {
  write_set_ret_NOMEM:
    retval = RET_NOMEM;
    break;
  write_set_ret_OPEN_FAIL:
    retval = RET_OPEN_FAIL;
    break;
  write_set_ret_WRITE_FAIL:
    retval = RET_WRITE_FAIL;
    break;
  }
  fclose_cond(outfile);
  wkspace_reset(wkspace_mark);
  return retval;
}

void unpack_set(uintptr_t marker_ct, uint32_t* setdef, uintptr_t* include_bitfield) {
  uintptr_t marker_ctl = (marker_ct + (BITCT - 1)) / BITCT;
  uint32_t range_ct = setdef[0];
  uint32_t keep_outer;
  uint32_t range_start;
  uint32_t uii;
  if (range_ct == 0xffffffffU) {
    range_start = setdef[1];
    range_ct = setdef[2];
    keep_outer = setdef[3];
    if (range_start) {
      if (keep_outer) {
        fill_ulong_one(include_bitfield, range_start / BITCT);
      } else {
        fill_ulong_zero(include_bitfield, range_start / BITCT);
      }
    }
    memcpy(&(include_bitfield[range_start / BITCT]), (uintptr_t*)(&(setdef[4])), ((range_ct + 127) / 128) * 16);
    uii = range_start + range_ct;
    if (uii < marker_ct) {
      if (keep_outer) {
        fill_bits(include_bitfield, uii, marker_ct - uii);
      } else {
        fill_ulong_zero(&(include_bitfield[uii / BITCT]), marker_ctl - uii / BITCT);
      }
    }
  } else {
    fill_ulong_zero(include_bitfield, marker_ctl);
    for (uii = 0; uii < range_ct; uii++) {
      range_start = setdef[uii * 2 + 1];
      fill_bits(include_bitfield, range_start, setdef[uii * 2 + 2] - range_start);
    }
  }
}

void unpack_set_unfiltered(uintptr_t marker_ct, uintptr_t unfiltered_marker_ct, uintptr_t* marker_exclude, uint32_t* setdef, uintptr_t* new_exclude) {
  uintptr_t unfiltered_marker_ctl = (unfiltered_marker_ct + (BITCT - 1)) / BITCT;
  uintptr_t last_uidx = next_unset_unsafe(marker_exclude, 0);
  uintptr_t marker_uidx = last_uidx;
  uint32_t range_ct = setdef[0];
  uint32_t range_end = 0;
  uintptr_t* bitfield_ptr;
  uint32_t* uiptr;
  uint32_t keep_outer;
  uint32_t range_start;
  uint32_t range_idx;
  memcpy(new_exclude, marker_exclude, unfiltered_marker_ctl * sizeof(intptr_t));
  if (range_ct == 0xffffffffU) {
    range_start = setdef[1];
    range_ct = setdef[2];
    keep_outer = setdef[3];
    bitfield_ptr = (uintptr_t*)(&(setdef[4]));
    if (range_start) {
      // if nonzero, range_start also must be greater than 1
      marker_uidx = jump_forward_unset_unsafe(marker_exclude, last_uidx + 1, range_start);
      if (!keep_outer) {
	fill_bits(new_exclude, last_uidx, marker_uidx - last_uidx);
      }
    }
    for (range_idx = 0; range_idx < range_ct; range_idx++, marker_uidx++) {
      next_unset_ul_unsafe_ck(marker_exclude, &marker_uidx);
      // we know that range representation is not more compact, so probably not
      // worthwhile to use next_unset/next_set/fill_bits() here
      if (!IS_SET(bitfield_ptr, range_idx)) {
	SET_BIT(new_exclude, marker_uidx);
      }
    }
    if ((!keep_outer) && (range_start + range_ct < marker_ct)) {
      fill_bits(new_exclude, marker_uidx, unfiltered_marker_ct - marker_uidx);
    }
  } else {
    uiptr = &(setdef[1]);
    range_idx = 0;
    if ((!setdef[1]) && range_ct) {
      range_start = *uiptr++;
      goto unpack_set_unfiltered_late_start;
    }
    for (; range_idx < range_ct; range_idx++) {
      range_start = *uiptr++;
      if (range_start > range_end) {
        marker_uidx = jump_forward_unset_unsafe(marker_exclude, last_uidx + 1, range_start - range_end);
      }
      fill_bits(new_exclude, last_uidx, marker_uidx - last_uidx);
      next_unset_ul_unsafe_ck(marker_exclude, &marker_uidx);
    unpack_set_unfiltered_late_start:
      range_end = *uiptr++;
      if (range_end == marker_ct) {
	last_uidx = unfiltered_marker_ct;
	break;
      }
      last_uidx = jump_forward_unset_unsafe(marker_exclude, marker_uidx + 1, range_end - range_start);
    }
    fill_bits(new_exclude, last_uidx, unfiltered_marker_ct - last_uidx);
  }
}

uint32_t extract_set_union(uint32_t** setdefs, uintptr_t set_ct, uintptr_t* set_incl, uintptr_t* filtered_union, uintptr_t marker_ct) {
  uintptr_t marker_ctl = (marker_ct + (BITCT - 1)) / BITCT;

  // these track known filled words at the beginning and end.  (just intended
  // to detect early exit opportunities; doesn't need to be perfect.)
  uint32_t unset_startw = 0;
  uint32_t unset_endw = marker_ctl;

  uint32_t* cur_setdef;
  uintptr_t set_idx;
  uint32_t range_ct;
  uint32_t range_idx;
  uint32_t range_start;
  uint32_t range_end;
  uint32_t keep_outer;
  uint32_t read_offset;
  fill_ulong_zero(filtered_union, marker_ctl);
  for (set_idx = 0; set_idx < set_ct; set_idx++) {
    if (set_incl && (!IS_SET(set_incl, set_idx))) {
      continue;
    }
    cur_setdef = setdefs[set_idx];
    range_ct = cur_setdef[0];
    if (range_ct == 0xffffffffU) {
      range_start = cur_setdef[1] / BITCT;
      range_end = range_start + (cur_setdef[2] / BITCT);
      keep_outer = cur_setdef[3];
      if (range_end > unset_startw) {
	read_offset = 0;
        if (range_start > unset_startw) {
          if (keep_outer) {
	    fill_ulong_one(filtered_union, range_start);
            unset_startw = range_start;
	  }
	} else {
          read_offset = unset_startw - range_start;
	  range_start = unset_startw;
	}
	if (range_end > unset_endw) {
          range_end = unset_endw;
	}
        if (range_start < range_end) {
	  bitfield_or(&(filtered_union[range_start]), (uintptr_t*)(&(cur_setdef[4 + (BITCT / 32) * read_offset])), range_end - range_start);
	}
      }
      if (keep_outer && (range_end < unset_endw)) {
	// may overfill end
	fill_ulong_one(&(filtered_union[range_end]), unset_endw - range_end);
        unset_endw = range_end;
      }
    } else if (range_ct) {
      cur_setdef++;
      if (unset_startw) {
	// skip all ranges with end <= unset_startw * BITCT
        read_offset = uint32arr_greater_than(cur_setdef, range_ct * 2, unset_startw * BITCT + 1) / 2;
        if (read_offset) {
	  if (range_ct == read_offset) {
	    continue;
	  }
          cur_setdef = &(cur_setdef[read_offset * 2]);
          range_ct -= read_offset;
	}
      }
      if (unset_endw < marker_ctl) {
        // and skip all ranges with start >= unset_endw * BITCT
        range_ct = (uint32arr_greater_than(cur_setdef, range_ct * 2, unset_endw * BITCT) + 1) / 2;
      }
      if (range_ct) {
	range_start = *(cur_setdef++);
        range_end = *(cur_setdef++);
        if (range_start < unset_startw * BITCT) {
	  range_start = unset_startw * BITCT;
	}
	if (range_ct > 1) {
          fill_bits(filtered_union, range_start, range_end - range_start);
	  for (range_idx = 2; range_idx < range_ct; range_idx++) {
	    range_start = *(cur_setdef++);
	    range_end = *(cur_setdef++);
	    fill_bits(filtered_union, range_start, range_end - range_start);
	  }
          range_start = *(cur_setdef++);
          range_end = *(cur_setdef++);
	}
	if (range_end > unset_endw * BITCT) {
	  range_end = unset_endw * BITCT;
	}
        fill_bits(filtered_union, range_start, range_end - range_start);
      }
    }
    while (1) {
      if (unset_startw >= unset_endw) {
        goto extract_set_union_exit_early;
      }
      if (~(filtered_union[unset_startw])) {
	// guaranteed to terminate
	while (!(~(filtered_union[unset_endw - 1]))) {
	  unset_endw--;
	}
	break;
      }
      unset_startw++;
    }
  }
 extract_set_union_exit_early:
  zero_trailing_bits(filtered_union, marker_ct);
  return popcount_longs(filtered_union, marker_ctl);
}

uint32_t extract_set_union_unfiltered(Set_info* sip, uintptr_t* set_incl, uintptr_t unfiltered_marker_ct, uintptr_t* marker_exclude, uintptr_t** union_marker_exclude_ptr, uintptr_t* union_marker_ct_ptr) {
  // If union = all remaining markers, simply makes union_marker_exclude_ptr
  // point to marker_exclude.  Otherwise, allocates union_marker_exclude on the
  // "stack".
  // Assumes marker_ct is initial value of *union_marker_ct_ptr.
  uintptr_t unfiltered_marker_ctl = (unfiltered_marker_ct + (BITCT - 1)) / BITCT;
  uintptr_t orig_marker_ct = *union_marker_ct_ptr;
  uintptr_t orig_marker_ctl = (orig_marker_ct + (BITCT - 1)) / BITCT;
  uintptr_t* union_marker_exclude;
  uintptr_t* filtered_union;
  if (wkspace_alloc_ul_checked(&union_marker_exclude, unfiltered_marker_ctl * sizeof(intptr_t)) ||
      wkspace_alloc_ul_checked(&filtered_union, orig_marker_ctl * sizeof(intptr_t))) {
    return 1;
  }
  *union_marker_ct_ptr = extract_set_union(sip->setdefs, sip->ct, set_incl, filtered_union, orig_marker_ct);
  if ((*union_marker_ct_ptr) == orig_marker_ct) {
    wkspace_reset(union_marker_exclude);
    *union_marker_exclude_ptr = marker_exclude;
  } else {
    uncollapse_copy_flip_include_arr(filtered_union, unfiltered_marker_ct, marker_exclude, union_marker_exclude);
    wkspace_reset(filtered_union);
    *union_marker_exclude_ptr = union_marker_exclude;
  }
  return 0;
}

uint32_t setdefs_compress(Set_info* sip, uintptr_t* set_incl, uintptr_t set_ct, uintptr_t unfiltered_marker_ct, uintptr_t* marker_exclude_orig, uintptr_t marker_ct_orig, uintptr_t* marker_exclude, uintptr_t marker_ct, uint32_t*** new_setdefs_ptr) {
  // currently assumes marker_exclude does not exclude anything in the union of
  // the remaining sets
  uintptr_t marker_ctlv = ((marker_ct + 127) / 128) * (128 / BITCT);
  uintptr_t topsize = 0;
  uint32_t set_uidx = 0;
  uintptr_t* cur_bitfield;
  uintptr_t* read_bitfield;
  uint32_t** new_setdefs;
  uint32_t* marker_midx_to_idx;
  uint32_t* cur_setdef;
  uintptr_t set_idx;
  uint32_t range_ct;
  uint32_t range_idx;
  uint32_t range_offset;
  uint32_t range_stop;
  uint32_t range_start;
  uint32_t range_end;
  uint32_t include_out_of_bounds;
  uint32_t marker_midx;
  new_setdefs = (uint32_t**)wkspace_alloc(set_ct * sizeof(intptr_t));
  if (!new_setdefs) {
    return 1;
  }
  cur_bitfield = (uintptr_t*)top_alloc(&topsize, marker_ctlv * sizeof(intptr_t));
  if (!cur_bitfield) {
    return 1;
  }
  marker_midx_to_idx = (uint32_t*)top_alloc(&topsize, marker_ct_orig * sizeof(int32_t));
  if (!marker_midx_to_idx) {
    return 1;
  }
  fill_midx_to_idx(marker_exclude_orig, marker_exclude, marker_ct, marker_midx_to_idx);
  wkspace_left -= topsize;
  for (set_idx = 0; set_idx < set_ct; set_uidx++, set_idx++) {
    if (set_incl) {
      next_set_unsafe_ck(set_incl, &set_uidx);
    }
    cur_setdef = sip->setdefs[set_uidx];
    fill_ulong_zero(cur_bitfield, marker_ctlv);
    range_ct = cur_setdef[0];
    range_start = marker_ct;
    range_end = 0;
    if (range_ct != 0xffffffffU) {
      if (range_ct) {
        range_start = marker_midx_to_idx[cur_setdef[1]];
	for (range_idx = 0; range_idx < range_ct; range_idx++) {
	  range_offset = *(++cur_setdef);
	  range_stop = *(++cur_setdef);
	  fill_bits(cur_bitfield, marker_midx_to_idx[range_offset], range_stop - range_offset);
	}
        range_end = marker_midx_to_idx[range_offset] + range_stop - range_offset;
      }
    } else {
      range_offset = cur_setdef[1];
      range_stop = cur_setdef[2];
      include_out_of_bounds = cur_setdef[3];
      read_bitfield = (uintptr_t*)(&(cur_setdef[4]));
      if (include_out_of_bounds && range_offset) {
        fill_ulong_one(cur_bitfield, range_offset / BITCT);
	range_start = 0;
      }
      for (marker_midx = 0; marker_midx < range_stop; marker_midx++) {
        if (IS_SET(read_bitfield, marker_midx)) {
          set_bit(cur_bitfield, marker_midx_to_idx[marker_midx + range_offset]);
	}
      }
      if (include_out_of_bounds && (range_offset + range_stop < marker_ct_orig)) {
        fill_bits(cur_bitfield, marker_midx_to_idx[range_offset + range_stop], marker_ct_orig - range_offset - range_stop);
        range_end = marker_ct;
      } else {
        range_end = 1 + last_set_bit(cur_bitfield, 1 + (marker_midx_to_idx[marker_ct_orig - 1] / BITCT));
      }
      if (range_start) {
        range_start = marker_midx_to_idx[next_set_unsafe(read_bitfield, 0) + range_offset];
      }
    }
    if (save_set_bitfield(cur_bitfield, marker_ct, range_start, range_end, 0, &(new_setdefs[set_idx]))) {
      wkspace_left += topsize;
      return 1;
    }
  }
  *new_setdefs_ptr = new_setdefs;
  wkspace_left += topsize;
  return 0;
}

int32_t load_range_list_sortpos(char* fname, uint32_t border_extend, uintptr_t subset_ct, char* sorted_subset_ids, uintptr_t max_subset_id_len, Chrom_info* chrom_info_ptr, uintptr_t* gene_ct_ptr, char** gene_names_ptr, uintptr_t* max_gene_id_len_ptr, uintptr_t** chrom_bounds_ptr, uint32_t*** genedefs_ptr, uintptr_t* chrom_max_gene_ct_ptr, const char* file_descrip) {
  // --annotate, --clump-range, --gene-report
  FILE* infile = NULL;
  uintptr_t gene_ct = 0;
  uintptr_t max_gene_id_len = 0;
  uintptr_t chrom_max_gene_ct = 0;
  uintptr_t topsize = 0;
  uint32_t chrom_code_end = chrom_info_ptr->max_code + 1 + chrom_info_ptr->name_ct;
  uint32_t chrom_idx = 0;
  Make_set_range** gene_arr;
  Make_set_range* msr_tmp;
  uint64_t* range_sort_buf;
  uintptr_t* chrom_bounds;
  uint32_t** genedefs;
  char* gene_names;
  char* bufptr;
  uint32_t* uiptr;
  uint64_t ullii;
  uintptr_t gene_idx;
  uintptr_t ulii;
  uint32_t range_first;
  uint32_t range_last;
  uint32_t uii;
  uint32_t ujj;
  uint32_t ukk;
  uint32_t umm;
  int32_t retval;
  if (fopen_checked(&infile, fname, "r")) {
    goto load_range_list_sortpos_ret_OPEN_FAIL;
  }
  retval = load_range_list(infile, 1, border_extend, 0, 0, 0, subset_ct, sorted_subset_ids, 0, NULL, chrom_info_ptr, &topsize, &gene_ct, gene_names_ptr, &max_gene_id_len, &gene_arr, &range_sort_buf, file_descrip);
  if (retval) {
    goto load_range_list_sortpos_ret_1;
  }
  gene_names = *gene_names_ptr;
  wkspace_left -= topsize;
  if (wkspace_alloc_ul_checked(chrom_bounds_ptr, (chrom_code_end + 1) * sizeof(intptr_t))) {
    goto load_range_list_sortpos_ret_NOMEM2;
  }
  chrom_bounds = *chrom_bounds_ptr;
  chrom_bounds[0] = 0;
  genedefs = (uint32_t**)wkspace_alloc(gene_ct * sizeof(intptr_t));
  if (!genedefs) {
    goto load_range_list_sortpos_ret_NOMEM2;
  }
  for (gene_idx = 0; gene_idx < gene_ct; gene_idx++) {
    bufptr = &(gene_names[gene_idx * max_gene_id_len]);
    // instead of subtracting 33/48 separately from each character, just
    // subtract
    // 48 * (1000 + 100 + 10) + 33 once at the end.
    // (last prefix character must be nonnumeric to prevent weird natural sort
    // interaction)
    uii = (((unsigned char)bufptr[0]) * 1000) + (((unsigned char)bufptr[1]) * 100) + (((unsigned char)bufptr[2]) * 10) + ((unsigned char)bufptr[3]) - 53313;
    if (chrom_idx < uii) {
      ulii = gene_idx - chrom_bounds[chrom_idx];
      if (ulii > chrom_max_gene_ct) {
	chrom_max_gene_ct = ulii;
      }
      do {
	chrom_bounds[++chrom_idx] = gene_idx;
      } while (chrom_idx < uii);
    }
    msr_tmp = gene_arr[gene_idx];
    uii = 0;
    while (msr_tmp) {
      range_sort_buf[uii++] = (((uint64_t)(msr_tmp->uidx_start)) << 32) | ((uint64_t)(msr_tmp->uidx_end));
      msr_tmp = msr_tmp->next;
    }
    if (!uii) {
      if (wkspace_left < 16) {
	goto load_range_list_sortpos_ret_NOMEM2;
      }
      genedefs[gene_idx] = (uint32_t*)wkspace_base;
      wkspace_left -= 16;
      wkspace_base = &(wkspace_base[16]);
      genedefs[gene_idx][0] = 0;
      continue;
    }
#ifdef __cplusplus
    std::sort((int64_t*)range_sort_buf, (int64_t*)(&(range_sort_buf[uii])));
#else
    qsort(range_sort_buf, uii, sizeof(int64_t), llcmp);
#endif
    ukk = 0; // current end of sorted interval list
    range_last = (uint32_t)range_sort_buf[0];
    for (ujj = 1; ujj < uii; ujj++) {
      ullii = range_sort_buf[ujj];
      range_first = (uint32_t)(ullii >> 32);
      if (range_first <= range_last) {
	umm = (uint32_t)ullii;
	if (umm > range_last) {
	  range_last = umm;
	  range_sort_buf[ukk] = (range_sort_buf[ukk] & 0xffffffff00000000LLU) | (ullii & 0xffffffffLLU);
	}
      } else {
	if (++ukk < ujj) {
	  range_sort_buf[ukk] = ullii;
	}
	range_last = (uint32_t)ullii;
      }
    }
    ulii = (((++ukk) * 2 + 4) * sizeof(int32_t)) & (~(15 * ONELU));
    if (wkspace_left < ulii) {
      goto load_range_list_sortpos_ret_NOMEM2;
    }
    genedefs[gene_idx] = (uint32_t*)wkspace_base;
    wkspace_left -= ulii;
    wkspace_base = &(wkspace_base[ulii]);
    uiptr = genedefs[gene_idx];
    *uiptr++ = ukk;
    for (uii = 0; uii < ukk; uii++) {
      ullii = range_sort_buf[uii];
      *uiptr++ = (uint32_t)(ullii >> 32);
      *uiptr++ = (uint32_t)ullii;
    }
  }
  ulii = gene_ct - chrom_bounds[chrom_idx];
  if (ulii > chrom_max_gene_ct) {
    chrom_max_gene_ct = ulii;
  }
  while (chrom_idx < chrom_code_end) {
    chrom_bounds[++chrom_idx] = gene_ct;
  }
  wkspace_left += topsize;
  if (fclose_null(&infile)) {
    goto load_range_list_sortpos_ret_READ_FAIL;
  }
  *gene_ct_ptr = gene_ct;
  *max_gene_id_len_ptr = max_gene_id_len;
  *chrom_max_gene_ct_ptr = chrom_max_gene_ct;
  *genedefs_ptr = genedefs;
  while (0) {
  load_range_list_sortpos_ret_NOMEM2:
    wkspace_left += topsize;
    retval = RET_NOMEM;
    break;
  load_range_list_sortpos_ret_OPEN_FAIL:
    retval = RET_OPEN_FAIL;
    break;
  load_range_list_sortpos_ret_READ_FAIL:
    retval = RET_READ_FAIL;
    break;
  }
 load_range_list_sortpos_ret_1:
  fclose_cond(infile);
  return retval;
}

int32_t annotate(Annot_info* aip, char* outname, char* outname_end, double pfilter, Chrom_info* chrom_info_ptr) {
  unsigned char* wkspace_mark = wkspace_base;
  gzFile gz_attribfile = NULL;
  FILE* infile = NULL;
  FILE* outfile = NULL;
  char* sorted_snplist = NULL;
  char* sorted_attr_ids = NULL; // natural-sorted
  char* sorted_snplist_attr_ids = NULL;
  char* sorted_subset_ids = NULL;
  char* range_names = NULL;
  char* filter_range_names = NULL;
  char* wptr = NULL;
  const char* snp_field = NULL;
  uintptr_t* attr_bitfields = NULL;
  uintptr_t* chrom_bounds = NULL;
  uintptr_t* chrom_filter_bounds = NULL;
  uint32_t** rangedefs = NULL;
  uint32_t** filter_rangedefs = NULL;
  uint32_t* range_idx_lookup = NULL;
  uint32_t* attr_id_remap = NULL;
  uint32_t* merged_attr_idx_buf = NULL;
  const char constsnpstr[] = "SNP";
  const char constdotstr[] = ".";
  const char constnastr[] = "NA";
  const char constdot4str[] = "   .";
  const char constna4str[] = "  NA";
  const char* no_annot_str = (aip->modifier & ANNOT_NA)? constnastr : constdotstr;
  const char* no_sign_str = (aip->modifier & ANNOT_NA)? constna4str : constdot4str;
  uintptr_t topsize = 0;
  uintptr_t snplist_ct = 0;
  uintptr_t max_snplist_id_len = 0;
  uintptr_t snplist_attr_ct = 0;
  uintptr_t max_snplist_attr_id_len = 0;
  uintptr_t attr_id_ct = 0;
  uintptr_t attr_id_ctl = 0;
  uintptr_t max_attr_id_len = 0;
  uintptr_t subset_ct = 0;
  uintptr_t max_subset_id_len = 0;
  uintptr_t range_ct = 0;
  uintptr_t max_range_name_len = 0;
  uintptr_t filter_range_ct = 0;
  uintptr_t max_filter_range_name_len = 0;
  uintptr_t chrom_max_range_ct = 0;
  uintptr_t chrom_max_filter_range_ct = 0;
  uintptr_t annot_row_ct = 0;
  uintptr_t total_row_ct = 0;
  uint32_t max_onevar_attr_ct = 0;
  uint32_t snp_field_len = 0;
  uint32_t border = aip->border;
  uint32_t need_var_id = (aip->attrib_fname || aip->snps_fname);
  uint32_t need_pos = (aip->ranges_fname || aip->filter_fname);
  uint32_t do_pfilter = (pfilter != 2.0);
  uint32_t token_ct = need_var_id + 2 * need_pos + do_pfilter;
  uint32_t block01 = (aip->modifier & ANNOT_BLOCK);
  uint32_t prune = (aip->modifier & ANNOT_PRUNE);
  uint32_t range_dist = !(aip->modifier & ANNOT_MINIMAL);
  uint32_t track_distance = (aip->modifier & ANNOT_DISTANCE);
  uint32_t col_idx = 0;
  uint32_t seq_idx = 0;
  uint32_t max_header_len = 3;
  uint32_t unique_annot_ct = 0;
  uint32_t unique_annot_ctlw = 0;
  int32_t chrom_idx = 0;
  int32_t min_dist = 0;
  int32_t retval = 0;

  // col_skips[0..(token_ct - 1)] stores deltas between adjacent column indices
  // ([0] = 0-based index of first column), and token_ptrs[0..(token_ct - 1)]
  // points to those token start positions in the current line.
  // Since the order of the columns may vary, col_sequence[0] = col_skips
  // position of CHR index, [1] = BP index, [2] = SNP index, and [3] = P index.
  char* token_ptrs[4];
  uint32_t col_skips[4];
  uint32_t col_sequence[4];

  Ll_str** attr_id_htable;
  Ll_str** ll_pptr;
  Ll_str* ll_ptr;
  char* loadbuf;
  char* merged_attr_ids;
  char* writebuf;
  char* bufptr;
  char* bufptr2;
  uintptr_t* ulptr;
  uint32_t* uiptr;
  uintptr_t loadbuf_size;
  uintptr_t topsize_bak;
  uintptr_t line_idx;
  uintptr_t range_idx;
  uintptr_t ulii;
  uintptr_t uljj;
  double pval;
  uint32_t slen;
  uint32_t write_idx;
  uint32_t cur_bp;
  uint32_t abs_min_dist;
  uint32_t at_least_one_annot;
  uint32_t uii;
  uint32_t ujj;
  int32_t sorted_idx;
  int32_t ii;
  if (need_var_id) {
    if (aip->snpfield) {
      snp_field = aip->snpfield;
      snp_field_len = strlen(snp_field);
      if (snp_field_len > 3) {
        max_header_len = 3;
      }
    } else {
      snp_field = constsnpstr;
      snp_field_len = 3;
    }
    if (aip->snps_fname) {
      if (fopen_checked(&infile, aip->snps_fname, "rb")) {
	goto annotate_ret_OPEN_FAIL;
      }
      retval = scan_token_ct_len(infile, tbuf, MAXLINELEN, &snplist_ct, &max_snplist_id_len);
      if (retval) {
	if (retval == RET_INVALID_FORMAT) {
	  logerrprint("Error: Pathologically long token in --annotate snps file.\n");
	}
	goto annotate_ret_1;
      }
      if (!snplist_ct) {
	sprintf(logbuf, "Error: %s is empty.\n", aip->snps_fname);
	goto annotate_ret_INVALID_FORMAT_WW;
      }
      if (wkspace_alloc_c_checked(&sorted_snplist, snplist_ct * max_snplist_id_len)) {
	goto annotate_ret_NOMEM;
      }
      rewind(infile);
      retval = read_tokens(infile, tbuf, MAXLINELEN, snplist_ct, max_snplist_id_len, sorted_snplist);
      if (retval) {
	goto annotate_ret_1;
      }
      if (fclose_null(&infile)) {
	goto annotate_ret_READ_FAIL;
      }
      qsort(sorted_snplist, snplist_ct, max_snplist_id_len, strcmp_casted);
      ulii = collapse_duplicate_ids(sorted_snplist, snplist_ct, max_snplist_id_len, NULL);
      if (ulii < snplist_ct) {
	snplist_ct = ulii;
	wkspace_shrink_top(sorted_snplist, snplist_ct * max_snplist_id_len);
      }
    }
    if (aip->attrib_fname) {
      if (gzopen_checked(&gz_attribfile, aip->attrib_fname, "rb")) {
	goto annotate_ret_OPEN_FAIL;
      }
      if (gzbuffer(gz_attribfile, 131072)) {
	goto annotate_ret_NOMEM;
      }
      line_idx = 0;
      tbuf[MAXLINELEN - 1] = ' ';
      // two-pass load.
      // 1. determine attribute set, as well as relevant variant ID count and
      //    max length
      // intermission. extract attribute names from hash table, natural sort,
      //               deallocate hash table
      // 2. save relevant variant IDs and attribute bitfields, then qsort_ext()
      attr_id_htable = (Ll_str**)top_alloc(&topsize, HASHMEM);
      for (uii = 0; uii < HASHSIZE; uii++) {
	attr_id_htable[uii] = NULL;
      }
      while (1) {
	line_idx++;
	if (!gzgets(gz_attribfile, tbuf, MAXLINELEN)) {
	  if (!gzeof(gz_attribfile)) {
	    goto annotate_ret_READ_FAIL;
	  }
	  break;
	}
	if (!tbuf[MAXLINELEN - 1]) {
	  sprintf(logbuf, "Error: Line %" PRIuPTR " of %s is pathologically long.\n", line_idx, aip->attrib_fname);
	  goto annotate_ret_INVALID_FORMAT_WW;
	}
	bufptr = skip_initial_spaces(tbuf);
	if (is_eoln_kns(*bufptr)) {
	  continue;
	}
	bufptr2 = token_endnn(bufptr);
	slen = (uintptr_t)(bufptr2 - bufptr);
	bufptr2 = skip_initial_spaces(bufptr2);
	if (is_eoln_kns(*bufptr2) || (sorted_snplist && (bsearch_str(bufptr, slen, sorted_snplist, max_snplist_id_len, snplist_ct) == -1))) {
	  continue;
	}
	snplist_attr_ct++;
	if (slen >= max_snplist_attr_id_len) {
	  max_snplist_attr_id_len = slen + 1;
	}
	ujj = 0;
	do {
	  ujj++;
	  bufptr = token_endnn(bufptr2);
	  slen = (uintptr_t)(bufptr - bufptr2);
	  bufptr = skip_initial_spaces(bufptr);
	  bufptr2[slen] = '\0';
	  uii = hashval2(bufptr2, slen++);
	  ll_pptr = &(attr_id_htable[uii]);
	  while (1) {
	    ll_ptr = *ll_pptr;
            if (!ll_ptr) {
#ifdef __LP64__
	      // we'll run out of memory way earlier in 32-bit mode
	      if (attr_id_ct == 0x80000000LLU) {
	        sprintf(logbuf, "Error: Too many unique attributes in %s (max 2147483648).\n", aip->attrib_fname);
	        goto annotate_ret_INVALID_FORMAT_WW;
	      }
#endif
	      attr_id_ct++;
	      ll_ptr = top_alloc_llstr(&topsize, slen);
	      if (!ll_ptr) {
	        goto annotate_ret_NOMEM;
	      }
	      ll_ptr->next = NULL;
	      memcpy(ll_ptr->ss, bufptr2, slen);
	      if (slen > max_attr_id_len) {
	        max_attr_id_len = slen;
	      }
	      *ll_pptr = ll_ptr;
	      break;
	    }
	    if (!strcmp(ll_ptr->ss, bufptr2)) {
	      break;
	    }
	    ll_pptr = &(ll_ptr->next);
	  }
	  bufptr2 = bufptr;
	} while (!is_eoln_kns(*bufptr2));
	if (ujj > max_onevar_attr_ct) {
	  max_onevar_attr_ct = ujj;
	}
      }
      if (!attr_id_ct) {
	sprintf(logbuf, "Error: No attributes in %s.\n", aip->attrib_fname);
	goto annotate_ret_INVALID_FORMAT_WW;
      }
      if (max_onevar_attr_ct > attr_id_ct) {
	// pathological case: a line of the attribute file has the same
	// attribute repeated over and over again for some reason
	max_onevar_attr_ct = attr_id_ct;
      }
      wkspace_left -= topsize;
      if (wkspace_alloc_c_checked(&sorted_attr_ids, attr_id_ct * max_attr_id_len)) {
	goto annotate_ret_NOMEM2;
      }
      wkspace_left += topsize;
      ulii = 0;
      for (uii = 0; uii < HASHSIZE; uii++) {
	ll_ptr = attr_id_htable[uii];
	while (ll_ptr) {
	  strcpy(&(sorted_attr_ids[ulii * max_attr_id_len]), ll_ptr->ss);
	  ulii++;
	  ll_ptr = ll_ptr->next;
	}
      }
      qsort(sorted_attr_ids, attr_id_ct, max_attr_id_len, strcmp_natural);
      topsize = 0;
      gzrewind(gz_attribfile);
      attr_id_ctl = (attr_id_ct + (BITCT - 1)) / BITCT;
      if (wkspace_alloc_c_checked(&sorted_snplist_attr_ids, snplist_attr_ct * max_snplist_attr_id_len) ||
	  wkspace_alloc_ul_checked(&attr_bitfields, snplist_attr_ct * attr_id_ctl * sizeof(intptr_t))) {
	goto annotate_ret_NOMEM;
      }
      fill_ulong_zero(attr_bitfields, snplist_attr_ct * attr_id_ctl);
      for (ulii = 0; ulii < snplist_attr_ct; ulii++) {
      annotate_skip_line:
	if (!gzgets(gz_attribfile, tbuf, MAXLINELEN)) {
	  goto annotate_ret_READ_FAIL;
	}
	bufptr = skip_initial_spaces(tbuf);
	if (is_eoln_kns(*bufptr)) {
	  goto annotate_skip_line;
	}
	bufptr2 = token_endnn(bufptr);
	slen = (uintptr_t)(bufptr2 - bufptr);
	bufptr2 = skip_initial_spaces(bufptr2);
	if (is_eoln_kns(*bufptr2) || (sorted_snplist && (bsearch_str(bufptr, slen, sorted_snplist, max_snplist_id_len, snplist_ct) == -1))) {
	  goto annotate_skip_line;
	}
	memcpyx(&(sorted_snplist_attr_ids[ulii * max_snplist_attr_id_len]), bufptr, slen, '\0');
	ulptr = &(attr_bitfields[ulii * attr_id_ctl]);
	do {
	  bufptr = token_endnn(bufptr2);
	  slen = (uintptr_t)(bufptr - bufptr2);
	  bufptr = skip_initial_spaces(bufptr);
	  bufptr2[slen] = '\0';
	  sorted_idx = bsearch_str_natural(bufptr2, sorted_attr_ids, max_attr_id_len, attr_id_ct);
	  set_bit(ulptr, sorted_idx);
	  bufptr2 = bufptr;
	} while (!is_eoln_kns(*bufptr2));
      }
      gzclose(gz_attribfile);
      gz_attribfile = NULL;
      if (qsort_ext(sorted_snplist_attr_ids, snplist_attr_ct, max_snplist_attr_id_len, strcmp_deref, (char*)attr_bitfields, attr_id_ctl * sizeof(intptr_t))) {
	goto annotate_ret_NOMEM;
      }
      LOGPRINTFWW("--annotate attrib: %" PRIuPTR " variant ID%s and %" PRIuPTR " unique attribute%s loaded from %s.\n", snplist_attr_ct, (snplist_attr_ct == 1)? "" : "s", attr_id_ct, (attr_id_ct == 1)? "" : "s", aip->attrib_fname);
    }
  }
  if (need_pos) {
    if (aip->ranges_fname) {
      if (aip->subset_fname) {
	if (fopen_checked(&infile, aip->subset_fname, "rb")) {
	  goto annotate_ret_OPEN_FAIL;
	}
	retval = scan_token_ct_len(infile, tbuf, MAXLINELEN, &subset_ct, &max_subset_id_len);
	if (retval) {
	  if (retval == RET_INVALID_FORMAT) {
	    logerrprint("Error: Pathologically long token in --annotate subset file.\n");
	  }
	  goto annotate_ret_1;
	}
	if (!subset_ct) {
	  logerrprint("Error: --annotate subset file is empty.\n");
	  goto annotate_ret_INVALID_FORMAT;
	}
	if (max_subset_id_len > MAX_ID_LEN_P1) {
	  logerrprint("Error: --annotate subset IDs are limited to " MAX_ID_LEN_STR " characters.\n");
	  goto annotate_ret_INVALID_FORMAT;
	}
	sorted_subset_ids = (char*)top_alloc(&topsize, subset_ct * max_subset_id_len);
	if (!sorted_subset_ids) {
	  goto annotate_ret_NOMEM;
	}
	rewind(infile);
	retval = read_tokens(infile, tbuf, MAXLINELEN, subset_ct, max_subset_id_len, sorted_subset_ids);
	if (retval) {
	  goto annotate_ret_1;
	}
	if (fclose_null(&infile)) {
	  goto annotate_ret_READ_FAIL;
	}
	qsort(sorted_subset_ids, subset_ct, max_subset_id_len, strcmp_casted);
	subset_ct = collapse_duplicate_ids(sorted_subset_ids, subset_ct, max_subset_id_len, NULL);
      }
      // normally can't use border here because we need nearest distance
      retval = load_range_list_sortpos(aip->ranges_fname, (block01 && (!track_distance))? border : 0, subset_ct, sorted_subset_ids, max_subset_id_len, chrom_info_ptr, &range_ct, &range_names, &max_range_name_len, &chrom_bounds, &rangedefs, &chrom_max_range_ct, "--annotate ranges");
      if (retval) {
	goto annotate_ret_1;
      }
#ifdef __LP64__
      if (range_ct > 0x80000000LLU) {
	sprintf(logbuf, "Error: Too many annotations in %s (max 2147483648, counting multi-chromosome annotations once per spanned chromosome).\n", aip->ranges_fname);
	goto annotate_ret_INVALID_FORMAT_WW;
      }
#endif
      if (range_ct != 1) {
        LOGPRINTFWW("--annotate ranges: %" PRIuPTR " annotations loaded from %s (counting multi-chromosome annotations once per spanned chromosome).\n", range_ct, aip->ranges_fname);
      } else {
	LOGPRINTFWW("--annotate ranges: 1 annotation loaded from %s.\n", aip->ranges_fname);
      }
    }
    topsize = 0;
    if (aip->filter_fname) {
      retval = load_range_list_sortpos(aip->filter_fname, border, 0, NULL, 0, chrom_info_ptr, &filter_range_ct, &filter_range_names, &max_filter_range_name_len, &chrom_filter_bounds, &filter_rangedefs, &chrom_max_filter_range_ct, "--annotate filter");
      if (retval) {
	goto annotate_ret_1;
      }
    }
  }
  if (block01) {
    ulii = attr_id_ct + range_ct;
    if (range_names) {
      // need [range_names idx -> merged natural sort order] and
      // [attribute idx -> merged natural sort order] lookup tables
      if (ulii > 0x3fffffff) {
	logerrprint("Error: Too many unique annotations for '--annotate block' (max 1073741823).\n");
        goto annotate_ret_INVALID_FORMAT;
      }
      if (wkspace_alloc_ui_checked(&range_idx_lookup, ulii * sizeof(int32_t))) {
	goto annotate_ret_NOMEM;
      }
      if (attr_id_ct) {
	attr_id_remap = &(range_idx_lookup[range_ct]);
      }
      // create a master natural-sorted annotation ID list

      // this must persist until the output header line has been written
      merged_attr_idx_buf = (uint32_t*)top_alloc(&topsize, ulii * sizeof(int32_t));
      if (!merged_attr_idx_buf) {
	goto annotate_ret_NOMEM;
      }
      uii = MAXV((max_range_name_len - 4), max_attr_id_len);
      // this is larger and doesn't need to persist
      topsize_bak = topsize;
      merged_attr_ids = (char*)top_alloc(&topsize, ulii * uii);
      if (!merged_attr_ids) {
	goto annotate_ret_NOMEM;
      }
      uiptr = merged_attr_idx_buf;
      for (uljj = 0; uljj < range_ct; uljj++) {
	strcpy(&(merged_attr_ids[uljj * uii]), &(range_names[uljj * max_range_name_len + 4]));
	*uiptr++ = uljj;
      }
      if (attr_id_ct) {
	for (ujj = 0, uljj = range_ct; uljj < ulii; uljj++, ujj++) {
	  strcpy(&(merged_attr_ids[uljj * uii]), &(sorted_attr_ids[ujj * max_attr_id_len]));
	  *uiptr++ = ujj + 0x80000000U;
	}
      }
      wkspace_left -= topsize;
      if (qsort_ext(merged_attr_ids, ulii, uii, strcmp_natural_deref, (char*)merged_attr_idx_buf, sizeof(int32_t))) {
	goto annotate_ret_NOMEM2;
      }
      wkspace_left += topsize;
      topsize = topsize_bak;

      // similar to collapse_duplicate_ids(), except we need to save lookup
      // info
      // uljj = read idx
      uljj = 0;
      write_idx = 0;
      bufptr = merged_attr_ids; // write pointer
      ujj = merged_attr_idx_buf[0];
      while (1) {
	if (ujj < 0x80000000U) {
	  range_idx_lookup[ujj] = 2 * write_idx + 1;
	} else {
          attr_id_remap[ujj & 0x7fffffff] = 2 * write_idx + 1;
	}
	if (++uljj == ulii) {
	  break;
	}
	ujj = merged_attr_idx_buf[uljj];
        if (strcmp(bufptr, &(merged_attr_ids[uljj * uii]))) {
          bufptr = &(bufptr[uii]);
	  if (++write_idx != uljj) {
	    merged_attr_idx_buf[write_idx] = ujj;
	    strcpy(bufptr, &(merged_attr_ids[uljj * uii]));
	  }
	}
      }
      unique_annot_ct = write_idx + 1;
    } else {
      unique_annot_ct = attr_id_ct;
    }
#ifdef __LP64__
    unique_annot_ctlw = (unique_annot_ct + 3) / 4;
#else
    unique_annot_ctlw = (unique_annot_ct + 1) / 2;
#endif
    LOGPRINTF("--annotate block: %u unique annotation%s present.\n", unique_annot_ct, (unique_annot_ct == 1)? "" : "s");
    if (unique_annot_ct > 1000) {
      logerrprint("Warning: Output file may be very large.  Are you sure you want >1000 additional\ncolumns per line?  If not, restart without 'block'.\n");
    }
    wkspace_left -= topsize;
    if (wkspace_alloc_c_checked(&writebuf, unique_annot_ctlw * sizeof(intptr_t))) {
      goto annotate_ret_NOMEM2;
    }
    ulptr = (uintptr_t*)writebuf;
    for (ulii = 0; ulii < unique_annot_ctlw; ulii++) {
      // fill with repeated " 0"
#ifdef __LP64__
      *ulptr++ = 0x3020302030203020LLU;
#else
      *ulptr++ = 0x30203020;
#endif
    }
    wptr = &(writebuf[unique_annot_ct * 2]);
  } else {
    // worst case: max_onevar_attr_ct attributes and chrom_max_range_ct range
    // annotations
    if (wkspace_alloc_c_checked(&writebuf, (max_onevar_attr_ct * max_attr_id_len) + (chrom_max_range_ct * (max_range_name_len + (3 + 16 * (border != 0)) * range_dist)))) {
      goto annotate_ret_NOMEM;
    }
  }
  loadbuf = (char*)wkspace_base;
  loadbuf_size = wkspace_left;
  wkspace_left += topsize;
  if (loadbuf_size > MAXLINEBUFLEN) {
    loadbuf_size = MAXLINEBUFLEN;
  } else if (loadbuf_size <= MAXLINELEN) {
    goto annotate_ret_NOMEM;
  }
  // drop undocumented support for gzipped PLINK report input files, since it
  // came with conditional gzipping of output, and that's a pain to do right if
  // we want to support lines longer than 128K
  retval = open_and_load_to_first_token(&infile, aip->fname, loadbuf_size, '\0', "--annotate file", loadbuf, &bufptr, &line_idx);
  if (retval) {
    goto annotate_ret_1;
  }
  ujj = 0; // bitfield tracking which columns have already been found
  do {
    bufptr2 = token_endnn(bufptr);
    slen = (uintptr_t)(bufptr2 - bufptr);
    if (slen <= max_header_len) {
      if (need_pos && (slen == 3) && (!memcmp(bufptr, "CHR", 3))) {
	uii = 0;
      } else if (need_pos && (slen == 2) && (!memcmp(bufptr, "BP", 2))) {
	uii = 1;
      } else if (need_var_id && (slen == snp_field_len) && (!memcmp(bufptr, snp_field, snp_field_len))) {
        uii = 2;
      } else if ((slen == do_pfilter) && (*bufptr == 'P')) {
        uii = 3;
      } else {
	uii = 4;
      }
      if (uii != 4) {
        if ((ujj >> uii) & 1) {
	  *bufptr2 = '\0';
          sprintf(logbuf, "Error: Duplicate column header '%s' in %s.\n", bufptr, aip->fname);
          goto annotate_ret_INVALID_FORMAT_WW;
	}
	ujj |= 1 << uii;
        if (!seq_idx) {
	  col_skips[0] = col_idx;
	} else {
	  col_skips[seq_idx] = col_idx - col_skips[seq_idx - 1];
	}
	col_sequence[seq_idx++] = uii;
      }
    }
    bufptr = skip_initial_spaces(bufptr2);
    col_idx++;
  } while (!is_eoln_kns(*bufptr));
  if (seq_idx != token_ct) {
    sprintf(logbuf, "Error: Missing column header%s in %s.\n", (seq_idx + 1 == token_ct)? "" : "s", aip->fname);
    goto annotate_ret_INVALID_FORMAT_WW;
  }
  memcpy(outname_end, ".annot", 7);
  if (fopen_checked(&outfile, outname, "w")) {
    goto annotate_ret_OPEN_FAIL;
  }
  if (fwrite_checked(loadbuf, (uintptr_t)(bufptr - loadbuf), outfile)) {
    goto annotate_ret_WRITE_FAIL;
  }
  if (track_distance) {
    fputs("        DIST         SGN", outfile);
  }
  if (block01) {
    if (!range_ct) {
      for (ulii = 0; ulii < attr_id_ct; ulii++) {
	putc(' ', outfile);
	fputs(&(sorted_attr_ids[ulii * max_attr_id_len]), outfile);
      }
    } else {
      for (uii = 0; uii < unique_annot_ct; uii++) {
	putc(' ', outfile);
	ujj = merged_attr_idx_buf[uii];
	if (ujj < 0x80000000U) {
	  fputs(&(range_names[ujj * max_range_name_len + 4]), outfile);
	} else {
	  fputs(&(sorted_attr_ids[(ujj & 0x7fffffff) * max_attr_id_len]), outfile);
	}
      }
      loadbuf_size += topsize;
      topsize = 0;
      if (loadbuf_size > MAXLINEBUFLEN) {
	loadbuf_size = MAXLINEBUFLEN;
      }
      loadbuf[loadbuf_size - 1] = ' ';
    }
  } else {
    fputs(" ANNOT", outfile);
  }
  putc('\n', outfile);
  while (fgets(loadbuf, loadbuf_size, infile)) {
    line_idx++;
    if (!loadbuf[loadbuf_size - 1]) {
      if (loadbuf_size == MAXLINEBUFLEN) {
        sprintf(logbuf, "Error: Line %" PRIuPTR " of %s is pathologically long.\n", line_idx, aip->fname);
	goto annotate_ret_INVALID_FORMAT_WW;
      } else {
        goto annotate_ret_NOMEM;
      }
    }
    bufptr = skip_initial_spaces(loadbuf);
    if (is_eoln_kns(*bufptr)) {
      continue;
    }
    bufptr = next_token_multz(bufptr, col_skips[0]);
    token_ptrs[col_sequence[0]] = bufptr;
    for (seq_idx = 1; seq_idx < token_ct; seq_idx++) {
      bufptr = next_token_mult(bufptr, col_skips[seq_idx]);
      token_ptrs[col_sequence[seq_idx]] = bufptr;
    }
    if (!bufptr) {
      continue;
    }
    if (need_pos) {
      // CHR
      chrom_idx = get_chrom_code(chrom_info_ptr, token_ptrs[0]);
      if (chrom_idx < 0) {
        continue;
      }

      // BP
      if (scan_uint_defcap(token_ptrs[1], &cur_bp)) {
	continue;
      }

      if (chrom_filter_bounds) {
	ulii = chrom_filter_bounds[((uint32_t)chrom_idx) + 1];
	for (range_idx = chrom_filter_bounds[(uint32_t)chrom_idx]; range_idx < ulii; range_idx++) {
          if (in_setdef(filter_rangedefs[range_idx], cur_bp)) {
	    break;
	  }
	}
	if (range_idx == ulii) {
	  continue;
	}
      }
    }

    if (sorted_snplist) {
      if (bsearch_str(token_ptrs[2], strlen_se(token_ptrs[2]), sorted_snplist, max_snplist_id_len, snplist_ct) == -1) {
	continue;
      }
    }

    // P
    if (do_pfilter) {
      if (scan_double(token_ptrs[3], &pval) || (!(pval <= pfilter))) {
	continue;
      }
    }

    abs_min_dist = 0x80000000U;
    if (!block01) {
      wptr = writebuf;
      if (chrom_bounds) {
	ulii = chrom_bounds[((uint32_t)chrom_idx) + 1];
	if (!border) {
	  for (range_idx = chrom_bounds[(uint32_t)chrom_idx]; range_idx < ulii; range_idx++) {
	    if (in_setdef(rangedefs[range_idx], cur_bp)) {
	      wptr = strcpya(wptr, &(range_names[range_idx * max_range_name_len + 4]));
	      if (range_dist) {
		wptr = memcpya(wptr, "(0)|", 4);
	      } else {
		*wptr++ = '|';
	      }
	    }
	  }
	  if (wptr != writebuf) {
	    abs_min_dist = 0;
	  }
	} else {
	  for (range_idx = chrom_bounds[(uint32_t)chrom_idx]; range_idx < ulii; range_idx++) {
	    if (in_setdef_dist(rangedefs[range_idx], cur_bp, border, &ii)) {
	      ujj = abs(ii);
	      if (ujj < abs_min_dist) {
		abs_min_dist = ujj;
		min_dist = ii;
	      }
	      wptr = strcpya(wptr, &(range_names[range_idx * max_range_name_len + 4]));
	      if (range_dist) {
		if (ii == 0) {
		  wptr = memcpya(wptr, "(0)|", 4);
		} else {
		  *wptr++ = '(';
		  if (ii > 0) {
		    *wptr++ = '+';
		  }
		  wptr = double_g_writewx4(wptr, ((double)ii) * 0.001, 1);
		  wptr = memcpya(wptr, "kb)|", 4);
		}
	      } else {
		*wptr++ = '|';
	      }
	    }
	  }
	}
      }
      if (sorted_snplist_attr_ids) {
	bufptr2 = token_ptrs[2];
	slen = (uintptr_t)(token_endnn(bufptr2) - bufptr2);
	sorted_idx = bsearch_str(bufptr2, slen, sorted_snplist_attr_ids, max_snplist_attr_id_len, snplist_attr_ct);
	if (sorted_idx != -1) {
	  ulptr = &(attr_bitfields[((uint32_t)sorted_idx) * attr_id_ctl]);
	  for (uii = 0; uii < attr_id_ct; uii += BITCT) {
	    ulii = *ulptr++;
	    while (ulii) {
	      ujj = CTZLU(ulii);
	      wptr = strcpyax(wptr, &(sorted_attr_ids[(uii + ujj) * max_attr_id_len]), '|');
	      ulii &= ulii - 1;
	    }
	  }
	}
      }
      at_least_one_annot = (wptr != writebuf);
      if (at_least_one_annot) {
	wptr--;
	annot_row_ct++;
      } else {
	if (prune) {
	  continue;
	}
	wptr = strcpya(wptr, no_annot_str);
      }
    } else {
      at_least_one_annot = 0;
      if (chrom_bounds) {
	ulii = chrom_bounds[((uint32_t)chrom_idx) + 1];
	if (!border) {
	  for (range_idx = chrom_bounds[(uint32_t)chrom_idx]; range_idx < ulii; range_idx++) {
	    if (in_setdef(rangedefs[range_idx], cur_bp)) {
	      writebuf[range_idx_lookup[range_idx]] = '1';
	      at_least_one_annot = 1;
	    }
	  }
	  if (at_least_one_annot) {
	    abs_min_dist = 0;
	  }
	} else if (!track_distance) {
	  for (range_idx = chrom_bounds[(uint32_t)chrom_idx]; range_idx < ulii; range_idx++) {
	    if (in_setdef(rangedefs[range_idx], cur_bp)) {
	      writebuf[range_idx_lookup[range_idx]] = '1';
	      at_least_one_annot = 1;
	    }
	  }
	} else {
	  for (range_idx = chrom_bounds[(uint32_t)chrom_idx]; range_idx < ulii; range_idx++) {
	    if (in_setdef_dist(rangedefs[range_idx], cur_bp, border, &ii)) {
	      ujj = abs(ii);
	      if (ujj < abs_min_dist) {
		abs_min_dist = ujj;
		min_dist = ii;
	      }
	      writebuf[range_idx_lookup[range_idx]] = '1';
	      at_least_one_annot = 1;
	    }
	  }
	}
      }
      if (sorted_snplist_attr_ids) {
	bufptr2 = token_ptrs[2];
	slen = (uintptr_t)(token_endnn(bufptr2) - bufptr2);
	sorted_idx = bsearch_str(bufptr2, slen, sorted_snplist_attr_ids, max_snplist_attr_id_len, snplist_attr_ct);
	if (sorted_idx != -1) {
	  ulptr = &(attr_bitfields[((uint32_t)sorted_idx) * attr_id_ctl]);
	  for (uii = 0; uii < attr_id_ct; uii += BITCT) {
	    ulii = *ulptr++;
	    if (ulii) {
	      do {
		ujj = CTZLU(ulii);
		writebuf[attr_id_remap[uii + ujj]] = '1';
		ulii &= ulii - 1;
	      } while (ulii);
	      at_least_one_annot = 1;
	    }
	  }
	}
      }

      if (at_least_one_annot) {
	annot_row_ct++;
      } else if (prune) {
	continue;
      }
    }
    bufptr = strchr(bufptr, '\0');
    if (bufptr[-1] == '\n') {
      bufptr--;
      if (bufptr[-1] == '\r') {
	bufptr--;
      }
    }
    total_row_ct++;
    if (fwrite_checked(loadbuf, bufptr - loadbuf, outfile)) {
      goto annotate_ret_WRITE_FAIL;
    }
    if (track_distance) {
      if (abs_min_dist != 0x80000000U) {
	bufptr2 = width_force(12, tbuf, double_g_write(tbuf, ((double)((int32_t)abs_min_dist)) * 0.001));
	if (!abs_min_dist) {
          bufptr2 = memcpya(bufptr2, no_sign_str, 4);
	} else {
          bufptr2 = memcpyl3a(bufptr2, "   ");
          if (min_dist > 0) {
	    *bufptr2++ = '+';
	  } else {
	    *bufptr2++ = '-';
	  }
	}
      } else {
	bufptr2 = memseta(tbuf, 32, 8);
	bufptr2 = memcpya(bufptr2, no_sign_str, 4);
	bufptr2 = memcpya(bufptr2, no_sign_str, 4);
      }
      if (fwrite_checked(tbuf, bufptr2 - tbuf, outfile)) {
	goto annotate_ret_WRITE_FAIL;
      }
    }
    putc(' ', outfile);
    if (fwrite_checked(writebuf, wptr - writebuf, outfile)) {
      goto annotate_ret_WRITE_FAIL;
    }

    putc('\n', outfile);
    if (block01 && at_least_one_annot) {
      // reinitialize
      ulptr = (uintptr_t*)writebuf;
      for (ulii = 0; ulii < unique_annot_ctlw; ulii++) {
#ifdef __LP64__
	*ulptr++ = 0x3020302030203020LLU;
#else
	*ulptr++ = 0x30203020;
#endif
      }
    }
  }
  if (fclose_null(&infile)) {
    goto annotate_ret_READ_FAIL;
  }
  if (fclose_null(&outfile)) {
    goto annotate_ret_WRITE_FAIL;
  }
  if (!prune) {
    LOGPRINTFWW("--annotate: %" PRIuPTR " out of %" PRIuPTR " row%s annotated; new report written to %s .\n", annot_row_ct, total_row_ct, (total_row_ct == 1)? "" : "s", outname);
  } else {
    LOGPRINTFWW("--annotate: %" PRIuPTR " row%s annotated; new report written to %s .\n", total_row_ct, (total_row_ct == 1)? "" : "s", outname);
  }
  while (0) {
  annotate_ret_NOMEM2:
    wkspace_left += topsize;
  annotate_ret_NOMEM:
    retval = RET_NOMEM;
    break;
  annotate_ret_OPEN_FAIL:
    retval = RET_OPEN_FAIL;
    break;
  annotate_ret_READ_FAIL:
    retval = RET_READ_FAIL;
    break;
  annotate_ret_WRITE_FAIL:
    retval = RET_WRITE_FAIL;
    break;
  annotate_ret_INVALID_FORMAT_WW:
    wordwrap(logbuf, 0);
    logerrprintb();
  annotate_ret_INVALID_FORMAT:
    retval = RET_INVALID_FORMAT;
    break;
  }
 annotate_ret_1:
  wkspace_reset(wkspace_mark);
  fclose_cond(infile);
  gzclose_cond(gz_attribfile);
  fclose_cond(outfile);
  return retval;
}

int32_t gene_report(char* fname, char* glist, char* subset_fname, uint32_t border, char* extractname, const char* snp_field, char* outname, char* outname_end, double pfilter, Chrom_info* chrom_info_ptr) {
  // similar to define_sets() and --clump
  unsigned char* wkspace_mark = wkspace_base;
  FILE* infile = NULL;
  FILE* outfile = NULL;
  uintptr_t topsize = 0;
  uintptr_t subset_ct = 0;
  uintptr_t max_subset_id_len = 0;
  uintptr_t extract_ct = 0;
  uintptr_t max_extract_id_len = 0;
  const char constsnpstr[] = "SNP";
  char* sorted_subset_ids = NULL;
  char* sorted_extract_ids = NULL;
  uintptr_t* chrom_bounds = NULL;
  uint32_t** genedefs = NULL;
  uint64_t saved_line_ct = 0;
  uint32_t do_pfilter = (pfilter != 2.0);
  uint32_t token_ct = 2 + (extractname != NULL) + do_pfilter;
  uint32_t snp_field_len = 0;
  uint32_t col_idx = 0;
  uint32_t seq_idx = 0;
  uint32_t cur_bp = 0;
  uint32_t found_header_bitfield = 0;
  int32_t retval = 0;

  // see --annotate comment on col_skips.
  char* token_ptrs[4];
  uint32_t col_skips[4];
  uint32_t col_sequence[4];
  uint64_t* gene_match_list_end;
  uint64_t* gene_match_list;
  uint64_t* gene_match_list_ptr;
  uint32_t* gene_chridx_to_nameidx;
  uint32_t* gene_nameidx_to_chridx;
  uint32_t* uiptr;
  char** line_lookup;
  char* gene_names;
  char* loadbuf;
  char* header_ptr;
  char* first_line_ptr;
  char* linebuf_top;
  char* bufptr;
  char* bufptr2;
  uintptr_t gene_ct;
  uintptr_t max_gene_name_len;
  uintptr_t chrom_max_gene_ct;
  uintptr_t loadbuf_size;
  uintptr_t gene_idx;
  uintptr_t linebuf_left;
  uintptr_t line_idx;
  uintptr_t ulii;
  uint64_t ullii;
  double pval;
  uint32_t slen;
  uint32_t header_len;
  uint32_t range_idx;
  uint32_t range_ct;
  uint32_t uii;
  uint32_t ujj;
  int32_t chrom_idx;
  if (subset_fname) {
    if (fopen_checked(&infile, subset_fname, "rb")) {
      goto gene_report_ret_OPEN_FAIL;
    }
    retval = scan_token_ct_len(infile, tbuf, MAXLINELEN, &subset_ct, &max_subset_id_len);
    if (retval) {
      if (retval == RET_INVALID_FORMAT) {
	logerrprint("Error: Pathologically long token in --gene-subset file.\n");
      }
      goto gene_report_ret_1;
    }
    if (!subset_ct) {
      logerrprint("Error: --gene-subset file is empty.\n");
      goto gene_report_ret_INVALID_FORMAT;
    }
    if (max_subset_id_len > MAX_ID_LEN_P1) {
      logerrprint("Error: --gene-subset IDs are limited to " MAX_ID_LEN_STR " characters.\n");
      goto gene_report_ret_INVALID_FORMAT;
    }
    sorted_subset_ids = (char*)top_alloc(&topsize, subset_ct * max_subset_id_len);
    if (!sorted_subset_ids) {
      goto gene_report_ret_NOMEM;
    }
    rewind(infile);
    retval = read_tokens(infile, tbuf, MAXLINELEN, subset_ct, max_subset_id_len, sorted_subset_ids);
    if (retval) {
      goto gene_report_ret_1;
    }
    if (fclose_null(&infile)) {
      goto gene_report_ret_READ_FAIL;
    }
    qsort(sorted_subset_ids, subset_ct, max_subset_id_len, strcmp_casted);
    subset_ct = collapse_duplicate_ids(sorted_subset_ids, subset_ct, max_subset_id_len, NULL);
  }
  if (extractname) {
    if (fopen_checked(&infile, extractname, "rb")) {
      goto gene_report_ret_OPEN_FAIL;
    }
    retval = scan_token_ct_len(infile, tbuf, MAXLINELEN, &extract_ct, &max_extract_id_len);
    if (retval) {
      goto gene_report_ret_1;
    }
    if (!extract_ct) {
      logerrprint("Error: Empty --extract file.\n");
      goto gene_report_ret_INVALID_FORMAT;
    }
    if (max_extract_id_len > MAX_ID_LEN_P1) {
      logerrprint("Error: --extract IDs are limited to " MAX_ID_LEN_STR " characters.\n");
      goto gene_report_ret_INVALID_FORMAT;
    }
    wkspace_left -= topsize;
    if (wkspace_alloc_c_checked(&sorted_extract_ids, extract_ct * max_extract_id_len)) {
      goto gene_report_ret_NOMEM2;
    }
    wkspace_left += topsize;
    rewind(infile);
    // todo: switch to hash table to avoid sort
    retval = read_tokens(infile, tbuf, MAXLINELEN, extract_ct, max_extract_id_len, sorted_extract_ids);
    if (retval) {
      goto gene_report_ret_1;
    }
    if (fclose_null(&infile)) {
      goto gene_report_ret_READ_FAIL;
    }
    qsort(sorted_extract_ids, extract_ct, max_extract_id_len, strcmp_casted);
    ulii = collapse_duplicate_ids(sorted_extract_ids, extract_ct, max_extract_id_len, NULL);
    if (ulii < extract_ct) {
      extract_ct = ulii;
      wkspace_shrink_top(sorted_extract_ids, extract_ct * max_extract_id_len);
    }
  }
  retval = load_range_list_sortpos(glist, 0, subset_ct, sorted_subset_ids, max_subset_id_len, chrom_info_ptr, &gene_ct, &gene_names, &max_gene_name_len, &chrom_bounds, &genedefs, &chrom_max_gene_ct, "--gene-report");
  wkspace_left += topsize;
  if (retval) {
    goto gene_report_ret_1;
  }
#ifdef __LP64__
  if (gene_ct > 0x80000000LLU) {
    sprintf(logbuf, "Error: Too many genes in %s (max 2147483648).\n", glist);
    goto gene_report_ret_INVALID_FORMAT_WW;
  }
#endif

  topsize = 0;
  // gene_names is sorted primarily by chromosome index, and secondarily by
  // gene name.  Final output will be the other way around, so we need a
  // remapping table.
  // This logic needs to change a bit if support for unplaced contigs is added
  // or MAX_CHROM_TEXTNUM_LEN changes.
  if (wkspace_alloc_ui_checked(&gene_chridx_to_nameidx, gene_ct * sizeof(int32_t)) ||
      wkspace_alloc_ui_checked(&gene_nameidx_to_chridx, gene_ct * sizeof(int32_t)) ||
      wkspace_alloc_c_checked(&loadbuf, gene_ct * max_gene_name_len)) {
    goto gene_report_ret_NOMEM;
  }
  for (gene_idx = 0; gene_idx < gene_ct; gene_idx++) {
    gene_nameidx_to_chridx[gene_idx] = gene_idx;
  }
  for (gene_idx = 0; gene_idx < gene_ct; gene_idx++) {
    bufptr = &(gene_names[gene_idx * max_gene_name_len]);
    slen = strlen(&(bufptr[4]));
    bufptr2 = memcpyax(&(loadbuf[gene_idx * max_gene_name_len]), &(bufptr[4]), slen, ' ');
    memcpyx(bufptr2, &(bufptr[2]), 2, '\0');
  }
  if (qsort_ext(loadbuf, gene_ct, max_gene_name_len, strcmp_natural_deref, (char*)gene_nameidx_to_chridx, sizeof(int32_t))) {
    goto gene_report_ret_NOMEM;
  }
  // We also need to perform the reverse lookup.
  for (gene_idx = 0; gene_idx < gene_ct; gene_idx++) {
    gene_chridx_to_nameidx[gene_nameidx_to_chridx[gene_idx]] = gene_idx;
  }
  wkspace_reset((unsigned char*)loadbuf);

  if (wkspace_left < MAXLINELEN + 64) {
    goto gene_report_ret_NOMEM;
  }
  // mirror wkspace_base/wkspace_base since we'll be doing nonstandard-size
  // allocations
  linebuf_top = (char*)wkspace_base;
  linebuf_left = wkspace_left;
  gene_match_list_end = (uint64_t*)(&(wkspace_base[wkspace_left]));
  gene_match_list = gene_match_list_end;

  header_ptr = linebuf_top;
  loadbuf = memcpya(header_ptr, "kb ) ", 5);
  if (border) {
    loadbuf = memcpya(loadbuf, " plus ", 6);
    loadbuf = double_g_write(loadbuf, ((double)((int32_t)border)) * 0.001);
    loadbuf = memcpya(loadbuf, "kb border ", 10);
  }
  loadbuf = memcpya(loadbuf, "\n\n        DIST ", 15);
  linebuf_left -= (loadbuf - header_ptr);
  loadbuf_size = linebuf_left;
  if (loadbuf_size > MAXLINEBUFLEN) {
    loadbuf_size = MAXLINEBUFLEN;
  }
  uii = 3; // maximum relevant header length
  col_idx = 0;
  if (extractname) {
    if (snp_field) {
      snp_field_len = strlen(snp_field);
      if (snp_field_len > uii) {
	uii = snp_field_len;
      }
    } else {
      snp_field = constsnpstr;
      snp_field_len = 3;
    }
  }
  retval = open_and_load_to_first_token(&infile, fname, loadbuf_size, '\0', "--gene-report file", loadbuf, &bufptr, &line_idx);
  if (retval) {
    goto gene_report_ret_1;
  }
  do {
    bufptr2 = token_endnn(bufptr);
    slen = (uintptr_t)(bufptr2 - bufptr);
    if (slen <= uii) {
      if ((slen == 3) && (!memcmp(bufptr, "CHR", 3))) {
	ujj = 0;
      } else if ((slen == 2) && (!memcmp(bufptr, "BP", 2))) {
	ujj = 1;
      } else if ((slen == snp_field_len) && (!memcmp(bufptr, snp_field, snp_field_len))) {
	ujj = 2;
      } else if ((slen == do_pfilter) && (*bufptr == 'P')) {
	ujj = 3;
      } else {
	ujj = 4;
      }
      if (ujj != 4) {
	if ((found_header_bitfield >> ujj) & 1) {
	  *bufptr2 = '\0';
	  sprintf(logbuf, "Error: Duplicate column header '%s' in %s.\n", bufptr, fname);
	  goto gene_report_ret_INVALID_FORMAT_WW;
	}
	found_header_bitfield |= 1 << ujj;
	if (!seq_idx) {
	  col_skips[0] = col_idx;
	} else {
	  col_skips[seq_idx] = col_idx - col_skips[seq_idx - 1];
	}
	col_sequence[seq_idx++] = ujj;
      }
    }
    bufptr = skip_initial_spaces(bufptr2);
    col_idx++;
  } while (!is_eoln_kns(*bufptr));
  if (seq_idx != token_ct) {
    sprintf(logbuf, "Error: Missing column header%s in %s.\n", (seq_idx + 1 == token_ct)? "" : "s", fname);
    goto gene_report_ret_INVALID_FORMAT_WW;
  }
  // assume *bufptr is now \n (if it isn't, header line is never written to
  // output anyway)
  header_len = 1 + (uintptr_t)(bufptr - header_ptr);
  linebuf_left -= header_len;
  linebuf_top = &(linebuf_top[header_len]);
  first_line_ptr = linebuf_top;
  while (1) {
    // store raw line contents at bottom of stack (growing upwards), and gene
    // matches at top of stack (8 byte entries, growing downwards).  Worst
    // case, a line matches chrom_max_gene_ct genes, so we prevent the text
    // loading buffer from using the last chrom_max_gene_ct * 8 bytes of
    // workspace memory.
    if (linebuf_left <= chrom_max_gene_ct * 8 + 8 + (uintptr_t)(((char*)gene_match_list_end) - ((char*)gene_match_list)) + MAXLINELEN) {
      goto gene_report_ret_NOMEM;
    }
    loadbuf_size = linebuf_left - (chrom_max_gene_ct * 8) - 8 - (uintptr_t)(((char*)gene_match_list_end) - ((char*)gene_match_list));
    if (loadbuf_size > MAXLINEBUFLEN) {
      loadbuf_size = MAXLINEBUFLEN;
    }
    // leave 8 bytes to save line length and parsed position
    loadbuf = &(linebuf_top[8]);
    loadbuf[loadbuf_size - 1] = ' ';
  gene_report_load_loop:
    if (!fgets(loadbuf, loadbuf_size, infile)) {
      break;
    }
    line_idx++;
    if (!loadbuf[loadbuf_size - 1]) {
      goto gene_report_ret_LONG_LINE;
    }
    bufptr = skip_initial_spaces(loadbuf);
    if (is_eoln_kns(*bufptr)) {
      goto gene_report_load_loop;
    }
    bufptr = next_token_multz(bufptr, col_skips[0]);
    token_ptrs[col_sequence[0]] = bufptr;
    for (seq_idx = 1; seq_idx < token_ct; seq_idx++) {
      bufptr = next_token_mult(bufptr, col_skips[seq_idx]);
      token_ptrs[col_sequence[seq_idx]] = bufptr;
    }
    if (!bufptr) {
      goto gene_report_load_loop;
    }
    // CHR
    chrom_idx = get_chrom_code(chrom_info_ptr, token_ptrs[0]);
    if (chrom_idx < 0) {
      // todo: log warning?
      goto gene_report_load_loop;
    }

    // BP
    if (scan_uint_defcap(token_ptrs[1], &cur_bp)) {
      goto gene_report_load_loop;
    }

    // variant ID
    if (sorted_extract_ids) {
      bufptr2 = token_ptrs[2];
      if (bsearch_str(bufptr2, strlen_se(bufptr2), sorted_extract_ids, max_extract_id_len, extract_ct) == -1) {
	goto gene_report_load_loop;
      }
    }

    // P
    if (do_pfilter) {
      if (scan_double(token_ptrs[3], &pval) || (!(pval <= pfilter))) {
        goto gene_report_load_loop;
      }
    }

    ulii = chrom_bounds[((uint32_t)chrom_idx) + 1];
    gene_match_list_ptr = gene_match_list;
    if (cur_bp > border) {
      uii = cur_bp - border;
    } else {
      uii = 0;
    }
    ujj = cur_bp + border;
    for (gene_idx = chrom_bounds[(uint32_t)chrom_idx]; gene_idx < ulii; gene_idx++) {
      if (interval_in_setdef(genedefs[gene_idx], uii, ujj)) {
	*(--gene_match_list) = (((uint64_t)gene_chridx_to_nameidx[gene_idx]) << 32) | saved_line_ct;
      }
    }
    if (gene_match_list_ptr == gene_match_list) {
      goto gene_report_load_loop;
    }

    slen = strlen(bufptr);
    // this could be a non-\n-terminated last line...
    if (bufptr[slen - 1] != '\n') {
      bufptr[slen++] = '\n';
    }
    slen += (uintptr_t)(bufptr - loadbuf);
    *((uint32_t*)linebuf_top) = slen;
    ((uint32_t*)linebuf_top)[1] = cur_bp;
    linebuf_left -= slen + 8;
    linebuf_top = &(linebuf_top[slen + 8]);
#ifdef __LP64__
    if (saved_line_ct == 0x100000000LLU) {
      sprintf(logbuf, "Error: Too many valid lines in %s (--gene-report can only handle 4294967296).\n", fname);
      goto gene_report_ret_INVALID_FORMAT_WW;
    }
#endif
    saved_line_ct++;
  }
  if (fclose_null(&infile)) {
    goto gene_report_ret_READ_FAIL;
  }
  // saved line index -> line contents lookup
  // allocate off far end since linebuf_top is not aligned at all
  if (linebuf_left < saved_line_ct * sizeof(intptr_t) + (uintptr_t)(((char*)gene_match_list_end) - ((char*)gene_match_list))) {
    goto gene_report_ret_NOMEM;
  }
  line_lookup = (char**)(&(((uintptr_t*)gene_match_list)[-((intptr_t)((uintptr_t)saved_line_ct))]));
  bufptr = first_line_ptr;
  for (uii = 0; uii < saved_line_ct; uii++) {
    line_lookup[uii] = bufptr;
    bufptr = &(bufptr[(*((uint32_t*)bufptr)) + 8]);
  }
#ifdef __cplusplus
  std::sort((int64_t*)gene_match_list, (int64_t*)gene_match_list_end);
#else
  qsort((int64_t*)gene_match_list, (uintptr_t)(gene_match_list_end - gene_match_list), sizeof(int64_t), llcmp);
#endif

  memcpy(outname_end, ".range.report", 14);
  if (fopen_checked(&outfile, outname, "w")) {
    goto gene_report_ret_OPEN_FAIL;
  }
  ulii = ~ZEROLU; // current gene index
  while (gene_match_list < gene_match_list_end) {
    ullii = *gene_match_list++;
    if ((uintptr_t)(ullii >> 32) != ulii) {
      // new gene
      if (ulii != ~ZEROLU) {
        fputs("\n\n", outfile);
      }
      ulii = (uintptr_t)(ullii >> 32);
      gene_idx = gene_nameidx_to_chridx[ulii];
      bufptr = &(gene_names[gene_idx * max_gene_name_len]);
      fputs(&(bufptr[4]), outfile);
      fputs(" -- chr", outfile);
      if (bufptr[2] != '0') {
	putc(bufptr[2], outfile);
      }
      putc(bufptr[3] + 15, outfile);
      putc(':', outfile);
      uiptr = genedefs[gene_idx];
      range_ct = *uiptr++;
      ujj = 0; // gene length
      for (range_idx = 0; range_idx < range_ct; range_idx++) {
	if (range_idx) {
	  fputs(", ", outfile);
	}
        cur_bp = *uiptr++;
        bufptr = uint32_write(tbuf, cur_bp);
	bufptr = memcpya(bufptr, "..", 2);
        uii = *uiptr++;
        bufptr = uint32_write(bufptr, uii - 1);
        fwrite(tbuf, 1, bufptr - tbuf, outfile);
	ujj += uii - cur_bp;
      }
      bufptr = memcpyl3a(tbuf, " ( ");
      bufptr = double_g_write(bufptr, ((double)((int32_t)ujj)) * 0.001);
      fwrite(tbuf, 1, bufptr - tbuf, outfile);
      if (fwrite_checked(header_ptr, header_len, outfile)) {
	goto gene_report_ret_WRITE_FAIL;
      }
      cur_bp = genedefs[gene_idx][1];
    }
    bufptr = line_lookup[(uint32_t)ullii];
    uii = *((uint32_t*)bufptr); // line length
    ujj = ((uint32_t*)bufptr)[1]; // bp
    bufptr2 = double_g_writewx4(tbuf, ((double)((int32_t)(ujj - cur_bp))) * 0.001, 10);
    bufptr2 = memcpyl3a(bufptr2, "kb ");
    fwrite(tbuf, 1, bufptr2 - tbuf, outfile);
    fwrite(&(bufptr[8]), 1, uii, outfile);
  }
  if (ulii != ~ZEROLU) {
    fputs("\n\n", outfile);
  }
  if (fclose_null(&outfile)) {
    goto gene_report_ret_WRITE_FAIL;
  }
  LOGPRINTFWW("--gene-report: gene-based report written to %s .\n", outname);
  while (0) {
  gene_report_ret_LONG_LINE:
    if (loadbuf_size == MAXLINEBUFLEN) {
      LOGERRPRINTFWW("Error: Line %" PRIuPTR " of %s is pathologically long.\n", line_idx, fname);
      retval = RET_INVALID_FORMAT;
      break;
    }
  gene_report_ret_NOMEM2:
    wkspace_left += topsize;
  gene_report_ret_NOMEM:
    retval = RET_NOMEM;
    break;
  gene_report_ret_OPEN_FAIL:
    retval = RET_OPEN_FAIL;
    break;
  gene_report_ret_READ_FAIL:
    retval = RET_READ_FAIL;
    break;
  gene_report_ret_WRITE_FAIL:
    retval = RET_WRITE_FAIL;
    break;
  gene_report_ret_INVALID_FORMAT_WW:
    wordwrap(logbuf, 0);
    logerrprintb();
  gene_report_ret_INVALID_FORMAT:
    retval = RET_INVALID_FORMAT;
    break;
  }
 gene_report_ret_1:
  wkspace_reset(wkspace_mark);
  fclose_cond(infile);
  fclose_cond(outfile);
  return retval;
}
