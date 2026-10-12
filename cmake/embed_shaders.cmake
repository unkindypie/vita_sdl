# Shader files are build inputs, not runtime filesystem dependencies. Expand
# local includes for GLSL 1.20 and the Vita compiler's single-source callback.
function(read_shader path output)
  get_filename_component(shader_dir "${path}" DIRECTORY)
  file(READ "${path}" source)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
  string(REGEX MATCHALL "#include \"[^\"]+\"" includes "${source}")
  foreach(include IN LISTS includes)
    string(REGEX REPLACE "#include \"([^\"]+)\"" "\\1" relative "${include}")
    read_shader("${shader_dir}/${relative}" expanded)
    string(REPLACE "${include}" "${expanded}" source "${source}")
  endforeach()
  set(${output} "${source}" PARENT_SCOPE)
endfunction()
set(SHADER_SOURCES "")
foreach(shader IN ITEMS gl/geometry.vert.glsl gl/geometry.frag.glsl gl/present.vert.glsl
    gl/ascii.frag.glsl gxm/geometry.vert.cg gxm/geometry.frag.cg gxm/ascii.frag.cg
    gxm/present.frag.cg gxm/probe.frag.cg gl/style.frag.glsl gxm/style.frag.cg)
  read_shader("${CMAKE_SOURCE_DIR}/shaders/${shader}" source)
  string(MAKE_C_IDENTIFIER "${shader}" identifier)
  string(APPEND SHADER_SOURCES "inline constexpr const char* ${identifier} = R\"SHADER(${source})SHADER\";\n")
endforeach()
configure_file("${CMAKE_SOURCE_DIR}/cmake/shaders.h.in"
    "${CMAKE_BINARY_DIR}/generated/shaders.h" @ONLY)
