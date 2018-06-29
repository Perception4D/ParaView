cmake_minimum_required(VERSION 3.3)

if (NOT ParaView_BINARY_DIR)
  find_package(ParaView REQUIRED)
  include(${PARAVIEW_USE_FILE})
endif()
find_package(OpenGL REQUIRED)

include(ParaViewPlugins)

include_directories(
  ${VTK_INCLUDE_DIRS}
)
set (CMAKE_CXX_STANDARD 11)

# create a paraview plugin containing server manager xml and the server
# manager classes to build
# this plugin can be loaded on the server side

encode_files_as_strings(ENCODED_STRING_FILES
  vtkPointCloud_fs.glsl
  vtkPointCloud_vs.glsl
)

add_paraview_plugin(PointCloudPlugin "1.0"
  SERVER_MANAGER_XML
    PointCloudRepresentation.xml
  SERVER_MANAGER_SOURCES
    vtkPointCloudRepresentation.cxx
  SOURCES
    ${ENCODED_STRING_FILES}
    vtkPointCloudMapper.cxx
)
target_link_libraries(PointCloudPlugin LINK_PRIVATE ${OPENGL_LIBRARIES} )
if (VTK_RENDERING_BACKEND STREQUAL "OpenGL")
  message(WARNING "You chose both PointCloudPlugin and OpenGL1 rendering backend. PointCloudPlugin requires OpenGL2 and will not be built.")
  set(PARAVIEW_BUILD_PLUGIN_PointCloudRepresentation OFF)
endif()
