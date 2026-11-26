idf_component_register(SRCS "app_main.cpp")
target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")
