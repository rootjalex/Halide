cmake_minimum_required(VERSION 3.16)
project(example_project)

enable_testing()

# Set up language settings
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

# Find Halide
find_package(Halide REQUIRED)

# Generator
add_executable(example_project.generator ExampleProject.cpp)
target_link_libraries(example_project.generator PRIVATE Halide::Generator Halide::Tools)

# Filters
add_halide_library(example_project FROM example_project.generator
                   STMT example_project_STMT
                   SCHEDULE example_project_SCHEDULE)

add_halide_library(example_project_auto_schedule FROM example_project.generator
                   GENERATOR example_project
                   STMT example_project_auto_schedule_STMT
                   SCHEDULE example_project_auto_schedule_SCHEDULE
                   AUTOSCHEDULER Halide::Adams2019)

# Main executable
add_executable(example_project_process filter.cpp)
target_link_libraries(example_project_process
                      PRIVATE
                      Halide::ImageIO # For halide_image_io.h
                      Halide::Tools # For halide_benchmark.h
                      example_project
                      example_project_auto_schedule)

# Test that the app actually works!
set(IMAGE ${CMAKE_CURRENT_LIST_DIR}/../images/gray.png)
if (EXISTS ${IMAGE})
    configure_file(${IMAGE} gray.png COPYONLY)
    add_test(NAME example_project_process
             COMMAND example_project_process gray.png out.png 0.1 10)
    set_tests_properties(example_project_process PROPERTIES
                         LABELS internal_app_tests
                         PASS_REGULAR_EXPRESSION "Success!"
                         SKIP_REGULAR_EXPRESSION "\\[SKIP\\]")
endif ()
