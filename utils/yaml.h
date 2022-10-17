#pragma once

#include <utils/cleanup.h>

#include <libfyaml.h>

namespace speedex
{

struct yaml : utils::unique_destructor_t<fy_document_destroy>
{
	yaml(std::string const& filename)
	{
		reset(fy_document_build_from_file(NULL, filename.c_str()));
	}

	yaml(const char* filename)
	{
		reset(fy_document_build_from_file(NULL, filename));
	}
};
	
}