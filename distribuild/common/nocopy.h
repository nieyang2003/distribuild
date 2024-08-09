#pragma once
namespace distribuild {

class NoCopy {
public:
    NoCopy() = default;
    ~NoCopy() = default;
private:
    NoCopy(const NoCopy&) = delete;
    NoCopy(const NoCopy&&) = delete;
    NoCopy& operator==(const NoCopy&&) = delete;
};

} // namespace distribuild