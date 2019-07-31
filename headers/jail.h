#define CONT_RM     1u   /* 0000 0001 */
#define CONT_CMD    2u   /* 0000 0010 */
#define CONT_BUILD  4u   /* 0000 0100 */
#define FLAG_4      8u   /* 0000 1000 */
#define FLAG_5      16u  /* 0001 0000 */
#define FLAG_6      32u  /* 0010 0000 */
#define FLAG_7      64u  /* 0100 0000 */
#define FLAG_8      128u /* 1000 0000 */

// int image_selected = 0;
// int prebuilt = 0;
// int rm_cont = 0;
// int build_cont = 0;
// int cmd_start = 0;

void* stalloc(long size);

void setup_src();

void setup_root();

void setup_variables();

void setup_resolvconf();

void setup_bashrc();

void setup_dev();

void cleanup();

int start();

int start_cmd();

void init(char const*, char const*, char const*, char**, unsigned int);

