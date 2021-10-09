/**
* Exercise: "DoubleEndedStackAllocator with Canaries" OR "Growing DoubleEndedStackAllocator with Canaries (VMEM)"
* Group members: Handl Anja (gs20m005), Tributsch Harald (gs20m008), Leithner Michael (gs20m012)
* 
* open issues:
* ============
* - virtual memory allocation for growing VMEM
*		Michael
* - allocate, align
*		Harald
* - free, reset
*		Anja
**/

#include "stdio.h"
#include "inttypes.h"
#include <stdlib.h>
#include <cassert>
#include <cstdint>
#include <string>
#include <malloc.h>

namespace Tests
{
	void Test_Case_Success(const char* name, bool passed)
	{
		printf("[%s] %s the test!\n", name, passed ? "passed" : "failed");
	}

	void Test_Case_Failure(const char* name, bool passed)
	{
		printf("[%s] %s the test!\n", name, !passed ? "passed" : "failed");
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
			printf("[Error]: Allocator returned nullptr!\n");
			return false;
		}

		return true;
	}
}

// Assignment functionality tests are going to be included here

#define WITH_DEBUG_CANARIES

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
//#define HTL_WITH_DEBUG_OUTPUT

	DoubleEndedStackAllocator(size_t max_size)
		: mNumFrontAllocs(0)
		, mNumBackAllocs(0)
	{
		// TODO:
		//	- VirtualAlloc
		//	- check if begin and end are clean?

		static_assert(sizeof(size_t) == sizeof(uintptr_t), "Size mismatch of size_t and uintptr_t");

		// reserve memory
		mBegin = mFront = reinterpret_cast<uintptr_t>(malloc(max_size));
		mEnd = mBack = mBegin + max_size;

		#ifdef HTL_WITH_DEBUG_OUTPUT
			printf("constructed allocator from \n[%" PRIxPTR "] to\n[%" PRIxPTR "]\n", mBegin, mEnd);
			printf("size: [%" PRIxPTR "]\n", max_size);
			printf("diff: [%" PRIxPTR "]\n", mEnd - mBegin);
		#endif
	}

	void* Allocate(size_t size, size_t alignment)
	{
		assert(IsPowerOf2(alignment));

		uintptr_t lastItem = mFront;

		if (mNumFrontAllocs != 0)
		{
			MetaData* meta = reinterpret_cast<MetaData*>(mFront - META_SIZE);
			mFront = mFront + meta->Size + CANARY_SIZE;
		}

		uintptr_t alignedAddress = AlignUp(mFront, alignment);

		bool overlap = false;
		if (mNumBackAllocs == 0)
		{
			overlap |= (alignedAddress + size + CANARY_SIZE) >= mBack;
		}
		else
		{
			overlap |= (alignedAddress + size + CANARY_SIZE) >= (mBack - META_SIZE - CANARY_SIZE);
		}

		if (overlap)
		{
			assert(!"Front Stack overlaps with Back Stack");
			return nullptr;
		}
		mFront = alignedAddress;
		WriteBeginCanary(alignedAddress);
		WriteMeta(alignedAddress, lastItem, size);
		WriteEndCanary(alignedAddress, size);
		
		++mNumFrontAllocs;
		
		return reinterpret_cast<void*>(mFront);
	}
	void* AllocateBack(size_t size, size_t alignment)
	{
		assert(IsPowerOf2(alignment));

		uintptr_t lastItem = mBack;

		if (mNumBackAllocs != 0)
		{
			MetaData* meta = reinterpret_cast<MetaData*>(mBack - META_SIZE);
			mBack = mBack - meta->Size - CANARY_SIZE;
		}

		uintptr_t alignedAddress = AlignDown(mBack, alignment, size);

		bool overlap = false;
		if (mNumFrontAllocs == 0)
		{
			overlap |= mBack - META_SIZE - CANARY_SIZE <= mFront;
		}
		else
		{
			overlap |= (mBack - META_SIZE - CANARY_SIZE) <= (mFront + GetMetaData(mFront)->Size + CANARY_SIZE);
		}

		if (overlap)
		{
			assert(!"Back Stack overlaps with Front Stack");
			return nullptr;
		}

		mBack = alignedAddress;
		WriteBeginCanary(alignedAddress);
		WriteMeta(alignedAddress, lastItem, size);
		WriteEndCanary(alignedAddress, size);

		++mNumBackAllocs;

		return reinterpret_cast<void*>(mBack);
	}

	void Free(void* memory)
	{
		ValidateMemoryPoiter(memory);

		// LIFO Check
		if (reinterpret_cast<uintptr_t>(memory) != mFront)
		{
			assert(!"Pointer doesn't match last allocated memory, couldn't free memory");
		}

		MetaData* currentMetadata = GetMetaData(reinterpret_cast<uintptr_t>(memory));

		CheckCanaries(reinterpret_cast<uintptr_t>(memory), currentMetadata->Size);

		// We don't care what the user has written in the memory, therefore we just set the Front to the LastItem and "ignore" the previously allocated memory
		mFront = currentMetadata->LastItem;
		mNumFrontAllocs--;
	}

	void FreeBack(void* memory)
	{
		ValidateMemoryPoiter(memory);

		// LIFO Check
		if (reinterpret_cast<uintptr_t>(memory) != mBack)
		{
			assert(!"Pointer doesn't match last allocated memory, couldn't free memory");
		}

		MetaData* currentMetadata = GetMetaData(reinterpret_cast<uintptr_t>(memory));

		CheckCanaries(reinterpret_cast<uintptr_t>(memory), currentMetadata->Size);

		// We don't care what the user has written in the memory, therefore we just set the Front to the LastItem and "ignore" the previously allocated memory
		mBack = currentMetadata->LastItem;
		mNumBackAllocs--;
	}

	void Reset(void)
	{
		mFront = mBegin;
		mBack = mEnd;
		// TODO: while Free()
	}

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

	~DoubleEndedStackAllocator(void)
	{
		Reset();
		free(reinterpret_cast<void*>(mBegin));
		// TODO: give reserved memory back to system
	}

private:
	// power of 2 always has exactly 1 bit set in binary representation (for signed values)
	bool IsPowerOf2(size_t val)
	{
		return val > 0 && !(val & (val - 1));
	}

	void WriteBeginCanary(uintptr_t alignedAddress)
	{
#ifdef WITH_DEBUG_CANARIES
		uintptr_t canaryAddress = alignedAddress - META_SIZE - CANARY_SIZE;
		*reinterpret_cast<uint32_t*>(canaryAddress) = CANARY;
#endif
	}

	void WriteEndCanary(uintptr_t alignedAddress, size_t size)
	{
#ifdef WITH_DEBUG_CANARIES
		uintptr_t canaryAddress = alignedAddress + size;
		*reinterpret_cast<uint32_t*>(canaryAddress) = CANARY;
#endif
	}

	// Memory Pointer needs to be valid
	void ValidateMemoryPoiter(void* memory)
	{
		if (memory == nullptr)
		{
			assert(!"Invalid Pointer, couldn't free memory");
		}

		if ((reinterpret_cast<uintptr_t>(memory) < mBegin) || (reinterpret_cast<uintptr_t>(memory) > mEnd))
		{
			assert(!"Pointer not in range of reserved space, couldn't free memory");
		}
	}

	// If Canaries are not valid, we're not allowed to free, because something has overwritten them
	void CheckCanaries(uintptr_t alignedAddress, size_t size)
	{
#ifdef WITH_DEBUG_CANARIES
		// Check Begin Canary
		uintptr_t canaryAddress = alignedAddress - META_SIZE - CANARY_SIZE;
		if (*reinterpret_cast<uint32_t*>(canaryAddress) != CANARY)
		{
			assert(!"Invalid Begin Canary");
			printf("[Warning]: Invalid Front Canary!\n");
		}
		// Check End Canary
		canaryAddress = alignedAddress + size;
		if (*reinterpret_cast<uint32_t*>(canaryAddress) != CANARY)
		{
			assert(!"Invalid End Canary");
			printf("[Warning]: Invalid End Canary!\n");
		}
#endif
	}

	void WriteMeta(uintptr_t alignedAddress, uintptr_t lastItem, size_t allocatedSize)
	{
		uintptr_t metaAddress = alignedAddress - META_SIZE;
		*reinterpret_cast<MetaData*>(metaAddress) = MetaData(lastItem, allocatedSize);
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
	static const ptrdiff_t META_SIZE = sizeof(MetaData);
	//const uint32_t CANARY = 0xDEADC0DE;
	static const uint32_t CANARY = 0xDEC0ADDE;

#ifdef WITH_DEBUG_CANARIES
	static const ptrdiff_t CANARY_SIZE = sizeof(CANARY);
#else
	static const ptrdiff_t CANARY_SIZE = 0;
#endif

	//					|	|	|	|
	//					4	8	12	16
	//			[0xCD]<meta>Front

	// [Begin... [0xCD] <meta>Front, <meta>Front......, <meta>Back, [0xCD] ... End] <- memalloc (VirtualAlloc)

	// first reservation 
	// [Begin...															... End]

	// allocate
	// [Begin, [0xCD], <meta>Front, [0xCD]									... End]

	// allocate
	// [Begin, [0xCD], <meta>Front, [0xCD], ... [0xCD], <meta>Front, [0xCD]	... End]

	// free
	// [Begin, [0xCD], <meta>Front, [0xCD], ...								... End]
	//									    ^
	
	// allocateBack
	// [Begin, [0xCD], <meta>Front, [0xCD], ... [0xCD], <meta>Back, [0xCD]	... End]

	// boundaries of our allocation
	uintptr_t mBegin;
	uintptr_t mEnd;

	// decision: (A) using pointer to next/prev free memory or (B) points to user space begin
	// --> (B)
	uintptr_t mFront;
	uintptr_t mBack;
	
	uint32_t mNumFrontAllocs;
	uint32_t mNumBackAllocs;

	uintptr_t AlignUp(uintptr_t address, size_t alignment)
	{
		size_t offsetAddress = address + META_SIZE + CANARY_SIZE;
		if (offsetAddress % alignment == 0)
		{
			return offsetAddress;
		}
		else
		{
			return (offsetAddress + (alignment - (offsetAddress % alignment)));
		}
	}

	uintptr_t AlignDown(uintptr_t address, size_t alignment, size_t allocSize)
	{
		size_t offsetAddress = address - allocSize - CANARY_SIZE;
		return (offsetAddress - (offsetAddress % alignment));
	}

	MetaData* GetMetaData(uintptr_t allocSpacePtr)
	{
		return reinterpret_cast<MetaData*>(allocSpacePtr - META_SIZE);
	}

#undef HTL_WITH_DEBUG_OUTPUT
};
/** TODO
* - reserve (virtual?) memory to be able to grow
*		AlignUp/Down utility functions
* - ability to grow
*		just request new memory and copy?
* - copy and move ctor/operator?
* - allocate
*		throw if overlap, invalid alignment, out of bounds, ...
*		-> assert? throw? just msg and return nullptr?
**/

int main()
{
	// You can add your own tests here, I will call my tests at then end with a fresh instance of your allocator and a specific max_size
	{
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
				Tests::Test_Case_Success("Verify Allocation Alignment", [&alloc]()
				{
					bool ret = true;
					ret &= reinterpret_cast<uintptr_t>(alloc.Allocate(sizeof(uint32_t), 2)) % 2 == 0;
					ret &= reinterpret_cast<uintptr_t>(alloc.Allocate(sizeof(uint32_t), 8)) % 8 == 0;
					ret &= reinterpret_cast<uintptr_t>(alloc.Allocate(sizeof(uint32_t), 64)) % 64 == 0;
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
				Tests::Test_Case_Success("Verify Back Allocation Alignment", [&alloc]()
				{
					bool ret = true;
					ret &= reinterpret_cast<uintptr_t>(alloc.AllocateBack(sizeof(uint32_t), 2)) % 2 == 0;
					ret &= reinterpret_cast<uintptr_t>(alloc.AllocateBack(sizeof(uint32_t), 8)) % 8 == 0;
					ret &= reinterpret_cast<uintptr_t>(alloc.AllocateBack(sizeof(uint32_t), 64)) % 64 == 0;
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
		}
		// Fail tests
#ifndef _DEBUG
		{
			{
				DoubleEndedStackAllocator alloc(sizeof(uint32_t) * 3 + 2 * DoubleEndedStackAllocator::GetCanaraySize() + DoubleEndedStackAllocator::GetMetaSize());
				Tests::Test_Case_Failure("Verfiy fail on Overlap", [&alloc]()
				{
					void* alloc1 = alloc.AllocateBack(sizeof(uint32_t), 1);
					void* alloc2 = alloc.AllocateBack(sizeof(uint32_t), 1);
					void* alloc3 = alloc.Allocate(sizeof(uint32_t), 1);
					void* alloc4 = alloc.Allocate(sizeof(uint32_t), 1);
					return alloc4 != nullptr;
				}());
			}
			{
				DoubleEndedStackAllocator alloc(sizeof(short));
				Tests::Test_Case_Failure("Verfiy fail on Overlap", [&alloc]()
				{
					void* alloc1 = alloc.Allocate(sizeof(uint32_t), 1);
					return alloc1 != nullptr;
				}());
			}
			{
				DoubleEndedStackAllocator alloc(sizeof(uint32_t) * 3 + 2 * DoubleEndedStackAllocator::GetCanaraySize() + DoubleEndedStackAllocator::GetMetaSize());
				Tests::Test_Case_Failure("Verfiy fail on Back Overlap", [&alloc]()
				{
					void* alloc1 = alloc.Allocate(sizeof(uint32_t), 1);
					void* alloc2 = alloc.Allocate(sizeof(uint32_t), 1);
					void* alloc3 = alloc.AllocateBack(sizeof(uint32_t), 1);
					void* alloc4 = alloc.AllocateBack(sizeof(uint32_t), 1);
					return alloc4 != nullptr;
				}());
			}
			{
				DoubleEndedStackAllocator alloc(sizeof(short));
				Tests::Test_Case_Failure("Verfiy fail on Overlap", [&alloc]()
				{
					void* alloc1 = alloc.AllocateBack(sizeof(uint32_t), 1);
					return alloc1 != nullptr;
				}());
			}
		}
#endif
	}

	// You can do whatever you want here in the main function

	// Here the assignment tests will happen - it will test basic allocator functionality.
	{

	}
}