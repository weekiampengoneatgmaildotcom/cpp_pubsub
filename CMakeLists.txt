cmake_minimum_required(VERSION 3.5)
project(cpp_pubsub)

# Default to C99
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# added by jipi
set(nlohmann_json_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/libdatachannel/deps/json/install)


# find dependencies
find_package(ament_cmake REQUIRED)
# uncomment the following section in order to fill in
# further dependencies manually.
# find_package(<dependency> REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(tutorial_interfaces REQUIRED)                         # CHANGE

include_directories(include)


find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
	#  "msg/Num.msg"
  "msg/KeyboardInput.msg"
  #"srv/AddThreeInts.srv"
)


find_package(PkgConfig)
find_package(LibDataChannel REQUIRED)
find_package(sensor_msgs REQUIRED)
#find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)

#pkg_check_modules()

add_executable(talker src/publisher_member_function.cpp src/parse_cl.cpp src/parse_cl.h)
#ament_target_dependencies(talker rclcpp std_msgs)
ament_target_dependencies(talker rclcpp std_msgs tutorial_interfaces)         # CHANGE

add_executable(listener src/subscriber_member_function.cpp)
#ament_target_dependencies(listener rclcpp std_msgs)
ament_target_dependencies(listener rclcpp std_msgs tutorial_interfaces)     # CHANGE

target_include_directories(talker PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
      /home/frodov3/nongros/libdatachannel/deps/json/include)

#target_include_directories(server PUBLIC
# "${GSTREAMER_INCLUDE_DIRECTORIES}")
#target_link_libraries(server
# "${GSTREAMER_LIBRARIES}")

target_link_libraries(talker LibDataChannel::LibDataChannel Threads::Threads)

install(TARGETS
  talker
  DESTINATION lib/${PROJECT_NAME})


if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # uncomment the line when a copyright and license is not present in all source files
  #set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # uncomment the line when this package is not in a git repo
  #set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
