#include "lib.h"
#include "sh.h"

void umain(int argc, char **argv) {
    int shell_id = syscall_env_destroy_shell();
    writef("\n"LIGHT_BLUE(-- -- -- --shell)" %d "LIGHT_BLUE(exist-- -- -- --)"\n", shell_id);
}
