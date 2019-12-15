# Add executable called "helloDemo" that is built from the source files
# "demo.cxx" and "demo_b.cxx". The extensions are automatically found.
set(CMAKE_EXPORT_COMPILE_COMMANDS YES)
add_executable (helloDemo 
    main.cpp
    # aaa.cpp

    )

# Link the executable to the Hello library. Since the Hello library has
# public include directories we will use those link directories when building
# helloDemo
target_link_libraries (helloDemo 
    pthread
    # LINK_PUBLIC Hello
    )

include_directories(
    .
    )
