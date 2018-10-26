# test this CMakeLists.txt in a standalone fashion as:
#	TYPE="Release"; rm -fr $TYPE; mkdir $TYPE; cd $TYPE; cmake -G Ninja -DCMAKE_BUILD_TYPE=$TYPE ..; cmake --build .; ctest; cd ..; ls -l "$TYPE/EventsFrameworkTests"
# The rules here should be used when importing this library into other projects

cmake_minimum_required (VERSION 3.9)
project                (EventsFrameworkTests VERSION 2018.10.09)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS    "${CMAKE_CXX_FLAGS} -march=native")	# binary is optimized, but machine dependent


file(GLOB_RECURSE SOURCE_FILES  cpp/*.h cpp/*.cpp)
message("Building executable './${CMAKE_BUILD_TYPE}/${PROJECT_NAME}' with: ${SOURCE_FILES}")
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# imported mutua libraries
##########################
# mutua libraries have the inclues inside the "cpp/" directory

# EventsFramework dependency
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

foreach (_referencedLib EventsFramework CppUtils)
	set(_referencedLib_SOURCES "../../${_referencedLib}/cpp/")
	message("    --> searching library '${_referencedLib}' in '~/.cmake/packages/${_referencedLib}' with headers in '${_referencedLib_SOURCES}'")
	if (NOT TARGET ${_referencedLib})
		find_package(${_referencedLib} CONFIG REQUIRED)
		add_library (${_referencedLib} STATIC IMPORTED GLOBAL)
		message("        (built in '${${_referencedLib}_DIR}')")
	else()
		add_library(mutua::${_referencedLib} ALIAS ${_referencedLib})
		message("        (defined by the parent CMakeLists.txt)")
	endif()
	include_directories("${_referencedLib_SOURCES}")
	target_link_libraries(${PROJECT_NAME} PRIVATE mutua::${_referencedLib})
endforeach()


# libboost unit test framework
##############################
add_library          (boost_unit_test_framework STATIC     IMPORTED GLOBAL)
set_target_properties(boost_unit_test_framework PROPERTIES IMPORTED_LOCATION /usr/lib/libboost_unit_test_framework.a)
target_link_libraries(${PROJECT_NAME}                      PRIVATE boost_unit_test_framework)


#enable_testing()
add_test(NAME "${PROJECT_NAME}" COMMAND "./${PROJECT_NAME}")