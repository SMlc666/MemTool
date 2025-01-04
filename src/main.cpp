#include "../include/MemTool.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>

int main(int argc, char **argv) {
  std::vector<int> vec{};
  vec.resize(100);
  int *mem = reinterpret_cast<int *>(malloc(sizeof(int) * 100));
  if (mem == nullptr) {
    std::cerr << "malloc failed" << std::endl;
  }
  std::cout << "mem ptr: " << mem << std::endl;
  MemTool::Wrapper::write(
      mem, 100000000000000000000000000000000000000000000000000000000);
  std::cout << "mem[0] = " << mem[0] << std::endl;
  std::cout << "mem[1] = " << mem[1] << std::endl;
  std::cout << "mem[2] = " << mem[2] << std::endl;
  std::cout << "mem[3] = " << mem[3] << std::endl;
  std::vector<MemTool::Native::Map> maps = MemTool::Native::parseAllMaps();
  for (auto &map : maps) {
    std::cout << map.getPathName() << std::endl;
    std::cout << "start address: " << map.getStartAddress() << std::endl;
    std::cout << "end address: " << map.getEndAddress() << std::endl;
    std::cout << "size: " << map.getSize() << std::endl;
    std::cout << "protection: " << map.getProtection() << std::endl;
  }
  vec[0] = 199;
  vec[3] = 100;
  std::cout << "vec size: " << vec.size() * sizeof(int) << " bytes"
            << std::endl;
  std::cout << "vec capacity: " << vec.capacity() * sizeof(int) << " bytes"
            << std::endl;
  std::cout << "page size: " << MemTool::Native::getPageSize() << " bytes"
            << std::endl;
  std::cout << "vec start address: "
            << MemTool::Native::getPageStart(vec.data()) << std::endl;
  std::cout << "vec end address: " << MemTool::Native::getPageEnd(vec.data())
            << std::endl;
  int buf;
  MemTool::Native::read(vec.data(), &buf, sizeof(int));
  std::cout << "vec[0] = " << buf << std::endl;
  int buf2 = 999;
  MemTool::Native::write(vec.data(), &buf2, sizeof(int));
  std::cout << "vec[0] = " << vec[0] << std::endl;
  int vec_3;
  MemTool::Native::read(&vec[3], &vec_3, sizeof(int));
  std::cout << "vec[3] = " << vec_3 << std::endl;
  return 0;
}
