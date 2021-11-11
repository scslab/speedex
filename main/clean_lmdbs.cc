#include "utils/manage_data_dirs.h"

using namespace speedex;

int main(int argc, char const *argv[])
{
	clear_all_data_dirs();
	make_all_data_dirs();
	return 0;
}
