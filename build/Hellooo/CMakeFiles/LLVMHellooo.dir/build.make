# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.17

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Disable VCS-based implicit rules.
% : %,v


# Disable VCS-based implicit rules.
% : RCS/%


# Disable VCS-based implicit rules.
% : RCS/%,v


# Disable VCS-based implicit rules.
% : SCCS/s.%


# Disable VCS-based implicit rules.
% : s.%


.SUFFIXES: .hpux_make_needs_suffix_list


# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/bin/cmake

# The command to remove a file.
RM = /usr/local/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/vagrant/hello-world-pass

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/vagrant/hello-world-pass/build

# Include any dependencies generated for this target.
include Hellooo/CMakeFiles/LLVMHellooo.dir/depend.make

# Include the progress variables for this target.
include Hellooo/CMakeFiles/LLVMHellooo.dir/progress.make

# Include the compile flags for this target's objects.
include Hellooo/CMakeFiles/LLVMHellooo.dir/flags.make

Hellooo/CMakeFiles/LLVMHellooo.dir/Hellooo.cpp.o: Hellooo/CMakeFiles/LLVMHellooo.dir/flags.make
Hellooo/CMakeFiles/LLVMHellooo.dir/Hellooo.cpp.o: ../Hellooo/Hellooo.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/vagrant/hello-world-pass/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object Hellooo/CMakeFiles/LLVMHellooo.dir/Hellooo.cpp.o"
	cd /home/vagrant/hello-world-pass/build/Hellooo && /usr/local/bin/clang++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/LLVMHellooo.dir/Hellooo.cpp.o -c /home/vagrant/hello-world-pass/Hellooo/Hellooo.cpp

Hellooo/CMakeFiles/LLVMHellooo.dir/Hellooo.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/LLVMHellooo.dir/Hellooo.cpp.i"
	cd /home/vagrant/hello-world-pass/build/Hellooo && /usr/local/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/vagrant/hello-world-pass/Hellooo/Hellooo.cpp > CMakeFiles/LLVMHellooo.dir/Hellooo.cpp.i

Hellooo/CMakeFiles/LLVMHellooo.dir/Hellooo.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/LLVMHellooo.dir/Hellooo.cpp.s"
	cd /home/vagrant/hello-world-pass/build/Hellooo && /usr/local/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/vagrant/hello-world-pass/Hellooo/Hellooo.cpp -o CMakeFiles/LLVMHellooo.dir/Hellooo.cpp.s

# Object files for target LLVMHellooo
LLVMHellooo_OBJECTS = \
"CMakeFiles/LLVMHellooo.dir/Hellooo.cpp.o"

# External object files for target LLVMHellooo
LLVMHellooo_EXTERNAL_OBJECTS =

lib/LLVMHellooo.so: Hellooo/CMakeFiles/LLVMHellooo.dir/Hellooo.cpp.o
lib/LLVMHellooo.so: Hellooo/CMakeFiles/LLVMHellooo.dir/build.make
lib/LLVMHellooo.so: Hellooo/CMakeFiles/LLVMHellooo.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/vagrant/hello-world-pass/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX shared module ../lib/LLVMHellooo.so"
	cd /home/vagrant/hello-world-pass/build/Hellooo && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/LLVMHellooo.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
Hellooo/CMakeFiles/LLVMHellooo.dir/build: lib/LLVMHellooo.so

.PHONY : Hellooo/CMakeFiles/LLVMHellooo.dir/build

Hellooo/CMakeFiles/LLVMHellooo.dir/clean:
	cd /home/vagrant/hello-world-pass/build/Hellooo && $(CMAKE_COMMAND) -P CMakeFiles/LLVMHellooo.dir/cmake_clean.cmake
.PHONY : Hellooo/CMakeFiles/LLVMHellooo.dir/clean

Hellooo/CMakeFiles/LLVMHellooo.dir/depend:
	cd /home/vagrant/hello-world-pass/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/vagrant/hello-world-pass /home/vagrant/hello-world-pass/Hellooo /home/vagrant/hello-world-pass/build /home/vagrant/hello-world-pass/build/Hellooo /home/vagrant/hello-world-pass/build/Hellooo/CMakeFiles/LLVMHellooo.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : Hellooo/CMakeFiles/LLVMHellooo.dir/depend
