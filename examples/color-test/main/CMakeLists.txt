idf_component_register(SRCS "entry.c" REQUIRES photoframe
    PRIV_INCLUDE_DIRS "../../../components/photoframe/private_include")
if(COLOR_TEST_REVERSE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE COLOR_TEST_REVERSE=1)
endif()
