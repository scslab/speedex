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
		maxjobs = 12
		for flag in make_flags:
			if "-j" in flag and "-jobserver" not in flag:
				if (len(flag) > 2):
					sys.argv.append(flag)
				else:
					sys.argv.append(flag + str(maxjobs))

print (sys.argv)

for arg in to_remove:
	sys.argv.remove(arg)

source_files = []

from setuptools import setup
from Cython.Build import cythonize

modules = cythonize(files + source_files)

for e in modules:
    e.extra_compile_args=["-std=gnu++17", "-I.", "-Ixdrpp/"]


setup(ext_modules = modules,
	 include_dirs=[".", "xdrpp"],
	 zip_safe = False)

