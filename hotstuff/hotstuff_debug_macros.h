#pragma once

#define HSLOG(component, s, ...) std::printf((std::string("%-45s") + component + s + "\n").c_str(), (std::string(__FILE__) + "." + std::to_string(__LINE__) + ":").c_str() __VA_OPT__(,) __VA_ARGS__)

#define VM_BRIDGE_LEVEL DEBUG_LEVEL_INFO
#define EVENT_LEVEL DEBUG_LEVEL_INFO
#define NETWORK_EVENT_LEVEL DEBUG_LEVEL_INFO
#define HSC_LEVEL DEBUG_LEVEL_INFO

#if VM_BRIDGE_LEVEL <= DEBUG_LEVEL_INFO
#define VM_BRIDGE_INFO(s, ...) HSLOG("VM BRIDGE: ", s, __VA_ARGS__)
#define VM_BRIDGE_INFO_F(s) s
#else
#define VM_BRIDGE_INFO(s, ...) (void)0
#define VM_BRIDGE_INFO_F(s) (void)0
#endif


#if EVENT_LEVEL <= DEBUG_LEVEL_INFO
#define EVENT_INFO(s, ...) HSLOG("HS EVENT: ", s, __VA_ARGS__)
#define EVENT_INFO_F(s) s
#else
#define EVENT_INFO(s, ...) (void)0
#define EVENT_INFO_F(s) (void)0
#endif

#if NETWORK_EVENT_LEVEL <= DEBUG_LEVEL_INFO
#define NETWORK_EVENT_INFO(s, ...) HSLOG("NET EVENT: ", s, __VA_ARGS__)
#define NETWORK_EVENT_INFO_F(s) s
#else
#define NETWORK_EVENT_INFO(s, ...) (void)0
#define NETWORK_EVENT_INFO_F(s) (void)0
#endif

#if HSC_LEVEL <= DEBUG_LEVEL_INFO
#define HSC_INFO(s, ...) HSLOG("HS CORE: ", s, __VA_ARGS__)
#define HSC_INFO_F(s) s
#else
#define HSC_INFO(s, ...) (void)0
#define HSC_INFO_F(s) (void)0
#endif