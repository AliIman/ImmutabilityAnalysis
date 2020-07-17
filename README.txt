These are the exact instructions that I did for building this project so far:


1. Creating a vagrant box

   cd /home/ali/Desktop/vagrant-boxes
   mkdir debian-buster
   vagrant init generic/debian10
   vim Vagrantfile
      Uncomment lines 52, 57, and 58
      Change 1024 to 8192 on line 57
      Add this line to line 16:
      config.disksize.size = "100GB"
   vagrant box update
   vagrant up
   vagrant ssh
   sudo apt update
   sudo apt upgrade


   df -h
      /dev/sda3        27G  2.0G   24G   8% /
   sudo fdisk -l
   sudo fdisk /dev/sda
      :p
      :d
      :3
      :n
      :p
      :   (partition number)
      :   (first sector)
      :   (last sector)
      :N  (No)
      :p
      :w
   sudo touch /forcefsck
   sudo poweroff
   vagrant up
   vagrant ssh
   sudo tune2fs -l /dev/sda3
   sudo resize2fs /dev/sda3 
   df -h



2. Building Cmake 3.17

   which cmake
      /usr/bin/cmake
   cmake --version
      cmake version 3.13.4
   sudo apt install libssl-dev
   cd /home/vagrant/my-builds/cmake-3.17.3
   cmake /home/vagrant/cmake-3.17.3/
      Lots of “not found” and “failed”. No errors
      Build files have been written to: /home/vagrant/my-builds/cmake-3.17.3
   make
   sudo make install
   which cmake
      /usr/local/bin/cmake
   cmake --version
      cmake version 3.17.3



3. Building LLVM 8.0.1

   cd /home/vagrant/my-builds/llvm-8.0.1
   cmake -DCMAKE_BUILD_TYPE=Debug -DLLVM_ENABLE_RTTI:BOOLEAN=1 /home/vagrant/llvm-8.0.1.src/
   make
   sudo make install
   which llvm-config
      /usr/local/bin/llvm-config
   llvm-config --version
      8.0.1



4. Building Clang 8.0.1

   cd /home/vagrant/my-builds/clang-8.0.1
   vim /home/vagrant/cfe-8.0.1.src/CMakeLists.txt
      Add line: cmake_policy(VERSION 3.17)
   cmake  /home/vagrant/cfe-8.0.1.src/
   make
   sudo make install
   which clang
      /usr/local/bin/clang
   clang --version
      clang version 8.0.1 (tags/RELEASE_801/final)



5. Building LLVM Immutability Analysis project

   sudo apt install libiberty-dev
   sudo apt install libpq-dev
   cd /home/vagrant/llvm-immutability-analysis/build/
   rm -rf *
   export CC=/usr/local/bin/clang
   export CXX=/usr/local/bin/clang++
   cmake ..
   make
