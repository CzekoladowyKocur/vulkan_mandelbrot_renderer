#pragma once
/* Win32 */
#include <Windows.h>
/* STD */
#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

class Buffer {
public:
  using Byte_t = uint8_t;

public:
  ~Buffer() {
    if (m_Data)
      delete[] m_Data;

    m_Data = nullptr;
    m_Size = 0;
  }

  /* Performs no allocation */
  void Set(const std::size_t size, const void *data) {
    if (m_Data)
      delete[] m_Data;

    m_Size = size;
    m_Data = reinterpret_cast<Byte_t *>(const_cast<void *>(data));
  }

  void Allocate(const std::size_t size) {
    if (m_Size && m_Size < size)
      delete[] m_Data;

    m_Size = size;
    m_Data = new Byte_t[size];
  }

  void Write(const std::size_t size, const void *data) {
    assert(m_Size >= size);
    memcpy(m_Data, data, size);
  }

  void Clear() {
    if (m_Data)
      delete[] m_Data;

    m_Size = 0;
  }

  [[nodiscard]]
  void *Data() const {
    return m_Data;
  }

  [[nodiscard]]
  std::size_t Size() const {
    return m_Size;
  }

private:
  Byte_t *m_Data{nullptr};
  std::size_t m_Size{0ull};
};
