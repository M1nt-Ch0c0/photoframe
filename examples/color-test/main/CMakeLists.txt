idf_component_register(SRCS "entry.c" REQUIRES app_sdk)
if(COLOR_TEST_REVERSE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE COLOR_TEST_REVERSE=1)
endif()
