# Run after tmxlite has finished adding its own compiler flags. Keep this
# dependency optimized and its debug logging disabled in every build configuration.
# The game's flags and configuration-specific MSVC runtime remain unchanged.
function(marpg_tmxlite_release_flags)
    foreach(language C CXX)
        foreach(configuration DEBUG RELWITHDEBINFO MINSIZEREL)
            set(CMAKE_${language}_FLAGS_${configuration}
                "${CMAKE_${language}_FLAGS_RELEASE}" PARENT_SCOPE)
        endforeach()
    endforeach()
endfunction()
cmake_language(DEFER CALL marpg_tmxlite_release_flags)
