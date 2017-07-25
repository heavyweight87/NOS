// Pager.cpp - Working with page tables.

#include <cstdint>
#include <new>
#include <Pager.h>
#include <Symbol.h>
#include INC_ARCH(PageTable.h)

namespace Kernel
{
	namespace Pager
	{
		static uintptr_t VirtMapEnd = Symbol::kernelEnd.Addr();

		template<Memory::PageBits bits> void MapPage(Memory::PhysAddr phys, uintptr_t virt, Memory::MemType type)
		{
			static_assert(IsValidSize(bits), "invalid page size");
			static_assert(PageSizeLevel<bits> < PAGE_LEVELS, "page table level too large");

			unsigned long tab = (virt & 0xffffffffffffULL) >> (bits + PAGE_BITS[PageSizeLevel<bits>]);
			unsigned long entry = (virt >> bits) & ((1 << PAGE_BITS[PageSizeLevel<bits>]) - 1);

			if(!PageTableLevel<PageSizeLevel<bits>>::Exists(tab))
				PageTableLevel<PageSizeLevel<bits>>::Create(tab, ParentType(type));

			PageTableEntry& pte = PageTableLevel<PageSizeLevel<bits>>::Table(tab).Entry(entry);
			pte.Set<bits>(phys, type);
			Invalidate(virt);
		}

		template<Memory::PageBits bits> void UnmapPage(uintptr_t virt)
		{
			static_assert(IsValidSize(bits), "invalid page size");
			static_assert(PageSizeLevel<bits> < PAGE_LEVELS, "page table level too large");

			unsigned int tab = (virt & 0xffffffffffffULL) >> (bits + PAGE_BITS[PageSizeLevel<bits>]);
			unsigned long entry = (virt >> bits) & ((1 << PAGE_BITS[PageSizeLevel<bits>]) - 1);

			PageTableLevel<PageSizeLevel<bits>>& pt = PageTableLevel<PageSizeLevel<bits>>::Table(tab);
			PageTableEntry& pte = pt.Entry(entry);
			pte.Clear();
			Invalidate(virt);

			if(pt.IsEmpty())
				pt.Destroy();
		}

#if (defined ARCH_X86_IA32) && (!defined CONFIG_PAE)
		template<> void MapPage<Memory::PGB_4M>(Memory::PhysAddr phys, uintptr_t virt, Memory::MemType type)
		{
			unsigned long entry = virt >> Memory::PGB_4M;

			PageTableEntry& pte = PageTableTop().Entry(entry);
			pte.Set<Memory::PGB_4M>(phys, type);
			Invalidate(virt);
		}

		template<> void UnmapPage<Memory::PGB_4M>(uintptr_t virt)
		{
			unsigned long entry = virt >> Memory::PGB_4M;

			PageTableEntry& pte = PageTableTop().Entry(entry);
			pte.Clear();
			Invalidate(virt);
		}
#endif

		template void MapPage<Memory::PGB_4K>(Memory::PhysAddr phys, uintptr_t virt, Memory::MemType type);
		template void UnmapPage<Memory::PGB_4K>(uintptr_t virt);
#if (defined CONFIG_PAE) || (defined ARCH_X86_AMD64)
		template void MapPage<Memory::PGB_2M>(Memory::PhysAddr phys, uintptr_t virt, Memory::MemType type);
		template void UnmapPage<Memory::PGB_2M>(uintptr_t virt);
#else
		template void MapPage<Memory::PGB_4M>(Memory::PhysAddr phys, uintptr_t virt, Memory::MemType type);
		template void UnmapPage<Memory::PGB_4M>(uintptr_t virt);
#endif
#ifdef ARCH_X86_AMD64
		template void MapPage<Memory::PGB_1G>(Memory::PhysAddr phys, uintptr_t virt, Memory::MemType type);
		template void UnmapPage<Memory::PGB_1G>(uintptr_t virt);
#endif

		void* MapBootRegion(Memory::PhysAddr start, Memory::PhysAddr length, Memory::MemType type)
		{
			Memory::PhysAddr first, last;
			uintptr_t virt;

			first = start & ~Memory::PGM_4K;
			last = ((start + length - 1) & ~Memory::PGM_4K) + Memory::PGS_4K;

			if(!Map(first, VirtMapEnd, last - first, type))
				return nullptr;

			virt = VirtMapEnd + (start & Memory::PGM_4K);
			VirtMapEnd += (last - first);
			return reinterpret_cast<void*>(virt);
		}

		void* MapBootString(Memory::PhysAddr start, Memory::MemType type)
		{
			Memory::PhysAddr first;
			void* ret;
			char* p;

			first = start & ~Memory::PGM_4K;

			if(!Map(first, VirtMapEnd, Memory::PGS_4K, type))
				return nullptr;

			ret = reinterpret_cast<void*>(VirtMapEnd + (start & Memory::PGM_4K));
			p = reinterpret_cast<char*>(ret);

			while(true)
			{
				VirtMapEnd += Memory::PGS_4K;
				first += Memory::PGS_4K;

				do
				{
					if(!(*(p++)))
						return(ret);
				}
				while((Memory::PGM_4K & reinterpret_cast<uintptr_t>(p)) != 0);

				if(!Map(first, VirtMapEnd, Memory::PGS_4K, type))
					return nullptr;
			}
		}

		void FreeBootMemory(void)
		{
		}
	}
}

