#define CONT_RM     1u   /* 0000 0001 */
#define CONT_CMD    2u   /* 0000 0010 */
#define CONT_BUILD  4u   /* 0000 0100 */
#define FLAG_4      8u   /* 0000 1000 */
#define FLAG_5      16u  /* 0001 0000 */
#define FLAG_6      32u  /* 0010 0000 */
#define FLAG_7      64u  /* 0100 0000 */
#define FLAG_8      128u /* 1000 0000 */

typedef struct cenv {
	char* key;
	char* val;
	struct cenv* next;
} cenv_t;

typedef struct container {
	char root[128];
	char cont_root[128];
	char cont_name[32];
	char cont_id[16];
	char** cmd_args;
	cenv_t* cont_envp;
	size_t* cont_stack;
	long cont_stack_size;
} container_t;


void* stalloc(long size);

void setup_src();

void setup_root();

void setup_variables(void*);

void setup_resolvconf();

void setup_bashrc();

void setup_dev();

void cleanup();

int start(void*);

int start_cmd();

void init(char const*, char const*, char const*, char**, struct cenv*, unsigned int);

