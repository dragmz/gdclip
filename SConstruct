#!/usr/bin/env python
# Builds the gdclip GDExtension for Godot 4 against godot-cpp.
#
#   scons                     # template_debug (what the editor loads)
#   scons target=template_release
#
# The shared library is written into demo/bin/ with godot-cpp's platform/target
# suffix, matching the paths declared in demo/gdclip.gdextension.

env = SConscript("godot-cpp/SConstruct")

# Clipper uses C++ exceptions for its coordinate-range checks, but godot-cpp
# defaults to -fno-exceptions. Re-enable them for our translation units; the
# flag is appended last and overrides godot-cpp's, and godot-cpp's own objects
# (already compiled) are unaffected.
env.Append(CXXFLAGS=["-fexceptions"])

# Engine-free geometry core + Clipper + the thin GDExtension binding.
env.Append(CPPPATH=["src/", ".", "clipper/"])
sources = Glob("src/*.cpp") + ["gdclip_core.cpp", "clipper/clipper.cpp"]

library = env.SharedLibrary(
    "demo/bin/libgdclip{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
    source=sources,
)

Default(library)
