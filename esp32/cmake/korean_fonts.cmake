# Checked-in generated fonts are compressed to keep source diffs small.
# Python is already a prerequisite of ESP-IDF and the desktop simulator.
function(muse_korean_font_sources result)
    find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
    set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
    set(sources)
    foreach(name muse_font_ko_16 muse_font_ko_ui_12)
        set(source "${root}/components/muse/fonts/${name}.c.gz")
        set(output "${CMAKE_CURRENT_BINARY_DIR}/korean_fonts/${name}.c")
        add_custom_command(OUTPUT "${output}"
            COMMAND "${Python3_EXECUTABLE}" "${root}/tools/muse/unpack_font.py" "${source}" "${output}"
            DEPENDS "${source}" "${root}/tools/muse/unpack_font.py"
            VERBATIM)
        list(APPEND sources "${output}")
    endforeach()
    set(${result} "${sources}" PARENT_SCOPE)
endfunction()
