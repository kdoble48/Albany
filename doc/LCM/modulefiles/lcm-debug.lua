whatis("LCM debug build type")

setenv("BUILD_TYPE", "debug")
setenv("BUILD_STRING", "DEBUG")
setenv("LCM_FPE_SWITCH", "ON")
setenv("LCM_DENORMAL_SWITCH", "ON")
setenv("LCM_CXX_FLAGS", "-msse3")
