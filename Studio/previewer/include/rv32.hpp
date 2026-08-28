#pragma once

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace gmpreview {

class GuestMemory {
public:
    struct Region {
        uint32_t base = 0;
        std::vector<uint8_t> bytes;
        std::string name;
        bool read_only = false;
    };

    void clear();
    void add(uint32_t base, size_t size, const std::string &name,
             bool read_only = false);
    Region *find(uint32_t address, size_t size = 1);
    const Region *find(uint32_t address, size_t size = 1) const;

    uint8_t read8(uint32_t address) const;
    uint16_t read16(uint32_t address) const;
    uint32_t read32(uint32_t address) const;
    void write8(uint32_t address, uint8_t value);
    void write16(uint32_t address, uint16_t value);
    void write32(uint32_t address, uint32_t value);
    void read(uint32_t address, void *destination, size_t size) const;
    void write(uint32_t address, const void *source, size_t size);
    std::string readString(uint32_t address, size_t maximum = 65536) const;

private:
    std::vector<Region> regions_;
};

class Rv32 {
public:
    static constexpr uint32_t kReturnSentinel = 0xfffffffcu;
    using TrapHandler = std::function<bool(Rv32 &, uint32_t)>;

    GuestMemory memory;
    uint32_t x[32]{};
    uint32_t f[32]{};
    uint32_t pc = 0;
    uint64_t instructions = 0;

    void reset();
    void setTrapHandler(TrapHandler handler);
    int32_t call(uint32_t address, const std::vector<uint32_t> &arguments,
                 uint32_t stack_top, uint64_t budget = 5000000);
    uint32_t argument(size_t index) const;
    void setReturn(uint32_t value);

private:
    TrapHandler trap_handler_;
    uint32_t reservation_ = 0;
    bool reservation_valid_ = false;

    void step();
    void step32(uint32_t instruction);
    void step16(uint16_t instruction);
    [[noreturn]] void unsupported(uint32_t instruction,
                                  const char *kind) const;
};

} // namespace gmpreview
