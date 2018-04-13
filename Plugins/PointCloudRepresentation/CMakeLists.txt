cmake_minimum_required(VERSION 3.3)

if (NOT ParaView_BINARY_DIR)
  find_package(ParaView REQUIRED)
  include(${PARAVIEW_USE_FILE})
endif()

# Set a consistent MACOSX_RPATH default across all CMake versions.
# When CMake 2.8.12 is required, change this default to 1.
# When CMake 3.0.0 is required, remove this block (see CMP0042).
if(NOT DEFINED CMAKE_MACOSX_RPATH)
  set(CMAKE_MACOSX_RPATH 0)
endif()

include(ParaViewPlugins)

include_directories(
  ${VTK_INCLUDE_DIRS}
)

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
