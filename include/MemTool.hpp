#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/mman.h>
#include <sys/types.h>
#include <type_traits>
#include <unistd.h>
#include <utility>
#include <vector>
// NOLINTBEGIN(cppcoreguidelines-pro-reinterpret-cast)
static const long pageSize = sysconf(_SC_PAGESIZE);
namespace MemTool {
namespace Native {
// 解析perms字符串，生成PROT_xxx的组合
// 参数:
//   perms: 权限字符串，如"rwx"
// 返回值:
//   PROT_xxx的组合
inline int parsePermsToProt(const std::string &perms) {
  int prot = PROT_NONE;
  if (perms.length() < 1) {
    return prot;
  }
  if (perms[0] == 'r') {
    prot |= PROT_READ;
  }
  if (perms[1] == 'w') {
    prot |= PROT_WRITE;
  }
  if (perms[2] == 'x') {
    prot |= PROT_EXEC;
  }
  return prot;
}
class Map {
public:
  inline Map()
      : startAddress(nullptr), endAddress(nullptr), size(0), protection(0) {}
  inline Map(void *m_startAddress, void *m_endAddress, size_t m_size,
             int m_protection, std::string m_pathName)
      : startAddress(m_startAddress), endAddress(m_endAddress), size(m_size),
        protection(m_protection), pathName(std::move(m_pathName)) {}
  [[nodiscard]] inline void *getStartAddress() const { return startAddress; }
  [[nodiscard]] inline void *getEndAddress() const { return endAddress; }
  [[nodiscard]] inline size_t getSize() const { return size; }
  [[nodiscard]] inline int getProtection() const { return protection; }
  [[nodiscard]] inline const std::string &getPathName() const {
    return pathName;
  }
  [[nodiscard]] inline bool isVaild() const {
    return (startAddress == nullptr) && (endAddress == nullptr) && (size == 0U);
  }
  template <typename T> inline bool isContains(const T address) const {
    static_assert(std::is_integral_v<T> || std::is_pointer_v<T>,
                  "T must be integral or pointer");
    auto m_address = reinterpret_cast<uintptr_t>(address);
    auto m_startAddress = reinterpret_cast<uintptr_t>(startAddress);
    auto m_endAddress = reinterpret_cast<uintptr_t>(endAddress);
    return m_address >= m_startAddress && m_address < m_endAddress;
  }

private:
  void *startAddress;
  void *endAddress;
  size_t size;
  int protection;
  std::string pathName;
};
// 解析/proc/self/maps中的一行，生成一个Map对象
// 参数:
//   line: /proc/self/maps中的一行
// 返回值:
//   生成的Map对象
inline Map parseMapByLine(const std::string &line) {
  std::istringstream iss(line);
  Map map{};
  char dash = 0;
  unsigned long long start = 0;
  unsigned long long end = 0;
  unsigned long long offest = 0;
  unsigned long long inode = 0;
  std::string perms;
  std::string dev;
  std::string path;
  int protect = 0;
  iss >> std::hex >> start >> dash >> end >> perms >> std::hex >> offest >>
      dev >> std::dec >> inode;
  protect = parsePermsToProt(perms);
  std::getline(iss >> std::ws, path);
  map = Map(reinterpret_cast<void *>(start), reinterpret_cast<void *>(end),
            end - start, protect, path);
  return map;
}
// 获取指定名称的内存映射
// 参数:
//   name: 内存映射的名称
// 返回值:
//   成功返回true，失败返回false
inline bool parseMapByName(const std::string &name, Map &map) {
  std::ifstream file("/proc/self/maps");
  if (!file.is_open()) {
    return false;
  }
  std::string line;
  while (std::getline(file, line)) {
    Map m = parseMapByLine(line);
    if (m.getPathName() == name) {
      map = m;
      return true;
    }
  }
  return false;
}
// 获取系统中所有的内存映射
// 返回值:
//   系统中所有的内存映射
inline std::vector<Map> parseAllMaps() {
  std::ifstream file("/proc/self/maps");
  if (!file.is_open()) {
    return {};
  }
  std::vector<Map> maps;
  std::string line;
  while (std::getline(file, line)) {
    Map map = parseMapByLine(line);
    if (map.isVaild()) {
      continue;
    }
    maps.push_back(map);
  }
  return maps;
}
// 获取指定地址所在的内存映射
// 参数:
//   address: 内存地址
// 返回值:
//   所在内存映射的Map对象
inline Map getAddressMap(const void *address) {
  std::vector<Map> maps = parseAllMaps();
  for (const auto &map : maps) {
    if (map.isContains(address)) {
      return map;
    }
  }
  return {};
}
// 获取系统页大小
// 返回值:
//   系统页大小
inline long getPageSize() { return pageSize; }
// 获取指定地址所在的页的起始地址
// 参数:
//   address: 内存地址
// 返回值:
//   所在页的起始地址
template <typename T> inline T getPageStart(T address) {
  static_assert(std::is_integral_v<T> || std::is_pointer_v<T>,
                "T must be integral or pointer");
  uintptr_t PageStart = reinterpret_cast<uintptr_t>(address) & ~(pageSize - 1);
  if constexpr (std::is_integral_v<T>) {
    return static_cast<T>(PageStart);
  } else if constexpr (std::is_pointer_v<T>) {
    return reinterpret_cast<T>(PageStart);
  }
}
// 获取指定地址所在的页的结束地址
// 参数:
//   address: 内存地址
// 返回值:
//   所在页的结束地址
template <typename T> inline T getPageEnd(T address) {
  static_assert(std::is_integral_v<T> || std::is_pointer_v<T>,
                "T must be integral or pointer");
  uintptr_t PageEnd =
      getPageStart(reinterpret_cast<uintptr_t>(address) + getPageSize() - 1);
  if constexpr (std::is_integral_v<T>) {
    return static_cast<T>(PageEnd);
  } else if constexpr (std::is_pointer_v<T>) {
    return reinterpret_cast<T>(PageEnd);
  }
}
// 计算从 address 开始、长度为 size 的内存区域跨越的最后一页的结束地址
// 参数:
//   address: 内存区域的起始地址
//   size: 内存区域的大小
// 返回值:
//   所在页的结束地址
template <typename T> inline T getPageRegionEnd(T address, size_t size) {
  static_assert(std::is_integral_v<T> || std::is_pointer_v<T>,
                "T must be integral or pointer");
  uintptr_t PageRegionEnd =
      getPageStart(reinterpret_cast<uintptr_t>(address) + size - 1);
  if constexpr (std::is_integral_v<T>) {
    return static_cast<T>(PageRegionEnd);
  } else if constexpr (std::is_pointer_v<T>) {
    return reinterpret_cast<T>(PageRegionEnd);
  }
}
// 计算从 address 开始、长度为 size 的内存区域跨越的页数
// 参数:
//   address: 内存区域的起始地址
//   size: 内存区域的大小
// 返回值:
//   跨越的页数
template <typename T> inline long getPageRegionSize(T address, size_t size) {
  static_assert(std::is_integral_v<T> || std::is_pointer_v<T>,
                "T must be integral or pointer");
  static_assert(sizeof(uintptr_t) <= sizeof(long),
                "Narrowing conversion detected");
  uintptr_t PageRegionEnd =
      getPageRegionEnd(reinterpret_cast<uintptr_t>(address), size);
  uintptr_t PageStart = getPageStart(reinterpret_cast<uintptr_t>(address));
  long PageRegionSize =
      static_cast<long>(PageRegionEnd - PageStart) + getPageSize();
  return PageRegionSize;
}
// 设置指定内存区域的保护属性
// 参数:
//   address: 内存区域的起始地址
//   size: 内存区域的大小
//   protection: 需要设置的保护属性
// 返回值:
//   成功返回0，失败返回非0值
int setProtection(const void *address, size_t size, int protection) {
  uintptr_t PageStart = getPageStart(reinterpret_cast<uintptr_t>(address));
  long PageRegionSize = getPageRegionSize(address, size);
  int ret =
      mprotect(reinterpret_cast<void *>(PageStart), PageRegionSize, protection);
  return ret;
}
// 从指定内存区域读取数据到缓冲区
// 参数:
//   address: 内存区域的起始地址
//   buffer: 用于存储读取数据的缓冲区
//   size: 需要读取的数据大小
// 返回值:
//   无
void read(const void *address, void *buffer, size_t size) {
  std::memcpy(buffer, address, size);
}
// 从buffer写入指定内存区域
// 参数:
//   address: 内存区域的起始地址
//   buffer: 用于写入数据的缓冲区
//   size: 需要写入的数据大小
// 返回值:
//   无
void write(void *address, const void *buffer, size_t size) {
  std::memcpy(address, buffer, size);
}
} // namespace Native
// 封装了对Native的调用，提供更易用的接口
// 例如：
//   MemTool::Wrapper::read(address, buffer, size);
//   MemTool::Wrapper::write(address, value);
namespace Wrapper {
template <typename T>
inline void read(const T address, void *buffer, size_t size) {
  Native::read(reinterpret_cast<void *>(address), buffer, size);
}
template <typename T, typename U> inline U read(const T address) {
  U value;
  Native::read(reinterpret_cast<void *>(address), &value, sizeof(U));
  return value;
}
template <typename T>
inline void write(T address, const void *buffer, size_t size) {
  Native::write(reinterpret_cast<void *>(address), buffer, size);
}
template <typename T, typename U> inline void write(T address, const U value) {
  Native::write(reinterpret_cast<void *>(address), &value, sizeof(U));
}
} // namespace Wrapper
namespace Safe {
// 安全地从指定内存区域读取数据到缓冲区
// 参数:
//   address: 内存区域的起始地址
//   buffer: 用于存储读取数据的缓冲区
//   size: 需要读取的数据大小
// 返回值:
//   成功返回true，失败返回false
template <typename T>
inline bool read(const T address, void *buffer, size_t size) {
  void *m_address = reinterpret_cast<void *>(address);
  if (m_address == nullptr || buffer == nullptr || size == 0) {
    return false;
  }
  Native::Map map = Native::getAddressMap(m_address);
  if (map.isVaild()) {
    return false;
  }
  int protection = map.getProtection();
  if (Native::setProtection(m_address, size, PROT_READ) == -1) {
    return false;
  }
  Native::read(m_address, buffer, size);
  Native::setProtection(m_address, size, protection);
}
// 安全地从指定内存区域读取数据到返回
// 参数:
//   address: 内存区域的起始地址
// 返回值:
//   成功返回读取的数据，失败返回空值
template <typename T, typename U> inline U read(const T address) {
  U value;
  if (!read(address, &value, sizeof(U))) {
    return {};
  }
  return value;
}
} // namespace Safe
} // namespace MemTool
// NOLINTEND(cppcoreguidelines-pro-reinterpret-cast)