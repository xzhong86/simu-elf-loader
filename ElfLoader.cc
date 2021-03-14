
#include "ExeLoader.h"
#include <elf.h>
#include <assert.h>
#include <fstream>

#define ASSERT_MSG(c, m)  assert((c) && (m))

namespace NS_ExeLoader {

union Elf_Ehdr {
    Elf32_Ehdr e32;
    Elf64_Ehdr e64;
};
union Elf_Phdr {
    Elf32_Phdr e32;
    Elf64_Phdr e64;
};
union Elf_Shdr {
    Elf32_Shdr e32;
    Elf64_Shdr e64;
};
union Elf_Sym {
    Elf32_Sym e32;
    Elf64_Sym e64;
};

static void bswap(uint8_t *ptr, unsigned n)
{
    ASSERT_MSG(n % 2 == 0, "bad size");
    for (unsigned i = 0; i < n / 2; i++) {
        uint8_t tmp = ptr[i];
        ptr[i] = ptr[n-1-i];
        ptr[n-1-i] = tmp;
    }
}
#define BSWAP(M)  bswap((uint8_t*)&(M), sizeof(M))

#define ELF_M(E64, S, M)   ( (E64) ? (S).e64.M : (S).e32.M )
#define ELF_SWAPM(E64, S, M)  do { (E64) ? BSWAP((S).e64.M) : BSWAP((S).e32.M) ; } while (0)

static void elf_swap(bool e64, Elf_Ehdr &ehdr)
{
    ELF_SWAPM(e64, ehdr, e_type);	/* Object file type */
    ELF_SWAPM(e64, ehdr, e_machine);	/* Architecture */
    ELF_SWAPM(e64, ehdr, e_version);	/* Object file version */
    ELF_SWAPM(e64, ehdr, e_entry);	/* Entry point virtual address */
    ELF_SWAPM(e64, ehdr, e_phoff);	/* Program header table file offset */
    ELF_SWAPM(e64, ehdr, e_shoff);	/* Section header table file offset */
    ELF_SWAPM(e64, ehdr, e_flags);	/* Processor-specific flags */
    ELF_SWAPM(e64, ehdr, e_ehsize);	/* ELF header size in bytes */
    ELF_SWAPM(e64, ehdr, e_phentsize);	/* Program header table entry size */
    ELF_SWAPM(e64, ehdr, e_phnum);	/* Program header table entry count */
    ELF_SWAPM(e64, ehdr, e_shentsize);	/* Section header table entry size */
    ELF_SWAPM(e64, ehdr, e_shnum);	/* Section header table entry count */
    ELF_SWAPM(e64, ehdr, e_shstrndx);	/* Section header string table index */
}

static void elf_swap(bool e64, Elf_Phdr &phdr)
{
    ELF_SWAPM(e64, phdr, p_type);		/* Segment type */
    ELF_SWAPM(e64, phdr, p_offset);		/* Segment file offset */
    ELF_SWAPM(e64, phdr, p_vaddr);		/* Segment virtual address */
    ELF_SWAPM(e64, phdr, p_paddr);		/* Segment physical address */
    ELF_SWAPM(e64, phdr, p_filesz);		/* Segment size in file */
    ELF_SWAPM(e64, phdr, p_memsz);		/* Segment size in memory */
    ELF_SWAPM(e64, phdr, p_flags);		/* Segment flags */
    ELF_SWAPM(e64, phdr, p_align);		/* Segment alignment */
}

static void elf_swap(bool e64, Elf_Shdr &shdr)
{
    ELF_SWAPM(e64, shdr, sh_name);
    ELF_SWAPM(e64, shdr, sh_type);
    ELF_SWAPM(e64, shdr, sh_flags);
    ELF_SWAPM(e64, shdr, sh_addr);
    ELF_SWAPM(e64, shdr, sh_offset);
    ELF_SWAPM(e64, shdr, sh_size);
    ELF_SWAPM(e64, shdr, sh_link);
    ELF_SWAPM(e64, shdr, sh_info);
    ELF_SWAPM(e64, shdr, sh_addralign);
    ELF_SWAPM(e64, shdr, sh_entsize);
}

static void elf_swap(bool e64, Elf_Sym &sym)
{
    ELF_SWAPM(e64, sym, st_name);
    ELF_SWAPM(e64, sym, st_shndx);
    ELF_SWAPM(e64, sym, st_value);
    ELF_SWAPM(e64, sym, st_size);
}

struct ElfInfo {
    Elf_Ehdr ehdr;
    Elf_Shdr strtab;
    Elf_Shdr symtab;
    bool is_64;
    bool is_be;
    bool has_strtab;
    bool has_symtab;

    uint8_t ei_class() const { return ehdr.e32.e_ident[EI_CLASS]; }
    uint8_t ei_data()  const { return ehdr.e32.e_ident[EI_DATA]; }
    uint8_t ei_osabi() const { return ehdr.e32.e_ident[EI_OSABI]; }
    bool check_magic() const {
        auto *ei = ehdr.e32.e_ident;
        return (ei[EI_MAG0] == ELFMAG0 &&
                ei[EI_MAG1] == ELFMAG1 &&
                ei[EI_MAG2] == ELFMAG2 &&
                ei[EI_MAG3] == ELFMAG3 );
    }
    bool isExecutable() const { return ELF_M(is_64, ehdr, e_type) == ET_EXEC; }
    void readInitEhdr(std::ifstream &input) {
        input.seekg(0);
        input.read((char*)&ehdr, sizeof(ehdr));
        is_64 = ei_class() == ELFCLASS64;
        is_be = ei_data()  == ELFDATA2MSB;
        if (is_be) elf_swap(is_64, ehdr);
    }

    template <typename T>
    void readItemAt(std::ifstream &input, T &item, uint64_t offset, unsigned size) {
        input.seekg(offset);
        input.read((char*)&item, size);
        assert(input.good());
        if (is_be) elf_swap(is_64, item);
    }
    void readShdrIdx(std::ifstream &input, Elf_Shdr &shdr, int shidx) {
        uint64_t shoffset  = ELF_M(is_64, ehdr, e_shoff);
        unsigned shentsize = ELF_M(is_64, ehdr, e_shentsize);
        readItemAt(input, shdr, shoffset + shidx * shentsize, shentsize);
    }
    void readSymIdx(std::ifstream &input, Elf_Sym &sym, int idx) {
        unsigned sym_size = ELF_M(is_64, symtab, sh_entsize);
        uint64_t sym_off  = ELF_M(is_64, symtab, sh_offset);
        readItemAt(input, sym, sym_off + idx * sym_size, sym_size);
    }
    
};

#define EHDR_M(E, M)  ELF_M((E).is_64, (E).ehdr, M)



// ==================  ==================

class ElfLoader : public ExeLoader {
    std::ifstream *fstream_;
    struct ElfInfo elf_info_;
    bool bad_elf_ = false;
    bool debug_;

public:
    ElfLoader(const std::string &path) : ExeLoader(path) {
        debug_ = false;
    }

    void openAndScanElf() {
        fstream_ = new std::ifstream(path_);
        std::ifstream &input = *fstream_;

        auto &elf = elf_info_;
        auto &e = elf.ehdr;
        elf.readInitEhdr(input);

        if (!input.good() || !elf.check_magic() || !elf.isExecutable()) {
            bad_elf_ = true;
            return;
        }
        bool is_64 = elf.is_64;
        bool is_be = elf.is_be;

        entry_point_ = EHDR_M(elf, e_entry);

        Elf_Shdr shdr;
        uint64_t shnum  = EHDR_M(elf, e_shnum);

        unsigned shstrndx = EHDR_M(elf, e_shstrndx);
        for (unsigned i = 0; i < shnum; i++) {
            elf.readShdrIdx(input, shdr, i);
            if (ELF_M(is_64, shdr, sh_type) == SHT_STRTAB && i != shstrndx) {
                elf.strtab = shdr;
                elf.has_strtab = true;
            }
            if (ELF_M(is_64, shdr, sh_type) == SHT_SYMTAB) {
                elf.symtab = shdr;
                elf.has_symtab = true;
            }
        }
    }

    void load(uint64_t va, unsigned size, off_t offs, prop_t prop) {
        std::ifstream &input = *fstream_;
        const unsigned CHUNKSIZE = 0x1000U;
        uint8_t buf[CHUNKSIZE];
        input.seekg(offs);
        unsigned rs = 0;
        while (rs < size) {
            unsigned sz = std::min(CHUNKSIZE, (size - rs));
            input.read((char*)buf, sz);
            assert(input.good() && "reading elf data");
            cb_.loadData(va + rs, buf, sz, prop);
            rs += sz;
        }
    }
    void clear(uint64_t va, unsigned size, prop_t prop) {
        cb_.setZero(va, size, prop);
    }

    void loadelf() {
        std::ifstream &input = *fstream_;
        auto &elf = elf_info_;
        bool is_64 = elf.is_64;
        uint64_t ph_offset  = EHDR_M(elf, e_phoff);
        unsigned ph_entsize = EHDR_M(elf, e_phentsize);
        Elf_Phdr phdr;
        for (unsigned u = 0; u < ELF_M(is_64, elf.ehdr, e_phnum); u++) {

            elf.readItemAt(input, phdr, ph_offset + ph_entsize * u, ph_entsize);

            if (ELF_M(is_64, phdr, p_type) == PT_LOAD && ELF_M(is_64, phdr, p_memsz)) {
                // Load section from file
                uint32_t flags = ELF_M(is_64, phdr, p_flags);
                uint64_t offs  = ELF_M(is_64, phdr, p_offset);
                uint64_t va    = ELF_M(is_64, phdr, p_vaddr);
                uint64_t fs    = ELF_M(is_64, phdr, p_filesz);
                uint64_t ms    = ELF_M(is_64, phdr, p_memsz);
                prop_t   prop  = ( ((flags & PF_R) ? (1 << Read) : 0) |
                                   ((flags & PF_W) ? (1 << Write) : 0) |
                                   ((flags & PF_X) ? (1 << Execute) : 0) );

                load(va, fs, offs, prop);

                // Any extra should be zeroed
                if (ms > fs) {
                    clear(va + fs, ms - fs, prop);
                }

                vm_start_ = std::min(vm_start_, va);
                vm_end_   = std::max(vm_end_, va + ms);
            }
        }
    }

    void loadFile() override {
        openAndScanElf();
        loadelf();
    }

    addr_t findSymbol(const string &name) override {
        std::ifstream &input = *fstream_;
        auto &elf = elf_info_;
        bool is_64 = elf.is_64;
        ASSERT_MSG(elf.has_symtab, "symtab not found in ELF");
        ASSERT_MSG(elf.has_strtab, "strtab not found in ELF");
        unsigned entsize = ELF_M(is_64, elf.symtab, sh_entsize);
        unsigned entnum  = ELF_M(is_64, elf.symtab, sh_size) / entsize;
        uint64_t str_off = ELF_M(is_64, elf.strtab, sh_offset);
        char buf[256];
        Elf_Sym sym;
        for (unsigned i = 0; i < entnum; i++ ) {
            elf.readSymIdx(input, sym, i);
            auto st_info = ELF_M(is_64, sym, st_info);
            auto st_type = is_64 ? ELF64_ST_TYPE(st_info) : ELF32_ST_TYPE(st_info);
            if (st_type == STT_FUNC || st_type == STT_OBJECT) {
                uint64_t name_off = ELF_M(is_64, sym, st_name);
                input.seekg(str_off + name_off);
                input.read(buf, sizeof(buf));
                if (buf == name)
                    return ELF_M(is_64, sym, st_value);
            }
        }
        return -1UL;
    }

    static bool checkElfFormat(const std::string &path) {
        std::ifstream input(path);
        ElfInfo elf;
        elf.readInitEhdr(input);
        if (!input.good() || !elf.check_magic() || !elf.isExecutable()) {
            return false;
        }
        return true;
    }
};


template <>
ExeLoader * ExeLoader::create<ElfLoader>(const std::string &path) {
    if (ElfLoader::checkElfFormat(path))
        return new ElfLoader(path);
    return nullptr;
}


} // namespace

