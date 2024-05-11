#include "out_stream.h"

namespace distribuild {

void TransparentOutStream::write(const char* dest, std::size_t bytes) {
    m_buffer.append(dest, bytes);
}

} // namespace distribuild