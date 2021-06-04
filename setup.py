import sys

files = []
to_remove = []
for arg in sys.argv:
	if "--pyx_files=" in arg:
		arguments = arg[12:]
		files = arguments.split(" ")
		to_remove.append(arg)
	elif "--makeflags=" in arg:
		arguments = arg[12:]
		make_flags = arguments.split(" ")
		to_remove.append(arg)
		for flag in make_flags:
			if "-j" in flag and "-jobserver" not in flag:
				sys.argv.append(flag)

for arg in to_remove:
	sys.argv.remove(arg)

source_files = []

from setuptools import setup
from Cython.Build import cythonize

modules = cythonize(files + source_files)

for e in modules:
    e.extra_compile_args=["-std=gnu++17"]


setup(ext_modules = modules,
	 include_dirs=[".", "xdrpp"],
	 zip_safe = False)

