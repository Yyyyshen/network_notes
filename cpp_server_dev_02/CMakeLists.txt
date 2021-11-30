cmake_minimum_required(VERSION 2.6) # 支持的最低版本

project(cpp_server_dev)				# 项目名称

set(CMAKE_CXX_FLAGS					# set设置变量，第一个为变量名称，其后是变量的值
"$(CMAKE_CXX_FLAGS) -std=c++0x -g -Wall -O0 -Wno-unused-variable -pthread")

set(srcs 
filepaths/to/xxx.cpps
)

aux_source_directory(				# 源码目录
	. SRC_LIST						# 获取当前目录下所有源文件，并将列表添加到SRC_LIST变量名下
)

include_directories(				# include目录
	$(PROJECT_SOURCE_DIR)/headers
)

link_directories(					# lib目录
	$(PROJECT_SOURCE_DIR)/libs
)

add_library(						# 使用指令生成库
	libname [SHARED|STATIC] sours	# 不用写全文件名，只写name，自动生成的就是libhello.so/libhello.a
)

TARGET_LINK_LIBRARIES(				# 指定生成的二进制文件所依赖的其他库
	lib1 lib2 libn
)

# 编写完成后
# 进入CMakeLists.txt文件目录
# 执行
# cmake .		生成makefile
# 继续执行
# make

# 支持递归执行，先执行父目录，再执行子目录