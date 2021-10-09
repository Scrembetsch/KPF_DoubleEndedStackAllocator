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

#include <stdlib.h>
#include <cassert>
#include <cstdint>

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
	{
		// TODO:
		//	- VirtualAlloc
		//	- check if begin and end are clean?

		// reserve memory
		mBegin = reinterpret_cast<uintptr_t>(malloc(max_size));
		mEnd = mBegin + max_size;

		#ifdef WITH_DEBUG_OUTPUT
			printf("constructed allocator from \n[%llx] to\n[%llx]\n", mBegin, mEnd);
			printf("size: [%llu]\n", max_size);
			printf("diff: [%llu]\n", mEnd - mBegin);
		#endif
	}

	void* Allocate(size_t size, size_t alignment)
	{
		assert(IsPowerOf2(alignment));
		// TODO: return nullptr if failed

		// TODO: align

		return nullptr;
	}
	void* AllocateBack(size_t size, size_t alignment)
	{
		return nullptr;
	}

	void Free(void* memory)
	{
		// TODO: check if pointer is valid?

		// TODO: reposition pointer and free memory
	}

	void FreeBack(void* memory) {}

	void Reset(void)
	{
		// TODO: while Free()
	}

	~DoubleEndedStackAllocator(void)
	{
		// give reserved memory back to system
		free((void *)mBegin);
	}

private:
	// power of 2 always has exactly 1 bit set in binary representation (for signed values)
	bool IsPowerOf2(size_t val)
	{
		return val > 0 && !(val & (val - 1));
	}

#ifdef WITH_DEBUG_CANARIES
	const ptrdiff_t CANARY_SIZE = 4;
#else
	const ptrdiff_t CANARY_SIZE = 0;
#endif

	const ptrdiff_t META_SIZE = 4; // contains allocated space size
	const uint32_t CANARY = 0xDEADC0DE;

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

	uintptr_t AlignUp()
	{
		return 0x0;
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
		{
			DoubleEndedStackAllocator allocator(1);
		}

		// You can remove this, just showcasing how the test functions can be used
		DoubleEndedStackAllocator allocator(1024u);
		Tests::Test_Case_Success("Allocate() returns nullptr", [&allocator](){ return allocator.Allocate(32, 1) == nullptr; }());

		{
			// TODO: temp tests, remove
			// success
			printf("allocator.Allocate(0, 1)...\n");
			allocator.Allocate(0, 1);
			printf("allocator.Allocate(0, 2)...\n");
			allocator.Allocate(0, 2);

			// fail
			#ifndef _DEBUG
				printf("allocator.Allocate(0, 0)...\n");
				allocator.Allocate(0, 0);
				printf("allocator.Allocate(0, 3)...\n");
				allocator.Allocate(0, 3);
			#endif
		}

	}

	// You can do whatever you want here in the main function

	// Here the assignment tests will happen - it will test basic allocator functionality.
	{

	}
}