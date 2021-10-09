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
		printf("[%s] [%s] the test!\n", name, passed ? "passed" : "failed");
	}

	void Test_Case_Failure(const char* name, bool passed)
	{
		printf("[%s] [%s] the test!\n", name, !passed ? "passed" : "failed");
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

#define WITH_DEBUG_CANARIES 0
#define WITH_DEBUG_OUTPUT 0

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

		#ifdef WITH_DEBUG_OUTPUT
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
		if ((alignedAddress + size + CANARY_SIZE) > (mBack - META_SIZE - CANARY_SIZE))
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
		mBack -= size + 2 * CANARY_SIZE + META_SIZE;
		++mNumBackAllocs;
		return nullptr;
	}

	void Free(void* memory)
	{
		mFront = mFront + (mFront - reinterpret_cast<uintptr_t>(memory));
		// TODO: check if pointer is valid?

		// TODO: reposition pointer and free memory
	}

	void FreeBack(void* memory)
	{
		mBack = mBack + (mBack - reinterpret_cast<uintptr_t>(memory));
	}

	void Reset(void)
	{
		mFront = mBegin;
		mBack = mEnd;
		// TODO: while Free()
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
	const ptrdiff_t META_SIZE = sizeof(MetaData);
	//const uint32_t CANARY = 0xDEADC0DE;
	const uint32_t CANARY = 0xDEC0ADDE;

#ifdef WITH_DEBUG_CANARIES
	const ptrdiff_t CANARY_SIZE = sizeof(CANARY);
#else
	const ptrdiff_t CANARY_SIZE = 0;
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

	uintptr_t AlignUp(uintptr_t current, size_t alignment)
	{
		size_t aligment_mask = ~(alignment - 1);
		size_t offset = CANARY_SIZE + META_SIZE;
		uintptr_t aligned_address = (offset + current + alignment) & aligment_mask;
		return aligned_address;
	}

	uintptr_t AlignDown()
	{
		return 0x0;
	}
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
				Tests::Test_Case_Success("Verify Allocation Success: ", [&alloc]()
				{
					return alloc.Allocate(sizeof(uint32_t), 1) != nullptr;
				}());
			}
			{
				DoubleEndedStackAllocator alloc(1024U);
				Tests::Test_Case_Success("Verify Allocation Alignment: ", [&alloc]()
				{
					bool ret = true;
					ret &= reinterpret_cast<uintptr_t>(alloc.Allocate(sizeof(uint32_t), 2)) % 2 == 0;
					ret &= reinterpret_cast<uintptr_t>(alloc.Allocate(sizeof(uint32_t), 8)) % 8 == 0;
					ret &= reinterpret_cast<uintptr_t>(alloc.Allocate(sizeof(uint32_t), 64)) % 64 == 0;
					return ret;
				}());
			}
		}
		// Fail tests
		{
			{

			}
		}

		// Assert tests
		{
			{

			}
		}
	}

	// You can do whatever you want here in the main function

	// Here the assignment tests will happen - it will test basic allocator functionality.
	{

	}
}