#pragma once

#define HSLOG(component, s, ...) std::printf((std::string("%-40s") + component + s + "\n").c_str(), (std::string(__FILE__) + "." + std::to_string(__LINE__) + ":").c_str() __VA_OPT__(,) __VA_ARGS__)

#define VM_BRIDGE_LEVEL DEBUG_LEVEL_INFO

#if VM_BRIDGE_LEVEL <= DEBUG_LEVEL_INFO
#define VM_BRIDGE_INFO(s, ...) HSLOG("VM BRIDGE: ", s, __VA_ARGS__)
#define VM_BRIDGE_INFO_F(s) s
#else
#define VM_BRIDGE_INFO(s, ...) (void)0
#define VM_BRIDGE_INFO_F(s) (void)0
#endif
