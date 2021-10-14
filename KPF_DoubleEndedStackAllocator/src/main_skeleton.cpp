/**
* Exercise: "Growing DoubleEndedStackAllocator with Canaries (VMEM)"
* Group members: Handl Anja (gs20m005), Tributsch Harald (gs20m008), Leithner Michael (gs20m012)
**/

#include <cassert>
#include <iostream>
#include <malloc.h>
// TODO: Rephrase this comment
//#include <unordered_set> // used for setup a set to test invalid aligns

// Color defines for test results
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"

namespace Tests
{
	void Test_Case_Success(const char* name, bool passed)
	{
		printf("[%s] %s" ANSI_COLOR_RESET " the test!\n", name, passed ? ANSI_COLOR_GREEN "passed" : ANSI_COLOR_RED "failed");
	}

	void Test_Case_Failure(const char* name, bool passed)
	{
		printf("[%s] %s" ANSI_COLOR_RESET " the test!\n", name, !passed ? ANSI_COLOR_GREEN "passed" : ANSI_COLOR_RED "failed");
	}

	/**
	* Example of how a test case can look like. The test cases in the end will check for
	* allocation success, proper alignment, overlaps and similar situations. This is an
	* example so you can already try to cover all cases you judge as being important by
	* yourselves.
	**/
	template<class A>
	bool VerifyAllocationSuccess(A& allocator, size_t size, size_t alignment)
	{
		void* mem = allocator.Allocate(size, alignment);
		if (mem == nullptr)
		{
			printf(ANSI_COLOR_RED "[Error]" ANSI_COLOR_RESET ": Allocator returned nullptr!\n");
			return false;
		}

		return true;
	}
}

// Assignment functionality tests are going to be included here

#define WITH_DEBUG_CANARIES		1	// Using extra space for canaries
#define HTL_WITH_DEBUG_OUTPUT	0	// Debug output
#define HTL_PREVENT_COPY		1	// Prevent copy ctor and operator
#define HTL_PREVENT_MOVE		1	// Prevent move ctor and operator
#define HTL_ALLOW_GROW			1	// Allow growing by using virtual memory

// TODO: can this be removed? Linux support is not given anymore :(
//#undef assert
//#define assert(arg)

#if HTL_ALLOW_GROW
	#include<windows.h>
#endif

/**
* You work on your DoubleEndedStackAllocator. Stick to the provided interface, this is
* necessary for testing your assignment in the end. Don't remove or rename the public
* interface of the allocator. Also don't add any additional initialization code, the
* allocator needs to work after it was created and its constructor was called. You can
* add additional public functions but those should only be used for your own testing.
**/
class DoubleEndedStackAllocator
{
public:
#if HTL_ALLOW_GROW
	// TODO: ask if this is okay -> supporting a default param to enable custom virtual memory size for testing
	DoubleEndedStackAllocator(size_t max_size, size_t realMaxSize = mAllocatedSize)
#else
	DoubleEndedStackAllocator(size_t max_size)
#endif
	{
		// TODO: Do we need this?
		static_assert(sizeof(size_t) == sizeof(uintptr_t), "Size mismatch of size_t and uintptr_t");

		// Reserve Memory and init Pointers
#if HTL_ALLOW_GROW
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		mPageSize = si.dwPageSize;
		//printf("page size: %u bytes, allocation granularity: %u\n", mPageSize, si.dwAllocationGranularity);
		// -> PageSize 4096 bytes, allocation granularity 65536

		// First reserve memory from virtual space
		void* begin = VirtualAlloc(NULL, realMaxSize, MEM_RESERVE, PAGE_READWRITE);
		if (!begin)
		{
			printf(ANSI_COLOR_RED "[Error]" ANSI_COLOR_RESET ": Not enough virtual memory to construct!\n");
			throw std::bad_alloc();
		}
#if HTL_WITH_DEBUG_OUTPUT
		printf("reserved virtual memory from [%llx] to [%llx] for size %zu\n", reinterpret_cast<uintptr_t>(begin), (reinterpret_cast<uintptr_t>(begin) + realMaxSize), realMaxSize);
#endif

		// Then commit a page of space for front...
		begin = VirtualAlloc(begin, mPageSize, MEM_COMMIT, PAGE_READWRITE);
		if (!begin)
		{
			printf(ANSI_COLOR_RED "[Error]" ANSI_COLOR_RESET ": Could not commit begin page\n");
			throw std::bad_alloc();
		}
		mBegin = mFront = reinterpret_cast<uintptr_t>(begin);
		mPageEnd = mFront + mPageSize;
#if HTL_WITH_DEBUG_OUTPUT
		printf("mPageEnd   [%llx]\n", mPageEnd);
#endif

		// ...and for back
		begin = VirtualAlloc(reinterpret_cast<void*>(mBegin + realMaxSize - mPageSize), mPageSize, MEM_COMMIT, PAGE_READWRITE);
		if (!begin)
		{
			printf(ANSI_COLOR_RED "[Error]" ANSI_COLOR_RESET ": Could not commit end page\n");
			throw std::bad_alloc();
		}
		mPageStart = reinterpret_cast<uintptr_t>(begin);
		mEnd = mBack = mPageStart + mPageSize;
#if HTL_WITH_DEBUG_OUTPUT
		printf("mPageStart [%llx]\n", mPageStart);
#endif

#else
		void* begin = malloc(max_size);
		if (!begin)
		{
			printf(ANSI_COLOR_RED "[Error]" ANSI_COLOR_RESET ": Not enough stack memory to construct!\n");
			throw std::bad_alloc();
		}

		mBegin = mFront = reinterpret_cast<uintptr_t>(begin);
		mEnd = mBack = mBegin + max_size;
#if HTL_WITH_DEBUG_OUTPUT
		// Custom debug output
		printf("constructed allocator from \n[%llx] to\n[%llx]\n", mBegin, mEnd);
		printf("size: [%zu]\n", max_size);
		printf("diff: [%llu]\n", mEnd - mBegin);
#endif

#endif
	}

	~DoubleEndedStackAllocator(void)
	{
		Reset();

		// release reserved memory back to system
		void* begin = reinterpret_cast<void*>(mBegin);
		if (begin)
		{
#if HTL_ALLOW_GROW
			VirtualFree(begin, 0, MEM_RELEASE);
#else
			free(begin);
#endif
		}
	}

	// allocate given amount of memory with alignment
	// if there is not enough memory left or input params are invalid -> assert and returns a nullptr
	void* Allocate(size_t size, size_t alignment)
	{
		if (false == IsPowerOf2(alignment))
		{
			assert(!"alignment musst be a power of 2!");
			return nullptr;
		}

		uintptr_t lastItem = mFront;
		uintptr_t newFront = mFront;

		// Jump to next free address -> If Front == Begin -> Front is free address
		if (mFront != mBegin)
		{
			MetaData* meta = reinterpret_cast<MetaData*>(mFront - META_SIZE);
			newFront = mFront + meta->Size + CANARY_SIZE;
		}

		uintptr_t alignedAddress = AlignUp(newFront + CANARY_SIZE + META_SIZE, alignment);

#if HTL_ALLOW_GROW
		// Commit additional space if necessary
		while ((alignedAddress + size + CANARY_SIZE) > mPageEnd)
		{
			void* begin = VirtualAlloc(reinterpret_cast<void*>(mPageEnd), mPageSize, MEM_COMMIT, PAGE_READWRITE);
			if (!begin)
			{
				printf(ANSI_COLOR_RED "[Error]" ANSI_COLOR_RESET ": Could not commit additional front page\n");
				assert(!"alignment musst be a power of 2!");
				return nullptr;
			}
			mPageEnd += mPageSize;
#if HTL_WITH_DEBUG_OUTPUT
			printf("commited new Page Front   [%llx]\n", mPageEnd);
#endif
		}
#endif // HTL_ALLOW_GROW

		// Check if front allocation would overlap with back allocation
		bool overlap = false;
		if (mBack == mEnd)
		{
			overlap |= (alignedAddress + size + CANARY_SIZE) >= mBack;	// In this case, there are no back allocations
		}
		else
		{
			overlap |= (alignedAddress + size + CANARY_SIZE) >= (mBack - META_SIZE - CANARY_SIZE);
		}

		if (overlap)
		{
			assert(!"Front Stack overlaps with Back Stack");
			//printf("[Warning]: Front Stack overlaps with Back Stack!\n");
			return nullptr;
		}

		mFront = alignedAddress;
#if WITH_DEBUG_CANARIES
		WriteBeginCanary(alignedAddress);
		WriteEndCanary(alignedAddress, size);
#endif
		WriteMeta(alignedAddress, lastItem, size);


		return reinterpret_cast<void*>(mFront);
	}

	void* AllocateBack(size_t size, size_t alignment)
	{
		if (false == IsPowerOf2(alignment))
		{
			assert(!"alignment musst be a power of 2!");
			return nullptr;
		}

		uintptr_t lastItem = mBack;
		uintptr_t newBack = mBack - size - CANARY_SIZE;

		// Jump to next free address -> If Back == End -> Back is free address
		if (mEnd != mBack)
		{
			newBack = mBack - META_SIZE - 2 * CANARY_SIZE - size;
		}

		uintptr_t alignedAddress = AlignDown(newBack, alignment);

#if HTL_ALLOW_GROW
		// Commit additional space if necessary
		while ((alignedAddress - META_SIZE - CANARY_SIZE) < mPageStart)
		{
			void* begin = VirtualAlloc(reinterpret_cast<void*>(mPageStart - mPageSize), mPageSize, MEM_COMMIT, PAGE_READWRITE);
			if (!begin)
			{
				printf(ANSI_COLOR_RED "[Error]" ANSI_COLOR_RESET ": Could not commit additional end page\n");
				assert(!"alignment musst be a power of 2!");
				return nullptr;
			}
			mPageStart = mPageStart - mPageSize;
#if HTL_WITH_DEBUG_OUTPUT
			printf("commited new PageBack     [%llx]\n", mPageStart);
#endif
		}
#endif // HTL_ALLOW_GROW

		// Check if back allocation would overlap with front allocation
		bool overlap = false;
		if (mFront == mBegin)
		{
			overlap |= mBack - META_SIZE - CANARY_SIZE <= mFront;	// In this case, there are no front allocations
		}
		else
		{
			overlap |= (mBack - META_SIZE - CANARY_SIZE) <= (mFront + GetMetaData(mFront)->Size + CANARY_SIZE);
		}

		if (overlap)
		{
			assert(!"Back Stack overlaps with Front Stack");
			//printf("[Warning]: Back Stack overlaps with Front Stack!\n");
			return nullptr;
		}

		mBack = alignedAddress;

#if WITH_DEBUG_CANARIES
		WriteBeginCanary(alignedAddress);
		WriteEndCanary(alignedAddress, size);
#endif
		
		WriteMeta(alignedAddress, lastItem, size);

		return reinterpret_cast<void*>(mBack);
	}

	// free previously allocated memory
	// does nothing if provided address does not fit last allocation (LIFO requirement)
	// asserts if detects overwritten canaries if WITH_DEBUG_CANARIES is enabled
	void Free(void* memory)
	{
		if (!ValidateMemoryPointer(memory))
		{
			return;
		}

		// LIFO check
		if (reinterpret_cast<uintptr_t>(memory) != mFront)
		{
			assert(!"Pointer doesn't match last allocated memory, couldn't free memory");
			//printf("[Warning]: Pointer doesn't match last allocated memory, couldn't free memory!\n");
			return;
		}

		MetaData* currentMetadata = GetMetaData(reinterpret_cast<uintptr_t>(memory));

#if WITH_DEBUG_CANARIES
		CheckCanaries(reinterpret_cast<uintptr_t>(memory), currentMetadata->Size);
#endif

		// we don't care what the user has written in the memory, therefore we just set the Front to the LastItem and "ignore" the previously allocated memory
		mFront = currentMetadata->LastItem;
	}

	void FreeBack(void* memory)
	{
		if (!ValidateMemoryPointer(memory))
		{
			return;
		}

		// LIFO check
		if (reinterpret_cast<uintptr_t>(memory) != mBack)
		{
			assert(!"Pointer doesn't match last allocated memory, couldn't free memory");
			//printf("[Warning]: Pointer doesn't match last allocated memory, couldn't free memory!\n");
			return;
		}

		MetaData* currentMetadata = GetMetaData(reinterpret_cast<uintptr_t>(memory));

#if WITH_DEBUG_CANARIES
		CheckCanaries(reinterpret_cast<uintptr_t>(memory), currentMetadata->Size);
#endif

		// we don't care what the user has written in the memory, therefore we just set the Back to the LastItem and "ignore" the previously allocated memory
		mBack = currentMetadata->LastItem;
	}

	void Reset(void)
	{
		while (mFront != mBegin)
		{
			Free(reinterpret_cast<void*>(mFront));
		}

		while (mBack != mEnd)
		{
			FreeBack(reinterpret_cast<void*>(mBack));
		}

		// TODO: Rephrase this comment
		// just setting internal pointers would be faster,
		// but would skip pointer and canary validation
		// mFront = mBegin;
		// mBack = mEnd;
	}

	// Needed for testing
	const void* Begin()
	{
		return reinterpret_cast<void*>(mBegin);
	}

	const void* Front()
	{
		return reinterpret_cast<void*>(mFront);
	}

	const void* End()
	{
		return reinterpret_cast<void*>(mEnd);
	}

	const void* Back()
	{
		return reinterpret_cast<void*>(mBack);
	}

	static size_t GetCanaraySize()
	{
		return CANARY_SIZE;
	}

	static size_t GetMetaSize()
	{
		return META_SIZE;
	}

private:

	// because no interface was given and the auto-generated default copy/move ctor would cause problems
	// we either have to implement our own custom functionality or remove them
	// -> we decided to prevent copy and move because in this context to us it does not make much sense
	// to copy or move an created allocator and we didn't want to crack our heads on errors, undefined behavior
	// or the usage of an internal mapping table to support invalidated pointers
#if HTL_PREVENT_COPY
	DoubleEndedStackAllocator(const DoubleEndedStackAllocator&) = delete;
	DoubleEndedStackAllocator& operator = (const DoubleEndedStackAllocator&) = delete;
#endif

#if HTL_PREVENT_MOVE
	DoubleEndedStackAllocator(const DoubleEndedStackAllocator&&) = delete;
	DoubleEndedStackAllocator& operator = (const DoubleEndedStackAllocator&&) = delete;
#endif

	// Power of 2 always has exactly 1 bit set in binary representation (for signed values)
	bool IsPowerOf2(size_t val)
	{
		return val > 0 && !(val & (val - 1));
	}

#if WITH_DEBUG_CANARIES
	void WriteBeginCanary(uintptr_t alignedAddress)
	{
		uintptr_t canaryAddress = alignedAddress - META_SIZE - CANARY_SIZE;
		//printf("Begin canaryAddress: [%llx]\n", canaryAddress);
		*reinterpret_cast<uint32_t*>(canaryAddress) = CANARY;
	}

	void WriteEndCanary(uintptr_t alignedAddress, size_t size)
	{
		uintptr_t canaryAddress = alignedAddress + size;
		//printf("End canaryAddress: [%llx]\n", canaryAddress);
		*reinterpret_cast<uint32_t*>(canaryAddress) = CANARY;
	}
#endif

	// Memory Pointer needs to be valid
	bool ValidateMemoryPointer(void* memory)
	{
		if (memory == nullptr)
		{
			assert(!"Invalid Pointer, couldn't free memory");
			return false;
		}

		if ((reinterpret_cast<uintptr_t>(memory) < mBegin) || (reinterpret_cast<uintptr_t>(memory) > mEnd))
		{
			assert(!"Pointer not in range of reserved space, couldn't free memory");
			return false;
		}
		return true;
	}

#if WITH_DEBUG_CANARIES
	// if canaries are not valid, we're not allowed to free, because something has overwritten them
	void CheckCanaries(uintptr_t alignedAddress, size_t size)
	{
		// check begin canary
		uintptr_t canaryAddress = alignedAddress - META_SIZE - CANARY_SIZE;
		if (*reinterpret_cast<uint32_t*>(canaryAddress) != CANARY)
		{
			assert(!"Invalid Begin Canary");
			//printf("[Warning]: Invalid Front Canary!\n");
		}

		// check end canary
		canaryAddress = alignedAddress + size;
		if (*reinterpret_cast<uint32_t*>(canaryAddress) != CANARY)
		{
			assert(!"Invalid End Canary");
			//printf("[Warning]: Invalid End Canary!\n");
		}
	}
#endif

	void WriteMeta(uintptr_t alignedAddress, uintptr_t lastItem, size_t allocatedSize)
	{
		uintptr_t metaAddress = alignedAddress - META_SIZE;
		//printf("metaAddress: [%llx]\n", metaAddress);
		*reinterpret_cast<MetaData*>(metaAddress) = MetaData(lastItem, allocatedSize);
	}

	uintptr_t AlignUp(uintptr_t address, size_t alignment)
	{
		uintptr_t adjust = address % alignment; // Needed adjustment bits
		if (adjust == 0)
		{
			return address;
		}
		//printf("[Info]: needed adjustment bits: [%llu]\n\n", adjust);
		return (address + (alignment - adjust));
	}

	uintptr_t AlignDown(uintptr_t address, size_t alignment)
	{
		size_t offsetAddress = address;
		//uintptr_t adjust = offsetAddress % alignment; // Needed adjustment bits
		//printf("[Info]: needed adjustment bits: [%llu]\n\n", adjust);
		return (offsetAddress - (offsetAddress % alignment));
	}

	struct MetaData
	{
		MetaData(uintptr_t lastItem, size_t size)
			: LastItem(lastItem)
			, Size(size)
		{
		}
		uintptr_t LastItem;
		size_t Size;
	};

	MetaData* GetMetaData(uintptr_t allocSpacePtr)
	{
		return reinterpret_cast<MetaData*>(allocSpacePtr - META_SIZE);
	}

	static const ptrdiff_t META_SIZE = sizeof(MetaData);

#if WITH_DEBUG_CANARIES
	//static const uint32_t CANARY = 0xDEADC0DE;
	static const uint32_t CANARY = 0xDEC0ADDE;	// Reverse, because little/big endian
	static const ptrdiff_t CANARY_SIZE = sizeof(CANARY);
#else
	static const ptrdiff_t CANARY_SIZE = 0;
#endif

	// Boundaries of our allocation
	uintptr_t mBegin = 0;
	uintptr_t mEnd = 0;

	// TODO: Should we remove this comment?
	// decision: (A) using pointer to next/prev free memory or (B) points to user space begin
	// --> (B)
	uintptr_t mFront = 0;
	uintptr_t mBack = 0;

#if HTL_ALLOW_GROW
	static const size_t mAllocatedSize = 1024 * 1024 * 1024; // arbitrary maximum size of reserved virtual memory, for malloc using ctor param max_size
	DWORD mPageSize = 0; // Size of commitable pages in virtual memory

	uintptr_t mPageEnd = 0; // End of committed pages for front
	uintptr_t mPageStart = 0; // Begin of commited pages for back
#endif
};

// Mini-Visualization of our Double Ended Stack for better understanding
//					|	|	|	|
//					4	8	12	16
//			[0xCD]<meta>Front

// malloc / virtualAlloc
// [Begin ...															... End]
//  ^


// allocate
// [Begin, [0xCD], <meta>Front, [0xCD]									... End]
//                       ^

// allocate back
// [Begin, [0xCD], <meta>Front, [0xCD], ... [0xCD], <meta>Back, [0xCD]	... End]
//                       ^                                ^

// free
// [Begin, ...								[0xCD], <meta>Back, [0xCD], ... End]
//	^													  ^

// You can do whatever you want here in the main function
int main()
{
	// You can add your own tests here, I will call my tests at then end with a fresh instance of your allocator and a specific max_size
	{
		const size_t canarySize = DoubleEndedStackAllocator::GetCanaraySize();
		const size_t metaSize = DoubleEndedStackAllocator::GetMetaSize();
		// TODO: max supported alignment?
		const size_t maxAlign = 512; // SIZE_MAX;
		

#if HTL_ALLOW_GROW
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		DWORD pageSize = si.dwPageSize;
		const size_t allocSize = 1024;
#else
		const size_t allocSize = sizeof(uint32_t);
#endif

		/* ensure copy and move is not available
		DoubleEndedStackAllocator alloc(64U);
		DoubleEndedStackAllocator alloc2(alloc);
		DoubleEndedStackAllocator alloc3 = alloc;
		DoubleEndedStackAllocator alloc4(std::move(alloc));
		DoubleEndedStackAllocator alloc5 = std::move(alloc);*/

		// Success tests
		{
			{
				DoubleEndedStackAllocator alloc(64U);
				Tests::Test_Case_Success("Verify Allocation Success", [&alloc]()
				{
					void* ptr = alloc.Allocate(sizeof(uint32_t), 1);
					return ptr != nullptr
						&& ptr == alloc.Front()
						&& ptr > alloc.Begin()
						&& ptr < alloc.Back();
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Success("Verify Allocation Alignment", [&alloc, maxAlign]()
				{
					bool ret = true;
					size_t align = 2;
					while (align < maxAlign)
					{
						ret &= reinterpret_cast<uintptr_t>(alloc.Allocate(sizeof(uint32_t), align)) % align == 0;
						align <<= 1;
					}
					return ret;
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Success("Verify Multi Allocation", [&alloc]()
				{
					void* alloc1 = alloc.Allocate(sizeof(uint32_t), 2);
					void* alloc2 = alloc.Allocate(sizeof(uint32_t), 2);
					void* alloc3 = alloc.Allocate(sizeof(uint32_t), 2);
					return alloc1 < alloc2
						&& alloc1 < alloc3
						&& alloc2 < alloc3
						&& alloc1 > alloc.Begin()
						&& alloc3 == alloc.Front()
						&& alloc3 < alloc.Back();
				}());
			}
			{
				DoubleEndedStackAllocator alloc(64U);
				Tests::Test_Case_Success("Verify Back Allocation Success", [&alloc]()
				{
					void* ptr = alloc.AllocateBack(sizeof(uint32_t), 1);
					return ptr != nullptr
						&& ptr == alloc.Back()
						&& ptr < alloc.End()
						&& ptr > alloc.Front();
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Success("Verify Back Allocation Alignment", [&alloc, maxAlign]()
				{
					bool ret = true;
					size_t align = 2;
					while (align < maxAlign)
					{
						ret &= reinterpret_cast<uintptr_t>(alloc.AllocateBack(sizeof(uint32_t), align)) % align == 0;
						align <<= 1;
					}
					return ret;
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Success("Verify Multi Back Allocation", [&alloc]()
				{
					void* alloc1 = alloc.AllocateBack(sizeof(uint32_t), 2);
					void* alloc2 = alloc.AllocateBack(sizeof(uint32_t), 2);
					void* alloc3 = alloc.AllocateBack(sizeof(uint32_t), 2);
					return alloc1 > alloc2
						&& alloc1 > alloc3
						&& alloc2 > alloc3
						&& alloc1 < alloc.End()
						&& alloc3 == alloc.Back()
						&& alloc3 > alloc.Front();
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Success("Verify Free Success", [&alloc]()
				{
					void* ptr = alloc.Allocate(sizeof(uint32_t), 1);
					alloc.Free(ptr);
					return alloc.Front() == alloc.Begin();
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Success("Verify FreeBack Success", [&alloc]()
				{
					void* ptr = alloc.AllocateBack(sizeof(uint32_t), 1);
					alloc.FreeBack(ptr);
					return alloc.Back() == alloc.End();
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Success("Verify Multiple Free Success", [&alloc]()
				{
					void* alloc1 = alloc.Allocate(sizeof(uint32_t), 2);
					void* alloc2 = alloc.Allocate(sizeof(uint32_t), 2);
					void* alloc3 = alloc.Allocate(sizeof(uint32_t), 2);
					alloc.Free(alloc3);
					alloc.Free(alloc2);
					alloc.Free(alloc1);
					return alloc.Front() == alloc.Begin();
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Success("Verify Multiple FreeBack Success", [&alloc]()
				{
					void* alloc1 = alloc.AllocateBack(sizeof(uint32_t), 2);
					void* alloc2 = alloc.AllocateBack(sizeof(uint32_t), 2);
					void* alloc3 = alloc.AllocateBack(sizeof(uint32_t), 2);
					alloc.FreeBack(alloc3);
					alloc.FreeBack(alloc2);
					alloc.FreeBack(alloc1);
					return alloc.Back() == alloc.End();
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Success("Verify Reset Success", [&alloc]()
				{
					alloc.Allocate(sizeof(uint32_t), 2);
					alloc.Allocate(sizeof(uint32_t), 2);
					alloc.Allocate(sizeof(uint32_t), 2);
					alloc.AllocateBack(sizeof(uint32_t), 2);
					alloc.AllocateBack(sizeof(uint32_t), 2);
					alloc.AllocateBack(sizeof(uint32_t), 2);
					alloc.Reset();
					return alloc.Front() == alloc.Begin()
						&& alloc.Back() == alloc.End();
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Success("Verify Alloc after Free Success", [&alloc]()
				{
					void* alloc1 = alloc.Allocate(sizeof(uint32_t) * 3, 2);
					alloc.Free(alloc1);
					void* alloc2 = alloc.Allocate(sizeof(uint32_t) * 2, 2);
					return alloc.Front() == alloc2;
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Success("Verify Alloc Back after Free Back Success", [&alloc]()
				{
					void* alloc1 = alloc.AllocateBack(sizeof(uint32_t) * 3, 2);
					alloc.FreeBack(alloc1);
					void* alloc2 = alloc.AllocateBack(sizeof(uint32_t) * 2, 2);
					return alloc.Back() == alloc2;
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Success("Verify Alloc after Free Success", [&alloc]()
				{
					alloc.Allocate(sizeof(uint32_t) * 3, 2);
					alloc.Reset();
					void* alloc2 = alloc.Allocate(sizeof(uint32_t) * 2, 2);
					return alloc.Front() == alloc2;
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Success("Verify Alloc Back after Free Back Success", [&alloc]()
				{
					alloc.AllocateBack(sizeof(uint32_t) * 3, 2);
					alloc.Reset();
					void* alloc2 = alloc.AllocateBack(sizeof(uint32_t) * 2, 2);
					return alloc.Back() == alloc2;
				}());
			}
			// additional tests for virtual alloc
#if HTL_ALLOW_GROW
			{
				Tests::Test_Case_Success("Verify overlapping memory reservation Success", [pageSize]()
				{
					try
					{
						DoubleEndedStackAllocator alloc(sizeof(uint32_t), (size_t)(pageSize * 1.5));
						return true;
					}
					catch (...) {}
					return false;
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Success("Verify dynamic front page reservation Success", [&alloc, allocSize]()
				{
					size_t totalSize = 0;
					while (totalSize < 4096) // Fill more than a page
					{
						alloc.Allocate(allocSize, 32);
						totalSize += allocSize;
					}
					return true;
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Success("Verify dynamic back page reservation Success", [&alloc, allocSize]()
				{
					size_t totalSize = 0;
					while (totalSize < 4096) // Fill more than a page
					{
						alloc.AllocateBack(allocSize, 32);
						totalSize += allocSize;
					}
					return true;
				}());
			}
#endif
		}

		// Fail tests
#ifndef _DEBUG
		{
			/* We could test if double free or free without allocate just does nothing,
			*  but because we can rely on LIFO check, we don't support neither
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Failure("Verify fail on double Free", [&alloc]()
				{
					void* ptr = alloc.Allocate(sizeof(uint32_t), 1);
					alloc.Free(ptr);
					alloc.Free(ptr);
					return false;
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Failure("Verify fail on double FreeBack", [&alloc]()
				{
					void* ptr = alloc.AllocateBack(sizeof(uint32_t), 1);
					alloc.FreeBack(ptr);
					alloc.FreeBack(ptr);
					return false;
				}());
			}
			{

				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Failure("Verify fail on Free without Allocate", [&alloc]()
				{
					alloc.Free(const_cast<void*>(alloc.Front()));
					return false;
				}());
			}
			{

				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Failure("Verify fail on FreeBack without Allocate", [&alloc]()
				{
					alloc.FreeBack(const_cast<void*>(alloc.Back()));
					return false;
				}());
			}*/
			{
				Tests::Test_Case_Failure("Verify fail on memory allocation", []()
				{
					try
					{
#if HTL_ALLOW_GROW
						DoubleEndedStackAllocator alloc(SIZE_MAX, SIZE_MAX);
#else
						DoubleEndedStackAllocator alloc(SIZE_MAX);
#endif
						return true;
					}
					catch (...) {}
					return false;
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Failure("Verify fail on invalid align", [&alloc, maxAlign]()
				{
					// TODO: static hash_set (or build dynamic depending on maxAlign)
					// -> but would need to include unordered_set.h only for one test
					//std::unordered_set<size_t> s{ 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 };

					bool ret = true;
					void* mem = nullptr;
					size_t align = 0;
					while (align < maxAlign)
					{
						//if (1 == s.count(align)) {}
						if (align == 1 || align == 2 || align == 4 || align == 8 || align == 16 ||
							align == 32 || align == 64 || align == 128 || align == 256 || align == 512) {}
						else
						{
							mem = alloc.Allocate(sizeof(uint32_t), align);
							ret &= (mem == nullptr);

							if (false == ret) return true;
						}
						align++;
					}
					return !ret;
				}());
			}
			{
#if HTL_ALLOW_GROW
				DoubleEndedStackAllocator alloc(sizeof(uint32_t), pageSize);
#else
				DoubleEndedStackAllocator alloc(sizeof(uint32_t) * 3 + 6 * canarySize + 3 * metaSize);
#endif
				Tests::Test_Case_Failure("Verify fail on Front Overlaps Back", [&alloc, allocSize]()
				{
					alloc.AllocateBack(allocSize, 1);
					alloc.AllocateBack(allocSize, 1);
					alloc.Allocate(allocSize, 1);
					void* alloc4 = alloc.Allocate(allocSize, 1);
					return alloc4 != nullptr;
				}());
			}
			{
#if HTL_ALLOW_GROW
				DoubleEndedStackAllocator alloc(sizeof(uint32_t), pageSize);
#else
				DoubleEndedStackAllocator alloc(sizeof(uint32_t));
#endif
				Tests::Test_Case_Failure("Verify fail on Front Oversize", [&alloc]()
				{
					void* alloc1 = alloc.Allocate(sizeof(uint64_t), 1);
					return alloc1 != nullptr;
				}());
			}
			{
				DoubleEndedStackAllocator alloc(sizeof(uint32_t) * 2);
				Tests::Test_Case_Failure("Verify fail on Front Overlaps End (MultiAlloc)", [&alloc]()
				{
					alloc.Allocate(sizeof(uint32_t), 1);
					void* alloc1 = alloc.Allocate(sizeof(uint32_t), 1);
					return alloc1 != nullptr;
				}());
			}
			{
				DoubleEndedStackAllocator alloc(sizeof(uint32_t) * 3 + 6 * canarySize + 3 * metaSize);
				Tests::Test_Case_Failure("Verify fail on Back Overlaps Front", [&alloc]()
				{
					alloc.Allocate(sizeof(uint32_t), 1);
					alloc.Allocate(sizeof(uint32_t), 1);
					alloc.AllocateBack(sizeof(uint32_t), 1);
					void* alloc4 = alloc.AllocateBack(sizeof(uint32_t), 1);
					return alloc4 != nullptr;
				}());
			}
			{
				DoubleEndedStackAllocator alloc(sizeof(short));
				Tests::Test_Case_Failure("Verify fail on back Oversize", [&alloc]()
				{
					void* alloc1 = alloc.AllocateBack(sizeof(uint32_t), 1);
					return alloc1 != nullptr;
				}());
			}
			{
				DoubleEndedStackAllocator alloc(sizeof(uint32_t) * 2);
				Tests::Test_Case_Failure("Verify fail on Back Overlaps Begin (MultiAlloc)", [&alloc]()
				{
					alloc.AllocateBack(sizeof(uint32_t), 1);
					void* alloc1 = alloc.AllocateBack(sizeof(uint32_t), 1);
					return alloc1 != nullptr;
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Failure("Verify fail on free of invalid memory pointer (nullptr)", [&alloc]()
				{
					void* ptr = alloc.Allocate(sizeof(uint32_t), 1);
					alloc.Free(nullptr);
					return ptr != alloc.Front();
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Failure("Verify fail on free of invalid memory pointer (OutOfBounds)", [&alloc]()
				{
					void* ptr = alloc.Allocate(sizeof(uint32_t), 1);
					void* helper = ptr;
					helper = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(helper) + 1);
					alloc.Free(helper);
					return ptr != alloc.Front();
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Failure("Verify fail on free of invalid memory pointer (LIFO Validation)", [&alloc]()
				{
					void* alloc1 = alloc.Allocate(sizeof(uint32_t), 2);
					void* alloc2 = alloc.Allocate(sizeof(uint32_t), 2);
					alloc.Free(alloc1);
					return alloc2 != alloc.Front();
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Failure("Verify fail on free back of invalid memory pointer (nullptr)", [&alloc]()
				{
					void* ptr = alloc.AllocateBack(sizeof(uint32_t), 1);
					alloc.FreeBack(nullptr);
					return ptr != alloc.Back();
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Failure("Verify fail on free back of invalid memory pointer (OutOfBounds)", [&alloc]()
				{
					void* ptr = alloc.AllocateBack(sizeof(uint32_t), 1);
					void* helper = ptr;
					helper = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(helper) + 1);
					alloc.FreeBack(helper);
					return ptr != alloc.Back();
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Failure("Verify fail on free back of invalid memory pointer (LIFO Validation)", [&alloc]()
				{
					void* alloc1 = alloc.AllocateBack(sizeof(uint32_t), 2);
					void* alloc2 = alloc.AllocateBack(sizeof(uint32_t), 2);
					alloc.FreeBack(alloc1);
					return alloc2 != alloc.Back();
				}());
			}
#if WITH_DEBUG_CANARIES
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Failure("Verify fail on free cause canaries were overwritten", [&alloc]()
				{
					void* ptr = alloc.Allocate(sizeof(uint32_t), 1);
					uint32_t* helper = reinterpret_cast<uint32_t*>(ptr);
					helper[1] = 0x00;
					alloc.Free(ptr);
					return ptr == alloc.Front();
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Failure("Verify fail on free back cause canaries were overwritten", [&alloc]()
				{
					void* ptr = alloc.AllocateBack(sizeof(uint32_t), 1);
					uint32_t* helper = reinterpret_cast<uint32_t*>(ptr);
					helper[1] = 0x00;
					alloc.FreeBack(ptr);
					return ptr == alloc.Back();
				}());
			}
#endif // WITH_DEBUG_CANARIES

		}
#endif // _DEBUG

	}

	// Here the assignment tests will happen - it will test basic allocator functionality.
	{

	}
}