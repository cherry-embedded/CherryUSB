get_filename_component(project_name ${CMAKE_CURRENT_LIST_DIR} NAME)

add_executable(${project_name}
main.c
)

# pull in common dependencies
target_link_libraries(${project_name} PRIVATE pico_stdlib cherryusblib hardware_resets hardware_irq)
# create map/bin/hex file etc.
pico_add_extra_outputs(${project_name})

# add url via pico_set_program_url
example_auto_set_url(${project_name})