/* Console entry point for the Jump tool.
 * Delegates immediately to jump_main() which handles all CLI logic. */
#include "jump.h"

int main(int argc, char *argv[]) {
    return jump_main(argc, argv);
}
