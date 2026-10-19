idf_component_register(SRCS "entry.c" REQUIRES app_sdk)
if(LIFECYCLE_STALL_ON_BOOT)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE LIFECYCLE_STALL_ON_BOOT=1)
endif()
