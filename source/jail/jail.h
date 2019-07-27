#include "jail.c"

#define J_RM    1   /* 0000 0001 */
#define FLAG_2  2   /* 0000 0010 */
#define FLAG_3  4   /* 0000 0100 */
#define FLAG_4  8   /* 0000 1000 */
#define FLAG_5  16  /* 0001 0000 */
#define FLAG_6  32  /* 0010 0000 */
#define FLAG_7  64  /* 0100 0000 */
#define FLAG_8  128 /* 1000 0000 */

// int image_selected = 0;
// int prebuilt = 0;
// int rm_cont = 0;
// int build_cont = 0;
// int cmd_start = 0;

void init(char const*, char const*, char const*);

void init2(char const*, char const*, char const*, char* const*);

int start();

int start_cmd();

void setup_src();

void setup_root();

void setup_variables();

void setup_resolvconf();

void setup_bashrc();

void setup_dev();

size_t* stalloc(long size);

void cleanup();
