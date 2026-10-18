"""pe_util.py - PE32 Manipulation and Inspection Helper for ZuCompleluxe Patcher Pipeline.

Provides robust, self-contained parsing, RVA/offset conversions, section additions,
IAT lookups, and valid PE checksum recalculations.
"""

import struct
import os

def align(val, alignment):
    if alignment <= 0:
        return val
    return (val + alignment - 1) // alignment * alignment

def compute_pe_checksum(data, pe_offset):
    """Computes standard Microsoft PE checksum."""
    checksum_offset = pe_offset + 24 + 64
    checksum = 0
    length = len(data)
    remainder = length % 4
    limit = length - remainder
    for i in range(0, limit, 4):
        if i == checksum_offset:
            continue
        dword = struct.unpack_from('<I', data, i)[0]
        checksum += dword
        if checksum > 0xFFFFFFFF:
            checksum = (checksum & 0xFFFFFFFF) + (checksum >> 32)
    if remainder:
        val = 0
        for j in range(remainder):
            val |= (data[limit + j] << (j * 8))
        checksum += val
        if checksum > 0xFFFFFFFF:
            checksum = (checksum & 0xFFFFFFFF) + (checksum >> 32)
    checksum = (checksum & 0xFFFF) + (checksum >> 16)
    checksum = checksum + (checksum >> 16)
    checksum = checksum & 0xFFFF
    return (checksum + length) & 0xFFFFFFFF


class Section:
    def __init__(self, name, virtual_size, virtual_address, raw_size, raw_offset, characteristics, header_offset):
        self.name = name  # bytes, up to 8 chars
        self.name_str = name.rstrip(b'\x00').decode('latin1', 'ignore')
        self.virtual_size = virtual_size
        self.virtual_address = virtual_address  # RVA
        self.raw_size = raw_size
        self.raw_offset = raw_offset
        self.characteristics = characteristics
        self.header_offset = header_offset

    def __repr__(self):
        return (f"<Section {self.name_str:8s} RVA=0x{self.virtual_address:08X} "
                f"VSz=0x{self.virtual_size:X} RawOff=0x{self.raw_offset:X} "
                f"RawSz=0x{self.raw_size:X} Chars=0x{self.characteristics:08X}>")


class PEFile:
    def __init__(self, filepath_or_data):
        if isinstance(filepath_or_data, (bytes, bytearray)):
            self.data = bytearray(filepath_or_data)
            self.filepath = None
        else:
            self.filepath = filepath_or_data
            with open(filepath_or_data, 'rb') as f:
                self.data = bytearray(f.read())
        self.parse()

    def parse(self):
        d = self.data
        if len(d) < 0x40 or d[:2] != b'MZ':
            raise ValueError("Not a valid DOS executable")
        self.pe_offset = struct.unpack_from('<I', d, 0x3C)[0]
        if self.pe_offset + 4 > len(d) or d[self.pe_offset:self.pe_offset+4] != b'PE\x00\x00':
            raise ValueError("Not a valid PE executable")

        coff_off = self.pe_offset + 4
        (self.machine, self.num_sections, self.time_date_stamp,
         _ptr_sym, _num_sym, self.opt_hdr_size, self.characteristics) = struct.unpack_from('<HHIIIHH', d, coff_off)

        opt_off = coff_off + 20
        self.magic = struct.unpack_from('<H', d, opt_off)[0]
        if self.magic != 0x10B:
            raise ValueError(f"Expected PE32 (0x10B), got magic 0x{self.magic:X}")

        (self.major_linker, self.minor_linker, self.size_of_code,
         self.size_of_init_data, self.size_of_uninit_data,
         self.entry_point_rva, self.base_of_code, self.base_of_data,
         self.image_base, self.section_alignment, self.file_alignment,
         self.major_os, self.minor_os, self.major_img, self.minor_img,
         self.major_subsys, self.minor_subsys, self.win32_version,
         self.size_of_image, self.size_of_headers, self.checksum,
         self.subsystem, self.dll_characteristics,
         self.size_of_stack_reserve, self.size_of_stack_commit,
         self.size_of_heap_reserve, self.size_of_heap_commit,
         self.loader_flags, self.num_rva_and_sizes) = struct.unpack_from('<BBIIIIIIIIIHHHHHHIIIIHHIIIIII', d, opt_off + 2)

        dirs_off = opt_off + 96
        self.data_directories = []
        for i in range(self.num_rva_and_sizes):
            rva, size = struct.unpack_from('<II', d, dirs_off + i * 8)
            self.data_directories.append({'rva': rva, 'size': size, 'offset': dirs_off + i * 8})

        sec_table_off = opt_off + self.opt_hdr_size
        self.sec_table_offset = sec_table_off
        self.sections = []
        for i in range(self.num_sections):
            sh_off = sec_table_off + i * 40
            name = bytes(d[sh_off:sh_off+8])
            vsize, vaddr, rsize, roff = struct.unpack_from('<IIII', d, sh_off + 8)
            chars = struct.unpack_from('<I', d, sh_off + 36)[0]
            sec = Section(name, vsize, vaddr, rsize, roff, chars, sh_off)
            self.sections.append(sec)

    def rva_to_offset(self, rva):
        for sec in self.sections:
            span = max(sec.virtual_size, sec.raw_size)
            if sec.virtual_address <= rva < sec.virtual_address + span:
                return sec.raw_offset + (rva - sec.virtual_address)
        return None

    def offset_to_rva(self, off):
        for sec in self.sections:
            if sec.raw_offset <= off < sec.raw_offset + sec.raw_size:
                return sec.virtual_address + (off - sec.raw_offset)
        return None

    def va_to_offset(self, va):
        rva = va - self.image_base
        return self.rva_to_offset(rva)

    def offset_to_va(self, off):
        rva = self.offset_to_rva(off)
        if rva is not None:
            return self.image_base + rva
        return None

    def find_section_by_name(self, name_str):
        name_bytes = name_str.encode('latin1')
        for sec in self.sections:
            if sec.name.rstrip(b'\x00') == name_bytes:
                return sec
        return None

    def add_section(self, name_str, raw_data, characteristics, virtual_size=None):
        """Appends a new section to the PE header and file data.
        name_str: up to 8 characters string (e.g. '.zdata')
        raw_data: bytes or bytearray
        characteristics: uint32 PE section flags
        virtual_size: optional int, defaults to max(len(raw_data), raw_size)
        """
        d = self.data
        if self.find_section_by_name(name_str):
            raise ValueError(f"Section {name_str} already exists in PE")

        # 1. Check free header space
        hdr_end = self.sec_table_offset + self.num_sections * 40
        first_raw = min((s.raw_offset for s in self.sections if s.raw_offset > 0), default=self.size_of_headers)
        if hdr_end + 40 > first_raw:
            raise RuntimeError("No free header space to add section header")
        if any(d[hdr_end:hdr_end + 40]):
            raise RuntimeError("Header space is not zeroed")

        # 2. Calculate new section VA and RawOffset
        last_sec = max(self.sections, key=lambda s: s.virtual_address)
        new_vaddr = align(last_sec.virtual_address + max(last_sec.virtual_size, last_sec.raw_size), self.section_alignment)

        raw_size = align(len(raw_data), self.file_alignment)
        if virtual_size is None:
            virtual_size = max(len(raw_data), raw_size)
        else:
            virtual_size = max(virtual_size, len(raw_data))

        # Pad existing file to file_alignment if needed
        file_len_aligned = align(len(d), self.file_alignment)
        if len(d) < file_len_aligned:
            d += b'\x00' * (file_len_aligned - len(d))
        new_raw_offset = len(d)

        # Pad raw data to file_alignment and append
        padded_raw = bytearray(raw_data)
        if len(padded_raw) < raw_size:
            padded_raw += b'\x00' * (raw_size - len(padded_raw))
        d += padded_raw

        # 3. Write section header (40 bytes)
        name_bytes = (name_str.encode('latin1') + b'\x00' * 8)[:8]
        sh = bytearray(40)
        sh[0:8] = name_bytes
        struct.pack_into('<IIIIIIHHI', sh, 8,
                         virtual_size, new_vaddr, raw_size, new_raw_offset,
                         0, 0, 0, 0, characteristics)
        d[hdr_end:hdr_end + 40] = sh

        # 4. Update PE Header fields
        self.num_sections += 1
        struct.pack_into('<H', d, self.pe_offset + 6, self.num_sections)

        new_size_of_image = align(new_vaddr + virtual_size, self.section_alignment)
        self.size_of_image = new_size_of_image
        struct.pack_into('<I', d, self.pe_offset + 24 + 56, new_size_of_image)

        # 5. Re-parse internal state
        self.parse()
        new_sec = self.find_section_by_name(name_str)
        return new_sec

    def set_data_directory(self, index, rva, size):
        """Updates DataDirectory[index]."""
        if index >= len(self.data_directories):
            raise IndexError(f"DataDirectory index {index} out of range")
        entry = self.data_directories[index]
        entry['rva'] = rva
        entry['size'] = size
        struct.pack_into('<II', self.data, entry['offset'], rva, size)

    def set_entry_point(self, entry_rva):
        self.entry_point_rva = entry_rva
        struct.pack_into('<I', self.data, self.pe_offset + 24 + 16, entry_rva)

    def get_imports(self):
        """Parses import descriptors and returns dict:
        { dll_name_lower: { func_name: iat_va } }
        """
        if len(self.data_directories) <= 1:
            return {}
        imp_dir = self.data_directories[1]
        imp_rva = imp_dir['rva']
        if imp_rva == 0:
            return {}

        imp_off = self.rva_to_offset(imp_rva)
        if imp_off is None:
            return {}

        results = {}
        d = self.data
        p = imp_off
        while True:
            if p + 20 > len(d):
                break
            oft, time_date, fwd, name_rva, ft = struct.unpack_from('<IIIII', d, p)
            if oft == 0 and time_date == 0 and fwd == 0 and name_rva == 0 and ft == 0:
                # Check if there is a secondary descriptor block (e.g. sidecars) immediately following
                next_p = p + 20
                if next_p + 20 <= len(d):
                    noft, _, _, nname, nft = struct.unpack_from('<IIIII', d, next_p)
                    if not (noft == 0 and nname == 0 and nft == 0):
                        p = next_p
                        continue
                break
            name_off = self.rva_to_offset(name_rva)
            if name_off is None:
                p += 20
                continue
            dll_name = d[name_off:d.index(b'\x00', name_off)].decode('latin1', 'ignore').lower()

            func_dict = {}
            thunk_rva = oft or ft
            thunk_off = self.rva_to_offset(thunk_rva)
            if thunk_off is not None:
                idx = 0
                while True:
                    entry_val = struct.unpack_from('<I', d, thunk_off + idx * 4)[0]
                    if entry_val == 0:
                        break
                    iat_slot_va = self.image_base + ft + idx * 4
                    if not (entry_val & 0x80000000):  # Import by name
                        hn_off = self.rva_to_offset(entry_val)
                        if hn_off is not None:
                            fn_name = d[hn_off + 2:d.index(b'\x00', hn_off + 2)].decode('latin1', 'ignore')
                            func_dict[fn_name] = iat_slot_va
                    else:
                        ordinal = entry_val & 0xFFFF
                        func_dict[f"ord_{ordinal}"] = iat_slot_va
                    idx += 1

            if dll_name not in results:
                results[dll_name] = {}
            results[dll_name].update(func_dict)
            p += 20

        return results

    def find_iat(self, dll_name, func_name):
        """Finds the IAT slot VA for a specific dll and function.
        Returns int VA or None.
        """
        imports = self.get_imports()
        dll_dict = imports.get(dll_name.lower())
        if not dll_dict:
            return None
        return dll_dict.get(func_name)

    def get_exports(self):
        """Extracts list of exported function names from IMAGE_DIRECTORY_ENTRY_EXPORT.
        Returns list of str (names).
        """
        if not self.data_directories or len(self.data_directories) < 1:
            return []
        export_dir = self.data_directories[0]
        if export_dir['rva'] == 0 or export_dir['size'] == 0:
            return []
        off = self.rva_to_offset(export_dir['rva'])
        if off is None or off + 40 > len(self.data):
            return []

        (characteristics, timedate, major, minor, name_rva, base,
         num_funcs, num_names, funcs_rva, names_rva, ords_rva) = struct.unpack_from('<IIHHIIIIIII', self.data, off)

        names_off = self.rva_to_offset(names_rva)
        if names_off is None or num_names == 0:
            return []

        exported_names = []
        for i in range(num_names):
            p_name_rva = struct.unpack_from('<I', self.data, names_off + i * 4)[0]
            p_name_off = self.rva_to_offset(p_name_rva)
            if p_name_off is not None:
                end = self.data.find(b'\x00', p_name_off)
                if end != -1:
                    name = self.data[p_name_off:end].decode('ascii', errors='ignore')
                    exported_names.append(name)
        return exported_names

    def update_checksum(self):
        """Calculates and writes valid PE checksum into OptionalHeader."""
        cs = compute_pe_checksum(self.data, self.pe_offset)
        self.checksum = cs
        struct.pack_into('<I', self.data, self.pe_offset + 24 + 64, cs)
        return cs

    def save(self, filepath=None):
        self.update_checksum()
        out_path = filepath or self.filepath
        if not out_path:
            raise ValueError("No output path specified")
        with open(out_path, 'wb') as f:
            f.write(self.data)
