#ifndef THREADS_INIT_H
#define THREADS_INIT_H

#include <debug.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern size_t ram_pages;

/* Page directory with kernel mappings only. */
extern uint32_t *init_page_dir;

extern bool power_off_when_done;

void power_off (void) NO_RETURN;
#endif /* threads/init.h */
