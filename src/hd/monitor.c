#include <stdio.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"
#include "monitor.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * monitor info
 *
 * Read the info out of the 'SuSE=' entry in /proc/cmdline. It contains
 * (among others) info from the EDID record got by our syslinux extension.
 *
 * We will try to look up our monitor id in the id file to get additional
 * info.
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#ifdef __PPC__
static void add_monitor(hd_data_t *hd_data, devtree_t *dt);
#endif
static void add_monitor_res(hd_t *hd, unsigned x, unsigned y, unsigned hz);

#if !defined(__PPC__)
void hd_scan_monitor(hd_data_t *hd_data)
{
  hd_t *hd;
  int i, j, k;
  char *s, *s0, *s1, *se, m[8], *t;
  unsigned u;
  hd_res_t *res;
  monitor_info_t *mi = NULL;

  if(!hd_probe_feature(hd_data, pr_monitor)) return;

  hd_data->module = mod_monitor;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "ddc");

  if(!(s = s0 = t = get_cmd_param(hd_data, 0))) return;

  s = strsep(&t, "^");

  se = s + strlen(s);

  if(se - s < 7 + 2 * 4) {
    free_mem(s0);
    return;
  }

  /* Ok, we've got it. Now we split the fields. */

  memcpy(m, s, 7); m[7] = 0; s += 7;

  hd = add_hd_entry(hd_data, __LINE__, 0);

  hd->base_class = bc_monitor;
  hd->vend = name2eisa_id(m);
  if(sscanf(m + 3, "%x", &u) == 1) hd->dev = MAKE_ID(TAG_EISA, u);
  if((u = device_class(hd_data, hd->vend, hd->dev))) {
    if((u >> 8) == bc_monitor) hd->sub_class = u & 0xff;
  }

  i = hex(s, 2); j = hex(s + 2, 2); s += 4;
  if(i > 0 && j > 0) {
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->size.type = res_size;
    res->size.unit = size_unit_cm;
    res->size.val1 = i;		/* width */
    res->size.val2 = j;		/* height */
  }

  i = hex(s, 2); s+= 2;
  if(i & (1 << 0)) add_monitor_res(hd, 720, 400, 70);
  if(i & (1 << 1)) add_monitor_res(hd, 720, 400, 88);
  if(i & (1 << 2)) add_monitor_res(hd, 640, 480, 60);
  if(i & (1 << 3)) add_monitor_res(hd, 640, 480, 67);
  if(i & (1 << 4)) add_monitor_res(hd, 640, 480, 72);
  if(i & (1 << 5)) add_monitor_res(hd, 640, 480, 75);
  if(i & (1 << 6)) add_monitor_res(hd, 800, 600, 56);
  if(i & (1 << 7)) add_monitor_res(hd, 800, 600, 60);

  i = hex(s, 2); s+= 2;
  if(i & (1 << 0)) add_monitor_res(hd,  800,  600, 72);
  if(i & (1 << 1)) add_monitor_res(hd,  800,  600, 75);
  if(i & (1 << 2)) add_monitor_res(hd,  832,  624, 75);

/*
  this would be an interlaced timing; dropping it
  if(i & (1 << 3)) add_monitor_res(hd, 1024,  768, 87);
*/

  if(i & (1 << 4)) add_monitor_res(hd, 1024,  768, 60);
  if(i & (1 << 5)) add_monitor_res(hd, 1024,  768, 70);
  if(i & (1 << 6)) add_monitor_res(hd, 1024,  768, 75);
  if(i & (1 << 7)) add_monitor_res(hd, 1280, 1024, 75);

  if(((se - s) & 1) || se - s > 8 * 4 + 2) {
    ADD2LOG("  ddc oops: %d bytes left?\n", (int) (se - s));
    free_mem(s0);
    return;
  }

  while(s + 4 <= se) {
    i = (hex(s, 2) + 31) * 8; j = hex(s + 2, 2); s += 4;
    k = 0;
    switch((j >> 6) & 3) {
      case 1: k = (i * 3) / 4; break;
      case 2: k = (i * 4) / 5; break;
      case 3: k = (i * 9) / 16; break;
    }
    if(k) add_monitor_res(hd, i, k, (j & 0x3f) + 60);
  }

  u = 0;
  if(se - s == 2) u = hex(s, 2) + 1990;

  if(u || t) {
    mi = new_mem(sizeof *mi);
    if(u) mi->manu_year = u;
    while((s = strsep(&t, "^"))) {
      for(s1 = s; *s1++; ) if(*s1 == '_') *s1 = ' ';
      switch(*s) {
        case '0':
          if(!mi->name && s[1]) mi->name = canon_str(s + 1, strlen(s + 1));
          break;
        case '1':
          u = 0;
          if(strlen(s) == 9) {
            i = hex(s + 1, 2);
            j = hex(s + 3, 2);
            if(i > j || !i) u = 1;
            mi->min_vsync = i;
            mi->max_vsync = j;
            i = hex(s + 5, 2);
            j = hex(s + 7, 2);
            if(i > j || !i) u = 1;
            mi->min_hsync = i;
            mi->max_hsync = j;
          }
          else {
            u = 1;
          }
          if(u) {
            mi->min_vsync = mi->max_vsync = mi->min_hsync = mi->max_hsync = 0;
            ADD2LOG("  ddc oops: invalid freq data\n");
          }
          break;
        case '2':
          if(!mi->vendor && s[1]) mi->vendor = canon_str(s + 1, strlen(s + 1));
          break;
        case '3':
          if(!mi->serial && s[1]) mi->serial = canon_str(s + 1, strlen(s + 1));
          break;
        default:
          ADD2LOG("  ddc oops: invalid tag 0x%02x\n", *s);
      }
    }
  }

  if(mi) {
    hd->detail = new_mem(sizeof *hd->detail);
    hd->detail->type = hd_detail_monitor;
    hd->detail->monitor.data = mi;

    hd->serial = new_str(mi->serial);

    if(
      mi->vendor &&
      ID_VALUE(hd->vend) &&
      !hd_vendor_name(hd_data, hd->vend)
    ) {
      add_vendor_name(hd_data, hd->vend, mi->vendor);
    }

    if(
      mi->name &&
      (ID_VALUE(hd->vend) || ID_VALUE(hd->dev)) &&
      !hd_device_name(hd_data, hd->vend, hd->dev)
    ) {
      add_device_name(hd_data, hd->vend, hd->dev, mi->name);
    }

    if(hd_data->debug) {
      ADD2LOG("----- DDC info -----\n");
      if(mi->vendor) {
        ADD2LOG("  vendor: \"%s\"\n", mi->vendor);
      }
      if(mi->name) {
        ADD2LOG("  model: \"%s\"\n", mi->name);
      }
      if(mi->serial) {
        ADD2LOG("  serial: \"%s\"\n", mi->serial);
      }
      if(mi->min_hsync) {
        ADD2LOG("  hsync: %u-%u kHz\n", mi->min_hsync, mi->max_hsync);
      }
      if(mi->min_vsync) {
        ADD2LOG("  vsync: %u-%u Hz\n", mi->min_vsync, mi->max_vsync);
      }
      if(mi->manu_year) {
        ADD2LOG("  manu. year: %u\n", mi->manu_year);
      }
      ADD2LOG("----- DDC info end -----\n");
    }
  }

  free_mem(s0);
}
#endif	/* !defined(__PPC__) */

#if defined(__PPC__)
void hd_scan_monitor(hd_data_t *hd_data)
{
  devtree_t *dt;

  if(!hd_probe_feature(hd_data, pr_monitor)) return;

  hd_data->module = mod_monitor;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "prom");

  for(dt = hd_data->devtree; dt; dt = dt->next) {
    if(dt->edid) {
      add_monitor(hd_data, dt);
    }
  }
}

void add_monitor(hd_data_t *hd_data, devtree_t *dt)
{
  hd_t *hd, *hd2;
  hd_res_t *res;
  monitor_info_t *mi = NULL;
  int i;
  unsigned char *edid = dt->edid;
  unsigned u, u1, u2;

  hd = add_hd_entry(hd_data, __LINE__, 0);

  hd->base_class = bc_monitor;

  for(hd2 = hd_data->hd; hd2; hd2 = hd2->next) {
    if(
      hd2->detail &&
      hd2->detail->type == hd_detail_devtree &&
      hd2->detail->devtree.data == dt
    ) {
      hd->attached_to = hd2->idx;
      break;
    }
  }

  u = (edid[8] << 8) + edid[9];
  hd->vend = MAKE_ID(TAG_EISA, u);
  u = (edid[0xb] << 8) + edid[0xa];
  hd->dev = MAKE_ID(TAG_EISA, u);
  if((u = device_class(hd_data, hd->vend, hd->dev))) {
    if((u >> 8) == bc_monitor) hd->sub_class = u & 0xff;
  }

  if(edid[0x15] > 0 && edid[0x16] > 0) {
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->size.type = res_size;
    res->size.unit = size_unit_cm;
    res->size.val1 = edid[0x15];	/* width */
    res->size.val2 = edid[0x16];	/* height */
  }

  u = edid[0x23];
  if(u & (1 << 0)) add_monitor_res(hd, 720, 400, 70);
  if(u & (1 << 1)) add_monitor_res(hd, 720, 400, 88);
  if(u & (1 << 2)) add_monitor_res(hd, 640, 480, 60);
  if(u & (1 << 3)) add_monitor_res(hd, 640, 480, 67);
  if(u & (1 << 4)) add_monitor_res(hd, 640, 480, 72);
  if(u & (1 << 5)) add_monitor_res(hd, 640, 480, 75);
  if(u & (1 << 6)) add_monitor_res(hd, 800, 600, 56);
  if(u & (1 << 7)) add_monitor_res(hd, 800, 600, 60);

  u = edid[0x24];
  if(u & (1 << 0)) add_monitor_res(hd,  800,  600, 72);
  if(u & (1 << 1)) add_monitor_res(hd,  800,  600, 75);
  if(u & (1 << 2)) add_monitor_res(hd,  832,  624, 75);

/*
  this would be an interlaced timing; dropping it
  if(u & (1 << 3)) add_monitor_res(hd, 1024,  768, 87);
*/

  if(u & (1 << 4)) add_monitor_res(hd, 1024,  768, 60);
  if(u & (1 << 5)) add_monitor_res(hd, 1024,  768, 70);
  if(u & (1 << 6)) add_monitor_res(hd, 1024,  768, 75);
  if(u & (1 << 7)) add_monitor_res(hd, 1280, 1024, 75);

  for(i = 0; i < 4; i++) {
    u1 = (edid[0x26 + 2 * i] + 31) * 8;
    u2 = edid[0x27 + 2 * i];
    u = 0;
    switch((u2 >> 6) & 3) {
      case 1: u = (u1 * 3) / 4; break;
      case 2: u = (u1 * 4) / 5; break;
      case 3: u = (u1 * 9) / 16; break;
    }
    if(u) add_monitor_res(hd, u1, u, (u2 & 0x3f) + 60);
  }

  mi = new_mem(sizeof *mi);
  mi->manu_year = 1990 + edid[0x11];

  for(i = 0x36; i < 0x36 + 4 * 0x12; i += 0x12) {
    if(!(edid[i] || edid[i + 1] || edid[i + 2])) {
      switch(edid[i + 3]) {
        case 0xfc:
          if(!mi->name && edid[i + 5]) mi->name = canon_str(edid + i + 5, 0xd);
          break;

        case 0xfd:
          u = 0;
          u1 = edid[i + 5];
          u2 = edid[i + 6];
          if(u1 > u2 || !u1) u = 1;
          mi->min_vsync = u1;
          mi->max_vsync = u2;
          u1 = edid[i + 7];
          u2 = edid[i + 8];
          if(u1 > u2 || !u1) u = 1;
          mi->min_hsync = u1;
          mi->max_hsync = u2;
          if(u) {
            mi->min_vsync = mi->max_vsync = mi->min_hsync = mi->max_hsync = 0;
            ADD2LOG("  ddc oops: invalid freq data\n");
          }
          break;

        case 0xfe:
          if(!mi->vendor && edid[i + 5]) mi->vendor = canon_str(edid + i + 5, 0xd);
          break;

        case 0xff:
          if(!mi->serial && edid[i + 5]) mi->serial = canon_str(edid + i + 5, 0xd);
          break;

        default:
          ADD2LOG("  ddc oops: invalid tag 0x%02x\n", edid[i + 3]);
      }
    }
  }

  if(mi) {
    hd->detail = new_mem(sizeof *hd->detail);
    hd->detail->type = hd_detail_monitor;
    hd->detail->monitor.data = mi;

    hd->serial = new_str(mi->serial);

    if(
      mi->vendor &&
      ID_VALUE(hd->vend) &&
      !hd_vendor_name(hd_data, hd->vend)
    ) {
      add_vendor_name(hd_data, hd->vend, mi->vendor);
    }

    if(
      mi->name &&
      (ID_VALUE(hd->vend) || ID_VALUE(hd->dev)) &&
      !hd_device_name(hd_data, hd->vend, hd->dev)
    ) {
      add_device_name(hd_data, hd->vend, hd->dev, mi->name);
    }

    if(hd_data->debug) {
      ADD2LOG("----- DDC info -----\n");
      if(mi->vendor) {
        ADD2LOG("  vendor: \"%s\"\n", mi->vendor);
      }
      if(mi->name) {
        ADD2LOG("  model: \"%s\"\n", mi->name);
      }
      if(mi->serial) {
        ADD2LOG("  serial: \"%s\"\n", mi->serial);
      }
      if(mi->min_hsync) {
        ADD2LOG("  hsync: %u-%u kHz\n", mi->min_hsync, mi->max_hsync);
      }
      if(mi->min_vsync) {
        ADD2LOG("  vsync: %u-%u Hz\n", mi->min_vsync, mi->max_vsync);
      }
      if(mi->manu_year) {
        ADD2LOG("  manu. year: %u\n", mi->manu_year);
      }
      ADD2LOG("----- DDC info end -----\n");
    }
  }
}

#endif	/* defined(__PPC__) */

void add_monitor_res(hd_t *hd, unsigned width, unsigned height, unsigned vfreq)
{
  hd_res_t *res;

  res = add_res_entry(&hd->res, new_mem(sizeof *res));
  res->monitor.type = res_monitor;
  res->monitor.width = width;
  res->monitor.height = height;
  res->monitor.vfreq = vfreq;
}
