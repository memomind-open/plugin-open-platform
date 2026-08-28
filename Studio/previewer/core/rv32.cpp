#include "rv32.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>

namespace gmpreview {
namespace {

int32_t signExtend(uint32_t value, unsigned bits)
{
    const uint32_t sign = UINT32_C(1) << (bits - 1);
    return static_cast<int32_t>((value ^ sign) - sign);
}

uint32_t bits(uint32_t value, unsigned high, unsigned low)
{
    return (value >> low) & ((UINT32_C(1) << (high - low + 1)) - 1);
}

float asFloat(uint32_t value)
{
    float result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

uint32_t asBits(float value)
{
    uint32_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

uint32_t arithmeticShiftRight(uint32_t value, unsigned amount)
{
    return static_cast<uint32_t>(static_cast<int32_t>(value) >> (amount & 31));
}

} // namespace

void GuestMemory::clear() { regions_.clear(); }

void GuestMemory::add(uint32_t base, size_t size, const std::string &name,
                      bool read_only)
{
    if (size == 0 || static_cast<uint64_t>(base) + size > UINT64_C(0x100000000))
        throw std::runtime_error("invalid guest memory region " + name);
    for (const Region &region : regions_) {
        const uint64_t end = static_cast<uint64_t>(base) + size;
        const uint64_t other_end = static_cast<uint64_t>(region.base) + region.bytes.size();
        if (base < other_end && region.base < end)
            throw std::runtime_error("overlapping guest memory region " + name);
    }
    Region region;
    region.base = base;
    region.bytes.resize(size, 0);
    region.name = name;
    region.read_only = read_only;
    regions_.push_back(std::move(region));
}

GuestMemory::Region *GuestMemory::find(uint32_t address, size_t size)
{
    const uint64_t end = static_cast<uint64_t>(address) + size;
    for (Region &region : regions_) {
        if (address >= region.base && end <= static_cast<uint64_t>(region.base) + region.bytes.size())
            return &region;
    }
    return nullptr;
}

const GuestMemory::Region *GuestMemory::find(uint32_t address, size_t size) const
{
    const uint64_t end = static_cast<uint64_t>(address) + size;
    for (const Region &region : regions_) {
        if (address >= region.base && end <= static_cast<uint64_t>(region.base) + region.bytes.size())
            return &region;
    }
    return nullptr;
}

uint8_t GuestMemory::read8(uint32_t address) const
{
    const Region *region = find(address);
    if (!region) {
        std::ostringstream out;
        out << "guest read outside memory at 0x" << std::hex << address;
        throw std::runtime_error(out.str());
    }
    return region->bytes[address - region->base];
}

uint16_t GuestMemory::read16(uint32_t address) const
{
    return static_cast<uint16_t>(read8(address) |
                                 static_cast<uint16_t>(read8(address + 1)) << 8);
}

uint32_t GuestMemory::read32(uint32_t address) const
{
    return static_cast<uint32_t>(read8(address)) |
           static_cast<uint32_t>(read8(address + 1)) << 8 |
           static_cast<uint32_t>(read8(address + 2)) << 16 |
           static_cast<uint32_t>(read8(address + 3)) << 24;
}

void GuestMemory::write8(uint32_t address, uint8_t value)
{
    Region *region = find(address);
    if (!region || region->read_only) {
        std::ostringstream out;
        out << "guest write outside writable memory at 0x" << std::hex << address;
        throw std::runtime_error(out.str());
    }
    region->bytes[address - region->base] = value;
}

void GuestMemory::write16(uint32_t address, uint16_t value)
{
    write8(address, static_cast<uint8_t>(value));
    write8(address + 1, static_cast<uint8_t>(value >> 8));
}

void GuestMemory::write32(uint32_t address, uint32_t value)
{
    write8(address, static_cast<uint8_t>(value));
    write8(address + 1, static_cast<uint8_t>(value >> 8));
    write8(address + 2, static_cast<uint8_t>(value >> 16));
    write8(address + 3, static_cast<uint8_t>(value >> 24));
}

void GuestMemory::read(uint32_t address, void *destination, size_t size) const
{
    uint8_t *target = static_cast<uint8_t *>(destination);
    for (size_t i = 0; i < size; ++i) target[i] = read8(address + static_cast<uint32_t>(i));
}

void GuestMemory::write(uint32_t address, const void *source, size_t size)
{
    const uint8_t *bytes = static_cast<const uint8_t *>(source);
    for (size_t i = 0; i < size; ++i) write8(address + static_cast<uint32_t>(i), bytes[i]);
}

std::string GuestMemory::readString(uint32_t address, size_t maximum) const
{
    std::string result;
    result.reserve(std::min<size_t>(maximum, 256));
    for (size_t i = 0; i < maximum; ++i) {
        const uint8_t c = read8(address + static_cast<uint32_t>(i));
        if (c == 0) return result;
        result.push_back(static_cast<char>(c));
    }
    throw std::runtime_error("unterminated guest string");
}

void Rv32::reset()
{
    std::fill(std::begin(x), std::end(x), 0);
    std::fill(std::begin(f), std::end(f), UINT32_C(0x7fc00000));
    pc = 0;
    instructions = 0;
    reservation_valid_ = false;
}

void Rv32::setTrapHandler(TrapHandler handler) { trap_handler_ = std::move(handler); }

uint32_t Rv32::argument(size_t index) const
{
    if (index < 8) return x[10 + index];
    return memory.read32(x[2] + static_cast<uint32_t>((index - 8) * 4));
}

void Rv32::setReturn(uint32_t value) { x[10] = value; }

int32_t Rv32::call(uint32_t address, const std::vector<uint32_t> &arguments,
                   uint32_t stack_top, uint64_t budget)
{
    std::fill(std::begin(x), std::end(x), 0);
    x[2] = stack_top & ~UINT32_C(15);
    for (size_t i = 8; i < arguments.size(); ++i)
        memory.write32(x[2] + static_cast<uint32_t>((i - 8) * 4), arguments[i]);
    for (size_t i = 0; i < std::min<size_t>(8, arguments.size()); ++i)
        x[10 + i] = arguments[i];
    x[1] = kReturnSentinel;
    pc = address;
    const uint64_t start = instructions;
    while (pc != kReturnSentinel) {
        if (instructions - start >= budget)
            throw std::runtime_error("RV32 instruction budget exceeded (possible infinite loop)");
        if (trap_handler_ && trap_handler_(*this, pc)) {
            ++instructions;
            x[0] = 0;
            continue;
        }
        try {
            step();
        } catch (const std::exception &error) {
            std::ostringstream message;
            message << error.what() << " while executing PC 0x" << std::hex << pc;
            throw std::runtime_error(message.str());
        }
    }
    return static_cast<int32_t>(x[10]);
}

void Rv32::step()
{
    const uint16_t first = memory.read16(pc);
    if ((first & 3u) != 3u) step16(first);
    else step32(memory.read32(pc));
    x[0] = 0;
    ++instructions;
}

void Rv32::step32(uint32_t ins)
{
    const uint32_t old_pc = pc;
    const uint32_t opcode = ins & 0x7f;
    const uint32_t rd = bits(ins, 11, 7);
    const uint32_t funct3 = bits(ins, 14, 12);
    const uint32_t rs1 = bits(ins, 19, 15);
    const uint32_t rs2 = bits(ins, 24, 20);
    const uint32_t funct7 = bits(ins, 31, 25);
    pc += 4;

    switch (opcode) {
    case 0x37: // LUI
        x[rd] = ins & 0xfffff000u;
        return;
    case 0x17: // AUIPC
        x[rd] = old_pc + (ins & 0xfffff000u);
        return;
    case 0x6f: { // JAL
        uint32_t imm = bits(ins, 31, 31) << 20 | bits(ins, 19, 12) << 12 |
                       bits(ins, 20, 20) << 11 | bits(ins, 30, 21) << 1;
        x[rd] = pc;
        pc = old_pc + static_cast<uint32_t>(signExtend(imm, 21));
        return;
    }
    case 0x67: { // JALR
        if (funct3 != 0) unsupported(ins, "JALR");
        const uint32_t target = (x[rs1] + static_cast<uint32_t>(signExtend(ins >> 20, 12))) & ~1u;
        x[rd] = pc;
        pc = target;
        return;
    }
    case 0x63: { // branches
        const uint32_t imm = bits(ins, 31, 31) << 12 | bits(ins, 7, 7) << 11 |
                             bits(ins, 30, 25) << 5 | bits(ins, 11, 8) << 1;
        bool take = false;
        switch (funct3) {
        case 0: take = x[rs1] == x[rs2]; break;
        case 1: take = x[rs1] != x[rs2]; break;
        case 4: take = static_cast<int32_t>(x[rs1]) < static_cast<int32_t>(x[rs2]); break;
        case 5: take = static_cast<int32_t>(x[rs1]) >= static_cast<int32_t>(x[rs2]); break;
        case 6: take = x[rs1] < x[rs2]; break;
        case 7: take = x[rs1] >= x[rs2]; break;
        default: unsupported(ins, "branch");
        }
        if (take) pc = old_pc + static_cast<uint32_t>(signExtend(imm, 13));
        return;
    }
    case 0x03: { // loads
        const uint32_t address = x[rs1] + static_cast<uint32_t>(signExtend(ins >> 20, 12));
        switch (funct3) {
        case 0: x[rd] = static_cast<uint32_t>(signExtend(memory.read8(address), 8)); break;
        case 1: x[rd] = static_cast<uint32_t>(signExtend(memory.read16(address), 16)); break;
        case 2: x[rd] = memory.read32(address); break;
        case 4: x[rd] = memory.read8(address); break;
        case 5: x[rd] = memory.read16(address); break;
        default: unsupported(ins, "load");
        }
        return;
    }
    case 0x23: { // stores
        const uint32_t raw = bits(ins, 31, 25) << 5 | bits(ins, 11, 7);
        const uint32_t address = x[rs1] + static_cast<uint32_t>(signExtend(raw, 12));
        switch (funct3) {
        case 0: memory.write8(address, static_cast<uint8_t>(x[rs2])); break;
        case 1: memory.write16(address, static_cast<uint16_t>(x[rs2])); break;
        case 2: memory.write32(address, x[rs2]); break;
        default: unsupported(ins, "store");
        }
        reservation_valid_ = false;
        return;
    }
    case 0x13: { // ALU immediate
        const uint32_t immediate = static_cast<uint32_t>(signExtend(ins >> 20, 12));
        switch (funct3) {
        case 0: x[rd] = x[rs1] + immediate; break;
        case 2: x[rd] = static_cast<int32_t>(x[rs1]) < static_cast<int32_t>(immediate); break;
        case 3: x[rd] = x[rs1] < immediate; break;
        case 4: x[rd] = x[rs1] ^ immediate; break;
        case 6: x[rd] = x[rs1] | immediate; break;
        case 7: x[rd] = x[rs1] & immediate; break;
        case 1:
            if (funct7 != 0) unsupported(ins, "SLLI");
            x[rd] = x[rs1] << (rs2 & 31);
            break;
        case 5:
            if (funct7 == 0) x[rd] = x[rs1] >> (rs2 & 31);
            else if (funct7 == 0x20) x[rd] = arithmeticShiftRight(x[rs1], rs2);
            else unsupported(ins, "shift immediate");
            break;
        default: unsupported(ins, "ALU immediate");
        }
        return;
    }
    case 0x33: { // ALU register / M
        if (funct7 == 1) {
            const int64_t a = static_cast<int32_t>(x[rs1]);
            const int64_t b = static_cast<int32_t>(x[rs2]);
            const uint64_t ua = x[rs1], ub = x[rs2];
            switch (funct3) {
            case 0: x[rd] = static_cast<uint32_t>(ua * ub); break;
            case 1: x[rd] = static_cast<uint32_t>((a * b) >> 32); break;
            case 2: x[rd] = static_cast<uint32_t>((a * static_cast<int64_t>(ub)) >> 32); break;
            case 3: x[rd] = static_cast<uint32_t>((ua * ub) >> 32); break;
            case 4:
                if (x[rs2] == 0) x[rd] = UINT32_MAX;
                else if (x[rs1] == UINT32_C(0x80000000) && x[rs2] == UINT32_MAX) x[rd] = x[rs1];
                else x[rd] = static_cast<uint32_t>(static_cast<int32_t>(x[rs1]) / static_cast<int32_t>(x[rs2]));
                break;
            case 5: x[rd] = x[rs2] == 0 ? UINT32_MAX : x[rs1] / x[rs2]; break;
            case 6:
                if (x[rs2] == 0) x[rd] = x[rs1];
                else if (x[rs1] == UINT32_C(0x80000000) && x[rs2] == UINT32_MAX) x[rd] = 0;
                else x[rd] = static_cast<uint32_t>(static_cast<int32_t>(x[rs1]) % static_cast<int32_t>(x[rs2]));
                break;
            case 7: x[rd] = x[rs2] == 0 ? x[rs1] : x[rs1] % x[rs2]; break;
            }
            return;
        }
        switch (funct3) {
        case 0:
            if (funct7 == 0) x[rd] = x[rs1] + x[rs2];
            else if (funct7 == 0x20) x[rd] = x[rs1] - x[rs2];
            else unsupported(ins, "ADD/SUB");
            break;
        case 1: x[rd] = x[rs1] << (x[rs2] & 31); break;
        case 2: x[rd] = static_cast<int32_t>(x[rs1]) < static_cast<int32_t>(x[rs2]); break;
        case 3: x[rd] = x[rs1] < x[rs2]; break;
        case 4: x[rd] = x[rs1] ^ x[rs2]; break;
        case 5:
            if (funct7 == 0) x[rd] = x[rs1] >> (x[rs2] & 31);
            else if (funct7 == 0x20) x[rd] = arithmeticShiftRight(x[rs1], x[rs2]);
            else unsupported(ins, "SRL/SRA");
            break;
        case 6: x[rd] = x[rs1] | x[rs2]; break;
        case 7: x[rd] = x[rs1] & x[rs2]; break;
        }
        return;
    }
    case 0x0f: // FENCE / FENCE.I: no-op in a single-threaded interpreter
        return;
    case 0x73: { // SYSTEM / basic CSR support
        if (funct3 == 0) {
            if ((ins >> 20) == 1) throw std::runtime_error("guest executed EBREAK");
            return;
        }
        const uint32_t source = funct3 >= 5 ? rs1 : x[rs1];
        const uint32_t old = 0; // fflags/frm/fcsr read as zero in the previewer
        (void)source;
        x[rd] = old;
        return;
    }
    case 0x2f: { // RV32A atomics (single-threaded semantics)
        if (funct3 != 2) unsupported(ins, "AMO");
        const uint32_t operation = bits(ins, 31, 27);
        const uint32_t address = x[rs1];
        const uint32_t old = memory.read32(address);
        if (operation == 2) { // LR.W
            reservation_ = address;
            reservation_valid_ = true;
            x[rd] = old;
        } else if (operation == 3) { // SC.W
            if (reservation_valid_ && reservation_ == address) {
                memory.write32(address, x[rs2]);
                x[rd] = 0;
            } else x[rd] = 1;
            reservation_valid_ = false;
        } else {
            uint32_t value = 0;
            switch (operation) {
            case 0: value = old + x[rs2]; break;
            case 1: value = x[rs2]; break;
            case 4: value = old ^ x[rs2]; break;
            case 8: value = old | x[rs2]; break;
            case 12: value = old & x[rs2]; break;
            case 16: value = static_cast<int32_t>(old) < static_cast<int32_t>(x[rs2]) ? old : x[rs2]; break;
            case 20: value = static_cast<int32_t>(old) > static_cast<int32_t>(x[rs2]) ? old : x[rs2]; break;
            case 24: value = std::min(old, x[rs2]); break;
            case 28: value = std::max(old, x[rs2]); break;
            default: unsupported(ins, "AMO operation");
            }
            memory.write32(address, value);
            x[rd] = old;
            reservation_valid_ = false;
        }
        return;
    }
    case 0x07: { // FLW
        if (funct3 != 2) unsupported(ins, "floating load");
        f[rd] = memory.read32(x[rs1] + static_cast<uint32_t>(signExtend(ins >> 20, 12)));
        return;
    }
    case 0x27: { // FSW
        if (funct3 != 2) unsupported(ins, "floating store");
        const uint32_t raw = bits(ins, 31, 25) << 5 | bits(ins, 11, 7);
        memory.write32(x[rs1] + static_cast<uint32_t>(signExtend(raw, 12)), f[rs2]);
        return;
    }
    case 0x43: case 0x47: case 0x4b: case 0x4f: { // fused single precision
        if (bits(ins, 26, 25) != 0) unsupported(ins, "fused double precision");
        const uint32_t rs3 = bits(ins, 31, 27);
        float value;
        if (opcode == 0x43) value = std::fma(asFloat(f[rs1]), asFloat(f[rs2]), asFloat(f[rs3]));
        else if (opcode == 0x47) value = std::fma(asFloat(f[rs1]), asFloat(f[rs2]), -asFloat(f[rs3]));
        else if (opcode == 0x4b) value = std::fma(-asFloat(f[rs1]), asFloat(f[rs2]), asFloat(f[rs3]));
        else value = std::fma(-asFloat(f[rs1]), asFloat(f[rs2]), -asFloat(f[rs3]));
        f[rd] = asBits(value);
        return;
    }
    case 0x53: { // RV32F (rounding is host-nearest for preview purposes)
        const float a = asFloat(f[rs1]), b = asFloat(f[rs2]);
        switch (funct7) {
        case 0x00: f[rd] = asBits(a + b); break;
        case 0x04: f[rd] = asBits(a - b); break;
        case 0x08: f[rd] = asBits(a * b); break;
        case 0x0c: f[rd] = asBits(a / b); break;
        case 0x2c: f[rd] = asBits(std::sqrt(a)); break;
        case 0x10:
            if (funct3 == 0) f[rd] = (f[rs1] & 0x7fffffff) | (f[rs2] & 0x80000000);
            else if (funct3 == 1) f[rd] = (f[rs1] & 0x7fffffff) | (~f[rs2] & 0x80000000);
            else if (funct3 == 2) f[rd] = f[rs1] ^ (f[rs2] & 0x80000000);
            else unsupported(ins, "FSGNJ");
            break;
        case 0x14:
            if (funct3 == 0) f[rd] = asBits(std::fmin(a, b));
            else if (funct3 == 1) f[rd] = asBits(std::fmax(a, b));
            else unsupported(ins, "FMIN/FMAX");
            break;
        case 0x50:
            if (funct3 == 0) x[rd] = a <= b;
            else if (funct3 == 1) x[rd] = a < b;
            else if (funct3 == 2) x[rd] = a == b;
            else unsupported(ins, "floating compare");
            break;
        case 0x60:
            if (rs2 == 0) {
                if (std::isnan(a)) x[rd] = UINT32_C(0x80000000);
                else if (a >= 2147483647.0f) x[rd] = UINT32_C(0x7fffffff);
                else if (a <= -2147483648.0f) x[rd] = UINT32_C(0x80000000);
                else x[rd] = static_cast<uint32_t>(static_cast<int32_t>(std::nearbyint(a)));
            } else if (rs2 == 1) {
                if (std::isnan(a) || a <= 0) x[rd] = 0;
                else if (a >= 4294967295.0f) x[rd] = UINT32_MAX;
                else x[rd] = static_cast<uint32_t>(std::nearbyint(a));
            } else unsupported(ins, "FCVT.W.S");
            break;
        case 0x68:
            if (rs2 == 0) f[rd] = asBits(static_cast<float>(static_cast<int32_t>(x[rs1])));
            else if (rs2 == 1) f[rd] = asBits(static_cast<float>(x[rs1]));
            else unsupported(ins, "FCVT.S.W");
            break;
        case 0x70:
            if (funct3 == 0) x[rd] = f[rs1];
            else if (funct3 == 1) {
                const uint32_t v = f[rs1];
                const float fv = asFloat(v);
                x[rd] = std::isnan(fv) ? (UINT32_C(1) << (std::isnan(fv) ? 9 : 0)) : 0;
            } else unsupported(ins, "FMV.X.W/FCLASS.S");
            break;
        case 0x78:
            if (funct3 != 0) unsupported(ins, "FMV.W.X");
            f[rd] = x[rs1];
            break;
        default: unsupported(ins, "floating operation");
        }
        return;
    }
    default:
        unsupported(ins, "32-bit instruction");
    }
}

void Rv32::step16(uint16_t ins)
{
    const uint32_t old_pc = pc;
    const uint32_t quadrant = ins & 3;
    const uint32_t funct3 = (ins >> 13) & 7;
    pc += 2;

    if (quadrant == 0) {
        const uint32_t rd_p = 8 + ((ins >> 2) & 7);
        const uint32_t rs1_p = 8 + ((ins >> 7) & 7);
        const uint32_t rs2_p = 8 + ((ins >> 2) & 7);
        const uint32_t offset = ((ins >> 5) & 1) << 6 |
                                ((ins >> 10) & 7) << 3 |
                                ((ins >> 6) & 1) << 2;
        switch (funct3) {
        case 0: { // C.ADDI4SPN
            const uint32_t immediate = ((ins >> 7) & 0xf) << 6 |
                                       ((ins >> 11) & 3) << 4 |
                                       ((ins >> 5) & 1) << 3 |
                                       ((ins >> 6) & 1) << 2;
            if (immediate == 0) unsupported(ins, "C.ADDI4SPN");
            x[rd_p] = x[2] + immediate;
            return;
        }
        case 2: x[rd_p] = memory.read32(x[rs1_p] + offset); return; // C.LW
        case 3: f[rd_p] = memory.read32(x[rs1_p] + offset); return; // C.FLW
        case 6: memory.write32(x[rs1_p] + offset, x[rs2_p]); return; // C.SW
        case 7: memory.write32(x[rs1_p] + offset, f[rs2_p]); return; // C.FSW
        default: unsupported(ins, "compressed quadrant 0");
        }
    }

    if (quadrant == 1) {
        const uint32_t rd = (ins >> 7) & 31;
        const uint32_t immediate6 = ((ins >> 12) & 1) << 5 | ((ins >> 2) & 0x1f);
        const uint32_t jump_imm = ((ins >> 12) & 1) << 11 |
                                  ((ins >> 11) & 1) << 4 |
                                  ((ins >> 9) & 3) << 8 |
                                  ((ins >> 8) & 1) << 10 |
                                  ((ins >> 7) & 1) << 6 |
                                  ((ins >> 6) & 1) << 7 |
                                  ((ins >> 3) & 7) << 1 |
                                  ((ins >> 2) & 1) << 5;
        switch (funct3) {
        case 0: x[rd] += static_cast<uint32_t>(signExtend(immediate6, 6)); return; // C.ADDI/NOP
        case 1: x[1] = pc; pc = old_pc + static_cast<uint32_t>(signExtend(jump_imm, 12)); return; // C.JAL
        case 2: x[rd] = static_cast<uint32_t>(signExtend(immediate6, 6)); return; // C.LI
        case 3:
            if (rd == 2) { // C.ADDI16SP
                const uint32_t imm = ((ins >> 12) & 1) << 9 |
                                     ((ins >> 6) & 1) << 4 |
                                     ((ins >> 5) & 1) << 6 |
                                     ((ins >> 3) & 3) << 7 |
                                     ((ins >> 2) & 1) << 5;
                x[2] += static_cast<uint32_t>(signExtend(imm, 10));
            } else {
                if (rd == 0 || immediate6 == 0) unsupported(ins, "C.LUI");
                x[rd] = static_cast<uint32_t>(signExtend(immediate6, 6)) << 12;
            }
            return;
        case 4: {
            const uint32_t target = 8 + ((ins >> 7) & 7);
            const uint32_t sub = (ins >> 10) & 3;
            if (sub == 0) x[target] >>= ((ins >> 2) & 31); // C.SRLI
            else if (sub == 1) x[target] = arithmeticShiftRight(x[target], (ins >> 2) & 31); // C.SRAI
            else if (sub == 2) x[target] &= static_cast<uint32_t>(signExtend(immediate6, 6)); // C.ANDI
            else {
                if ((ins >> 12) & 1) unsupported(ins, "RV64 compressed ALU");
                const uint32_t source = 8 + ((ins >> 2) & 7);
                switch ((ins >> 5) & 3) {
                case 0: x[target] -= x[source]; break;
                case 1: x[target] ^= x[source]; break;
                case 2: x[target] |= x[source]; break;
                case 3: x[target] &= x[source]; break;
                }
            }
            return;
        }
        case 5: pc = old_pc + static_cast<uint32_t>(signExtend(jump_imm, 12)); return; // C.J
        case 6: case 7: {
            const uint32_t rs1 = 8 + ((ins >> 7) & 7);
            const uint32_t branch_imm = ((ins >> 12) & 1) << 8 |
                                        ((ins >> 10) & 3) << 3 |
                                        ((ins >> 5) & 3) << 6 |
                                        ((ins >> 3) & 3) << 1 |
                                        ((ins >> 2) & 1) << 5;
            const bool take = funct3 == 6 ? x[rs1] == 0 : x[rs1] != 0;
            if (take) pc = old_pc + static_cast<uint32_t>(signExtend(branch_imm, 9));
            return;
        }
        }
    }

    if (quadrant == 2) {
        const uint32_t rd = (ins >> 7) & 31;
        const uint32_t rs2 = (ins >> 2) & 31;
        switch (funct3) {
        case 0: // C.SLLI
            if (rd == 0 || ((ins >> 12) & 1)) unsupported(ins, "C.SLLI");
            x[rd] <<= rs2;
            return;
        case 2: { // C.LWSP
            if (rd == 0) unsupported(ins, "C.LWSP");
            const uint32_t offset = ((ins >> 12) & 1) << 5 |
                                    ((ins >> 4) & 7) << 2 |
                                    ((ins >> 2) & 3) << 6;
            x[rd] = memory.read32(x[2] + offset);
            return;
        }
        case 3: { // C.FLWSP
            const uint32_t offset = ((ins >> 12) & 1) << 5 |
                                    ((ins >> 4) & 7) << 2 |
                                    ((ins >> 2) & 3) << 6;
            f[rd] = memory.read32(x[2] + offset);
            return;
        }
        case 4:
            if (((ins >> 12) & 1) == 0) {
                if (rs2 == 0) { // C.JR
                    if (rd == 0) unsupported(ins, "C.JR");
                    pc = x[rd] & ~1u;
                } else x[rd] = x[rs2]; // C.MV
            } else {
                if (rd == 0 && rs2 == 0) throw std::runtime_error("guest executed C.EBREAK");
                if (rs2 == 0) { // C.JALR
                    const uint32_t target = x[rd] & ~1u;
                    x[1] = pc;
                    pc = target;
                } else x[rd] += x[rs2]; // C.ADD
            }
            return;
        case 6: { // C.SWSP
            const uint32_t offset = ((ins >> 9) & 0xf) << 2 |
                                    ((ins >> 7) & 3) << 6;
            memory.write32(x[2] + offset, x[rs2]);
            return;
        }
        case 7: { // C.FSWSP
            const uint32_t offset = ((ins >> 9) & 0xf) << 2 |
                                    ((ins >> 7) & 3) << 6;
            memory.write32(x[2] + offset, f[rs2]);
            return;
        }
        default: unsupported(ins, "compressed quadrant 2");
        }
    }
    unsupported(ins, "compressed instruction");
}

[[noreturn]] void Rv32::unsupported(uint32_t instruction, const char *kind) const
{
    std::ostringstream out;
    out << "unsupported " << kind << " 0x" << std::hex << std::setfill('0')
        << std::setw(instruction <= 0xffff ? 4 : 8) << instruction
        << " at PC 0x" << std::setw(8) << (pc - (instruction <= 0xffff ? 2 : 4));
    throw std::runtime_error(out.str());
}

} // namespace gmpreview
