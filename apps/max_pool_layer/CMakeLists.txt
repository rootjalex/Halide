add_executable(max_pool_layer_process process.cpp)
halide_use_image_io(max_pool_layer_process)

halide_generator(max_pool_layer.generator SRCS max_pool_layer_generator.cpp)
foreach(AUTO_SCHEDULE false true)
    if(${AUTO_SCHEDULE})
        set(LIB max_pool_layer_auto_schedule)
    else()
        set(LIB max_pool_layer)
    endif()
    halide_library_from_generator(${LIB}
                                  GENERATOR max_pool_layer.generator
                                  GENERATOR_ARGS auto_schedule=${AUTO_SCHEDULE})
    target_link_libraries(max_pool_layer_process PRIVATE ${LIB})
endforeach()
