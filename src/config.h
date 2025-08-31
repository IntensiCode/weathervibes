#ifndef CONFIG_H
#define CONFIG_H

#include "app.h"

void config_init(void);
void config_load(void);
void config_save(void);
void config_free(void);

#endif